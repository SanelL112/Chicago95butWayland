# Configuration Guide

## 1. Configuration File Locations

All **Chicago-Wayland** session configuration files are stored in `~/.config/chicago-wayland/` (synced from `/home/sanellathiya/chicago-wayland/config/` by `./install.sh` or `./update.sh`) so they never conflict with Hyprland (`~/.config/hypr/`):

| Path | Purpose |
| :--- | :--- |
| `~/.config/chicago-wayland/labwc/rc.xml` | Labwc compositor settings: theme (`Chicago95`), square corners (`cornerRadius=0`), cascade placement, keyboard/mouse bindings, window rules |
| `~/.config/chicago-wayland/labwc/menu.xml` | Labwc window titlebar menu (`client-menu`) and fallback root menu |
| `~/.config/chicago-wayland/labwc/environment` | Session environment variables (`GTK_THEME=Chicago95`, `QT_STYLE_OVERRIDE=Windows`, `QT_WAYLAND_DISABLE_WINDOWDECORATION=1`, cursor theme) |
| `~/.config/chicago-wayland/labwc/autostart` | Startup script launching `kanshi`, `chicago-desktop`, `chicago-taskbar`, `mako`, and `swayidle` |
| `~/.config/chicago-wayland/panel/chicago95.sfwbar` | Optional `sfwbar` taskbar configuration (used when `CHICAGO_PANEL=sfwbar`) |
| `~/.config/chicago-wayland/kanshi/config` | Multi-monitor output layout profiles (`eDP-1`, `DP-6`, `DP-9`) |
| `~/.config/chicago-wayland/mako/config` | Windows 95 tooltip-styled notification daemon config (`#ffffe1` background, 1px black border) |
| `~/.config/chicago-wayland/kitty/kitty.conf` | MS-DOS Prompt terminal configuration (VGA 16-color palette, non-blinking underline cursor) |
| `~/.local/share/themes/Chicago95/labwc/themerc` | Labwc Openbox-3 window decoration theme |

---

## 2. Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Super` (tap & release) or `Ctrl+Esc` | Toggle Windows 95 **Start Menu** (`SIGUSR1` to `chicago-taskbar`) |
| `Super + D` | **Show Desktop** (Minimize / Restore all windows via `SIGUSR2` to `chicago-taskbar`) |
| `Super + R` | Open Windows 95 **Run...** dialog (`chicago-control-panel --run`) |
| `Super + F` | Open Windows 95 **Find: All Programs & Files** (`chicago-control-panel --find`) |
| `Super + E` | Open **Windows Explorer** (File Manager) |
| `Super + C` | Open **Control Panel** (`chicago-control-panel`) |
| `Super + Return` | Open **MS-DOS Prompt** (`kitty`) |
| `Super + L` | **Lock Workstation** (`swaylock -f -c 008080`) |
| `Alt + F4` | Close active window |
| `Alt + Tab` / `Alt + Shift + Tab` | Classic Windows 95 **Window Switcher** |
| `Alt + Space` | Open active window's **Titlebar System Menu** (Restore, Move, Size, Minimize, Maximize, Close) |
| `Super + Left / Right` | Snap active window to left / right half of monitor |
| `Super + Up / Down` | Maximize / Minimize active window |
| `Print` | Save full-screen PNG screenshot to `~/Pictures/chicago95-YYYYMMDD-HHMMSS.png` |
| `XF86AudioRaiseVolume / Lower / Mute` | Adjust PipeWire audio volume (`wpctl`) |
| `XF86MonBrightnessUp / Down` | Adjust internal panel backlight (`brightnessctl`) |

---

## 3. Environment Knobs

You can customize behavior via environment variables before launching `start-chicago-wayland`:

- `CHICAGO_BG_COLOR="#008080"`: Change desktop background color (e.g., `#008080` Win95 Teal, `#000080` Navy, `#3a6ea5` Win2000 Blue).
- `CHICAGO_WALLPAPER="/path/to/image.png"`: Display a wallpaper image on the desktop surface.
- `CHICAGO_PANEL="chicago-taskbar"`: Choose `"chicago-taskbar"` (default native C taskbar + Start menu) or `"sfwbar"`.
- `CHICAGO_STARTUP_SOUND="1"`: Play the Windows 95 startup sound on session login (default `0` for silent, low-power startup).
