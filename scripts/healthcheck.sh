#!/usr/bin/env bash
# scripts/healthcheck.sh: Validates Chicago-Wayland binaries, XML syntax, themes, and Hyprland isolation.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
export PATH="$REPO_DIR/bin:$HOME/.local/bin:$RUNTIME_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$RUNTIME_DIR/usr/lib64:${LD_LIBRARY_PATH:-}"

DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}"
PASS=0
FAIL=0

check_item() {
    local desc="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "  [PASS] $desc"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $desc"
        FAIL=$((FAIL + 1))
    fi
}

echo "--- Chicago-Wayland Health Check ---"

check_item "Compositor binary (labwc) executable" command -v labwc
check_libraries() {
    local compositor dependencies
    compositor="$(command -v labwc)" || return 1
    dependencies="$(ldd "$compositor")" || return 1
    [[ "$dependencies" != *"not found"* ]]
}
check_item "Labwc shared libraries resolved" check_libraries
check_item "Custom desktop binary (chicago-desktop)" test -x "$REPO_DIR/bin/chicago-desktop"
check_item "Custom taskbar/Start menu binary (chicago-taskbar)" test -x "$REPO_DIR/bin/chicago-taskbar"
check_item "Custom Control Panel/dialogs binary (chicago-control-panel)" test -x "$REPO_DIR/bin/chicago-control-panel"
check_item "Session launcher (start-chicago-wayland)" test -x "$REPO_DIR/bin/start-chicago-wayland"
check_item "Labwc rc.xml valid XML syntax" python3 -c "import xml.etree.ElementTree as ET; ET.parse('$REPO_DIR/config/labwc/rc.xml')"
check_item "Labwc menu.xml valid XML syntax" python3 -c "import xml.etree.ElementTree as ET; ET.parse('$REPO_DIR/config/labwc/menu.xml')"
check_item "Chicago95 Labwc window decoration theme installed" test -f "$DATA_DIR/themes/Chicago95/labwc/themerc"
check_item "Chicago95 GTK3 theme installed" test -f "$DATA_DIR/themes/Chicago95/gtk-3.0/gtk.css"
check_item "Chicago95 icon theme installed" test -d "$DATA_DIR/icons/Chicago95"
check_item "Chicago95 cursor theme installed" test -d "$DATA_DIR/icons/Chicago95_Cursor_Black"
check_item "Wayland session .desktop file present" test -f "$DATA_DIR/wayland-sessions/chicago-wayland.desktop"
for tool in swaylock swayidle grim slurp wl-copy wl-paste wlr-randr; do
    check_item "Runtime utility: $tool" command -v "$tool"
done
check_item "swaylock PAM service installed" test -f /etc/pam.d/swaylock
for tool in chicago-lock chicago-lock-custom chicago-lock-face chicago-capture; do
    check_item "Custom binary: $tool" test -x "$REPO_DIR/bin/$tool"
done
check_item "Custom lock shared libraries resolved" bash -c 'deps="$(ldd "$1")" && [[ "$deps" != *"not found"* ]]' healthcheck "$REPO_DIR/bin/chicago-lock-custom"
if "$REPO_DIR/bin/chicago-lock-face" --check; then
    echo '  [PASS] Optional Howdy helper and face-only PAM policy available (camera not tested)'
else
    echo '  [SKIP] Optional Howdy face authentication is unavailable; password locking remains available'
fi
# This checks installed prerequisites only; active-session behavior needs integration QA.

echo "------------------------------------"
echo "Summary: $PASS passed, $FAIL failed."
[[ "$FAIL" -eq 0 ]]
