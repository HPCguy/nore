#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STAGE0_BIN="${STAGE0_BIN:-$ROOT_DIR/norec-stage0}"
SELFHOST_BIN="${NOREC_BIN:-$ROOT_DIR/norec}"
CC_BIN="${CC:-clang}"
RUNS="${RUNS:-3}"
OUT_DIR="$ROOT_DIR/tmp/benchmarks/compiler-matrix"
TIME_BIN="/usr/bin/time"
RAW_METRICS="$OUT_DIR/raw.tsv"

# The first matrix keeps the full compile path only where the root file is executable.
MATRIX_SOURCES=(
    "compiler/main.nore"
    "compiler/frontend/parser.nore"
    "compiler/sema/check.nore"
    "compiler/codegen/c_main.nore"
)

require_tooling() {
    if [ ! -x "$TIME_BIN" ]; then
        echo "missing timing tool: $TIME_BIN" >&2
        exit 1
    fi
    if [ ! -x "$STAGE0_BIN" ]; then
        echo "missing stage-0 compiler: $STAGE0_BIN" >&2
        exit 1
    fi
    if [ ! -x "$SELFHOST_BIN" ]; then
        echo "missing self-hosted compiler: $SELFHOST_BIN" >&2
        exit 1
    fi
}

read_time_metric() {
    metric_name="$1"
    time_path="$2"
    awk -v name="$metric_name" '$1 == name { print $2 }' "$time_path"
}

read_bench_metric() {
    metric_name="$1"
    bench_path="$2"
    awk -v name="$metric_name" '
        BEGIN {
            found = 0
        }
        /^bench / {
            for (i = 1; i <= NF; i++) {
                split($i, pair, "=")
                if (pair[1] == name) {
                    print pair[2]
                    found = 1
                    exit
                }
            }
        }
        END {
            if (!found) {
                print "-"
            }
        }
    ' "$bench_path"
}

sanitize_name() {
    printf '%s' "$1" | tr '/.' '__'
}

render_table() {
    if command -v column >/dev/null 2>&1; then
        column -s $'\t' -t
    else
        cat
    fi
}

print_row() {
    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9" "${10}" "${11}" "${12}" "${13}" "${14}" "${15}" "${16}" "${17}" "${18}" "${19}"
}

read_line_count() {
    data_path="$1"
    wc -l < "$data_path" | tr -d ' '
}

emit_selfhost_c() {
    source_path="$1"
    output_path="$2"
    "$SELFHOST_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" >/dev/null 2>&1
}

emit_stage0_c() {
    source_path="$1"
    output_path="$2"
    rm -f "$output_path"
    "$STAGE0_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" >/dev/null 2>&1
}

warmup_selfhost_emit_c() {
    source_rel="$1"
    source_path="$ROOT_DIR/$source_rel"
    stem="$(sanitize_name "$source_rel")"
    output_path="$OUT_DIR/${stem}-warmup.c"
    time_path="$OUT_DIR/${stem}-warmup.time"
    bench_path="$OUT_DIR/${stem}-warmup.bench"
    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$SELFHOST_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" --bench >/dev/null 2>"$bench_path"
    rm -f "$output_path" "$time_path" "$bench_path"
}

warmup_stage0_emit_c() {
    source_rel="$1"
    source_path="$ROOT_DIR/$source_rel"
    stem="$(sanitize_name "$source_rel")"
    output_path="$OUT_DIR/${stem}-stage0-warmup.c"
    time_path="$OUT_DIR/${stem}-stage0-warmup.time"
    bench_path="$OUT_DIR/${stem}-stage0-warmup.bench"
    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$STAGE0_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" --bench >/dev/null 2>"$bench_path"
    rm -f "$output_path" "$time_path" "$bench_path"
}

