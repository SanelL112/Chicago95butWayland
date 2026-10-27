#!/usr/bin/env bash
# install.sh: Installer for Chicago-Wayland (Windows 95 Native Wayland Desktop for Fedora)
# Preserves Hyprland configuration; defaults to safe user-local installation.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
CONFIG_DEST="${XDG_CONFIG_HOME:-$HOME/.config}/chicago-wayland"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/chicago-wayland"
DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}"
INIT_BACKUP="$STATE_DIR/initial-backup"

INSTALL_SYSTEM_SESSION=0
INSTALL_SYSTEM_PKGS=0

for arg in "$@"; do
    case "$arg" in
        --system-session)
            INSTALL_SYSTEM_SESSION=1
            ;;
        --system-packages)
            INSTALL_SYSTEM_PKGS=1
            INSTALL_SYSTEM_SESSION=1
            ;;
        --user)
            INSTALL_SYSTEM_SESSION=0
            ;;
    esac
done

if [[ "$EUID" == 0 ]]; then
    echo 'Run this installer as the desktop user; it uses sudo for system steps only.' >&2
    exit 1
fi

echo "========================================================"
echo " Installing Chicago-Wayland (Windows 95 Wayland Desktop)"
echo "========================================================"

# 1. Backup existing user GTK configs (never touch ~/.config/hypr)
mkdir -p "$INIT_BACKUP/gtk-3.0" "$INIT_BACKUP/gtk-4.0"
if [[ ! -f "$INIT_BACKUP/.backed_up" ]]; then
    echo "[1/6] Creating safety backup of existing GTK3/GTK4 user configs..."
    for ver in gtk-3.0 gtk-4.0; do
        if [[ -d "$HOME/.config/$ver" ]]; then
            cp -a "$HOME/.config/$ver/." "$INIT_BACKUP/$ver/" 2>/dev/null || true
        fi
    done
    touch "$INIT_BACKUP/.backed_up"
else
    echo "[1/6] Safety backup already present at $INIT_BACKUP"
fi

# 2. Ensure Labwc & Wayland runtime tools are available (system or user-local RPM extraction)
echo "[2/6] Checking Wayland compositor (labwc) and utilities..."
if [[ "$INSTALL_SYSTEM_PKGS" -eq 1 ]]; then
    echo "      Installing Fedora packages via sudo dnf install..."
    sudo dnf install -y gcc make pkgconf-pkg-config gtk3-devel gtk-layer-shell-devel pam-devel gtk-session-lock-devel meson ninja-build wayland-devel wayland-protocols-devel git python3-pillow python3-gobject power-profiles-daemon labwc sfwbar swaybg swaylock swayidle mako kanshi xdg-desktop-portal-wlr grim slurp swappy wl-clipboard wlr-randr kitty thunar NetworkManager wireplumber
