#!/usr/bin/python3
"""Private-bus power UI integration and display policy regression tests."""
import importlib.machinery
import importlib.util
import json
import os
from pathlib import Path
import runpy
import shlex
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
policy = runpy.run_path(str(ROOT/'bin/chicago-power'), run_name='power_policy_test')

PROVIDER = r'''
import gi,sys
gi.require_version('Gio','2.0')
from gi.repository import Gio,GLib
name='net.hadess.PowerProfiles'
conn=Gio.bus_get_sync(Gio.BusType.SYSTEM,None)
profile='performance'
xml=''' + repr('''<node><interface name="net.hadess.PowerProfiles"><property name="ActiveProfile" type="s" access="readwrite"/><property name="Profiles" type="aa{sv}" access="read"/></interface></node>''') + r'''
info=Gio.DBusNodeInfo.new_for_xml(xml).interfaces[0]
def get(_conn,_sender,_path,_iface,prop):
 if prop=='ActiveProfile':return GLib.Variant('s',profile)
 return GLib.Variant('aa{sv}',[{'Profile':GLib.Variant('s',p)} for p in ['power-saver','balanced']])
def set_(_conn,_sender,_path,_iface,prop,value):
 global profile
 profile=value.unpack()
 conn.emit_signal(None,'/net/hadess/PowerProfiles','org.freedesktop.DBus.Properties','PropertiesChanged',GLib.Variant('(sa{sv}as)',(name,{'ActiveProfile':GLib.Variant('s',profile)},[])))
 return True
conn.register_object('/net/hadess/PowerProfiles',info,None,get,set_)
conn.call_sync('org.freedesktop.DBus','/org/freedesktop/DBus','org.freedesktop.DBus','RequestName',GLib.Variant('(su)',(name,0)),None,Gio.DBusCallFlags.NONE,3000,None)
print('ready',flush=True)
GLib.MainLoop().run()
'''

class PowerClient(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp=tempfile.TemporaryDirectory();base=Path(cls.tmp.name)
        source=base/'power.c';cls.helper=base/'power'
        source.write_text(r'''
#include "chicago_power.h"
static gboolean done(gpointer loop) { g_main_loop_quit(loop); return G_SOURCE_REMOVE; }
static void changed(GDBusProxy *p,GVariant *v,const gchar * const *i,gpointer data) {
 (void)p;(void)v;(void)i;
 char profile[64];chicago_power_get(profile,sizeof(profile));puts(profile);fflush(stdout);
 g_main_loop_quit(data);
}
int main(int argc,char **argv) {
 if(argc<2)return 2;
 GMainLoop *loop=g_main_loop_new(NULL,FALSE);
 if(!strcmp(argv[1],"get")) { char p[64];chicago_power_get(p,sizeof(p));puts(p);return 0; }
 if(!strcmp(argv[1],"watch")) { g_signal_connect(chicago_power_connect(),"g-properties-changed",G_CALLBACK(changed),loop); puts("ready");fflush(stdout); }
 else if(!strcmp(argv[1],"cycle"))chicago_power_cycle();
 else chicago_power_set(argv[1]);
 g_timeout_add(1500,done,loop);g_main_loop_run(loop);return 0;
}
''')
        flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','gtk+-3.0'],text=True))
        subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(ROOT/'src/custom-components/include'),str(source),'-o',str(cls.helper),*flags],check=True)
    @classmethod
    def tearDownClass(cls):cls.tmp.cleanup()
    def setUp(self):
        self.bus=subprocess.Popen(['dbus-daemon','--session','--nofork','--print-address=1'],stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True)
        address=self.bus.stdout.readline().strip()
        self.env={**os.environ,'DBUS_SYSTEM_BUS_ADDRESS':address}
        self.provider=subprocess.Popen(['python3','-c',PROVIDER],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertEqual(self.provider.stdout.readline().strip(),'ready')
    def tearDown(self):
        for process in [self.provider,self.bus]:
            process.terminate();process.communicate(timeout=3)
    def command(self,*args):return subprocess.check_output(list(map(str,args)),env=self.env,text=True,timeout=5).strip()
    def test_native_client_reads_daemon_not_firmware(self):
        self.assertEqual(self.command(self.helper,'get'),'performance')
    def test_native_cycle_skips_unavailable_performance(self):
        self.command(self.helper,'cycle')
        self.assertEqual(self.command(self.helper,'get'),'power-saver')
        self.command(self.helper,'cycle')
        self.assertEqual(self.command(self.helper,'get'),'balanced')
        self.command(self.helper,'cycle')
        self.assertEqual(self.command(self.helper,'get'),'power-saver')
    def test_cli_and_native_ui_share_state_and_signals(self):
        watch=subprocess.Popen([str(self.helper),'watch'],env=self.env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
        self.assertEqual(watch.stdout.readline().strip(),'ready')
        self.command(ROOT/'bin/chicago-power','set','balanced')
        stdout,stderr=watch.communicate(timeout=3)
        self.assertEqual(watch.returncode,0,stderr)
        self.assertEqual(stdout.strip(),'balanced')
        self.assertEqual(self.command(ROOT/'bin/chicago-power','get'),'balanced')
    def test_invalid_native_request_does_not_touch_service(self):
        self.command(self.helper,'not-a-profile')
        self.assertEqual(self.command(self.helper,'get'),'performance')

class DisplayPolicy(unittest.TestCase):
    def outputs(self,current):
        return [dict(name='DP-1',enabled=True,modes=[dict(width=1920,height=1080,refresh=75,current=True)]),dict(name='eDP-1',enabled=True,position={'x':0,'y':0},scale=1,modes=[dict(width=1920,height=1200,refresh=59.95,preferred=True,current=current==60),dict(width=1920,height=1200,refresh=47.96,current=current==48)])]
    def test_saver_changes_only_internal_refresh(self):
        data=self.outputs(60);before=json.dumps(data,sort_keys=True)
        self.assertEqual(policy['panel_change'](data,True),['wlr-randr','--output','eDP-1','--mode','1920x1200@47.960Hz'])
        self.assertEqual(json.dumps(data,sort_keys=True),before)
    def test_ac_restores_preferred_and_repeated_application_is_noop(self):
        self.assertEqual(policy['panel_change'](self.outputs(48),False)[-1],'1920x1200@59.950Hz')
        self.assertIsNone(policy['panel_change'](self.outputs(48),True))
        self.assertIsNone(policy['panel_change'](self.outputs(60),False))
    def test_disabled_panel_and_external_only_are_untouched(self):
        data=self.outputs(60);data[1]['enabled']=False
        self.assertIsNone(policy['panel_change'](data,True))
        self.assertIsNone(policy['panel_change'](data[:1],True))
    def test_cycle_supports_two_profile_system(self):
        self.assertEqual(policy['next_profile']('balanced',['balanced','power-saver']),'power-saver')

if __name__=='__main__':unittest.main(verbosity=2)
