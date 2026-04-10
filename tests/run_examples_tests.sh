#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/test_setup.sh"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
COMPILER_BIN="$NORE_BIN"
TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/nore-examples.XXXXXX")"
CAT_INPUT="$TMP_DIR/example.txt"
JSON_INPUT="$TMP_DIR/example.json"

trap 'rm -rf "$TMP_DIR"' EXIT

cd "$ROOT_DIR"

printf 'alpha beta\nz\n' > "$CAT_INPUT"
printf '{"a":1}\n' > "$JSON_INPUT"

cat_output=""
if ! cat_output=$("$COMPILER_BIN" --run examples/cat.nore -- "$CAT_INPUT" 2>&1); then
    echo "cat example failed"
    echo "$cat_output"
    exit 1
fi
cat_expected=$'alpha beta\nz'
if [ "$cat_output" != "$cat_expected" ]; then
    echo "unexpected output from cat example"
    exit 1
fi
echo "PASS: examples/cat.nore"

wc_output=""
if ! wc_output=$("$COMPILER_BIN" --run examples/wc.nore -- "$CAT_INPUT" 2>&1); then
    echo "wc example failed"
    echo "$wc_output"
    exit 1
fi
wc_expected="$(printf "%8d%8d%8d %s" 2 3 13 "$CAT_INPUT")"
if [ "$wc_output" != "$wc_expected" ]; then
    echo "unexpected output from wc example"
    exit 1
fi
echo "PASS: examples/wc.nore"

json_output=""
if ! json_output=$("$COMPILER_BIN" --run examples/json.nore -- "$JSON_INPUT" 2>&1); then
    echo "json example failed"
    echo "$json_output"
    exit 1
fi
json_expected=$'{ (1 keys)\n  "a": 1\n2 nodes total'
if [ "$json_output" != "$json_expected" ]; then
    echo "unexpected output from json example"
    exit 1
fi
echo "PASS: examples/json.nore"

echo ""
echo "Results: 3 passed, 0 failed"
