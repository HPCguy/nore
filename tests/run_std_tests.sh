#!/bin/bash
# Stdlib test runner for Nore compiler
# Tests that std/ library programs compile, run, and exit successfully

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_NORE_BIN="${SCRIPT_DIR}/../bootstrap/bootstrap.sh"
if [ ! -x "$DEFAULT_NORE_BIN" ]; then
    DEFAULT_NORE_BIN="${SCRIPT_DIR}/../nore"
fi
NORE_BIN="${NORE_BIN:-$DEFAULT_NORE_BIN}"

if [ ! -x "$NORE_BIN" ]; then
    echo "Error: compiler entrypoint not found at $NORE_BIN"
    echo "Run 'make' first to build the compiler"
    exit 1
fi

for f in "$SCRIPT_DIR"/std/*.nore; do
    output=$("$NORE_BIN" --run "$f" 2>&1)
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "PASS: std/$(basename "$f")"
        ((PASS++))
    else
        echo "FAIL: std/$(basename "$f") (exit code $exit_code)"
        echo "  Output: $output"
        ((FAIL++))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
