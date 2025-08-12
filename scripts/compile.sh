#!/usr/bin/env bash
set -euo pipefail

# Ensure build directory exists
mkdir -p build

# Configure CMake if not already configured
if [ ! -f build/CMakeCache.txt ]; then
	cmake -S . -B build
fi

# Build
cmake --build build -j