#!/usr/bin/env bash
# Build and run all tests via CTest. Assumes you are in `nix develop`
# and have run scripts/setup.sh once.
set -euo pipefail

# shellcheck source=scripts/qt-env.sh
source "$(dirname "$0")/qt-env.sh"

cmake --build build
ctest --test-dir build --output-on-failure