warmup_full_compile() {
    label="$1"
    compiler_bin="$2"
    output_path="$OUT_DIR/${label}-warmup"
    time_path="$OUT_DIR/${label}-warmup.time"
    bench_path="$OUT_DIR/${label}-warmup.bench"
    rm -f "$output_path" "$time_path" "$bench_path"
    if [ "$label" = "norec-full" ]; then
        "$TIME_BIN" -p -o "$time_path" "$compiler_bin" "$ROOT_DIR/compiler/main.nore" --bench -o "$output_path" >/dev/null 2>"$bench_path"
    else
        "$TIME_BIN" -p -o "$time_path" "$compiler_bin" "$ROOT_DIR/compiler/main.nore" --bench -o "$output_path" >/dev/null 2>"$bench_path"
    fi
    rm -f "$output_path" "$time_path" "$bench_path"
}

warmup_clang_c() {
    compiler_label="$1"
    source_path="$ROOT_DIR/compiler/main.nore"
    c_path="$OUT_DIR/${compiler_label}-clang-warmup.c"
    bin_path="$OUT_DIR/${compiler_label}-clang-warmup"
    time_path="$OUT_DIR/${compiler_label}-clang-warmup.time"

    rm -f "$c_path" "$bin_path" "$time_path"
    if [ "$compiler_label" = "norec" ]; then
        emit_selfhost_c "$source_path" "$c_path"
    else
        emit_stage0_c "$source_path" "$c_path"
    fi
    "$TIME_BIN" -p -o "$time_path" "$CC_BIN" -std=c99 -O2 -fwrapv "$c_path" -o "$bin_path" >/dev/null 2>&1
    rm -f "$c_path" "$bin_path" "$time_path"
}

measure_selfhost_emit_c() {
    source_rel="$1"
    run_id="$2"
    source_path="$ROOT_DIR/$source_rel"
    stem="$(sanitize_name "$source_rel")"
    output_path="$OUT_DIR/${stem}-emit-c-${run_id}.c"
    time_path="$OUT_DIR/${stem}-emit-c-${run_id}.time"
    bench_path="$OUT_DIR/${stem}-emit-c-${run_id}.bench"

    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$SELFHOST_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" --bench >/dev/null 2>"$bench_path"

    print_row \
        "norec" \
        "$source_rel" \
        "emit-c" \
        "$run_id" \
        "$(read_time_metric real "$time_path")" \
        "$(read_time_metric user "$time_path")" \
        "$(read_time_metric sys "$time_path")" \
        "$(read_bench_metric load_ns "$bench_path")" \
        "$(read_bench_metric check_ns "$bench_path")" \
        "$(read_bench_metric codegen_ns "$bench_path")" \
        "$(read_bench_metric tail_ns "$bench_path")" \
        "$(read_bench_metric total_ns "$bench_path")" \
        "$(read_bench_metric modules "$bench_path")" \
        "$(read_bench_metric tokens "$bench_path")" \
        "$(read_bench_metric nodes "$bench_path")" \
        "$(read_bench_metric symbols "$bench_path")" \
        "$(read_bench_metric types "$bench_path")" \
        "$(read_bench_metric c_bytes "$bench_path")" \
        "$(read_line_count "$output_path")" \
        >> "$RAW_METRICS"

    rm -f "$output_path" "$time_path" "$bench_path"
}

measure_stage0_emit_c() {
    source_rel="$1"
    run_id="$2"
    source_path="$ROOT_DIR/$source_rel"
    stem="$(sanitize_name "$source_rel")"
    output_path="$OUT_DIR/${stem}-stage0-emit-c-${run_id}.c"
    time_path="$OUT_DIR/${stem}-stage0-emit-c-${run_id}.time"
    bench_path="$OUT_DIR/${stem}-stage0-emit-c-${run_id}.bench"

    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$STAGE0_BIN" --emit-c "$source_path" "$output_path" "$ROOT_DIR" --bench >/dev/null 2>"$bench_path"

    print_row \
        "stage0" \
        "$source_rel" \
        "emit-c" \
        "$run_id" \
        "$(read_time_metric real "$time_path")" \
        "$(read_time_metric user "$time_path")" \
        "$(read_time_metric sys "$time_path")" \
        "$(read_bench_metric load_ns "$bench_path")" \
        "$(read_bench_metric check_ns "$bench_path")" \
        "$(read_bench_metric codegen_ns "$bench_path")" \
        "$(read_bench_metric tail_ns "$bench_path")" \
        "$(read_bench_metric total_ns "$bench_path")" \
        "-" "-" "-" "-" "-" \
        "$(wc -c < "$output_path" | tr -d ' ')" \
        "$(read_line_count "$output_path")" \
        >> "$RAW_METRICS"

    rm -f "$output_path" "$time_path" "$bench_path"
}

