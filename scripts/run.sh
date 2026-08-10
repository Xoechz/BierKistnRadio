#!/usr/bin/env bash
# Run the local build/bin/bierkistnRadio, auto-picking wayland or xcb
# based on WAYLAND_DISPLAY. Assumes you are inside `nix develop`.
set -euo pipefail

BIN="build/bin/bierkistnRadio"

if [[ ! -x "$BIN" ]]; then
    echo "error: $BIN not found. Run scripts/setup.sh then scripts/build.sh first." >&2
    exit 1
fi

if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    export QT_QPA_PLATFORM=wayland
else
    export QT_QPA_PLATFORM=xcb
fi

exec "$BIN"
