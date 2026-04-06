#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMPILER_BIN="${NORE_BIN:-$ROOT_DIR/norec}"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
HELLO_BIN="$BIN_DIR/driver_hello"
BENCH_HELLO_BIN="$BIN_DIR/driver_bench_hello"
PATH_HELLO_BIN="$BIN_DIR/driver_hello_path"
CC_FAIL_BIN="$BIN_DIR/driver_cc_fail"
EMIT_C="$BIN_DIR/driver_emit.c"
LEGACY_EMIT_C="$BIN_DIR/driver_legacy_emit.c"
PATH_EMIT_C="$BIN_DIR/driver_emit_std_path.c"
TIME_NS_SRC="$BIN_DIR/driver_time_ns.nore"
TIME_NS_BIN="$BIN_DIR/driver_time_ns"

mkdir -p "$BIN_DIR"
rm -f "$HELLO_BIN" "$BENCH_HELLO_BIN" "$PATH_HELLO_BIN" "$CC_FAIL_BIN" "$EMIT_C" "$LEGACY_EMIT_C" "$PATH_EMIT_C" "$TIME_NS_SRC" "$TIME_NS_BIN"

version_output=$("$COMPILER_BIN" --version 2>&1)
expected_version="norec v0.1.1"
if [ "${COMPILER_TEST_MODE:-norec}" = "stage0" ]; then
    expected_version="norec-stage0 v0.1.1"
fi
if [ "$version_output" != "$expected_version" ]; then
    echo "unexpected version output: $version_output"
    exit 1
fi

if [ "${COMPILER_TEST_MODE:-norec}" = "norec" ]; then
    help_output=$("$COMPILER_BIN" --help 2>&1)
    if ! printf '%s\n' "$help_output" | grep -q -- "^norec v0.1.1$"; then
        echo "help output does not include version banner"
        exit 1
    fi
    if ! printf '%s\n' "$help_output" | grep -q -- "norec --emit-c"; then
        echo "help output does not document --emit-c"
        exit 1
    fi
    if ! printf '%s\n' "$help_output" | grep -q -- "--help"; then
        echo "help output does not document --help"
        exit 1
    fi
    if ! printf '%s\n' "$help_output" | grep -q -- "--version"; then
        echo "help output does not document --version"
        exit 1
    fi
    if ! printf '%s\n' "$help_output" | grep -q -- "--codegen"; then
        echo "help output does not document debug dump flags"
        exit 1
    fi
fi

if [ "${COMPILER_TEST_MODE:-norec}" = "norec" ]; then
    unknown_flag_output=""
    if unknown_flag_output=$("$COMPILER_BIN" --unknown-flag 2>&1); then
        echo "unknown flag unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$unknown_flag_output" | grep -q -- "error\\[D002\\]"; then
        echo "unknown flag output does not include D002"
        exit 1
    fi
    if ! printf '%s\n' "$unknown_flag_output" | grep -q -- "^usage:"; then
        echo "unknown flag output does not include usage"
        exit 1
    fi

    missing_input_output=""
    if missing_input_output=$("$COMPILER_BIN" 2>&1); then
        echo "missing input unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$missing_input_output" | grep -q -- "error\\[D001\\]"; then
        echo "missing input output does not include D001"
        exit 1
    fi
    if ! printf '%s\n' "$missing_input_output" | grep -q -- "^usage:"; then
        echo "missing input output does not include usage"
        exit 1
    fi

    legacy_emit_output=""
    if legacy_emit_output=$("$COMPILER_BIN" tests/success/print_hello.nore "$LEGACY_EMIT_C" 2>&1); then
        echo "legacy positional emit mode unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$legacy_emit_output" | grep -q -- "error\\[D003\\]"; then
        echo "legacy positional emit output does not include D003"
        exit 1
    fi
    if ! printf '%s\n' "$legacy_emit_output" | grep -q -- "^usage:"; then
        echo "legacy positional emit output does not include usage"
        exit 1
    fi

    missing_output_output=""
    if missing_output_output=$("$COMPILER_BIN" tests/success/print_hello.nore -o 2>&1); then
        echo "missing output path unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$missing_output_output" | grep -q -- "error\\[D004\\]"; then
        echo "missing output path output does not include D004"
        exit 1
    fi
    if ! printf '%s\n' "$missing_output_output" | grep -q -- "^usage:"; then
        echo "missing output path output does not include usage"
        exit 1
    fi

    emit_c_usage_output=""
    if emit_c_usage_output=$("$COMPILER_BIN" --emit-c tests/success/print_hello.nore 2>&1); then
        echo "bad --emit-c usage unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$emit_c_usage_output" | grep -q -- "error\\[D001\\]"; then
        echo "bad --emit-c usage output does not include D001"
        exit 1
    fi
    if ! printf '%s\n' "$emit_c_usage_output" | grep -q -- "^usage:"; then
        echo "bad --emit-c usage output does not include usage"
        exit 1
    fi

    emit_c_write_output=""
    if emit_c_write_output=$("$COMPILER_BIN" --emit-c tests/success/print_hello.nore tmp/no/such/dir/driver_emit_fail.c 2>&1); then
        echo "emit-c write failure unexpectedly succeeded"
        exit 1
    fi
    if ! printf '%s\n' "$emit_c_write_output" | grep -q -- "error\\[D006\\]"; then
        echo "emit-c write failure output does not include D006"
        exit 1
    fi
