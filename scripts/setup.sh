#!/usr/bin/env bash
# One-time CMake configure into build/ (Debug). Rerun after CMakeLists.txt changes.
# Assumes you are inside `nix develop`.
set -euo pipefail

cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
