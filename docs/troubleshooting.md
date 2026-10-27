# Troubleshooting & Maintenance Guide

## 1. Running Health Checks and Diagnostics

At any time, validate your installation and verify that Hyprland remains untouched:
```bash
~/chicago-wayland/scripts/healthcheck.sh
~/chicago-wayland/tests/run_tests.sh
```

---

## 2. Common Questions & Solutions

### Q1: How do I select Chicago-Wayland at the GDM login screen?
By default, `./install.sh --user` installs the session user-locally to `~/.local/share/wayland-sessions/chicago-wayland.desktop` (which `uwsm select` / `uwsm start` detects automatically).
Because GDM (`gdm.service`) only scans `/usr/share/wayland-sessions/`, run the following command once with `sudo` to add **Windows 95 (Chicago-Wayland)** to the GDM gear menu:
```bash
~/chicago-wayland/install.sh --system-session
```

### Q2: How can I test Chicago-Wayland inside my current Hyprland session without logging out?
Run:
```bash
start-chicago-wayland --nested
```
This opens a nested Wayland window running `labwc`, `chicago-desktop`, and `chicago-taskbar` without altering your running Hyprland session's GTK configs or notification daemon.

### Q3: What if my system loses power during a standalone Chicago-Wayland session and GTK apps look like Windows 95 in Hyprland?
During a standalone session, `start-chicago-wayland` swaps `~/.config/gtk-3.0` and `~/.config/gtk-4.0` with a backup in `~/.local/state/chicago-wayland/gtk-backup/` and restores them automatically on session exit (`trap restore_gtk_configs EXIT INT TERM HUP`).
If the machine suffers a hard power-off before exit, simply run:
```bash
~/chicago-wayland/scripts/restore.sh
```
This immediately restores your original Matugen GTK3/GTK4 files.

### Q4: How do I switch between `chicago-taskbar` and `sfwbar`?
By default, `chicago-taskbar` is used because it renders the authentic Windows 95 Start menu with the vertical `"Windows 95"` sidebar banner and uses fewer wakeups. To use `sfwbar` instead, launch with:
```bash
CHICAGO_PANEL=sfwbar start-chicago-wayland --nested
```

### Q5: How do I reload Labwc after editing `~/.config/chicago-wayland/labwc/rc.xml` or `themerc`?
Run:
```bash
labwc -r
```
or click **Start ▸ Settings ▸ Reload Window Manager (labwc -r)**.