fi

"$COMPILER_BIN" tests/success/print_hello.nore -o "$HELLO_BIN"

hello_output=$("$HELLO_BIN")
expected_hello=$'Hello, World!\n42\n-123\n0\ntest slice\nabc\n99'
if [ "$hello_output" != "$expected_hello" ]; then
    echo "unexpected output from driver-built hello binary"
    exit 1
fi

"$COMPILER_BIN" tests/success/print_hello.nore --bench -o "$BENCH_HELLO_BIN"

bench_output=$("$BENCH_HELLO_BIN")
if [ "$bench_output" != "$expected_hello" ]; then
    echo "unexpected output from --bench driver-built hello binary"
    exit 1
fi

cat > "$TIME_NS_SRC" <<'EOF'
native func time_ns(): i64

func main(): void = {
    val first: i64 = time_ns()
    val second: i64 = time_ns()
    assert first >= 0
    assert second >= 0
}
EOF

"$COMPILER_BIN" "$TIME_NS_SRC" -o "$TIME_NS_BIN"
"$TIME_NS_BIN"

"$COMPILER_BIN" --emit-c tests/success/import_std_math.nore "$EMIT_C" "$ROOT_DIR"
if [ ! -s "$EMIT_C" ]; then
    echo "--emit-c did not produce output"
    exit 1
fi

if [ "${COMPILER_TEST_MODE:-norec}" = "norec" ]; then
    (
        cd /tmp
        PATH="$ROOT_DIR:$PATH" norec "$ROOT_DIR/tests/success/print_hello.nore" -o "$PATH_HELLO_BIN"
    )

    path_output=$("$PATH_HELLO_BIN")
    if [ "$path_output" != "$expected_hello" ]; then
        echo "unexpected output from PATH-invoked norec binary"
        exit 1
    fi

    (
        cd /tmp
        PATH="$ROOT_DIR:$PATH" norec --emit-c "$ROOT_DIR/tests/success/import_std_math.nore" "$PATH_EMIT_C"
    )

    if [ ! -s "$PATH_EMIT_C" ]; then
        echo "PATH-invoked norec --emit-c did not produce output"
        exit 1
    fi
fi

if CC=false "$COMPILER_BIN" tests/success/print_hello.nore -o "$CC_FAIL_BIN" >/dev/null 2>&1; then
    echo "driver ignored CC override"
    exit 1
fi

"$COMPILER_BIN" --run tests/success/args.nore -- alpha beta gamma
