#!/usr/bin/env bash
# Build and run all tests via CTest. Assumes you are in `nix develop`
# and have run scripts/setup.sh once.
# Entry point: no consumed positional arguments; sources Qt paths and uses build/.
# Returns 0 only when build and CTest pass; otherwise exits at the failed command.
set -euo pipefail

# shellcheck source=scripts/qt-env.sh
source "$(dirname "$0")/qt-env.sh"

cmake --build build
ctest --test-dir build --output-on-failure
