#!/usr/bin/env bash
set -euo pipefail

# Minimal format script
# - Runs clang-format -i on tracked headers/sources if available

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

CF_BIN="${CLANG_FORMAT:-clang-format}"

cd "$ROOT_DIR"

# Format
if command -v "$CF_BIN" >/dev/null 2>&1; then
  FILES=$(git ls-files '*.h' '*.hpp' '*.hh' '*.hxx' '*.c' '*.cc' '*.cpp' '*.cxx')
  if [ -n "$FILES" ]; then
    echo "Formatting sources with $CF_BIN..."
    $CF_BIN -i $FILES
  else
    echo "No files to format."
  fi
else
  echo "clang-format not found; skipping formatting." >&2
fi

# No clang-tidy step (removed by request)
