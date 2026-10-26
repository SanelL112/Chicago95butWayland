#!/usr/bin/env bash
# install.sh: Installer for Chicago-Wayland (Windows 95 Native Wayland Desktop for Fedora)
# Preserves Hyprland configuration; defaults to safe user-local installation.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
CONFIG_DEST="$HOME/.config/chicago-wayland"
STATE_DIR="$HOME/.local/state/chicago-wayland"
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
    sudo dnf install -y labwc sfwbar swaybg swaylock swayidle mako kanshi xdg-desktop-portal-wlr
elif ! command -v labwc >/dev/null 2>&1 && [[ ! -x "$RUNTIME_DIR/usr/bin/labwc" ]]; then
    echo "      Extracting Fedora 44 runtime packages into user-local $RUNTIME_DIR..."
    mkdir -p "$RUNTIME_DIR"
    TMP_RPM_DIR="$(mktemp -d)"
    (
        cd "$TMP_RPM_DIR"
        dnf5 download --resolve --arch=x86_64 --arch=noarch \
            labwc sfwbar swaybg swaylock swayidle mako kanshi xdg-desktop-portal-wlr
        cd "$RUNTIME_DIR"
        for rpm in "$TMP_RPM_DIR"/*.rpm; do
            rpm2cpio "$rpm" | cpio -idmu --quiet
        done
    )
    rm -rf "$TMP_RPM_DIR"
else
    echo "      Runtime available at $RUNTIME_DIR/usr/bin/labwc"
fi

# 3. Compile custom native C components (chicago-desktop, chicago-taskbar, chicago-control-panel)
echo "[3/6] Building native C Windows 95 desktop components..."
make -C "$REPO_DIR/src/custom-components"

# 4. Install binaries into ~/.local/bin
echo "[4/6] Installing binaries to $HOME/.local/bin..."
mkdir -p "$HOME/.local/bin"
chmod +x "$REPO_DIR/bin/start-chicago-wayland" "$REPO_DIR/config/labwc/autostart"
for bin_name in chicago-desktop chicago-taskbar chicago-control-panel start-chicago-wayland; do
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

mkdir -p "$HOME/.local/share/themes/Chicago95/labwc"
cp -rf "$REPO_DIR/themes/window-decorations/Chicago95/labwc/." "$HOME/.local/share/themes/Chicago95/labwc/"
cp -rf "$REPO_DIR/themes/window-decorations/Chicago95/openbox-3/." "$HOME/.local/share/themes/Chicago95/openbox-3/"

# 6. Install Wayland session desktop entry
echo "[6/6] Registering Wayland session entry..."
mkdir -p "$HOME/.local/share/wayland-sessions"
cp -f "$REPO_DIR/sessions/chicago-wayland.desktop" "$HOME/.local/share/wayland-sessions/chicago-wayland.desktop"

if [[ "$INSTALL_SYSTEM_SESSION" -eq 1 ]]; then
    echo "      Installing /usr/share/wayland-sessions/chicago-wayland.desktop (requires sudo)..."
    sudo cp -f "$REPO_DIR/sessions/chicago-wayland.desktop" /usr/share/wayland-sessions/chicago-wayland.desktop
else
    echo "      Installed user session: $HOME/.local/share/wayland-sessions/chicago-wayland.desktop"
    echo "      (To also expose in GDM system-wide login screen, run: sudo ./install.sh --system-session)"
fi

echo "========================================================"
echo " Installation complete! Running healthcheck..."
echo "========================================================"
"$REPO_DIR/scripts/healthcheck.sh"
