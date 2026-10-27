# Chicago95 custom session lock

The custom dialog has been restored on GTK session-lock surfaces. The teal
background, navy titlebar, key/flag artwork, username/password form, and beveled
OK/Cancel buttons come from the original Chicago95 interface. Every connected
monitor receives a dialog. Cancel and the titlebar cross clear input; neither
closes the lock. The application no longer creates layer-shell lock overlays.

## Operation

`chicago-lock` starts the custom backend. It returns success only after that
backend receives the compositor's `locked` event, so existing swayidle
before-sleep calls retain the readiness handshake. `--foreground` keeps it
running until authentication ends. `chicago-lock --swaylock` explicitly selects
the maintained fallback.

The fallback is automatic only when the custom backend cannot start or cannot
find Wayland/session-lock support before issuing any lock request. A rejected
request, timeout, unexpected termination or protocol failure returns an error;
it never triggers an unlock or a replacement locker. After a crash the
compositor remains responsible for hiding the session. No generic cleanup,
Cancel, signal handler or failure callback sends an unlock request.

The backend forks before initializing GTK. Its parent waits on a private pipe
for an acknowledgement byte, with a startup timeout. The child checks protocol
support, registers locked/finished handlers, requests the lock and creates one
surface per GDK monitor before mapping each GTK window. Monitor additions and
removals update a reference-counted map with no eight-monitor limit. Removing
an output destroys its surface without affecting other output dialogs.

Authentication uses the real UID's username and the existing system `swaylock`
PAM service, with empty passwords disallowed. A worker thread performs one PAM
conversation at a time. Username prompts receive the username; hidden prompts
receive the submitted password. Entries clear after submission, the worker's
password copy is explicitly wiped when released, and core dumps are disabled.
Failures keep input disabled through PAM's delay plus a UI retry delay.
Successful PAM authentication is accepted only while the lock is active; it
sends unlock_and_destroy and synchronizes the Wayland display before exit.
Howdy face authentication is now available when the existing system helper
/usr/local/bin/howdy-pam and the packaged PAM policy are present. A scan begins
only after the locked event; the Face Auth button retries it. Password entry
stays available during scanning. Submitting a password or pressing Cancel stops
the camera worker; --no-auto-face skips the automatic scan but keeps the button.

Face verification runs in an isolated child process using pam_start_confdir
with the packaged config/pam/chicago-lock-face service. This policy contains
only pam_exec calling the existing system Howdy helper. It does not include the
login/password fallback or password-failure accounting from the Hyprlock service.
The face conversation rejects all hidden/password prompts and accepts no empty
password token. A scan can unlock only on PAM_SUCCESS while the lock remains
active and the scan has not been canceled. There is one scanner at a time, with
a 15-second limit, process-group cancellation and a forced-stop backstop. The
helper and compare.py are terminated on cancellation or locker exit. Face
failure never disables password fallback or clears a partially typed password.
No system PAM files or biometric models are modified.

## Dependency and installation

The implementation uses upstream [gtk-session-lock](https://github.com/Cu3PO42/gtk-session-lock)
0.2.0 at commit b9ddb2792b613d14622acada73c64f16a2635b40. This environment's
ordinary network access was unavailable, so the source was retrieved through
the GitHub connector and its 44 source blobs verified against upstream Git
hashes. The corresponding source, revision marker and manifest are retained in:

    ~/.local/share/chicago-wayland/deps/gtk-session-lock-src/

Build dependencies include PAM development headers and gtk-session-lock-devel,
or Meson/Ninja for `scripts/build-lock-dependency.sh`. The script supports a
pinned offline source through CHICAGO_SESSION_LOCK_SOURCE and an installation
prefix through CHICAGO_LOCK_PREFIX. It is also called by install.sh. The built
library in lib/ is loaded via the custom backend's relative runtime path; the
backend does not load gtk-layer-shell. The system installer copies this library
alongside the executable tree. License texts are in third_party/gtk-session-lock.
The library uses GTK internals, so GTK upgrades require repeating live checks.

## Verification and limits

The custom backend and launcher build with -Wall -Wextra -Wformat=2. Clang
analysis passed for the launcher, backend and authentication helper with no
warnings. The dependency bootstrap was also tested in a fresh temporary prefix.
Eighteen regression cases cover the previous fixes plus actual locker callback
paths for readiness, duplicate output handling/removal, PAM success/denial/setup
and conversation failures, late/duplicate authentication, timeout, refused locks,
explicit fallback, missing display and crash handling. These use mock PAM and
protocol operations; they never authenticate a real user or lock the active
session. The complete suite reports 24 passed, 0 failed, 4 skipped.

Live compositor/rendering, real password authentication, physical monitor
hotplug, crash behavior on the compositor, and suspend/resume could not be
verified here because this environment denies private Wayland sockets. See
[validation.md](validation.md) for those checks. No active-session lock was
triggered and no root PAM files were modified. The existing local command link
points to the rebuilt launcher, so its current keybindings need no changes.


## Face-auth verification update

The regression suite now has 19 cases. It includes mocked face PAM success,
denial, setup failure and password-prompt rejection; canceled/late matches;
password fallback isolation; process-group cancellation of a mock scanner and
its child; and propagation of --no-auto-face. The packaged face policy and helper
availability were checked without starting the real camera. Real face recognition
still requires a live test on the desktop; it is not claimed as verified by the
mocked tests.

For that test, run `chicago-lock`, look at the IR camera, and verify that the
custom dialog unlocks. On a second attempt look away or cover the camera, then
verify that a missed scan keeps the screen locked and typing the normal system
password unlocks it. `chicago-lock --no-auto-face` permits a password-first test,
and `chicago-lock --swaylock` selects the maintained fallback.


## User-reported live test — 2026-10-27

The user confirmed that both Howdy face unlock and password unlock work on the
actual desktop. This confirms those two live authentication paths. Monitor
hotplug, compositor behavior after a locker crash, and suspend/resume remain
separate checks; they are not covered by this report. No system PAM changes
or sudo access were needed for this implementation.
