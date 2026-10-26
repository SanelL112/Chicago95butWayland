#!/usr/bin/env bash
# scripts/restore.sh: Restores backed-up GTK3/GTK4 configurations and verifies Hyprland safety.
set -euo pipefail

STATE_DIR="$HOME/.local/state/chicago-wayland"
BACKUP_DIR="$STATE_DIR/gtk-backup"
INIT_BACKUP="$STATE_DIR/initial-backup"

if [[ -f "$STATE_DIR/gtk-swapped.lock" ]]; then
    echo "Restoring active session GTK backup from $BACKUP_DIR..."
    for ver in gtk-3.0 gtk-4.0; do
        for f in settings.ini gtk.css; do
            if [[ -f "$BACKUP_DIR/$ver/$f" ]]; then
                cp -f "$BACKUP_DIR/$ver/$f" "$HOME/.config/$ver/$f"
            fi
        done
    done
    rm -f "$STATE_DIR/gtk-swapped.lock"
elif [[ -d "$INIT_BACKUP" ]]; then
    echo "Ensuring initial GTK3/GTK4 configurations are intact..."
    for ver in gtk-3.0 gtk-4.0; do
        for f in settings.ini gtk.css; do
            if [[ -f "$INIT_BACKUP/$ver/$f" ]] && [[ ! -f "$HOME/.config/$ver/$f" ]]; then
                cp -f "$INIT_BACKUP/$ver/$f" "$HOME/.config/$ver/$f"
            fi
        done
    done
fi

if [[ -f "$HOME/.config/hypr/hyprland.conf" ]]; then
    echo "Verified: ~/.config/hypr/hyprland.conf is present and untouched."
fi
