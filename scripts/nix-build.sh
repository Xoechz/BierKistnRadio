#!/usr/bin/env bash
# Full reproducible Nix build (x86_64) -> result/bin/bierkistnRadio.
# Use for a clean verification of the packaged app.
# Entry point: no consumed positional arguments; Nix/builders/network are external inputs.
# Returns Nix build status (0 success); builds package and updates result, without deployment.
set -euo pipefail

nix build .#bierkistnRadio --print-build-logs
