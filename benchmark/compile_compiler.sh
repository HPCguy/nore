#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
STAGE0_BIN="${STAGE0_BIN:-$ROOT_DIR/norec-stage0}"
SELFHOST_BIN="${NOREC_BIN:-$ROOT_DIR/norec}"
SOURCE_PATH="${BENCH_SOURCE:-$ROOT_DIR/compiler/main.nore}"
RUNS="${RUNS:-3}"
OUT_DIR="$ROOT_DIR/tmp/benchmarks/compiler"
TIME_BIN="/usr/bin/time"
STAGE0_METRICS="$OUT_DIR/norec-stage0.metrics"
SELFHOST_METRICS="$OUT_DIR/norec.metrics"

# Keep benchmark setup failures explicit so timing output only reflects real compile work.
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
    if [ ! -f "$SOURCE_PATH" ]; then
        echo "missing benchmark source: $SOURCE_PATH" >&2
        exit 1
    fi
}

# Read one POSIX time metric from the sidecar output file.
read_metric() {
    metric_name="$1"
    time_path="$2"
    awk -v name="$metric_name" '$1 == name { print $2 }' "$time_path"
}

# Average one metric column from the per-run metric log.
average_metric() {
    column="$1"
    metrics_path="$2"
    awk -v column="$column" '
        { sum += $column }
        END {
            if (NR == 0) {
                printf "0.000"
            } else {
                printf "%.3f", sum / NR
            }
        }
    ' "$metrics_path"
}

# One warm-up compile reduces first-run filesystem noise without affecting the reported numbers.
warmup_compile() {
    label="$1"
    compiler_bin="$2"
    warmup_output="$OUT_DIR/${label}-warmup"
    warmup_time="$OUT_DIR/${label}-warmup.time"
    rm -f "$warmup_output" "$warmup_time"
    "$TIME_BIN" -p -o "$warmup_time" "$compiler_bin" "$SOURCE_PATH" -o "$warmup_output" >/dev/null 2>&1
    rm -f "$warmup_output" "$warmup_time"
}

# Measure one end-to-end compiler build, including generated-C compilation through the driver.
measure_compile() {
    label="$1"
    compiler_bin="$2"
    run_id="$3"
    metrics_path="$4"
    output_path="$OUT_DIR/${label}-${run_id}"
    time_path="$OUT_DIR/${label}-${run_id}.time"

    rm -f "$output_path" "$time_path"
    "$TIME_BIN" -p -o "$time_path" "$compiler_bin" "$SOURCE_PATH" -o "$output_path" >/dev/null 2>&1

    real_time="$(read_metric real "$time_path")"
    user_time="$(read_metric user "$time_path")"
    sys_time="$(read_metric sys "$time_path")"

    printf "%-8s %3s %8s %8s %8s\n" "$label" "$run_id" "$real_time" "$user_time" "$sys_time"
    printf "%s %s %s\n" "$real_time" "$user_time" "$sys_time" >> "$metrics_path"

    rm -f "$output_path" "$time_path"
}

require_tooling
mkdir -p "$OUT_DIR"
rm -f "$STAGE0_METRICS" "$SELFHOST_METRICS"

warmup_compile "stage0" "$STAGE0_BIN"
warmup_compile "norec" "$SELFHOST_BIN"

echo "Benchmark target: $SOURCE_PATH"
echo "Runs per compiler: $RUNS"
echo "Timing tool: $TIME_BIN -p"
echo ""
printf "%-8s %3s %8s %8s %8s\n" "compiler" "run" "real" "user" "sys"

run_id=1
while [ "$run_id" -le "$RUNS" ]; do
    measure_compile "stage0" "$STAGE0_BIN" "$run_id" "$STAGE0_METRICS"
    measure_compile "norec" "$SELFHOST_BIN" "$run_id" "$SELFHOST_METRICS"
    run_id=$((run_id + 1))
done

stage0_real="$(average_metric 1 "$STAGE0_METRICS")"
stage0_user="$(average_metric 2 "$STAGE0_METRICS")"
stage0_sys="$(average_metric 3 "$STAGE0_METRICS")"
selfhost_real="$(average_metric 1 "$SELFHOST_METRICS")"
selfhost_user="$(average_metric 2 "$SELFHOST_METRICS")"
selfhost_sys="$(average_metric 3 "$SELFHOST_METRICS")"

speedup="$(awk -v stage0="$stage0_real" -v selfhost="$selfhost_real" 'BEGIN {
    if (selfhost == 0) {
        printf "0.000"
    } else {
        printf "%.3f", stage0 / selfhost
    }
}')"

delta="$(awk -v stage0="$stage0_real" -v selfhost="$selfhost_real" 'BEGIN {
    if (stage0 == 0) {
        printf "0.0"
    } else {
        printf "%.1f", ((selfhost - stage0) / stage0) * 100.0
    }
}')"

echo ""
printf "%-8s %3s %8s %8s %8s\n" "compiler" "avg" "real" "user" "sys"
printf "%-8s %3s %8s %8s %8s\n" "stage0" "avg" "$stage0_real" "$stage0_user" "$stage0_sys"
printf "%-8s %3s %8s %8s %8s\n" "norec" "avg" "$selfhost_real" "$selfhost_user" "$selfhost_sys"
echo ""
echo "Real-time speedup (stage0 / norec): ${speedup}x"
echo "Real-time delta vs stage0: ${delta}%"
