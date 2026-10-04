#!/usr/bin/env bash
# Remove the build/ directory.
# Entry point: no consumed positional arguments; repository cwd determines the target.
# Returns rm status (normally 0, including absent build/); recursively deletes local build.
set -euo pipefail

rm -rf build
