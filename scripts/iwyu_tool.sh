#!/usr/bin/env bash
set -euo pipefail
# Aggregate IWYU results using iwyu_tool.py for more readable output.
# Usage: scripts/iwyu_tool.sh [--verbose N] [--] [extra args...] 
# Examples:
#   scripts/iwyu_tool.sh --verbose 3
#   scripts/iwyu_tool.sh -- -Xiwyu --mapping_file=./path/to/mapping.imp

# Find compile_commands.json
BUILD_DIR=${BUILD_DIR:-build}
COMPILE_DB="${BUILD_DIR}/compile_commands.json"
if [[ ! -f "$COMPILE_DB" ]]; then
  echo "compile_commands.json not found at $COMPILE_DB. Configure with: cmake -S . -B $BUILD_DIR -DCMAKE_EXPORT_COMPILE_COMMANDS=ON" >&2
  exit 1
fi

# Locate iwyu_tool.py
IWYU_TOOL=$(command -v iwyu_tool.py || true)
if [[ -z "${IWYU_TOOL}" ]]; then
  echo "iwyu_tool.py not found. Install via pipx/pip: pipx install include-what-you-use (or brew install include-what-you-use)." >&2
  exit 1
fi

set -x
"$IWYU_TOOL" -p "$COMPILE_DB" "$@"
