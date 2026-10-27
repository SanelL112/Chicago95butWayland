#!/usr/bin/env bash
# tests/run_tests.sh: Automated validation & Wayland integration test suite for Chicago-Wayland
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNTIME_DIR="$HOME/.local/share/chicago-wayland/runtime"
export PATH="$REPO_DIR/bin:$HOME/.local/bin:$RUNTIME_DIR/usr/bin:$PATH"
export LD_LIBRARY_PATH="$RUNTIME_DIR/usr/lib64:${LD_LIBRARY_PATH:-}"

PASS=0
FAIL=0
SKIP=0
export REPO_DIR

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
    python3 -c 'import sys,xml.etree.ElementTree as ET; ET.parse(sys.argv[1])' "$REPO_DIR/config/labwc/rc.xml"
assert_test "XML validation: config/labwc/menu.xml" \
    python3 -c 'import sys,xml.etree.ElementTree as ET; ET.parse(sys.argv[1])' "$REPO_DIR/config/labwc/menu.xml"
assert_test "XML validation: wlr-foreign-toplevel-management-unstable-v1.xml" \
    python3 -c 'import sys,xml.etree.ElementTree as ET; ET.parse(sys.argv[1])' "$REPO_DIR/src/custom-components/protocols/wlr-foreign-toplevel-management-unstable-v1.xml"

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
assert_test "Binary present: chicago-lock-face" test -x "$REPO_DIR/bin/chicago-lock-face"
assert_test "Binary present: chicago-lock-custom" test -x "$REPO_DIR/bin/chicago-lock-custom"
assert_test "Binary present: chicago-lock" test -x "$REPO_DIR/bin/chicago-lock"
assert_test "Binary present: chicago-capture" test -x "$REPO_DIR/bin/chicago-capture"
assert_test "Binary present: start-chicago-wayland" test -x "$REPO_DIR/bin/start-chicago-wayland"
assert_test "Touchpad 2-finger click & tap configured in rc.xml" \
    bash -c "grep -q '<clickMethod>clickfinger</clickMethod>' '$REPO_DIR/config/labwc/rc.xml' && grep -q '<tapButtonMap>lrm</tapButtonMap>' '$REPO_DIR/config/labwc/rc.xml'"
assert_test "Control Center keybinds W-a and W-c configured in rc.xml" \
    bash -c "grep -q 'key=\"W-a\"' '$REPO_DIR/config/labwc/rc.xml' && grep -q 'key=\"W-c\"' '$REPO_DIR/config/labwc/rc.xml'"
assert_test "Capture Tool keybinds Print, W-S-s, W-S-a configured in rc.xml" \
    bash -c "grep -q 'chicago-capture --interactive' '$REPO_DIR/config/labwc/rc.xml' && grep -q 'chicago-capture --snip' '$REPO_DIR/config/labwc/rc.xml' && grep -q 'chicago-capture --edit' '$REPO_DIR/config/labwc/rc.xml'"
assert_test "Brightness sync with DDC/CI configured in rc.xml" \
    grep -q "brightness-sync.py sync" "$REPO_DIR/config/labwc/rc.xml"
assert_test "Theme assets: close-active.png, max-active.png, iconify-active.png" \
    bash -c "test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/close-active.png' && test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/max-active.png' && test -f '$REPO_DIR/themes/window-decorations/Chicago95/labwc/iconify-active.png'"
assert_test "Windows 95 Era Wallpapers: Setup, Inside Your Computer, Clouds, Teal/Amber/Green colorways" \
    python3 -c "
import os
from PIL import Image
wp_dir = '$REPO_DIR/themes/backgrounds/Wallpaper'
required = [
    'Setup.png',
    'Setup (Windows 95 Teal).png',
    'Setup (Phosphor Amber).png',
    'Setup (Cyber Green).png',
    'Setup Widescreen (Original Blue).png',
    'Inside Your Computer.png',
    'Inside Your Computer (Setup Blue).png',
    'Clouds.png',
    'Winlogo.png'
]
for r in required:
    p = os.path.join(wp_dir, r)
    assert os.path.isfile(p), f'Missing {r}'
    im = Image.open(p)
    assert im.size[0] > 0 and im.size[1] > 0
