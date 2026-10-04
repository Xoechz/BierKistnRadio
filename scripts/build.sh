#!/usr/bin/env bash
# Incremental build via Ninja into build/bin/bierkistnRadio. Fast iteration.
# Assumes you are inside `nix develop` and have run scripts/setup.sh once.
# Entry point: no consumed positional arguments; repository cwd/existing build are inputs.
# Returns CMake build exit status (0 success); incrementally updates local binaries.
set -euo pipefail

cmake --build build
