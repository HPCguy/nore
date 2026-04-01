#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
DRIVER="$ROOT_DIR/bootstrap/bootstrap.sh"

run_expect_code() {
    local source_path="$1"
    local expected_code="$2"
    local output=""

    if output=$(bash "$DRIVER" "$source_path" 2>&1); then
        echo "expected $source_path to fail with [$expected_code]"
        exit 1
    fi

    if ! printf '%s\n' "$output" | grep -q "\\[$expected_code\\]"; then
        echo "expected $source_path to fail with [$expected_code]"
        echo "$output"
        exit 1
    fi
}

run_expect_code "$SCRIPT_DIR/diag_fixtures/slice_local_inferred.nore" "S046"
run_expect_code "$SCRIPT_DIR/diag_fixtures/u8_negative_i64_min.nore" "S050"
