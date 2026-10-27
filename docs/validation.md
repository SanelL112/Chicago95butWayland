# Validation after the review fixes

Run `make -C src/custom-components` and `python3 tests/regression.py`, then
`./tests/run_tests.sh`. The smoke test has its own HOME, runtime directory,
D-Bus session and process group. It never moves the user's desktop.conf and
cannot replace the active desktop/taskbar. `./scripts/benchmark.sh` uses the
same isolation and measures only its own compositor process group.

The smoke test covers XML/assets, clipboard roundtrip, three dialog processes
remaining alive, and the basic desktop/taskbar palette. It does not automate
Start menu input, minimize/restore, physical outputs or secure-lock behavior.
A skip is not a live-test pass. Exit 77 from the isolation helper or benchmark
means the environment cannot host a private compositor socket.

On an unrestricted test session, validate these before relying on the changes:

1. Open/close Start and Control Center; launch multiple applications, minimize,
   restore, maximize, and close them from task buttons and context menus.
2. Leave a task context menu open while its application exits. The menu should
   close and the taskbar should remain running.
3. Set icons to primary only, then attach/remove an output. The selection must
   remain applied; no stale menu or taskbar surfaces should remain.
4. Lock with Super+L. Verify all outputs are covered, a newly attached output
   reveals no session content, and ordinary application input is blocked.
   In a disposable test session, terminate swaylock and verify the compositor
   stays locked. Recovery may require ending that test session.
5. Verify password authentication and the before-sleep readiness handshake;
   suspend/resume must not reveal the desktop before authentication.
6. Verify Wi-Fi names with colons/backslashes and passwords containing quotes,
   dollar signs and backticks are submitted literally.
7. Start and exit the session with GTK files originally present and absent;
   verify the original settings and symlinks are restored. SIGKILL recovery
   is handled by the next launch or scripts/restore.sh after the session ends.

The custom GTK dialog now uses gtk-session-lock surfaces. Howdy face authentication uses the packaged face-only PAM policy and the existing
system helper. Test a face match, a miss, password fallback, and cancellation
without leaving the camera worker running. `chicago-lock --swaylock` selects the maintained fallback.
Window capture is explicitly a manual region fallback. The installer downloads
missing theme/icon assets only when run, or accepts an offline CHICAGO95_SOURCE.
