#!/usr/bin/env bash
set -euo pipefail

# Re-exec with bash if invoked from a different shell
if [[ -z "${BASH_VERSION:-}" ]]; then
	exec bash "$0" "$@"
fi

# Ensure build directory exists
mkdir -p build

# Parse known script flags and separate configure args from build flags
BUILD_VERBOSE=0
# Default: configure only if build dir not initialized
DO_CONFIGURE=0
CONFIGURE_ARGS=()
for arg in "$@"; do
	case "$arg" in
		--verbose)
			BUILD_VERBOSE=1
			;;
		--no-configure)
			DO_CONFIGURE=0
			;;
		--configure)
			DO_CONFIGURE=1
			;;
		*)
			CONFIGURE_ARGS+=("$arg")
			;;
	esac
done

# Auto-detect if configuration is needed (no CMakeCache.txt yet)
if [[ ! -f build/CMakeCache.txt ]]; then
	DO_CONFIGURE=1
fi

# Configure if requested/needed. Forward remaining args to CMake (-G, -D ...)
if [[ "$DO_CONFIGURE" -eq 1 ]]; then
	if [[ ${#CONFIGURE_ARGS[@]:-0} -gt 0 ]]; then
		cmake -S . -B build "${CONFIGURE_ARGS[@]}"
	else
		cmake -S . -B build
	fi
fi

# Build
BUILD_CMD=(cmake --build build -j)
if [[ "$BUILD_VERBOSE" -eq 1 ]]; then
	BUILD_CMD+=(--verbose)
fi
"${BUILD_CMD[@]}"