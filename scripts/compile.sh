#!/usr/bin/env bash
set -euo pipefail

# Ensure build directory exists
mkdir -p build

# Always (re)configure CMake to pick up CMakeLists changes
# Forward any extra args to CMake configuration (e.g. generator, toolchain)
cmake -S . -B build "$@"

# Build
cmake --build build -j