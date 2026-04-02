#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMPILER_BIN="${NORE_BIN:-$ROOT_DIR/bootstrap/bootstrap.sh}"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
HELLO_BIN="$BIN_DIR/driver_hello"

mkdir -p "$BIN_DIR"
rm -f "$HELLO_BIN"

"$COMPILER_BIN" tests/success/print_hello.nore -o "$HELLO_BIN"

hello_output=$("$HELLO_BIN")
expected_hello=$'Hello, World!\n42\n-123\n0\ntest slice\nabc\n99'
if [ "$hello_output" != "$expected_hello" ]; then
    echo "unexpected output from driver-built hello binary"
    exit 1
fi

"$COMPILER_BIN" --run tests/success/args.nore -- alpha beta gamma
