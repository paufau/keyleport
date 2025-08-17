#!/usr/bin/env bash
set -euo pipefail
# Run Include-What-You-Use on this project in a separate build directory.
# Usage: scripts/iwyu.sh [generator]
# Example: scripts/iwyu.sh Ninja

GENERATOR=${1:-"Unix Makefiles"}
BUILD_DIR="build-iwyu"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake -G "$GENERATOR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DKEYLEPORT_ENABLE_IWYU=ON ..
cmake --build . --parallel

echo "Done. IWYU suggestions (if any) were printed during compilation."
