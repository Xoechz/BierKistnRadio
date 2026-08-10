#!/usr/bin/env bash
#Cross-build the aarch64-linux package for the Raspberry Pi.
#The system repo consumes packages.aarch64-linux.bierkistnRadio from this flake.
set -euo pipefail

nix build .#packages.aarch64-linux.bierkistnRadio --print-build-logs
