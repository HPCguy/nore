#!/bin/bash
# Bootstrap test runner for Nore compiler
# Tests that bootstrap compiler modules compile, run, and exit successfully
# Only runs *_test.nore files (skips fixtures and sample modules)

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NORE="${SCRIPT_DIR}/../nore"

if [ ! -x "$NORE" ]; then
    echo "Error: nore binary not found at $NORE"
    echo "Run 'make' first to build the compiler"
    exit 1
fi

for dir in support lexer parser imports sema codegen selfhost; do
    test_dir="$SCRIPT_DIR/bootstrap/$dir"
    [ -d "$test_dir" ] || continue

    found=0
    for f in "$test_dir"/*_test.nore; do
        [ -f "$f" ] || continue
        found=1

        output=$("$NORE" --run "$f" 2>&1)
        exit_code=$?

        if [ $exit_code -eq 0 ]; then
            echo "PASS: $dir/$(basename "$f")"
            ((PASS++))
        else
            echo "FAIL: $dir/$(basename "$f") (exit code $exit_code)"
            echo "  Output: $output"
            ((FAIL++))
        fi
    done

    if [ $found -eq 0 ]; then
        echo "SKIP: $dir/ (no tests yet)"
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
