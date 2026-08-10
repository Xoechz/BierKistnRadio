#!/usr/bin/env bash
# Full reproducible Nix build (x86_64) -> result/bin/bierkistnRadio.
# Use for a clean verification of the packaged app.
set -euo pipefail

nix build .#bierkistnRadio --print-build-logs
