#!/usr/bin/env bash
# Build the aarch64-linux package for the Raspberry Pi. This is a native ARM
# derivation: on x86_64 Nix uses an ARM builder or configured binfmt emulation,
# rather than a pkgsCross toolchain. The system repo consumes this same output.
set -euo pipefail

nix build .#packages.aarch64-linux.bierkistnRadio --print-build-logs
