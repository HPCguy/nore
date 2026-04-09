# Compiler Performance Notes

This note records the current stopping point of compiler-performance work in Nore.

It focuses on measured results that were kept, the current comparison between stage-0 and the self-hosted compiler, and the conclusions that are stable enough to carry forward.

It is not a commitment to future language features.

## Metric Hierarchy

The primary optimization metric is the compiler-only path:

- `RUNS=5 ./benchmark/compiler_matrix.sh`
- compare `stage0 emit-c` vs `norec emit-c`
- read `total_ns` first

This is the best current measure of compiler work because it excludes the Clang tail and uses the same coarse phase boundary in both compilers:

- `load_ns`
- `check_ns`
- `codegen_ns`
- `tail_ns`
- `total_ns = load + check + codegen + tail`

With the current benchmark shape, `codegen_ns` means the same thing in both compilers:

- build generated C in memory

And `tail_ns` means:

- `emit-c`: write the generated C to the output file
- `full`: write the generated C and run Clang

The supporting metrics are:

- phase timings for both compilers: `load_ns`, `check_ns`, `codegen_ns`, `tail_ns`
- `clang-c` rows for emitted-C compile cost
- `full` rows for end-to-end user-facing compile time

So the intended reading order is:

1. `emit-c total_ns` for compiler work
2. coarse phase timings for diagnosis
3. `clang-c` and `full` for release-facing context

## Current Stable Results

All numbers below come from a clean `RUNS=5 ./benchmark/compiler_matrix.sh` on the current tree, after aligning the stage-0 and self-hosted `codegen_ns` boundary.

### `compiler/main.nore`

| Metric | `stage0` | `norec` | Reading |
| --- | ---: | ---: | --- |
| `emit-c` real | `0.020s` | `0.022s` | Rounded wall-clock still looks like near-parity |
| `emit-c total_ns` | `18.868 ms` | `18.674 ms` | Near-parity, with a slight self-hosted edge on this run |
| `emit-c load_ns` | `7.355 ms` | `8.011 ms` | Load is close, with stage-0 still ahead |
| `emit-c check_ns` | `2.096 ms` | `5.642 ms` | Sema remains the main compiler-only gap |
| `emit-c codegen_ns` | `8.835 ms` | `4.317 ms` | Self-hosted is faster in C generation itself |
| `emit-c tail_ns` | `0.582 ms` | `0.704 ms` | Emit tail is small in both compilers |
| `clang-c` real | `1.882s` | `1.646s` | Self-hosted emitted C compiles faster in Clang |
| `full` real | `1.878s` | `1.666s` | Self-hosted is faster end-to-end |
| `full total_ns` | `1880.597 ms` | `1659.572 ms` | End-to-end win is dominated by the Clang tail |
| emitted C bytes | `1240769` | `989674` | Self-hosted emits about `20.2%` fewer bytes |
| emitted C lines | `13335` | `12335` | Self-hosted emits about `7.5%` fewer lines |

The important reading change is:

- for compiler-only comparison, use `emit-c total_ns`
- do not rely on rounded `/usr/bin/time -p` output alone

So the current root-workload picture is:

- compiler-only work is effectively at parity, with a small self-hosted lead in this run
- stage-0 is still much faster in sema
- self-hosted gives that back and more in codegen
- end-to-end self-hosted is clearly faster because its emitted C is smaller and cheaper for Clang to compile

### Working Hypothesis

This is speculative, not a measured conclusion, but the current numbers are consistent with the following explanation:

- stage-0 likely keeps semantic checking cheaper because it works more directly over a pointer-rich AST and mutable scope/function structures
- self-hosted likely pays more during sema because it normalizes more information into flat compiler tables and stable ids
- that extra sema bookkeeping may help self-hosted codegen later, because the lowered C pass can consume precomputed symbol/type/module state more directly

So a plausible reading is:

- stage-0 does less normalization work up front, which helps `check_ns`
- self-hosted spends more to build reusable checked state, which helps `codegen_ns`

This should not be read as a complete explanation of the gap. Some of the self-hosted codegen advantage is also likely due to better output shaping and smaller emitted C, not just the cost transfer from sema.

### Other Large Compiler Sources

On the larger single-file `emit-c total_ns` rows:

- `compiler/frontend/parser.nore`: `stage0 3.710 ms`, `norec 3.827 ms`
- `compiler/sema/check.nore`: `stage0 11.641 ms`, `norec 12.362 ms`
- `compiler/codegen/c_main.nore`: `stage0 10.778 ms`, `norec 10.258 ms`

These rows sharpen the same reading:

- parser-heavy work is close
- sema-heavy work still favors stage-0
- codegen-heavy work already favors self-hosted
- the root `compiler/main.nore` workload lands near overall parity because those effects offset each other

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

- rounded `norec emit-c`: `0.030s` -> `0.022s`
- `load_ns`: `8.10 ms` -> `8.01 ms`
- `check_ns`: `8.04 ms` -> `5.64 ms`
- `codegen_ns`: `4.48 ms` -> `4.32 ms`

The phase buckets that were tracked across the whole pass sum to:

- `load + check + codegen`: `20.62 ms` -> `17.97 ms`

That is about a `12.9%` reduction in the measured self-hosted compiler core on the root workload.

With the current benchmark shape, the self-hosted `emit-c total_ns` is now:

- `18.67 ms`

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

- for pure compiler work, the root `compiler/main.nore` workload is now effectively at parity
- the more precise current reading comes from `emit-c total_ns`, not rounded shell time
- on `compiler/main.nore`, self-hosted is slightly ahead on compiler-only total in the current run
- stage-0 still has the clearer advantage in sema-heavy work
- self-hosted already has the advantage in codegen-heavy work
- for end-to-end builds, self-hosted is already better because it emits smaller C that Clang compiles faster
- inside the self-hosted compiler, sema remains the largest internal bucket, but the easiest lookup-only wins have mostly been harvested

So the current state is:

- near-parity with a slight self-hosted edge on the root compiler-only benchmark
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
