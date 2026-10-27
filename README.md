# Chicago-Wayland: Native Windows 95 Wayland Desktop for Fedora

**Chicago-Wayland** is a lightweight, native Wayland desktop environment for Fedora Linux 44 designed to recreate the visual styling, window management, taskbar, Start menu, desktop icons, and system dialogs of **Microsoft Windows 95** while operating on a modern `wlroots` / `labwc` Wayland stack.

---

## Key Features

1. **Authentic Windows 95 Visual Styling & Server-Side Decorations (`labwc 0.9.6`)**
   - Square window corners (`<cornerRadius>0</cornerRadius>`), zero drop shadows (`<dropShadows>no</dropShadows>`), and zero compositor animations.
   - Classic `#000080` navy active titlebars with bold white text and `#808080` gray inactive titlebars with `#c0c0c0` text.
   - Pixel-accurate 14×13 raised 3D minimize, maximize/restore, close, and window-icon menu buttons.
   - Classic cascading window placement, click-to-focus + raise-on-focus, edge snapping with 3-line classic wireframe outlines (`#ffffff,#000080,#000000`), and classic `Alt+Tab` OSD switcher.

2. **Native Wayland Desktop Surface & Icons (`chicago-desktop`)**
   - Written in C11 (`gtk+-3.0` + `gtk-layer-shell` + Cairo + `GFileMonitor` `inotify`).
   - Classic `#008080` Windows 95 teal desktop (with optional custom color palette and wallpaper support).
   - Interactive desktop icons (**My Computer**, **My Documents**, **Network Neighborhood**, **Recycle Bin** with dynamic empty/full state, **MS-DOS Prompt**, **Control Panel**, plus `~/Desktop` entries).
   - Single-click `#000080` navy label highlight + 1px dotted focus box, double-click/Enter launch, rubber-band selection box, and classic Windows 95 right-click desktop context menu.

3. **Native Wayland Taskbar, Start Menu & System Tray (`chicago-taskbar`)**
   - Written in C11 (`gtk+-3.0` + `gtk-layer-shell` + `wlr-foreign-toplevel-management-unstable-v1`).
   - **Start Button & Start Menu**: Classic 4-color Windows flag icon, vertical rotated **"Windows 95"** sidebar banner, dynamic `.desktop` application categories (**Programs ▸**), **Documents ▸**, **Settings ▸**, **Find...**, **Help**, **Run...**, **MS-DOS Prompt**, **Windows Explorer**, **Lock Workstation**, and **Shut Down...**.
   - **Running Application Taskbar**: Event-driven via `zwlr_foreign_toplevel_manager_v1`; sunken 3D dithered well for the active window, raised 3D bevels for inactive/minimized windows, click to focus/minimize/restore, and right-click context menu.
   - **System Tray & Clock**: Sunken 3D notification well with interactive Volume Control popup (`wpctl`), Network status (`nmcli`), Power Meter (`BAT0` / `upower`), and a **minute-boundary aligned clock** (wakes up only once every 60 seconds = 0.017 Hz) with a Date/Time Calendar popup.

4. **Native Windows 95 Control Panel & System Dialogs (`chicago-control-panel`)**
   - Multi-mode native C binary providing:
     - **Control Panel / System Properties** (`General`, `Display`, `Audio`, `Network`, `Power`, `Input & Keys` tabs)
     - **Run...** dialog (`--run`) with command runner and file browser
     - **Find: All Programs & Files** (`--find`) interactive live search & launcher
     - **Shut Down Windows** (`--shutdown`) modal dialog (Log Out, Stand by / Suspend, Lock, Shut Down, Restart)
     - **Windows Help** (`--help-viewer`) built-in documentation reader

5. **Strict Isolation from Hyprland**
   - Installed as a separate selectable Wayland session (`sessions/chicago-wayland.desktop`).
   - Never modifies `~/.config/hypr/`.
   - Uses `~/.config/chicago-wayland/` for all session configs and automatically backs up and restores `~/.config/gtk-3.0` and `~/.config/gtk-4.0` on session exit.

---

## Repository Structure

