# Compiler Performance Notes

This note records the current stopping point of compiler-performance work in Nore.

It focuses on measured results that were kept, the current comparison between stage-0 and the self-hosted compiler, and the conclusions that are stable enough to carry forward.

It is not a commitment to future language features.

## Metric Hierarchy

The primary optimization metric is the compiler-only path:

- `RUNS=5 ./benchmark/compiler_matrix.sh`
- compare `stage0 emit-c` vs `norec emit-c`

This is the best current measure of compiler work because it excludes the Clang tail.

The supporting metrics are:

- self-hosted internal phase timings: `load_ns`, `check_ns`, `codegen_ns`, `tail_ns`
- `clang-c` rows for emitted-C compile cost
- `full` rows for end-to-end user-facing compile time

So the intended reading order is:

1. `emit-c` for compiler work
2. self-hosted phase timings for diagnosis
3. `clang-c` and `full` for release-facing context

## Current Stable Results

All numbers below come from a clean `RUNS=5 ./benchmark/compiler_matrix.sh` on the current tree.

### `compiler/main.nore`

| Metric | `stage0` | `norec` | Reading |
| --- | ---: | ---: | --- |
| `emit-c` real | `0.020s` | `0.020s` | Rounded wall-clock parity on the root workload |
| `clang-c` real | `1.892s` | `1.662s` | Self-hosted emitted C compiles faster in Clang |
| `full` real | `1.922s` | `1.706s` | Self-hosted is faster end-to-end |
| emitted C bytes | `1242190` | `991595` | Self-hosted emits about `20.2%` fewer bytes |
| emitted C lines | `13339` | `12357` | Self-hosted emits about `7.4%` fewer lines |

Important caveat:

- `/usr/bin/time -p` rounds aggressively, so `0.020s` vs `0.020s` should be read as near-parity, not proof that the two compilers are exactly equal in compiler-only work.

For the same `compiler/main.nore` workload, the self-hosted compiler-only phase split is:

- `load_ns`: `7.977 ms`
- `check_ns`: `5.898 ms`
- `codegen_ns`: `4.214 ms`
- `tail_ns` on `emit-c`: `0.345 ms`

That puts the measured self-hosted compiler pipeline at about:

- `18.09 ms` before the small `emit-c` tail

For the self-hosted `full` path on the same workload:

- `tail_ns`: about `1.681 s`

So the current end-to-end result is still dominated by Clang work on emitted C, even though the compiler-only metric remains the right target for internal optimization decisions.

### Other Large Compiler Sources

The larger single-file `emit-c` rows still show stage-0 ahead at rounded timer resolution:

- `compiler/frontend/parser.nore`: `stage0 0.000s`, `norec 0.010s`
- `compiler/sema/check.nore`: `stage0 0.010s`, `norec 0.020s`
- `compiler/codegen/c_main.nore`: `stage0 0.010s`, `norec 0.020s`

These rows are also rounded, but they reinforce the same reading:

- self-hosted has reached rounded parity on the root `compiler/main.nore` compiler-only path
- stage-0 is still the compiler-only baseline to beat overall

## What Was Achieved

### Starting Point: `ccb81f8` Add compiler-only benchmark matrix

This established the benchmark structure and the first stable baseline:

- `stage0 compiler/main.nore emit-c`: `0.020s`
- `norec compiler/main.nore emit-c`: `0.030s`
- `norec load_ns`: `8.10 ms`
- `norec check_ns`: `8.04 ms`
- `norec codegen_ns`: `4.48 ms`

It also clarified the metric hierarchy:

- `emit-c` is the primary compiler-performance metric
- `full` is the user-facing release metric

### `a5541cc` Cache common type lookups

What changed:

- removed the unnecessary `type_named()` reverse scan
- cached `slice-of(type)` and `table-row-of(table)` relations

What it did:

- produced a modest win
- cleaned up repeated linear scans in a narrow part of sema/codegen

### `909282c` Specialize sema scope lookups

What changed:

- kept the existing full per-scope symbol chain
- added narrower namespace-specific chains
- retargeted hot sema lookups to those chains

This was the first major win.

### `ac8d8bb` Refine sema lookup chains

What changed:

- narrowed runtime-value lookup further
- kept module-alias lookup off the runtime-value path

This was a smaller follow-up win than `909282c`, but it kept the sema direction consistent.

### Net Effect Of The Kept Work

Comparing the initial baseline to the current `RUNS=5` result for `compiler/main.nore`:

- `norec emit-c`: rounded `0.030s` -> rounded `0.020s`
- `load_ns`: `8.10 ms` -> `7.98 ms`
- `check_ns`: `8.04 ms` -> `5.90 ms`
- `codegen_ns`: `4.48 ms` -> `4.21 ms`

If you look only at the measured self-hosted internal pipeline before the `emit-c` tail:

- `load + check + codegen`: `20.62 ms` -> `18.09 ms`

That is about a `12.3%` reduction in the measured self-hosted compiler pipeline on the root workload.

Most of the win came from semantic lookup specialization, not from frontend or codegen changes.

## What Did Not Earn A Permanent Change

Later experiments that were tried and then reverted include:

- deeper parser-family instrumentation beyond the coarse permanent timings
- parser micro-optimizations that did not move the matrix enough
- arena bookkeeping experiments
- field-lookup cache experiments for record-like types

The right reading is:

- the current benchmark work answered useful questions
- but not every measured hotspot deserved a lasting optimization or extra instrumentation layer

## Current Conclusion

At this stopping point, the stable conclusions are:

- for pure compiler work, stage-0 is still the baseline to beat
- on `compiler/main.nore`, self-hosted has reached rounded `emit-c` parity, but not a clear compiler-only win
- for end-to-end builds, self-hosted is already better because it emits smaller C that Clang compiles faster
- inside the self-hosted compiler, sema remains the largest internal bucket, but the easiest lookup-only wins have mostly been harvested

So the current state is:

- near-parity on the root compiler-only benchmark
- clear self-hosted win on end-to-end builds
- no strong reason to keep drilling deeper without a fresh hotspot signal

## Data-Layout Takeaway

The successful optimizations so far validate a narrow implementation pattern:

- keep one flat source-of-truth table
- add cheap derived access paths for the queries that matter

In practice, that has meant:

- cached one-to-one type relations
- namespace-specific symbol chains over one base symbol table

This is useful evidence for compiler-internal data-layout work.

It is not yet evidence for:

- first-class `IndexSet` language features
- view syntax
- compiler directives justified only by elegance
