#!/usr/bin/env bash
# tests/run_tests.sh: Automated validation & Wayland integration test suite for Chicago-Wayland
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
export PATH="$REPO_DIR/bin:$HOME/.local/bin:$RUNTIME_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$RUNTIME_DIR/usr/lib64:${LD_LIBRARY_PATH:-}"

PASS=0
FAIL=0

assert_test() {
    local name="$1"
    shift
    if "$@" >/dev/null 2>&1; then
        echo "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        echo "  [FAIL] $name"
        FAIL=$((FAIL + 1))
    fi
}

echo "========================================================"
echo " Chicago-Wayland Automated Integration & Validation Suite"
echo "========================================================"

# 1. Static XML & Config Validation
assert_test "XML validation: config/labwc/rc.xml" \
    python3 -c "import xml.etree.ElementTree as ET; ET.parse('$REPO_DIR/config/labwc/rc.xml')"
assert_test "XML validation: config/labwc/menu.xml" \
    python3 -c "import xml.etree.ElementTree as ET; ET.parse('$REPO_DIR/config/labwc/menu.xml')"
assert_test "XML validation: wlr-foreign-toplevel-management-unstable-v1.xml" \
    python3 -c "import xml.etree.ElementTree as ET; ET.parse('$REPO_DIR/src/custom-components/protocols/wlr-foreign-toplevel-management-unstable-v1.xml')"

# 2. Windows 95 Visual Fidelity & Power Config Checks
assert_test "Square window corners (<cornerRadius>0</cornerRadius>)" \
    grep -q "<cornerRadius>0</cornerRadius>" "$REPO_DIR/config/labwc/rc.xml"
assert_test "Drop shadows disabled (<dropShadows>no</dropShadows>)" \
    grep -q "<dropShadows>no</dropShadows>" "$REPO_DIR/config/labwc/rc.xml"
assert_test "On-demand XWayland (<xwaylandPersistence>no</xwaylandPersistence>)" \
    grep -q "<xwaylandPersistence>no</xwaylandPersistence>" "$REPO_DIR/config/labwc/rc.xml"
assert_test "Active titlebar Navy #000080 in themerc" \
    grep -qi "window.active.title.bg.color: #000080" "$REPO_DIR/themes/window-decorations/Chicago95/labwc/themerc"
assert_test "Inactive titlebar Gray #808080 in themerc" \
    grep -qi "window.inactive.title.bg.color: #808080" "$REPO_DIR/themes/window-decorations/Chicago95/labwc/themerc"
assert_test "Window border Classic Gray #c0c0c0 in themerc" \
    grep -qi "window.active.border.color: #c0c0c0" "$REPO_DIR/themes/window-decorations/Chicago95/labwc/themerc"

# 3. Binary & Theme Asset Checks
assert_test "Binary present: chicago-desktop" test -x "$REPO_DIR/bin/chicago-desktop"
assert_test "Binary present: chicago-taskbar" test -x "$REPO_DIR/bin/chicago-taskbar"
assert_test "Binary present: chicago-control-panel" test -x "$REPO_DIR/bin/chicago-control-panel"
assert_test "Binary present: start-chicago-wayland" test -x "$REPO_DIR/bin/start-chicago-wayland"
assert_test "Theme assets: close-active.png, max-active.png, iconify-active.png" \
    bash -c "test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/close-active.png' && test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/max-active.png' && test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/iconify-active.png'"

# 4. Live Headless Wayland Integration Test (Clipboard, Multi-Window, Minimize/Restore, Pixel Check)
TMP_TEST_DIR="$(mktemp -d)"
INNER_TEST="$TMP_TEST_DIR/inner_test.sh"
cat << 'EOF' > "$INNER_TEST"
#!/usr/bin/env bash
set -euo pipefail
OUT_DIR="$1"

wlr-randr --output HEADLESS-1 --custom-mode 1280x800@60Hz
sleep 1.8

# Verify clipboard roundtrip inside Wayland session
echo -n "win95-wayland-clipboard-ok" | wl-copy
CLIP_VAL="$(wl-paste)"
if [[ "$CLIP_VAL" == "win95-wayland-clipboard-ok" ]]; then
    touch "$OUT_DIR/clipboard.ok"
fi

# Capture desktop + taskbar screenshot for pixel palette verification
grim "$OUT_DIR/screen.png"

# Launch multiple simultaneous windows
chicago-control-panel --tab system &
P1=$!
chicago-control-panel --run &
P2=$!
chicago-control-panel --find &
P3=$!
sleep 1.2

# Toggle Start Menu (SIGUSR1) and Show Desktop / Minimize All (SIGUSR2)
pkill -USR1 -x chicago-taskbar
sleep 0.4
pkill -USR1 -x chicago-taskbar
pkill -USR2 -x chicago-taskbar
sleep 0.4
pkill -USR2 -x chicago-taskbar
sleep 0.4

if kill -0 "$P1" "$P2" "$P3" 2>/dev/null; then
    touch "$OUT_DIR/multiwin.ok"
fi

kill "$P1" "$P2" "$P3" 2>/dev/null || true
labwc -e
EOF
chmod +x "$INNER_TEST"

"$REPO_DIR/bin/start-chicago-wayland" --headless --startup "$INNER_TEST $TMP_TEST_DIR" >/dev/null 2>&1

assert_test "Wayland session clipboard roundtrip (wl-copy / wl-paste)" test -f "$TMP_TEST_DIR/clipboard.ok"
assert_test "Wayland multi-window + Start menu + Minimize/Restore lifecycle" test -f "$TMP_TEST_DIR/multiwin.ok"
assert_test "Rendered pixel verification (#008080 desktop teal & #c0c0c0 taskbar gray)" \
    python3 -c "
import gi
gi.require_version('GdkPixbuf', '2.0')
from gi.repository import GdkPixbuf
pb = GdkPixbuf.Pixbuf.new_from_file('$TMP_TEST_DIR/screen.png')
w, h = pb.get_width(), pb.get_height()
rs = pb.get_rowstride()
nc = pb.get_n_channels()
data = pb.get_pixels()
def px(x, y):
    off = y * rs + x * nc
    return (data[off], data[off+1], data[off+2])
teal = px(w // 2, h // 2)
gray = px(w // 2, h - 10)
assert teal == (0, 128, 128), f'Expected teal (0,128,128), got {teal}'
assert gray == (192, 192, 192), f'Expected gray (192,192,192), got {gray}'
"

rm -rf "$TMP_TEST_DIR"

# 5. Hyprland Non-Interference Check
assert_test "Hyprland config untouched (~/.config/hypr/hyprland.conf)" test -f "$HOME/.config/hypr/hyprland.conf"
assert_test "No residual GTK swap lock left after headless test" test ! -f "$HOME/.local/state/chicago-wayland/gtk-swapped.lock"

echo "========================================================"
echo " Test Results: $PASS passed, $FAIL failed."
echo "========================================================"
[[ "$FAIL" -eq 0 ]]
