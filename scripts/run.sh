#!/usr/bin/env bash
# Run the local build/bin/bierkistnRadio, auto-picking wayland or xcb
# based on WAYLAND_DISPLAY. Assumes you are inside `nix develop`.
# Entry point: no positional arguments are forwarded; cwd, display and Qt prefixes are inputs.
# Returns build failure or exec'd application's exit status; selects platform and starts GUI.
set -euo pipefail

# shellcheck source=scripts/qt-env.sh
source "$(dirname "$0")/qt-env.sh"

cmake --build build

BIN="build/bin/bierkistnRadio"

if [[ -n "${WAYLAND_DISPLAY:-}" ]]; then
    export QT_QPA_PLATFORM=wayland
else
    export QT_QPA_PLATFORM=xcb
fi

exec "$BIN"
