#!/bin/bash
# Compiler test runner for the Nore-written compiler.
# Runs compiler unit tests plus shell-based selfhost/bootstrap tests.

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
NORE_BIN="${NORE_BIN:-${SCRIPT_DIR}/../nore}"

if [ ! -x "$NORE_BIN" ]; then
    echo "Error: compiler entrypoint not found at $NORE_BIN"
    echo "Run 'make' first to build the compiler"
    exit 1
fi

# mode: "nore" runs via the configured compiler entrypoint, "shell" runs the script directly.
run_test_dir() {
    test_dir="$1"
    label="$2"
    mode="$3"
    found=0

    if [ "$mode" = "shell" ]; then
        pattern="*_test.sh"
    else
        pattern="*_test.nore"
    fi

    for f in "$test_dir"/$pattern; do
        [ -f "$f" ] || continue
        found=1

        if [ "$mode" = "shell" ]; then
            chmod +x "$f"
            output=$("$f" 2>&1)
        else
            output=$("$NORE_BIN" --run "$f" 2>&1)
        fi
        exit_code=$?

        if [ $exit_code -eq 0 ]; then
            echo "PASS: $label/$(basename "$f")"
            ((PASS++))
        else
            echo "FAIL: $label/$(basename "$f") (exit code $exit_code)"
            echo "  Output: $output"
            ((FAIL++))
        fi
    done

    if [ $found -eq 0 ]; then
        echo "SKIP: $label/ (no tests yet)"
    fi
}

for dir in support lexer parser imports sema codegen; do
    test_dir="$SCRIPT_DIR/compiler/$dir"
    [ -d "$test_dir" ] || continue
    run_test_dir "$test_dir" "$dir" "nore"
done

test_dir="$SCRIPT_DIR/compiler/selfhost"
[ -d "$test_dir" ] && run_test_dir "$test_dir" "selfhost" "shell"

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