```text
chicago-wayland/
├── README.md
├── LICENSES.md
├── install.sh
├── uninstall.sh
├── update.sh
├── bin/
│   ├── start-chicago-wayland
│   ├── chicago-desktop
│   ├── chicago-taskbar
│   └── chicago-control-panel
├── config/
│   ├── labwc/
│   │   ├── rc.xml
│   │   ├── menu.xml
│   │   ├── environment
│   │   └── autostart
│   ├── panel/
│   │   └── chicago95.sfwbar
│   ├── gtk/
│   │   ├── settings.ini
│   │   └── gtk-win95-override.css
│   ├── kanshi/
│   │   └── config
│   ├── kitty/
│   │   └── kitty.conf
│   └── mako/
│       └── config
├── themes/
│   ├── window-decorations/Chicago95/{labwc,openbox-3}/
│   ├── cursors/Chicago95_Cursor_Black/
│   ├── fonts/
│   └── sounds/startup.ogg
├── src/
│   └── custom-components/
│       ├── Makefile
│       ├── include/chicago_common.h
│       ├── protocols/wlr-foreign-toplevel-management-unstable-v1.xml
│       └── src/
│           ├── chicago_desktop.c
│           ├── chicago_taskbar.c
│           └── chicago_control_panel.c
├── scripts/
│   ├── healthcheck.sh
│   ├── benchmark.sh
│   └── restore.sh
├── sessions/
│   └── chicago-wayland.desktop
├── tests/
│   └── run_tests.sh
└── docs/
    ├── architecture.md
    ├── configuration.md
    ├── compatibility.md
    ├── benchmarks.md
    └── troubleshooting.md
```

---

## Quick Start

### 1. Install (User-Local, Zero Root Required)
```bash
cd ~/chicago-wayland
./install.sh --user
```

### 2. Optional: Register Session with GDM Login Screen (Requires `sudo`)
```bash
./install.sh --system-session
```

### 3. Test in a Nested Window Inside Hyprland
```bash
start-chicago-wayland --nested
```

### 4. Run Health Check, Automated Tests, and Benchmarks
```bash
./scripts/healthcheck.sh
./tests/run_tests.sh
./scripts/benchmark.sh
```

### 5. Uninstall / Restore
```bash
./uninstall.sh
```

## Security and validation

`chicago-lock` runs the custom Chicago95 GTK interface on secure
`ext-session-lock-v1` surfaces through gtk-session-lock. It preserves the teal
background, navy titlebar, key/flag artwork and password dialog. PAM uses the
system swaylock authentication service. Howdy IR face authentication uses a
separate face-only PAM policy and your existing system Howdy helper. An automatic
scan starts after lock acknowledgement, with a Face Auth button for retry.
Cancel clears the entry; it cannot dismiss the lock. The command returns success
only after the compositor acknowledges locking, including for swayidle before-sleep. A face miss leaves password entry available; `--no-auto-face`
disables the automatic scan while retaining the retry button.

Use `chicago-lock --swaylock` for the maintained fallback, or `--foreground` to
keep the command running until authentication finishes. Automatic fallback is
limited to missing backend/library or unsupported display/protocol before a
custom lock request is sent. A denied, failed, crashed or timed-out lock request
never triggers an unlock or a replacement locker.

For a source build, install gtk-session-lock-devel and pam-devel, or run
`scripts/build-lock-dependency.sh` for the pinned user-local library before make.
The installer performs this dependency step. The library is loaded relative to
the custom executable from lib/, so the installed locker can move with its tree.
See [docs/custom-lock.md](docs/custom-lock.md) for implementation and verification.

`python3 tests/regression.py` checks settings recovery, safe argument handling,
per-display singleton behavior, capture cleanup, locker delegation, and installation
assets using temporary homes and mock tools. `tests/run_tests.sh` additionally runs
static asset checks and an isolated compositor smoke test. Blocked live checks are
reported as skipped; `--static` explicitly skips the compositor. Physical monitor,
lock crash, suspend/resume, and input behavior still need the checks in
[docs/validation.md](docs/validation.md).

`chicago-capture --window` is a compatibility alias for manual region selection;
the UI labels it "Window region (manual)". It does not claim automatic window
geometry. Clipboard-only captures use private temporary files and remove them
once clipboard/editor consumers finish reading.

The installer works from any checkout location, installs all shell commands,
fonts, backgrounds and application entries, and retrieves missing GTK/icon assets
from the [upstream Chicago95 project](https://github.com/grassmunk/Chicago95).
For offline installs set `CHICAGO95_SOURCE=/path/to/Chicago95` to an existing
upstream checkout. `--system-packages` installs Fedora build/runtime dependencies;
`--user` requires the development headers and runtime utilities to be available.
Run installation as your desktop user; the script uses sudo only for system steps.
