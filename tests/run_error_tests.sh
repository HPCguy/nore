#!/bin/bash
# Error code test runner for Nore compiler
# Tests that expected error codes are produced for each test file

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NORE="${SCRIPT_DIR}/../nore"

# Check if nore binary exists
if [ ! -x "$NORE" ]; then
    echo "Error: nore binary not found at $NORE"
    echo "Run 'make' first to build the compiler"
    exit 1
fi

for f in "$SCRIPT_DIR"/errors/*.nore; do
    # Extract expected error code from filename (e.g., P001 from P001_missing_rparen.nore)
    expected=$(basename "$f" .nore | cut -d'_' -f1)

    # Run compiler and capture stderr
    output=$("$NORE" "$f" 2>&1)

    if echo "$output" | grep -q "\[$expected\]"; then
        echo "PASS: $(basename "$f")"
        ((PASS++))
    else
        echo "FAIL: $(basename "$f") (expected [$expected])"
        echo "  Got: $output"
        ((FAIL++))
    fi
done

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
