#!/usr/bin/env bash
set -euo pipefail

# Ensure build directory exists
mkdir -p build

# Always (re)configure CMake to pick up CMakeLists changes
cmake -S . -B build

# Build
cmake --build build -j