#!/usr/bin/env bash
# toggle-scratchpad-terminal.sh: Quake-style dropdown MS-DOS Prompt scratchpad for Chicago-Wayland
set -euo pipefail

APP_ID="scratchpad-terminal"
CONF_PATH="$HOME/.config/chicago-wayland/kitty/kitty.conf"

if command -v wlrctl >/dev/null 2>&1 && wlrctl toplevel find "app_id:$APP_ID" >/dev/null 2>&1; then
    # Toggle minimize / focus using wlrctl
    # If the scratchpad is active, minimize it; otherwise focus/unminimize it
    wlrctl toplevel focus "app_id:$APP_ID" 2>/dev/null || wlrctl toplevel minimize "app_id:$APP_ID"
else
    # Launch Kitty with scratchpad class and MS-DOS title/styling
    if [[ -f "$CONF_PATH" ]]; then
        exec kitty --class "$APP_ID" --app-id "$APP_ID" -c "$CONF_PATH" --title "MS-DOS Prompt [Scratchpad]"
    else
        exec kitty --class "$APP_ID" --app-id "$APP_ID" --title "MS-DOS Prompt [Scratchpad]"
    fi
fi
