#!/usr/bin/env bash
# Incremental build via Ninja into build/bin/bierkistnRadio. Fast iteration.
# Assumes you are inside `nix develop` and have run scripts/setup.sh once.
set -euo pipefail

cmake --build build
