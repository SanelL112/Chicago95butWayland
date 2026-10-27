# Licensing and Attribution

## Chicago-Wayland Custom Source Code & Configuration

All custom source code (`src/custom-components/`), scripts (`install.sh`, `uninstall.sh`, `update.sh`, `scripts/`, `tests/`),Labwc configurations (`config/`), and documentation (`docs/`) created for **Chicago-Wayland** are licensed under the **GNU General Public License v3.0 or later (GPL-3.0-or-later)**.

---

## Reused Upstream Open-Source Projects & Assets

### 1. Chicago95 Theme, Icons, Cursors, Fonts, and Openbox-3 Assets
- **Upstream Repository**: [https://github.com/grassmunk/Chicago95](https://github.com/grassmunk/Chicago95)
- **Authors**: Grassmunk, AdrianoML, EMH-Mark-I, and Chicago95 contributors
- **License**:
  - GTK2 / GTK3 / GTK4 themes, Openbox-3 / XFWM4 window decorations, and scripts: **GPL-3.0-or-later**
  - Icons and cursors (`Icons/Chicago95`, `Cursors/Chicago95_Cursor_Black`, `Cursors/Chicago95_Cursor_White`): **GPL-3.0-or-later / MIT** (see upstream `CREDITS`)
  - Fonts (`Fonts/vga_font/LessPerfectDOSVGA.ttf`): **MIT / Open Font License**

### 2. wlr-foreign-toplevel-management-unstable-v1 Wayland Protocol XML
- **Source**: `wlr-protocols` (`protocols/wlr-foreign-toplevel-management-unstable-v1.xml`)
- **Copyright**: Copyright © 2018 Ilia Bozhinov
- **License**: MIT / X11 Permissive License

### 3. Fedora Linux Runtime Packages (User-Local or System)
- **Labwc** (`0.9.6`): GPL-2.0-only
- **wlroots** (`0.19.3`): MIT
- **sfwbar** (`1.0~beta16.1`): GPL-3.0-only
- **swaybg / swayidle / swaylock / mako / kanshi**: MIT

No proprietary Microsoft Windows binaries or unlicensed copyrighted DLL/EXE resources are bundled.


### 4. gtk-session-lock (custom lock-screen surfaces)
- Upstream: https://github.com/Cu3PO42/gtk-session-lock
- Pinned revision: `b9ddb2792b613d14622acada73c64f16a2635b40` (0.2.0)
- License: GPLv3, with some source files additionally available under MIT.
- License texts and build/source provenance: `third_party/gtk-session-lock/`.
- The bootstrap script retains the corresponding unmodified library source in
  the user dependency directory. The generated library in `lib/` is a build artifact.