measure_selfhost_full() {
    run_id="$1"
    output_path="$OUT_DIR/norec-full-${run_id}"
    time_path="$OUT_DIR/norec-full-${run_id}.time"
    bench_path="$OUT_DIR/norec-full-${run_id}.bench"

    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$SELFHOST_BIN" "$ROOT_DIR/compiler/main.nore" --bench -o "$output_path" >/dev/null 2>"$bench_path"

    print_row \
        "norec" \
        "compiler/main.nore" \
        "full" \
        "$run_id" \
        "$(read_time_metric real "$time_path")" \
        "$(read_time_metric user "$time_path")" \
        "$(read_time_metric sys "$time_path")" \
        "$(read_bench_metric load_ns "$bench_path")" \
        "$(read_bench_metric check_ns "$bench_path")" \
        "$(read_bench_metric codegen_ns "$bench_path")" \
        "$(read_bench_metric tail_ns "$bench_path")" \
        "$(read_bench_metric total_ns "$bench_path")" \
        "$(read_bench_metric modules "$bench_path")" \
        "$(read_bench_metric tokens "$bench_path")" \
        "$(read_bench_metric nodes "$bench_path")" \
        "$(read_bench_metric symbols "$bench_path")" \
        "$(read_bench_metric types "$bench_path")" \
        "$(read_bench_metric c_bytes "$bench_path")" \
        "-" \
        >> "$RAW_METRICS"

    rm -f "$output_path" "$time_path" "$bench_path"
}

measure_stage0_full() {
    run_id="$1"
    output_path="$OUT_DIR/stage0-full-${run_id}"
    time_path="$OUT_DIR/stage0-full-${run_id}.time"
    bench_path="$OUT_DIR/stage0-full-${run_id}.bench"

    rm -f "$output_path" "$time_path" "$bench_path"
    "$TIME_BIN" -p -o "$time_path" "$STAGE0_BIN" "$ROOT_DIR/compiler/main.nore" --bench -o "$output_path" >/dev/null 2>"$bench_path"

    print_row \
        "stage0" \
        "compiler/main.nore" \
        "full" \
        "$run_id" \
        "$(read_time_metric real "$time_path")" \
        "$(read_time_metric user "$time_path")" \
        "$(read_time_metric sys "$time_path")" \
        "$(read_bench_metric load_ns "$bench_path")" \
        "$(read_bench_metric check_ns "$bench_path")" \
        "$(read_bench_metric codegen_ns "$bench_path")" \
        "$(read_bench_metric tail_ns "$bench_path")" \
        "$(read_bench_metric total_ns "$bench_path")" \
        "-" "-" "-" "-" "-" "-" "-" \
        >> "$RAW_METRICS"

    rm -f "$output_path" "$time_path" "$bench_path"
}

