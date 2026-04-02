#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMPILER_BIN="${NORE_BIN:-$ROOT_DIR/norec}"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
HELLO_BIN="$BIN_DIR/driver_hello"
PATH_HELLO_BIN="$BIN_DIR/driver_hello_path"
CC_FAIL_BIN="$BIN_DIR/driver_cc_fail"

mkdir -p "$BIN_DIR"
rm -f "$HELLO_BIN" "$PATH_HELLO_BIN" "$CC_FAIL_BIN"

"$COMPILER_BIN" tests/success/print_hello.nore -o "$HELLO_BIN"

hello_output=$("$HELLO_BIN")
expected_hello=$'Hello, World!\n42\n-123\n0\ntest slice\nabc\n99'
if [ "$hello_output" != "$expected_hello" ]; then
    echo "unexpected output from driver-built hello binary"
    exit 1
fi

if [ "${COMPILER_TEST_MODE:-selfhost}" = "selfhost" ]; then
    (
        cd /tmp
        PATH="$ROOT_DIR:$PATH" norec "$ROOT_DIR/tests/success/print_hello.nore" -o "$PATH_HELLO_BIN"
    )

    path_output=$("$PATH_HELLO_BIN")
    if [ "$path_output" != "$expected_hello" ]; then
        echo "unexpected output from PATH-invoked norec binary"
        exit 1
    fi
fi

if CC=false "$COMPILER_BIN" tests/success/print_hello.nore -o "$CC_FAIL_BIN" >/dev/null 2>&1; then
    echo "driver ignored CC override"
    exit 1
fi

"$COMPILER_BIN" --run tests/success/args.nore -- alpha beta gamma
