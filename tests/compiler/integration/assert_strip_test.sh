#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd)"
COMPILER_BIN="${NORE_BIN:-$ROOT_DIR/norec}"
BIN_DIR="$ROOT_DIR/tmp/bootstrap/bins"
PURE_SRC="$BIN_DIR/assert_strip_pure.nore"
EFFECT_SRC="$BIN_DIR/assert_strip_effect.nore"
NESTED_EFFECT_SRC="$BIN_DIR/assert_strip_nested_effect.nore"

mkdir -p "$BIN_DIR"

cat > "$PURE_SRC" <<'EOF'
func main(): void = {
    assert false
}
EOF

pure_output=""
set +e
pure_output=$("$COMPILER_BIN" --strip-asserts --run "$PURE_SRC" 2>&1)
pure_status=$?
set -e

if [ "$pure_status" -ne 0 ]; then
    echo "effect-free stripped assert exited with $pure_status, expected 0"
    echo "$pure_output"
    exit 1
fi

if printf '%s\n' "$pure_output" | grep -q -- "error\\[R001\\]"; then
    echo "effect-free stripped assert still emitted R001"
    echo "$pure_output"
    exit 1
fi

cat > "$EFFECT_SRC" <<'EOF'
func bump(mut ref n: i64): bool = {
    n = n + 1
    true
}

func main(): void = {
    mut n: i64 = 0
    assert bump(mut ref n)
}
EOF

effect_output=""
set +e
effect_output=$("$COMPILER_BIN" --strip-asserts --run "$EFFECT_SRC" 2>&1)
effect_status=$?
set -e

if [ "$effect_status" -ne 1 ]; then
    echo "effectful stripped assert exited with $effect_status, expected 1"
    echo "$effect_output"
    exit 1
fi

if ! printf '%s\n' "$effect_output" | grep -q -- "error\\[S091\\]"; then
    echo "effectful stripped assert output does not include S091"
    echo "$effect_output"
    exit 1
fi

cat > "$NESTED_EFFECT_SRC" <<'EOF'
func bump(mut ref n: i64): bool = {
    n = n + 1
    true
}

func main(): void = {
    mut n: i64 = 0
    val ok: bool = {
        assert bump(mut ref n)
        true
    }
    assert ok
}
EOF

nested_output=""
set +e
nested_output=$("$COMPILER_BIN" --strip-asserts --run "$NESTED_EFFECT_SRC" 2>&1)
nested_status=$?
set -e

if [ "$nested_status" -ne 1 ]; then
    echo "nested effectful stripped assert exited with $nested_status, expected 1"
    echo "$nested_output"
    exit 1
fi

if ! printf '%s\n' "$nested_output" | grep -q -- "error\\[S091\\]"; then
    echo "nested effectful stripped assert output does not include S091"
    echo "$nested_output"
    exit 1
fi