"

assert_test "Configuration, argv safety, singleton, locker, capture and installer regressions" \
    python3 "$REPO_DIR/tests/regression.py"

# 4. Isolated headless smoke test: clipboard, dialog processes, palette.
# Start-menu interaction, minimize/restore and physical lock/suspend require manual QA.
TMP_TEST_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_TEST_DIR"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
INNER_TEST="$TMP_TEST_DIR/inner_test.sh"
cat << 'EOF' > "$INNER_TEST"
#!/usr/bin/env bash
set -euo pipefail
OUT_DIR="$1"

sleep 1.8

# Capture desktop + taskbar screenshot for pixel palette verification
grim "$OUT_DIR/screen.png"

# Verify clipboard roundtrip inside Wayland session
echo -n "win95-wayland-clipboard-ok" | wl-copy
CLIP_VAL="$(wl-paste)"
if [[ "$CLIP_VAL" == "win95-wayland-clipboard-ok" ]]; then
    touch "$OUT_DIR/clipboard.ok"
fi

# Launch multiple simultaneous windows
chicago-control-panel --system &
P1=$!
chicago-control-panel --run &
P2=$!
chicago-control-panel --find &
P3=$!
sleep 1.0

if kill -0 "$P1" "$P2" "$P3" 2>/dev/null; then
    touch "$OUT_DIR/multiwin.ok"
fi

kill "$P1" "$P2" "$P3" 2>/dev/null || true
labwc -e
EOF
chmod +x "$INNER_TEST"

# No user files are moved: the helper supplies a private HOME and runtime.
HYPR_BEFORE="$(find "$HOME/.config/hypr" -type f -exec sha256sum {} + 2>/dev/null | sort || true)"
STARTUP_CMD="$(python3 -c 'import shlex,sys; print(shlex.join(sys.argv[1:]))' "$INNER_TEST" "$TMP_TEST_DIR")"
INTEGRATION_STATUS=0
if [[ "${1:-}" == "--static" ]]; then
    INTEGRATION_STATUS=77
else
    python3 "$REPO_DIR/scripts/isolated.py" "$REPO_DIR/bin/start-chicago-wayland" \
        --headless --startup "$STARTUP_CMD" || INTEGRATION_STATUS=$?
fi
if [[ "$INTEGRATION_STATUS" == 77 ]]; then
    echo '  [SKIP] Live clipboard, rendering and dialog smoke checks'
    SKIP=$((SKIP + 3))
elif [[ "$INTEGRATION_STATUS" != 0 ]]; then
    echo "  [FAIL] Isolated compositor smoke test (exit $INTEGRATION_STATUS)"
    FAIL=$((FAIL + 1))
else
assert_test "Wayland session clipboard roundtrip (wl-copy / wl-paste)" test -f "$TMP_TEST_DIR/clipboard.ok"
assert_test "Three dialog processes remain alive" test -f "$TMP_TEST_DIR/multiwin.ok"
assert_test "Rendered pixel verification (#008080 desktop teal & #c0c0c0 taskbar gray)" \
    python3 -c "
from PIL import Image
im = Image.open('$TMP_TEST_DIR/screen.png').convert('RGB')
w, h = im.size
teal = im.getpixel((w // 2, h // 2))
gray = im.getpixel((w // 2, h - 10))
assert teal == (0, 128, 128), f'Expected teal (0,128,128), got {teal}'
assert gray == (192, 192, 192), f'Expected gray (192,192,192), got {gray}'
"

fi

# 5. Verify the pre-existing Hyprland files have the same hashes.
HYPR_AFTER="$(find "$HOME/.config/hypr" -type f -exec sha256sum {} + 2>/dev/null | sort || true)"
assert_test "Hyprland configuration file hashes unchanged" test "$HYPR_BEFORE" = "$HYPR_AFTER"
echo '  [SKIP] Manual QA: Start menu, minimize/restore, lock crash/hotplug and suspend/resume'
SKIP=$((SKIP + 1))

echo "========================================================"
echo " Test Results: $PASS passed, $FAIL failed, $SKIP skipped."
echo "========================================================"
[[ "$FAIL" -eq 0 ]]
