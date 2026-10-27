# Review fixes — 2026-10-27

The review findings were addressed in the current working tree. Existing source
changes and the W95FA font were retained. No commit was created and the running
desktop session was not restarted.

- Locking now delegates to swaylock with Chicago95 colors and readiness handling.
  The unsafe GTK overlay/PAM/face-auth dialog was removed. Every lock command in
  the repository uses the wrapper.
- Wi-Fi SSIDs/passwords use argument vectors, with escaped nmcli fields parsed
  correctly. The taskbar still attempts saved/open connections before opening
  the password dialog on failure. Desktop file paths are shell-quoted, and file
  location/capture actions use argument vectors.
- Singleton locks are scoped by Wayland display. Duplicate instances exit without
  killing another shell, and child programs cannot inherit the lock descriptor.
- Tests and benchmarks use a private HOME, XDG paths, runtime, D-Bus session and
  process group. Tests never move the user's desktop.conf. Benchmark metrics and
  cleanup are limited to the private compositor group.
- GTK files, original absence, symlinks, and all changed gsettings are backed up
  before modification and restored on exit/failure. Stale backup data is cleared
  only after recovery. Legacy gsettings snapshots are parsed as data, not executed.
- Task context menus close before their borrowed window data is freed.
- Monitor-surface rebuilds reapply the current desktop configuration.
- The installer handles arbitrary checkout locations, includes every command and
  bundled asset, generates a valid user session path, checks build/runtime/PAM
  prerequisites and obtains missing upstream GTK/icon assets (or uses an offline
  CHICAGO95_SOURCE). User-local uninstallation only removes this checkout's links.
- Captures reserve unique private files, wait for clipboard consumers before
  cleanup, remove failed/temporary captures and label window capture as manual
  region selection. Automatic window geometry is not implemented.
- Test labels reflect actual checks; skipped runtime/manual QA is explicit.

Verification: all five programs rebuilt with -Wall -Wextra -Wformat=2; Clang
static analysis completed for all five with no warnings. Fourteen regression
checks passed, including settings restoration on setup/compositor failure,
legacy snapshot safety, literal Wi-Fi arguments, escaped fields, non-destructive
singletons, secure-lock delegation, capture cleanup, unique saved filenames,
offline asset installation and a complete user install into a temporary HOME.
The top-level suite reported 23 passed, 0 failed, 4 skipped. Shell/Python syntax
and git diff whitespace checks passed. Benchmark isolation correctly returned 77
when this environment denied creation of a private compositor socket.

Live rendering, physical monitor hotplug, input, suspend/resume and a real
lock-crash test could not be performed in this environment. Follow
[validation.md](validation.md) for the remaining live checks. The full installer
was tested in a temporary HOME with mock runtime tools and theme fixtures; no
system package install or theme download was executed on the active machine.

The rebuilt programs are available through ~/.local/bin links. Previous command
copies were backed up to:

    /home/sanellathiya/.local/state/chicago-wayland/command-backup-20261027-182821

Source/binary snapshots from before these fixes are also in
/tmp/chicago-before-fixes. The changes take effect as programs are next started;
existing running desktop/taskbar processes have not been replaced. Run
./install.sh --user to refresh installed configuration/assets from the updated
repository when desired; that installer copies the repository session configs.


## Custom interface restored

The GTK Chicago95 interface has subsequently been restored using gtk-session-lock
and ext-session-lock-v1, with the swaylock option retained. See
[custom-lock.md](custom-lock.md). The earlier replacement by swaylock above
records the first security fix. The regression suite now includes custom-lock
PAM and lifecycle tests; its updated results are recorded in custom-lock.md.
