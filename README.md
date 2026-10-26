# Chicago-Wayland: Native Windows 95 Wayland Desktop for Fedora

**Chicago-Wayland** is a lightweight, native Wayland desktop environment for Fedora Linux 44 designed to recreate the visual styling, window management, taskbar, Start menu, desktop icons, and system dialogs of **Microsoft Windows 95** while operating on a modern, zero-polling `wlroots` / `labwc` Wayland stack.

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
sudo ./install.sh --system-session
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
