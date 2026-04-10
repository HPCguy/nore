#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
source "$SCRIPT_DIR/setup_stage0.sh"

STAGE1_DIR="$ROOT_DIR/tmp/bootstrap/stage1"
STAGE2_DIR="$ROOT_DIR/tmp/bootstrap/stage2"
GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
STAGE1="$STAGE1_DIR/bootstrap-compiler"
STAGE2="$STAGE2_DIR/nore2"
STAGE3="$STAGE2_DIR/nore3"

mkdir -p "$STAGE1_DIR" "$STAGE2_DIR" "$GENERATED_DIR"

cd "$ROOT_DIR"

"$STAGE0" compiler/main.nore -o "$STAGE1"

"$STAGE1" --emit-c compiler/main.nore "$GENERATED_DIR/nore_stage2.c" .
"$CC_BIN" "${CLANG_FLAGS[@]}" "$GENERATED_DIR/nore_stage2.c" -o "$STAGE2"

"$STAGE2" --emit-c compiler/main.nore "$GENERATED_DIR/nore_stage3.c" .
"$CC_BIN" "${CLANG_FLAGS[@]}" "$GENERATED_DIR/nore_stage3.c" -o "$STAGE3"

cmp -s "$GENERATED_DIR/nore_stage2.c" "$GENERATED_DIR/nore_stage3.c"
