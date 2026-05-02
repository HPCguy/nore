#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
source "$SCRIPT_DIR/setup_stage0.sh"

STAGE1_DIR="$ROOT_DIR/tmp/bootstrap/stage1"
STAGE2_DIR="$ROOT_DIR/tmp/bootstrap/stage2"
GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
STAGE1="$STAGE1_DIR/bootstrap-compiler"
STRIPPED="$STAGE2_DIR/nore-stripped"
HELLO_BIN="$BIN_DIR/stripped_hello"

mkdir -p "$STAGE1_DIR" "$STAGE2_DIR" "$GENERATED_DIR" "$BIN_DIR"

cd "$ROOT_DIR"

"$STAGE0" compiler/main.nore -o "$STAGE1"

"$STAGE1" --emit-c compiler/main.nore "$GENERATED_DIR/nore_stripped.c" . --strip-asserts
"$CC_BIN" "${CLANG_FLAGS[@]}" "$GENERATED_DIR/nore_stripped.c" -o "$STRIPPED"

"$STRIPPED" --emit-c tests/success/print_hello.nore "$GENERATED_DIR/stripped_hello.c" .
"$CC_BIN" "${CLANG_FLAGS[@]}" "$GENERATED_DIR/stripped_hello.c" -o "$HELLO_BIN"

hello_output=$("$HELLO_BIN")
expected_hello=$'Hello, World!\n42\n-123\n0\ntest slice\nabc\n99'
if [ "$hello_output" != "$expected_hello" ]; then
    echo "unexpected output from stripped_hello"
    exit 1
fi
