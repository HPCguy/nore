#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STAGE0="$ROOT_DIR/nore"
STAGE1_DIR="$ROOT_DIR/tmp/bootstrap/stage1"
STAGE2_DIR="$ROOT_DIR/tmp/bootstrap/stage2"
GENERATED_DIR="$ROOT_DIR/tmp/bootstrap/generated-c"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
STAGE1="$STAGE1_DIR/bootstrap-compiler"
STAGE2_C="$GENERATED_DIR/nore_stage2.c"
STAGE2="$STAGE2_DIR/nore2"
CC_BIN="${CC:-clang}"
CLANG_FLAGS=(-std=c99 -O2 -fwrapv)
CALLER_DIR="$PWD"

usage() {
    echo "usage: bootstrap-driver.sh [--run] <input.nore> [-o output] [-- <program args>]" >&2
}

resolve_path() {
    case "$1" in
        /*) printf '%s\n' "$1" ;;
        *) printf '%s/%s\n' "$CALLER_DIR" "$1" ;;
    esac
}

default_output_path() {
    local input_path="$1"
    local input_name
    input_name="$(basename "$input_path")"

    if [[ "$input_name" == *.nore ]]; then
        printf '%s/%s\n' "$CALLER_DIR" "${input_name%.nore}"
        return
    fi

    printf '%s/a.out\n' "$CALLER_DIR"
}

build_stage2_compiler() {
    mkdir -p "$STAGE1_DIR" "$STAGE2_DIR" "$GENERATED_DIR" "$BIN_DIR"
    (
        cd "$ROOT_DIR"
        "$STAGE0" compiler/main.nore -o "$STAGE1"
        "$STAGE1" compiler/main.nore "$STAGE2_C" .
    )
    "$CC_BIN" "${CLANG_FLAGS[@]}" "$STAGE2_C" -o "$STAGE2"
}

run_mode=0
input_arg=""
output_arg=""
program_args=()

while [ $# -gt 0 ]; do
    case "$1" in
        --help)
            usage
            exit 0
            ;;
        --run)
            run_mode=1
            shift
            ;;
        -o)
            if [ $# -lt 2 ]; then
                echo "error: missing output path after -o" >&2
                usage
                exit 1
            fi
            output_arg="$2"
            shift 2
            ;;
        --)
            shift
            program_args=("$@")
            break
            ;;
        -*)
            echo "error: unknown flag: $1" >&2
            usage
            exit 1
            ;;
        *)
            if [ -n "$input_arg" ]; then
                echo "error: unexpected extra input path: $1" >&2
                usage
                exit 1
            fi
            input_arg="$1"
            shift
            ;;
    esac
done

if [ -z "$input_arg" ]; then
    usage
    exit 1
fi

input_path="$(resolve_path "$input_arg")"
if [ ! -f "$input_path" ]; then
    echo "error: input file not found: $input_arg" >&2
    exit 1
fi

if [ -n "$output_arg" ]; then
    output_path="$(resolve_path "$output_arg")"
else
    output_path="$(default_output_path "$input_path")"
fi

mkdir -p "$(dirname "$output_path")"
mkdir -p "$GENERATED_DIR" "$BIN_DIR"

generated_c="$(mktemp "$GENERATED_DIR/bootstrap_driver_XXXXXX")"
temp_output=""

cleanup() {
    rm -f "$generated_c"
    if [ -n "$temp_output" ]; then
        rm -f "$temp_output"
    fi
}

trap cleanup EXIT

if [ "$run_mode" -eq 1 ] && [ -z "$output_arg" ]; then
    temp_output="$(mktemp "$BIN_DIR/bootstrap_run_XXXXXX")"
    output_path="$temp_output"
fi

build_stage2_compiler
"$STAGE2" "$input_path" "$generated_c" "$ROOT_DIR"
"$CC_BIN" "${CLANG_FLAGS[@]}" -x c "$generated_c" -o "$output_path"

if [ "$run_mode" -eq 1 ]; then
    "$output_path" "${program_args[@]}"
fi
