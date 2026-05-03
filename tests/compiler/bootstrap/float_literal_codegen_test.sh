#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
source "$SCRIPT_DIR/setup_stage0.sh"

GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
SOURCE="$ROOT_DIR/tests/compiler/bootstrap/float_literal_codegen_repo/root.nore"
GENERATED="$GENERATED_DIR/stage0_float_literals.c"
BIN="$BIN_DIR/stage0_float_literals"

mkdir -p "$GENERATED_DIR" "$BIN_DIR"

cd "$ROOT_DIR"

"$STAGE0" --emit-c "$SOURCE" "$GENERATED" .

if grep -q "1.41421;" "$GENERATED"; then
    echo "stage0 truncated f64 global literal"
    exit 1
fi

if ! grep -Eq "1\\.4142135[0-9]*" "$GENERATED"; then
    echo "stage0 did not preserve enough precision for f64 global literal"
    exit 1
fi

if grep -Eq "gammaa - 1\\)" "$GENERATED"; then
    echo "stage0 emitted f64 subtraction with an integer literal"
    exit 1
fi

if ! grep -Eq "gammaa - 1\\.0" "$GENERATED"; then
    echo "stage0 did not emit integral f64 literal with a decimal marker"
    exit 1
fi

"$CC_BIN" "${CLANG_FLAGS[@]}" "$GENERATED" -o "$BIN"
"$BIN"
