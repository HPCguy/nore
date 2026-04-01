#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
STAGE0="$ROOT_DIR/nore"
STAGE1_DIR="$ROOT_DIR/tmp/bootstrap/stage1"
GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
STAGE1="$STAGE1_DIR/bootstrap-compiler"
CC_BIN="${CC:-clang}"
CLANG_FLAGS=(-std=c99 -O2 -fwrapv)

mkdir -p "$STAGE1_DIR" "$GENERATED_DIR" "$BIN_DIR"

cd "$ROOT_DIR"

# Keep the current bootstrap import policy simple: compile and run from repo root.
"$STAGE0" compiler/main.nore -o "$STAGE1"

"$STAGE1" tests/success/print_hello.nore "$GENERATED_DIR/bootstrap_hello.c" .
"$CC_BIN" "${CLANG_FLAGS[@]}" \
    "$GENERATED_DIR/bootstrap_hello.c" \
    -o "$BIN_DIR/bootstrap_hello"

hello_output=$("$BIN_DIR/bootstrap_hello")
expected_hello=$'Hello, World!\n42\n-123\n0\ntest slice\nabc\n99'
if [ "$hello_output" != "$expected_hello" ]; then
    echo "unexpected output from bootstrap_hello"
    exit 1
fi

"$STAGE1" tests/success/args.nore "$GENERATED_DIR/bootstrap_args.c" .
"$CC_BIN" "${CLANG_FLAGS[@]}" \
    "$GENERATED_DIR/bootstrap_args.c" \
    -o "$BIN_DIR/bootstrap_args"
"$BIN_DIR/bootstrap_args" alpha beta gamma
