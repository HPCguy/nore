#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMPILER_BIN="${NORE_BIN:-$ROOT_DIR/norec}"
COMPILER_TEST_MODE="${COMPILER_TEST_MODE:-norec}"

run_expect_code() {
    local source_path="$1"
    local expected_code="$2"
    local output=""

    if output=$("$COMPILER_BIN" "$source_path" 2>&1); then
        echo "expected $source_path to fail with [$expected_code]"
        exit 1
    fi

    if ! printf '%s\n' "$output" | grep -q "\\[$expected_code\\]"; then
        echo "expected $source_path to fail with [$expected_code]"
        echo "$output"
        exit 1
    fi
}

if [ "$COMPILER_TEST_MODE" = "stage0" ]; then
    run_expect_code "$ROOT_DIR/tests/errors/S046_slice_local_var.nore" "S046"
    run_expect_code "$ROOT_DIR/tests/errors/S050_u8_out_of_range.nore" "S050"
else
    run_expect_code "$SCRIPT_DIR/diag_fixtures/slice_local_inferred.nore" "S046"
    run_expect_code "$SCRIPT_DIR/diag_fixtures/u8_negative_i64_min.nore" "S050"
fi