measure_clang_c() {
    compiler_label="$1"
    run_id="$2"
    source_path="$ROOT_DIR/compiler/main.nore"
    c_path="$OUT_DIR/${compiler_label}-clang-c-${run_id}.c"
    bin_path="$OUT_DIR/${compiler_label}-clang-c-${run_id}"
    time_path="$OUT_DIR/${compiler_label}-clang-c-${run_id}.time"

    rm -f "$c_path" "$bin_path" "$time_path"
    if [ "$compiler_label" = "norec" ]; then
        emit_selfhost_c "$source_path" "$c_path"
    else
        emit_stage0_c "$source_path" "$c_path"
    fi
    "$TIME_BIN" -p -o "$time_path" "$CC_BIN" -std=c99 -O2 -fwrapv "$c_path" -o "$bin_path" >/dev/null 2>&1

    print_row \
        "$compiler_label" \
        "compiler/main.nore" \
        "clang-c" \
        "$run_id" \
        "$(read_time_metric real "$time_path")" \
        "$(read_time_metric user "$time_path")" \
        "$(read_time_metric sys "$time_path")" \
        "-" "-" "-" "-" "-" "-" "-" "-" "-" "-" \
        "$(wc -c < "$c_path" | tr -d ' ')" \
        "$(read_line_count "$c_path")" \
        >> "$RAW_METRICS"

    rm -f "$c_path" "$bin_path" "$time_path"
}

print_average_rows() {
    awk -F '\t' '
        function add_num(group, col, value) {
            if (value != "-") {
                sum[group, col] += value + 0
                count[group, col] += 1
            }
        }
        NR == 1 {
            next
        }
        {
            group = $1 FS $2 FS $3
            if (!(group in seen_order)) {
                order[++order_count] = group
                seen_order[group] = 1
            }
            for (col = 5; col <= 19; col++) {
                add_num(group, col, $col)
            }
        }
        END {
            for (i = 1; i <= order_count; i++) {
                group = order[i]
                split(group, parts, FS)
                printf "%s\t%s\t%s\tavg", parts[1], parts[2], parts[3]
                for (col = 5; col <= 19; col++) {
                    if (count[group, col] == 0) {
                        printf "\t-"
                    } else if (col <= 7) {
                        printf "\t%.3f", sum[group, col] / count[group, col]
                    } else {
                        printf "\t%.0f", sum[group, col] / count[group, col]
                    }
                }
                printf "\n"
            }
        }
    ' "$RAW_METRICS"
}

require_tooling
mkdir -p "$OUT_DIR"

print_row \
    "compiler" \
    "source" \
    "mode" \
    "run" \
    "real" \
    "user" \
    "sys" \
    "load_ns" \
    "check_ns" \
    "codegen_ns" \
    "tail_ns" \
    "total_ns" \
    "modules" \
    "tokens" \
    "nodes" \
    "symbols" \
    "types" \
    "c_bytes" \
    "c_lines" \
    > "$RAW_METRICS"

for source_rel in "${MATRIX_SOURCES[@]}"; do
    warmup_stage0_emit_c "$source_rel"
    warmup_selfhost_emit_c "$source_rel"
done
warmup_full_compile "stage0-full" "$STAGE0_BIN"
warmup_full_compile "norec-full" "$SELFHOST_BIN"
warmup_clang_c "stage0"
warmup_clang_c "norec"

run_id=1
while [ "$run_id" -le "$RUNS" ]; do
    for source_rel in "${MATRIX_SOURCES[@]}"; do
        measure_stage0_emit_c "$source_rel" "$run_id"
        measure_selfhost_emit_c "$source_rel" "$run_id"
    done
    measure_clang_c "stage0" "$run_id"
    measure_clang_c "norec" "$run_id"
    measure_stage0_full "$run_id"
    measure_selfhost_full "$run_id"
    run_id=$((run_id + 1))
done

echo "Benchmark output dir: $OUT_DIR"
echo "Runs per measurement: $RUNS"
echo ""
echo "Per-run metrics:"
render_table < "$RAW_METRICS"
echo ""
echo "Average metrics:"
{
    print_row \
        "compiler" \
        "source" \
        "mode" \
        "run" \
        "real" \
        "user" \
        "sys" \
        "load_ns" \
        "check_ns" \
        "codegen_ns" \
        "tail_ns" \
        "total_ns" \
        "modules" \
        "tokens" \
        "nodes" \
        "symbols" \
        "types" \
        "c_bytes" \
        "c_lines"
    print_average_rows
} | render_table
