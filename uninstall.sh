#!/usr/bin/env bash
# uninstall.sh: Cleanly removes Chicago-Wayland files and restores any swapped user settings.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "========================================================"
echo " Uninstalling Chicago-Wayland Desktop Environment"
echo "========================================================"

# 1. Restore any swapped GTK settings first
if [[ -x "$REPO_DIR/scripts/restore.sh" ]]; then
    "$REPO_DIR/scripts/restore.sh"
fi

# 2. Remove user-local binaries
for bin_name in chicago-desktop chicago-taskbar chicago-control-panel start-chicago-wayland; do
    rm -f "$HOME/.local/bin/$bin_name"
done

# 3. Remove project-owned configuration and session files
rm -rf "$HOME/.config/chicago-wayland"
rm -f "$HOME/.local/share/wayland-sessions/chicago-wayland.desktop"

if [[ "${1:-}" == "--purge-runtime" ]]; then
    rm -rf "$HOME/.local/share/chicago-wayland"
fi

if [[ "${1:-}" == "--system" ]]; then
    sudo rm -f /usr/share/wayland-sessions/chicago-wayland.desktop
    sudo rm -f /usr/local/bin/start-chicago-wayland /usr/local/bin/chicago-desktop /usr/local/bin/chicago-taskbar /usr/local/bin/chicago-control-panel
fi

echo "Uninstall complete. Hyprland and user configurations remain intact."
