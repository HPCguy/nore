#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STAGE0_DIR="$ROOT_DIR/tmp/bootstrap/stage0"
STAGE1_DIR="$ROOT_DIR/tmp/bootstrap/stage1"
GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
STAGE0="$STAGE0_DIR/norec-stage0"
STAGE1="$STAGE1_DIR/bootstrap-compiler"
NOREC_C="$GENERATED_DIR/norec.c"
NOREC="$ROOT_DIR/norec"
CC_BIN="${CC:-clang}"
CLANG_FLAGS=(-std=c99 -O2 -fwrapv)

build_stage0_compiler() {
    mkdir -p "$STAGE0_DIR"
    if [ ! -x "$STAGE0" ] || [ "$SCRIPT_DIR/norec-stage0.c" -nt "$STAGE0" ] || [ "$SCRIPT_DIR/Makefile" -nt "$STAGE0" ]; then
        make -C "$SCRIPT_DIR" CC="$CC_BIN" STAGE0="$STAGE0" stage0 >/dev/null
    fi
    rm -rf "$STAGE0_DIR/std"
    ln -sfn "$ROOT_DIR/std" "$STAGE0_DIR/std"
}

compiler_sources_newer_than_norec() {
    if [ ! -x "$NOREC" ]; then
        return 0
    fi

    if [ "$SCRIPT_DIR/bootstrap.sh" -nt "$NOREC" ] || [ "$SCRIPT_DIR/Makefile" -nt "$NOREC" ] || [ "$SCRIPT_DIR/norec-stage0.c" -nt "$NOREC" ]; then
        return 0
    fi

    if find "$ROOT_DIR/compiler" "$ROOT_DIR/std" -type f -name '*.nore' -newer "$NOREC" -print -quit | grep -q .; then
        return 0
    fi

    return 1
}

build_norec() {
    mkdir -p "$STAGE1_DIR" "$GENERATED_DIR" "$BIN_DIR"
    build_stage0_compiler
    if ! compiler_sources_newer_than_norec; then
        return
    fi
    (
        cd "$ROOT_DIR"
        "$STAGE0" compiler/main.nore -o "$STAGE1"
        "$STAGE1" compiler/main.nore "$NOREC_C" .
    )
    "$CC_BIN" "${CLANG_FLAGS[@]}" "$NOREC_C" -o "$NOREC"
}

if [ $# -eq 0 ]; then
    build_norec
    exit 0
fi

build_norec
exec "$NOREC" "$@"
