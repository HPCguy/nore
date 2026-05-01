#!/bin/bash
# Compiler test runner for the Nore-written compiler.
# Runs compiler unit tests plus shell-based integration and rebuild tests.

PASS=0
FAIL=0
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/test_setup.sh"
COMPILER_TEST_MODE="${COMPILER_TEST_MODE:-norec}"
COMPILER_TEST_PROFILE="${COMPILER_TEST_PROFILE:-core}"
export NORE_BIN
export COMPILER_TEST_MODE

# Keep all reporting in one place so profiles can mix directories and named files.
run_test_file() {
    local test_path="$1"
    local label="$2"
    local mode="$3"
    local output=""
    local exit_code=0

    if [ ! -f "$test_path" ]; then
        echo "FAIL: $label/$(basename "$test_path") (missing test file)"
        ((FAIL++))
        return
    fi

    if [ "$mode" = "shell" ]; then
        chmod +x "$test_path"
        output=$("$test_path" 2>&1)
        exit_code=$?
    else
        output=$("$NORE_BIN" --run "$test_path" 2>&1)
        exit_code=$?
    fi

    if [ $exit_code -eq 0 ]; then
        echo "PASS: $label/$(basename "$test_path")"
        ((PASS++))
    else
        echo "FAIL: $label/$(basename "$test_path") (exit code $exit_code)"
        echo "  Output: $output"
        ((FAIL++))
    fi
}

# mode: "nore" runs via the configured compiler entrypoint, "shell" runs the script directly
# but still inherits NORE_BIN so shell integration tests can honor the selected path.
run_test_dir() {
    local test_dir="$1"
    local label="$2"
    local mode="$3"
    local found=0
    local pattern=""
    local f=""

    if [ "$mode" = "shell" ]; then
        pattern="*_test.sh"
    else
        pattern="*_test.nore"
    fi

    for f in "$test_dir"/$pattern; do
        [ -f "$f" ] || continue
        found=1
        run_test_file "$f" "$label" "$mode"
    done

    if [ $found -eq 0 ]; then
        echo "SKIP: $label/ (no tests yet)"
    fi
}

run_named_tests() {
    local label="$1"
    local mode="$2"
    local found=0
    local rel_path=""
    shift 2

    for rel_path in "$@"; do
        found=1
        run_test_file "$SCRIPT_DIR/$rel_path" "$label" "$mode"
    done

    if [ $found -eq 0 ]; then
        echo "SKIP: $label/ (no tests yet)"
    fi
}

run_profile_fast() {
    local dir=""

    for dir in support lexer parser imports; do
        run_test_dir "$SCRIPT_DIR/compiler/$dir" "$dir" "nore"
    done

    run_named_tests "sema" "nore" \
        "compiler/sema/binding_test.nore" \
        "compiler/sema/builtin_success_test.nore" \
        "compiler/sema/effects_test.nore" \
        "compiler/sema/enum_success_test.nore" \
        "compiler/sema/injected_success_test.nore" \
        "compiler/sema/type_success_test.nore"

    run_named_tests "codegen" "nore" \
        "compiler/codegen/entrypoint_codegen_test.nore" \
        "compiler/codegen/compiler_main_validation_test.nore" \
        "compiler/codegen/byte_view_storage_codegen_test.nore" \
        "compiler/codegen/target_os_ref_test.nore"

    run_named_tests "integration" "shell" \
        "compiler/integration/assert_runtime_test.sh" \
        "compiler/integration/assert_strip_test.sh" \
        "compiler/integration/driver_test.sh"
}

run_profile_core() {
    run_profile_fast

    run_named_tests "codegen" "nore" \
        "compiler/codegen/compiler_main_codegen_test.nore" \
        "compiler/codegen/std_io_module_codegen_test.nore"
}

run_profile_bootstrap() {
    run_named_tests "integration" "shell" \
        "compiler/integration/assert_runtime_test.sh" \
        "compiler/integration/assert_strip_test.sh" \
        "compiler/integration/driver_test.sh"

    run_named_tests "bootstrap" "shell" \
        "compiler/bootstrap/smoke_test.sh" \
        "compiler/bootstrap/self_compile_test.sh"
}

run_profile_all() {
    local dir=""

    for dir in support lexer parser imports sema codegen; do
        run_test_dir "$SCRIPT_DIR/compiler/$dir" "$dir" "nore"
    done

    run_test_dir "$SCRIPT_DIR/compiler/integration" "integration" "shell"
    run_test_dir "$SCRIPT_DIR/compiler/bootstrap" "bootstrap" "shell"
}

case "$COMPILER_TEST_PROFILE" in
    fast)
        run_profile_fast
        ;;
    core)
        run_profile_core
        ;;
    bootstrap)
        run_profile_bootstrap
        ;;
    all)
        run_profile_all
        ;;
    *)
        echo "Unknown compiler test profile: $COMPILER_TEST_PROFILE"
        echo "Expected one of: fast, core, bootstrap, all"
        exit 1
        ;;
esac

echo ""
echo "Results: $PASS passed, $FAIL failed"
exit $FAIL
