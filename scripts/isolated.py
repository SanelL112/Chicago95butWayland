#!/usr/bin/env python3
"""Private HOME, runtime, D-Bus and process group for desktop checks."""
import contextlib
import os
from pathlib import Path
import shutil
import signal
import socket
import subprocess
import sys
import tempfile


@contextlib.contextmanager
def isolated_session():
    with tempfile.TemporaryDirectory(prefix='chicago-session-') as tmp:
        base = Path(tmp)
        home = base / 'home'
        runtime = base / 'runtime'
        home.mkdir()
        runtime.mkdir(mode=0o700)
        env = os.environ.copy()
        original_home = Path.home()
        for name in ('WAYLAND_DISPLAY', 'DISPLAY', 'DBUS_SESSION_BUS_ADDRESS',
                     'XDG_SESSION_DESKTOP', 'CHICAGO_STARTUP_SOUND'):
            env.pop(name, None)
        env.update(HOME=str(home), XDG_RUNTIME_DIR=str(runtime),
                   XDG_CONFIG_HOME=str(home / '.config'),
                   XDG_DATA_HOME=str(home / '.local/share'),
                   XDG_STATE_HOME=str(home / '.local/state'),
                   XDG_CACHE_HOME=str(home / '.cache'),
                   GSETTINGS_BACKEND='memory', CHICAGO_NESTED='1',
                   CHICAGO_RUNTIME_DIR=os.environ.get('CHICAGO_RUNTIME_DIR', str(
                       original_home / '.local/share/chicago-wayland/runtime')))
        # Verify this environment can actually host a Wayland socket first.
        probe = socket.socket(socket.AF_UNIX)
        try:
            probe.bind(str(runtime / 'probe'))
        finally:
            probe.close()
        yield env


def stop_group(proc):
    # Children may outlive the compositor; always reap its entire private group.
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    try:
        proc.wait(timeout=3)
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.wait()
    try:
        os.killpg(proc.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def run(command, timeout=25):
    try:
        with isolated_session() as env:
            if not shutil.which('dbus-run-session'):
                print('[SKIP] dbus-run-session is unavailable', file=sys.stderr)
                return 77
            proc = subprocess.Popen(['dbus-run-session', '--', *command],
                                    env=env, start_new_session=True)
            try:
                return proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                print('Isolated session exceeded its timeout', file=sys.stderr)
                return 124
            finally:
                stop_group(proc)
    except PermissionError as exc:
        print(f'[SKIP] This environment cannot create a private Wayland socket: {exc}',
              file=sys.stderr)
        return 77


if __name__ == '__main__':
    sys.exit(run(sys.argv[1:]))
