#!/usr/bin/env bash
# Build an unmodified, pinned gtk-session-lock when no suitable system package exists.
set -euo pipefail
REVISION=b9ddb2792b613d14622acada73c64f16a2635b40
PREFIX="${CHICAGO_LOCK_PREFIX:-$HOME/.local/share/chicago-wayland/deps/gtk-session-lock}"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
if pkg-config --atleast-version=0.2.0 gtk-session-lock-0; then exit 0; fi
SOURCE="${CHICAGO_SESSION_LOCK_SOURCE:-${PREFIX}-src}"
if [[ ! -f "$SOURCE/UPSTREAM_REVISION" || "$(cat "$SOURCE/UPSTREAM_REVISION")" != "$REVISION" ]]; then
    if [[ -e "$SOURCE" ]]; then
        echo "Unrecognized dependency source at $SOURCE; supply CHICAGO_SESSION_LOCK_SOURCE with the pinned checkout." >&2
        exit 1
    fi
    mkdir -p "$SOURCE"
    git -C "$SOURCE" init --quiet
    git -C "$SOURCE" remote add origin https://github.com/Cu3PO42/gtk-session-lock.git
    git -C "$SOURCE" fetch --depth 1 origin "$REVISION"
    git -C "$SOURCE" checkout --detach --quiet FETCH_HEAD
    printf '%s\n' "$REVISION" > "$SOURCE/UPSTREAM_REVISION"
fi
BUILD="${PREFIX}-build"
meson setup --wipe "$BUILD" "$SOURCE" --prefix="$PREFIX" --libdir=lib \
    -Dintrospection=false -Dvapi=false -Dtests=false -Dexamples=false
ninja -C "$BUILD"
ninja -C "$BUILD" install
