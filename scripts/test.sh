#!/usr/bin/env bash
# Build and run all tests via CTest. Assumes you are in `nix develop`
# and have run scripts/setup.sh once.
set -euo pipefail

cmake --build build
ctest --test-dir build --output-on-failure
