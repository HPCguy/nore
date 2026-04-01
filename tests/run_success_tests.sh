#!/bin/bash
# Success test runner for Nore compiler
# Tests that programs compile, run, and exit successfully (exit code 0)

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEFAULT_NORE_BIN="${SCRIPT_DIR}/../bootstrap/bootstrap.sh"
if [ ! -x "$DEFAULT_NORE_BIN" ]; then
    DEFAULT_NORE_BIN="${SCRIPT_DIR}/../nore"
fi
NORE_BIN="${NORE_BIN:-$DEFAULT_NORE_BIN}"

# Check if the configured compiler entrypoint exists
if [ ! -x "$NORE_BIN" ]; then
    echo "Error: compiler entrypoint not found at $NORE_BIN"
    echo "Run 'make' first to build the compiler"
    exit 1
fi

for f in "$SCRIPT_DIR"/success/*.nore; do
    # Run compiler with --run and capture output
    output=$("$NORE_BIN" --run "$f" 2>&1)
    exit_code=$?

    if [ $exit_code -eq 0 ]; then
        echo "PASS: $(basename "$f")"
        ((PASS++))
    else
        echo "FAIL: $(basename "$f") (exit code $exit_code)"
        echo "  Output: $output"
        ((FAIL++))
    fi
done

# Run stdlib tests (tests/std/*.nore)
if ls "$SCRIPT_DIR"/std/*.nore 1>/dev/null 2>&1; then
    echo ""
    echo "--- stdlib tests ---"
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
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
