#!/usr/bin/env python3
"""Regression checks using temporary homes and mock tools; no active GUI needed."""
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import time
import unittest

REPO = Path(__file__).resolve().parents[1]


class Regression(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.lock_prefix = Path(os.environ.get('CHICAGO_LOCK_PREFIX', str(Path.home()/'.local/share/chicago-wayland/deps/gtk-session-lock')))
        cls.build = tempfile.TemporaryDirectory(prefix='chicago-regression-build-')
        cls.helper = Path(cls.build.name) / 'helper'
        source = cls.helper.with_suffix('.c')
        source.write_text(r'''
#include "chicago_common.h"
int main(int argc, char **argv) {
    if (!strcmp(argv[1], "parse")) {
        char **p = chicago_parse_wifi_row("*:Lab\\:Net\\\\Name:99:WPA2");
        g_assert_cmpstr(p[0], ==, "*");
        g_assert_cmpstr(p[1], ==, "Lab:Net\\Name");
        g_assert_cmpstr(p[2], ==, "99");
        g_assert_cmpstr(p[3], ==, "WPA2");
        g_strfreev(p);
        p = chicago_parse_wifi_row("bad");
        g_assert_cmpstr(p[3], ==, "");
        g_strfreev(p);
    } else if (!strcmp(argv[1], "wifi")) {
        chicago_wifi_connect(argv[2], argv[3]);
        g_usleep(300000);
    } else {
        chicago_single_instance_lock("regression-shell");
        puts("ready"); fflush(stdout);
        sleep(10);
    }
    return argc < 2;
}
''')
        flags = shlex.split(subprocess.check_output([
            'pkg-config', '--cflags', '--libs', 'gtk+-3.0', 'gtk-layer-shell-0',
            'gio-unix-2.0', 'wayland-client'], text=True))
        subprocess.run(['cc', '-Wall', '-Wextra', '-I'+str(REPO/'src/custom-components/include'),
                        str(source), '-o', str(cls.helper), *flags], check=True)

        auth = REPO/'src/custom-components/src/chicago_lock_auth.c'
        includes = '-I'+str(REPO/'src/custom-components/include')
        cls.auth_test = Path(cls.build.name)/'lock-auth'
        subprocess.run(['cc','-Wall','-Wextra',includes,str(REPO/'tests/lock_auth.c'),str(auth),
                        '-o',str(cls.auth_test)],check=True)
        face_fixture = Path(cls.build.name)/'chicago-lock-face'
        face_fixture.write_text("#!/usr/bin/python3\nimport os,signal,subprocess,sys,time\nchild=subprocess.Popen(['/usr/bin/sleep','30'])\nsignal.signal(signal.SIGTERM,lambda *_: (child.wait(),sys.exit(1)))\nwith open(os.environ['MOCK_FACE_LOG'],'w') as log:\n log.write(f'{os.getpid()} {child.pid}\\n')\ntime.sleep(30)\n")
        face_fixture.chmod(0o755)
        cls.lifecycle_test = Path(cls.build.name)/'lock-lifecycle'
        pkg_env = os.environ.copy()
        pkg_env['PKG_CONFIG_PATH'] = str(cls.lock_prefix/'lib/pkgconfig')+':'+pkg_env.get('PKG_CONFIG_PATH','')
        gtk_flags = shlex.split(subprocess.check_output([
            'pkg-config','--cflags','--libs','gtk-session-lock-0','gtk+-3.0','gio-unix-2.0','wayland-client'],
            env=pkg_env,text=True))
        subprocess.run(['cc','-Wall','-Wextra',includes,str(REPO/'tests/lock_lifecycle.c'),str(auth),
                        '-o',str(cls.lifecycle_test),*gtk_flags,'-lpam',
                        '-Wl,-rpath,'+str(REPO/'lib'),
                        '-Wl,--wrap=gtk_session_lock_lock_unlock_and_destroy',
                        '-Wl,--wrap=gtk_session_lock_lock_destroy','-Wl,--wrap=gdk_display_sync'],check=True)

    @classmethod
    def tearDownClass(cls):
        cls.build.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='chicago-regression-')
        self.base = Path(self.tmp.name)
        self.home = self.base/'home'
        self.home.mkdir()
        self.mock = self.base/'mock'
        self.mock.mkdir()
        runtime = self.base/'runtime'
        runtime.mkdir(mode=0o700)
        self.env = os.environ.copy()
        self.env.update(HOME=str(self.home), PATH=str(self.mock)+':/usr/bin:/bin',
                        XDG_CONFIG_HOME=str(self.home/'.config'),
                        XDG_DATA_HOME=str(self.home/'.local/share'),
                        XDG_STATE_HOME=str(self.home/'.local/state'),
                        XDG_CACHE_HOME=str(self.home/'.cache'),
                        XDG_RUNTIME_DIR=str(runtime), WAYLAND_DISPLAY='wayland-test',
                        CHICAGO_RUNTIME_DIR=str(self.base/'no-runtime'),
                        CHICAGO_LOCK_PREFIX=str(self.lock_prefix),
                        MOCK_FACE_LOG=str(self.base/'face-worker.log'),
                        MOCK_LOG=str(self.base/'args.json'), MOCK_STATE=str(self.base/'settings.json'))
        self.tool('labwc', '#!/bin/sh\nexit "${MOCK_LABWC_STATUS:-0}"\n')
        self.tool('gsettings', '''#!/usr/bin/python3
import json, os, sys
from pathlib import Path
p=Path(os.environ['MOCK_STATE'])
state=json.loads(p.read_text()) if p.exists() else {}
key=' '.join(sys.argv[2:4])
if sys.argv[1]=='get':
 print(state.get(key, "'Original'"))
else:
 state[key]=sys.argv[4];p.write_text(json.dumps(state))
''')

    def tearDown(self):
        self.tmp.cleanup()

    def tool(self, name, text):
        f=self.mock/name
        f.write_text(text)
        f.chmod(0o755)
        return f

    def run_cmd(self, *args, check=True, **kwargs):
        try:
            return subprocess.run([str(x) for x in args], env=self.env, capture_output=True,
                                  text=True, timeout=10, check=check, **kwargs)
        except subprocess.CalledProcessError as exc:
            self.fail(f"Command failed: {args}\n{exc.stdout}\n{exc.stderr}")

    def launch(self, **kwargs):
        return self.run_cmd('bash', REPO/'bin/start-chicago-wayland', **kwargs)

    def test_absent_gtk_files_stay_absent(self):
        self.launch()
        for ver in ['gtk-3.0','gtk-4.0']:
            self.assertEqual(list((self.home/'.config'/ver).iterdir()), [])
        self.assertFalse((self.home/'.local/state/chicago-wayland/gtk-swapped.lock').exists())

    def test_existing_configs_and_all_gsettings_restore_on_compositor_failure(self):
        settings = {'org.gnome.desktop.interface gtk-theme': "'Chicago95'",
                    'org.gnome.desktop.interface font-name': "'Font with spaces 11'",
                    'org.gnome.desktop.wm.preferences button-layout': "'close:'"}
        for schema,key,*_ in [line.split() for line in (REPO/'config/gtk/gsettings.tsv').read_text().splitlines()]:
            settings.setdefault(schema+' '+key, "'Original'")
        Path(self.env['MOCK_STATE']).write_text(json.dumps(settings))
        gtk=self.home/'.config/gtk-3.0'
        gtk.mkdir(parents=True)
        (gtk/'settings.ini').write_text('original\n')
        target=self.base/'original.css'
        target.write_text('original css')
        (gtk/'gtk.css').symlink_to(target)
        self.env['MOCK_LABWC_STATUS']='7'
        self.assertEqual(self.launch(check=False).returncode,7)
        self.assertEqual((gtk/'settings.ini').read_text(),'original\n')
        self.assertTrue((gtk/'gtk.css').is_symlink())
        self.assertEqual(target.read_text(),'original css')
        self.assertEqual(json.loads(Path(self.env['MOCK_STATE']).read_text()), settings)
        # A second session after user deletion must not resurrect stale backups.
        (gtk/'settings.ini').unlink()
        self.launch(check=False)
        self.assertFalse((gtk/'settings.ini').exists())

    def test_partial_setup_failure_restores_files(self):
        # Source the actual transaction with a deliberately incomplete config tree.
        root=self.base/'broken'
        (root/'config/gtk').mkdir(parents=True)
        shutil.copy(REPO/'config/gtk/gsettings.tsv', root/'config/gtk')
        shutil.copy(REPO/'config/gtk/settings.ini', root/'config/gtk')
        self.env['REPO_DIR']=str(root)
        self.run_cmd('bash','-c', 'set -e; source "$1"; trap restore_gtk_configs EXIT; apply_session_gtk_configs',
                     'test',REPO/'scripts/session-settings.sh',check=False)
        self.assertFalse((self.home/'.config/gtk-3.0/settings.ini').exists())
        self.assertFalse((self.home/'.local/state/chicago-wayland/gtk-swapped.lock').exists())

    def test_legacy_settings_snapshot_is_read_as_data(self):
        state=self.home/'.local/state/chicago-wayland'
        state.mkdir(parents=True)
        (state/'gtk-swapped.lock').touch()
        (state/'gtk-backup').mkdir()
        marker=self.base/'legacy-injected'
        value='$(touch '+str(marker)+')'
        (state/'saved_gsettings.sh').write_text(
            "#!/bin/bash\ngsettings set org.gnome.desktop.interface font-name '"+value+"'\n")
        self.run_cmd('bash', REPO/'scripts/restore.sh')
        self.assertEqual(json.loads(Path(self.env['MOCK_STATE']).read_text())[
            'org.gnome.desktop.interface font-name'],value)
        self.assertFalse(marker.exists())
        self.assertFalse((state/'saved_gsettings.sh').exists())

    def test_user_install_in_clean_home(self):
        upstream=self.base/'upstream'
        for name in ['Theme/Chicago95/gtk-3.0/gtk.css','Icons/Chicago95/index.theme']:
            p=upstream/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
        self.env['CHICAGO95_SOURCE']=str(upstream)
        for name in ['make','fc-cache','ldd','swaylock','swayidle','grim','slurp','wl-copy','wl-paste','wlr-randr']:
            self.tool(name,'#!/bin/sh\nexit 0\n')
        result=self.run_cmd('bash',REPO/'install.sh','--user')
        self.assertIn('0 failed',result.stdout)
        for name in ['chicago-lock','chicago-lock-custom','chicago-lock-face','chicago-capture','chicago-taskbar','toggle-scratchpad-terminal.sh','start-chicago-wayland']:
            self.assertEqual((self.home/'.local/bin'/name).resolve(),REPO/'bin'/name)
        entry=(self.home/'.local/share/wayland-sessions/chicago-wayland.desktop').read_text()
        self.assertIn('TryExec='+str(self.home/'.local/bin/start-chicago-wayland'),entry)
        self.assertFalse((self.home/'.config/gtk-3.0/settings.ini').exists())

    def test_nmcli_escaped_fields(self):
        self.run_cmd(self.helper,'parse')

    def test_wifi_arguments_are_literal(self):
        self.tool('nmcli', '''#!/usr/bin/python3
import json,os,sys
open(os.environ['MOCK_LOG'],'w').write(json.dumps(sys.argv[1:]))
''')
        marker=self.base/'injected'
        ssid='$(touch '+str(marker)+') " Lab:Net'
        password='p$a`ss`"word\\'
        self.run_cmd(self.helper,'wifi',ssid,password)
        self.assertEqual(json.loads(Path(self.env['MOCK_LOG']).read_text()),
                         ['device','wifi','connect',ssid,'password',password])
        self.assertFalse(marker.exists())

    def test_singletons_are_display_scoped_and_do_not_kill(self):
        first=subprocess.Popen([str(self.helper),'lock'],env=self.env,stdout=subprocess.PIPE,text=True)
        second=None
        try:
            self.assertEqual(first.stdout.readline().strip(),'ready')
            duplicate=self.run_cmd(self.helper,'lock')
            self.assertEqual(duplicate.stdout,'')
            self.assertIsNone(first.poll())
            env=self.env.copy();env['WAYLAND_DISPLAY']='other-display'
            second=subprocess.Popen([str(self.helper),'lock'],env=env,stdout=subprocess.PIPE,text=True)
            self.assertEqual(second.stdout.readline().strip(),'ready')
            self.assertIsNone(first.poll())
        finally:
            for proc in (first,second):
                if proc:
                    proc.terminate();proc.wait(timeout=3);proc.stdout.close()

    def test_secure_lock_delegation_and_error_propagation(self):
        self.tool('swaylock','''#!/usr/bin/python3
import json,os,sys
open(os.environ['MOCK_LOG'],'w').write(json.dumps(sys.argv[1:]))
sys.exit(9)
''')
        result=self.run_cmd(REPO/'bin/chicago-lock','--swaylock',check=False)
        self.assertEqual(result.returncode,9)
        args=json.loads(Path(self.env['MOCK_LOG']).read_text())
        self.assertIn('--daemonize',args)
        self.assertEqual(args[:2],['--config','/dev/null'])

    def test_custom_lock_authentication_and_secret_handling(self):
        self.run_cmd(self.auth_test)

    def test_custom_lock_lifecycle_fails_closed(self):
        try:
            self.run_cmd(self.lifecycle_test)
        finally:
            log=Path(self.env['MOCK_FACE_LOG'])
            if log.exists():
                import signal
                leader=int(log.read_text().split()[0])
                try:
                    args=Path(f'/proc/{leader}/cmdline').read_bytes().split(bytes([0]))
                    if str(Path(self.build.name)/'chicago-lock-face').encode() in args:
                        os.killpg(leader,signal.SIGKILL)
                except (FileNotFoundError,ProcessLookupError):
                    pass

    def test_custom_lock_without_display_never_reports_ready(self):
        self.env.pop('WAYLAND_SOCKET', None)
        result=self.run_cmd(REPO/'bin/chicago-lock-custom','--foreground',check=False)
        self.assertEqual(result.returncode,78)
        self.tool('swaylock', '#!/bin/sh\nexit 9\n')
        result=self.run_cmd(REPO/'bin/chicago-lock',check=False)
        self.assertEqual(result.returncode,9)

    def test_custom_lock_fallback_only_before_request(self):
        launcher=self.base/'chicago-lock'
        shutil.copy2(REPO/'bin/chicago-lock',launcher)
        backend=self.base/'chicago-lock-custom'
        self.tool('swaylock', '#!/bin/sh\necho fallback > "$MOCK_LOG"\nexit 9\n')
        for code in (78,127,1,0):
            backend.write_text('#!/bin/sh\nexit '+str(code)+'\n');backend.chmod(0o755)
            log=Path(self.env['MOCK_LOG'])
            log.unlink(missing_ok=True)
            result=self.run_cmd(launcher,check=False)
            self.assertEqual(result.returncode,9 if code in (78,127) else code)
            self.assertEqual(log.exists(),code in (78,127))
        backend.write_text('#!/bin/sh\nkill -TERM $$\n')
        log.unlink(missing_ok=True)
        result=self.run_cmd(launcher,check=False)
        self.assertEqual(result.returncode,1)
        self.assertFalse(log.exists())

    def test_no_auto_face_option_reaches_backend(self):
        launcher=self.base/'chicago-lock'
        shutil.copy2(REPO/'bin/chicago-lock',launcher)
        backend=self.base/'chicago-lock-custom'
        backend.write_text('#!/usr/bin/python3\nimport json,os,sys\nopen(os.environ["MOCK_LOG"],"w").write(json.dumps(sys.argv[1:]))\n')
        backend.chmod(0o755)
        self.run_cmd(launcher,'--no-auto-face','--foreground')
        self.assertEqual(json.loads(Path(self.env['MOCK_LOG']).read_text()),['--foreground','--no-auto-face'])

    def capture_mocks(self):
        self.tool('grim','''#!/usr/bin/python3
import os,sys
from pathlib import Path
p=Path(sys.argv[-1]);p.write_bytes(b'image bytes')
open(os.environ['MOCK_LOG'],'w').write(str(p))
sys.exit(int(os.environ.get('MOCK_GRIM_STATUS','0')))
''')
        self.tool('wl-copy','''#!/usr/bin/python3
import os,sys
from pathlib import Path
Path(os.environ['MOCK_LOG']+'.clipboard').write_bytes(sys.stdin.buffer.read())
''')
        self.tool('notify-send','#!/bin/sh\nexit 0\n')
        self.tool('canberra-gtk-play','#!/bin/sh\nexit 0\n')

    def test_clipboard_only_file_is_removed_after_read(self):
        self.capture_mocks()
        self.run_cmd(REPO/'bin/chicago-capture','--full','--clipboard-only')
        path=Path(Path(self.env['MOCK_LOG']).read_text())
        self.assertFalse(path.exists())
        self.assertEqual(Path(self.env['MOCK_LOG']+'.clipboard').read_bytes(),b'image bytes')

    def test_capture_failure_removes_temp_file(self):
        self.capture_mocks();self.env['MOCK_GRIM_STATUS']='1'
        result=self.run_cmd(REPO/'bin/chicago-capture','--full','--clipboard-only',check=False)
        self.assertNotEqual(result.returncode,0)
        self.assertFalse(Path(Path(self.env['MOCK_LOG']).read_text()).exists())

    def test_saved_captures_do_not_overwrite_each_other(self):
        self.capture_mocks()
        self.run_cmd(REPO/'bin/chicago-capture','--full')
        first=Path(Path(self.env['MOCK_LOG']).read_text())
        self.run_cmd(REPO/'bin/chicago-capture','--full')
        second=Path(Path(self.env['MOCK_LOG']).read_text())
        self.assertNotEqual(first,second)
        self.assertTrue(first.exists() and second.exists())

    def test_asset_install_without_network(self):
        upstream=self.base/'upstream'
        for name in ['Theme/Chicago95/gtk-3.0/gtk.css','Icons/Chicago95/index.theme']:
            p=upstream/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('fixture')
        self.env['CHICAGO95_SOURCE']=str(upstream)
        self.tool('fc-cache','#!/bin/sh\nexit 0\n')
        self.run_cmd('bash',REPO/'scripts/install-assets.sh')
        data=self.home/'.local/share'
        for name in ['themes/Chicago95/gtk-3.0/gtk.css','icons/Chicago95/index.theme',
                     'themes/Chicago95/labwc/themerc','applications/chicago-capture.desktop',
                     'backgrounds/chicago95/Wallpaper/Setup.png','fonts/chicago-wayland/W95FA.otf']:
            self.assertTrue((data/name).is_file(), name)

    def test_session_entry_path_with_spaces(self):
        # Exercise the actual installer entry generator without installing anything.
        script=(REPO/'install.sh').read_text().split("<<'PY_SESSION'\n",1)[1].split('\nPY_SESSION',1)[0]
        output=self.base/'session.desktop'
        binary=str(self.home/'path with spaces/start-chicago-wayland')
        self.run_cmd('python3','-c',script,REPO/'sessions/chicago-wayland.desktop',output,binary)
        content=output.read_text()
        self.assertIn('Exec="'+binary+'"\n',content)
        self.assertIn('TryExec='+binary+'\n',content)
        self.assertEqual(content.count('\nTryExec='),1)


if __name__ == '__main__':
    unittest.main(verbosity=2)