elif ! command -v labwc >/dev/null 2>&1 && [[ ! -x "$RUNTIME_DIR/usr/bin/labwc" ]]; then
    echo "      Extracting Fedora 44 runtime packages into user-local $RUNTIME_DIR..."
    mkdir -p "$RUNTIME_DIR"
    TMP_RPM_DIR="$(mktemp -d)"
    (
        cd "$TMP_RPM_DIR"
        dnf5 download --resolve --arch=x86_64 --arch=noarch \
            labwc sfwbar swaybg swaylock swayidle mako kanshi xdg-desktop-portal-wlr grim slurp swappy wl-clipboard wlr-randr
        cd "$RUNTIME_DIR"
        for rpm in "$TMP_RPM_DIR"/*.rpm; do
            rpm2cpio "$rpm" | cpio -idmu --quiet
        done
    )
    rm -rf "$TMP_RPM_DIR"
else
    echo "      Runtime available at $RUNTIME_DIR/usr/bin/labwc"
fi

export PATH="$RUNTIME_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$RUNTIME_DIR/usr/lib64:${LD_LIBRARY_PATH:-}"
for tool in make cc pkg-config wayland-scanner; do
    command -v "$tool" >/dev/null || { echo "Missing build prerequisite: $tool (use --system-packages)." >&2; exit 1; }
done
pkg-config --exists gtk+-3.0 gtk-layer-shell-0 wayland-client gio-unix-2.0 cairo pango || {
    echo 'Missing development headers: use --system-packages or install GTK3, gtk-layer-shell and Wayland development packages.' >&2
    exit 1
}
if [[ ! -f /etc/pam.d/swaylock ]]; then
    echo 'The system swaylock PAM service is missing; install the Fedora swaylock package before using screen locking.' >&2
    exit 1
fi
# 3. Compile custom native C components (chicago-desktop, chicago-taskbar, chicago-control-panel)
echo "[3/6] Building native C Windows 95 desktop components..."
"$REPO_DIR/scripts/build-lock-dependency.sh"
make -C "$REPO_DIR/src/custom-components"

# 4. Install binaries into ~/.local/bin
echo "[4/6] Installing binaries to $HOME/.local/bin..."
mkdir -p "$HOME/.local/bin"
chmod +x "$REPO_DIR/bin/start-chicago-wayland" "$REPO_DIR/config/labwc/autostart"
for bin_name in chicago-desktop chicago-taskbar chicago-control-panel chicago-power chicago-lock chicago-lock-custom chicago-lock-face chicago-capture toggle-scratchpad-terminal.sh start-chicago-wayland; do
    ln -sf "$REPO_DIR/bin/$bin_name" "$HOME/.local/bin/$bin_name"
done

# 5. Install configurations and themes
echo "[5/6] Installing Labwc, panel, notification, and theme configurations..."
mkdir -p "$CONFIG_DEST"
cp -rf "$REPO_DIR/config/labwc" "$CONFIG_DEST/"
cp -rf "$REPO_DIR/config/panel" "$CONFIG_DEST/"
cp -rf "$REPO_DIR/config/mako" "$CONFIG_DEST/"
cp -rf "$REPO_DIR/config/kanshi" "$CONFIG_DEST/"
cp -rf "$REPO_DIR/config/kitty" "$CONFIG_DEST/"
cp -rf "$REPO_DIR/config/gtk" "$CONFIG_DEST/"

"$REPO_DIR/scripts/install-assets.sh"

# 6. Install Wayland session desktop entry
echo "[6/6] Registering Wayland session entry..."
mkdir -p "$DATA_DIR/wayland-sessions"
python3 - "$REPO_DIR/sessions/chicago-wayland.desktop" "$DATA_DIR/wayland-sessions/chicago-wayland.desktop" "$HOME/.local/bin/start-chicago-wayland" <<'PY_SESSION'
from pathlib import Path
import sys
# Desktop Entry Exec quoting, including literal percent and reserved characters.
slash = chr(92)
path = sys.argv[3]
for char in (slash, '"', '`', '$'):
    path = path.replace(char, slash + char)
path = path.replace('%', '%%').replace(slash, slash * 2)
lines = Path(sys.argv[1]).read_text().splitlines()
lines = [('Exec="' + path + '"') if line.startswith('Exec=') else
         ('TryExec=' + sys.argv[3]) if line.startswith('TryExec=') else line
         for line in lines]
Path(sys.argv[2]).write_text('\n'.join(lines) + '\n')
PY_SESSION

if [[ "$INSTALL_SYSTEM_SESSION" -eq 1 ]]; then
    echo "      Installing binaries to /usr/local/bin and session entry to /usr/share/wayland-sessions/..."
    sudo mkdir -p /usr/local/share/chicago-wayland /usr/share/wayland-sessions
    sudo cp -a "$REPO_DIR/bin" "$REPO_DIR/config" "$REPO_DIR/scripts" "$REPO_DIR/themes" "$REPO_DIR/lib" "$REPO_DIR/third_party" /usr/local/share/chicago-wayland/
    for bin_name in chicago-desktop chicago-taskbar chicago-control-panel chicago-power chicago-lock chicago-lock-custom chicago-lock-face chicago-capture toggle-scratchpad-terminal.sh start-chicago-wayland; do
        sudo ln -sf "/usr/local/share/chicago-wayland/bin/$bin_name" "/usr/local/bin/$bin_name"
    done
    sudo install -m 0644 "$REPO_DIR/sessions/chicago-wayland.desktop" /usr/share/wayland-sessions/chicago-wayland.desktop
    sudo restorecon -v /usr/local/bin/start-chicago-wayland /usr/local/bin/chicago-* /usr/share/wayland-sessions/chicago-wayland.desktop 2>/dev/null || true
else
    echo "      Installed user session: $HOME/.local/share/wayland-sessions/chicago-wayland.desktop"
    echo "      (To also expose in GDM system-wide login screen, run: ./install.sh --system-session)"
fi

echo "========================================================"
echo " Installation complete! Running healthcheck..."
echo "========================================================"
"$REPO_DIR/scripts/healthcheck.sh"
