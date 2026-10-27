#!/usr/bin/env bash
# Recover an interrupted settings transaction without racing an active session.
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$REPO_DIR/scripts/session-settings.sh"
mkdir -p "$STATE_DIR"
exec 9>"$STATE_DIR/session.lock"
if ! flock -n 9; then
    echo 'Chicago-Wayland is active; settings will be restored when it exits.' >&2
    exit 1
fi
restore_gtk_configs
