#!/usr/bin/env bash
# Install bundled assets and optionally the upstream GTK/icon theme.
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}"
THEME_SOURCE="${CHICAGO95_SOURCE:-}"
# Empty gtk/icons directories in this repository are placeholders, not themes.
if [[ ! -f "$DATA_DIR/themes/Chicago95/gtk-3.0/gtk.css" || ! -f "$DATA_DIR/icons/Chicago95/index.theme" ]]; then
    if [[ -z "$THEME_SOURCE" ]]; then
        CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/chicago-wayland"
        mkdir -p "$CACHE_DIR"
        THEME_SOURCE="$CACHE_DIR/Chicago95"
        if [[ ! -d "$THEME_SOURCE/.git" ]]; then
            command -v git >/dev/null || { echo 'Install git or set CHICAGO95_SOURCE to an upstream Chicago95 checkout.' >&2; exit 1; }
            TMP_THEME="$(mktemp -d "$CACHE_DIR/theme.XXXXXX")"
            trap 'rm -rf "$TMP_THEME"' EXIT
            git clone --depth 1 https://github.com/grassmunk/Chicago95.git "$TMP_THEME"
            # Validate before promoting the downloaded tree.
            test -f "$TMP_THEME/Theme/Chicago95/gtk-3.0/gtk.css"
            test -f "$TMP_THEME/Icons/Chicago95/index.theme"
            mv "$TMP_THEME" "$THEME_SOURCE"
            trap - EXIT
        fi
    fi
    test -f "$THEME_SOURCE/Theme/Chicago95/gtk-3.0/gtk.css"
    test -f "$THEME_SOURCE/Icons/Chicago95/index.theme"
    mkdir -p "$DATA_DIR/themes/Chicago95" "$DATA_DIR/icons/Chicago95"
    cp -a "$THEME_SOURCE/Theme/Chicago95/." "$DATA_DIR/themes/Chicago95/"
    cp -a "$THEME_SOURCE/Icons/Chicago95/." "$DATA_DIR/icons/Chicago95/"
    mkdir -p "$DATA_DIR/chicago-wayland/upstream-licenses"
    find "$THEME_SOURCE" -maxdepth 1 -type f \( -iname '*license*' -o -iname '*copying*' -o -iname '*credit*' \) \
        -exec cp -a {} "$DATA_DIR/chicago-wayland/upstream-licenses/" \;
fi
mkdir -p "$DATA_DIR/themes/Chicago95/labwc" "$DATA_DIR/themes/Chicago95/openbox-3" \
    "$DATA_DIR/icons" "$DATA_DIR/fonts/chicago-wayland" "$DATA_DIR/backgrounds/chicago95" "$DATA_DIR/applications"
cp -a "$REPO_DIR/themes/window-decorations/Chicago95/labwc/." "$DATA_DIR/themes/Chicago95/labwc/"
cp -a "$REPO_DIR/themes/window-decorations/Chicago95/openbox-3/." "$DATA_DIR/themes/Chicago95/openbox-3/"
cp -a "$REPO_DIR/themes/cursors/." "$DATA_DIR/icons/"
cp -a "$REPO_DIR/themes/fonts/." "$DATA_DIR/fonts/chicago-wayland/"
cp -a "$REPO_DIR/themes/backgrounds/." "$DATA_DIR/backgrounds/chicago95/"
mkdir -p "$DATA_DIR/chicago-wayland/lockscreen"
cp -a "$REPO_DIR/themes/lockscreen/." "$DATA_DIR/chicago-wayland/lockscreen/"
cp -a "$REPO_DIR/themes/applications/." "$DATA_DIR/applications/"
if command -v fc-cache >/dev/null; then fc-cache "$DATA_DIR/fonts/chicago-wayland"; fi
