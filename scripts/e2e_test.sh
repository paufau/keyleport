#!/usr/bin/env bash
set -euo pipefail

# Simple E2E test for keyleport sender/receiver on macOS/Linux
# Assumes binary is at ../src/keyleport relative to this script.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
BIN="${ROOT_DIR}/keyleport"
PORT=8089
MSG="hello-from-test-$(date +%s)"
LOG_DIR="${ROOT_DIR}/scripts/.logs"
RCV_LOG="${LOG_DIR}/receiver.out"

mkdir -p "${LOG_DIR}"

# Ensure binary exists
if [[ ! -x "${BIN}" ]]; then
  echo "Binary not found at ${BIN}. Build it first." >&2
  exit 1
fi

# Start receiver in background
"${BIN}" --mode receiver --port "${PORT}" >"${RCV_LOG}" 2>&1 &
RCV_PID=$!
trap 'kill ${RCV_PID} 2>/dev/null || true' EXIT

# Wait for receiver to start listening (best-effort sleep)
sleep 0.5

# Send message via sender (pipe input)
printf "%s\n" "${MSG}" | "${BIN}" --mode sender --ip 127.0.0.1 --port "${PORT}" >/dev/null 2>&1 || {
  echo "Sender failed" >&2
  exit 2
}

# Give receiver a moment to accept and print
sleep 0.5

# Check output
if grep -q "${MSG}" "${RCV_LOG}"; then
  echo "PASS: message seen by receiver"
  exit 0
else
  echo "FAIL: message not found in receiver output" >&2
  echo "Receiver log:" >&2
  cat "${RCV_LOG}" >&2 || true
  exit 3
fi
