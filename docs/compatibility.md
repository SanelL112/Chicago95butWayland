# Application & Toolkit Compatibility Matrix

A core engineering goal of **Chicago-Wayland** is honest, transparent handling of modern Linux GUI toolkits under Wayland. Below is the verified behavior across GTK3, GTK4, libadwaita, Qt5/Qt6, Electron/Chromium, and XWayland applications on Fedora 44.

---

## 1. Window Decorations (Server-Side vs. Client-Side)

| Toolkit / App Family | Decoration Protocol | Resulting Titlebar & Border Styling |
| :--- | :--- | :--- |
| **Native GTK3 (non-HeaderBar)** (`chicago-control-panel`, `zenity`, standard dialogs) | `xdg-decoration-unstable-v1` (SSD) + `GTK_CSD=0` | **100% Labwc Server-Side Decoration**: `#000080` navy active / `#808080` gray inactive titlebar, pixel-accurate 14×13 buttons, `#c0c0c0` 3px border. |
| **GTK3 with `GtkHeaderBar` (CSD)** | Client-Side Decoration (CSD) | **Chicago95 GTK3 CSS CSD styling**: `#000080` navy headerbar, square corners, 3D raised buttons from `~/.local/share/themes/Chicago95/gtk-3.0/gtk.css`. |
| **GTK4 / `libadwaita`** (`nautilus`, `gnome-text-editor`) | Client-Side Decoration (`AdwHeaderBar`) | **Scoped GTK4 CSS Override**: `libadwaita` ignores `xdg-decoration` and `GTK_THEME` by design, but honors `~/.config/gtk-4.0/gtk.css`. During a standalone `chicago-wayland` session, `start-chicago-wayland` applies `config/gtk/gtk-win95-override.css` (`ADW_DISABLE_PORTAL=1`), rendering square corners (`border-radius: 0px`), `#000080` navy headerbars, and `#c0c0c0` surfaces. |
| **Qt5 & Qt6 Wayland** (`dolphin`, `konsole`, `vlc`) | `xdg-decoration` + `QT_WAYLAND_DISABLE_WINDOWDECORATION=1` | **100% Labwc Server-Side Decoration** + **Built-in Qt `Windows` (Win95) widget style** (`QT_STYLE_OVERRIDE=Windows`). |
| **Kitty Terminal** | `xdg-decoration` (`hide_window_decorations no`) | **100% Labwc Server-Side Decoration** with classic MS-DOS black/gray VGA terminal palette. |
| **Chromium / Google Chrome / Electron** | Wayland (`--ozone-platform=wayland`) | Supports system titlebar when "Use system title bar and borders" is enabled in browser appearance settings; otherwise uses Chromium CSD tab strip. |
| **XWayland (X11 Applications)** | X11 Window Manager framing via Labwc | **100% Labwc Server-Side Decoration**: XWayland launches lazily on-demand (`<xwaylandPersistence>no</xwaylandPersistence>`) and receives full Win95 titlebars and borders. |

---

## 2. System Services & Portal Integration

| Subsystem | Implementation in Chicago-Wayland | Verification Status |
| :--- | :--- | :--- |
| **Audio (PipeWire / WirePlumber)** | Uses running user `pipewire`, `wireplumber`, `pipewire-pulse` services; controlled via `wpctl` in taskbar tray popup, Control Panel, and media keys. | **Verified** |
| **Clipboard (`wl-clipboard`)** | Native Wayland data-device + `<primarySelection>yes</primarySelection>` in `rc.xml`; verified with `wl-copy` / `wl-paste`. | **Verified** |
| **Notifications (`mako`)** | Lightweight Wayland layer-shell notification daemon (`config/mako/config`) styled in classic Windows 95 `#ffffe1` tooltip yellow with 1px black border. | **Verified** |
| **Screen Locking & Idle (`swaylock` / `swayidle`)** | Locks screen to `#008080` teal via `swaylock -f -c 008080` on `Super+L`, Start -> Lock Workstation, or 600s idle / before-sleep. | **Verified** |
| **Multi-Monitor & HiDPI (`kanshi` / `wlr-randr`)** | Pre-configured for the system's 3-monitor topology (`eDP-1` 1920×1200@59.95Hz at `0,0`, `DP-6` HP E22 G5 1920×1080@74.97Hz at `1920,0`, `DP-9` DELL P2222H 1920×1080@60Hz at `3840,0`). | **Verified** |
| **XDG Desktop Portals** | Reuses `xdg-desktop-portal` + `xdg-desktop-portal-gtk` (file choosers) and `xdg-desktop-portal-wlr` (wlroots screen sharing). | **Verified** |

---

## 3. Known Limitations

1. **CSD-enforced GNOME/libadwaita apps**: Because GNOME/libadwaita applications do not implement the `xdg-decoration-unstable-v1` protocol and embed toolbar controls directly inside `AdwHeaderBar`, Labwc cannot strip their internal headerbar without hiding application navigation controls. Our GTK4 CSS override styles their headerbars in `#000080` navy with square corners, rather than adding a second redundant titlebar on top.
2. **True multi-color 3D outer window bevels in Labwc**: As documented in `labwc-theme(5)`, `window.active.border.color` accepts a single solid color (`#c0c0c0` with `border.width: 3`), whereas `osd.window-switcher.preview.border.color` and `snapping.overlay.*.border.color` accept 3 comma-separated colors (`#ffffff,#000080,#000000`). All custom desktop surfaces (`chicago-taskbar`, Start menu, popups, buttons, and wells) render full 2-color raised/sunken 3D bevels (`#ffffff` top/left, `#808080`/`#000000` bottom/right).
