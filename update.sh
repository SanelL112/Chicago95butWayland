#!/usr/bin/env bash
# update.sh: Rebuilds native components and refreshes Chicago-Wayland configurations.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
echo "Updating Chicago-Wayland build and configurations..."
"$REPO_DIR/install.sh" --user
