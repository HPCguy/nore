# Compiler Performance Notes

This note tracks compiler-performance work that has already happened, the metrics used to judge it, and a few future data-layout directions that may support similar wins later.

It is intentionally not a commitment to language features. It is a record of what helped the current compiler and what patterns may be worth revisiting.

## Metric Hierarchy

The primary optimization metric is the compiler-only path:

- `RUNS=5 ./benchmark/compiler_matrix.sh`
- compare `stage0 emit-c` vs `norec emit-c`

This is the best current measure of the compiler itself, because it excludes the Clang tail.

The supporting metrics are:

- self-hosted internal phase timings: `load_ns`, `check_ns`, `codegen_ns`, `tail_ns`
- `clang-c` rows for emitted-C compile cost
- `full` rows for end-to-end user-facing compile time

So the intended reading order is:

1. `emit-c` for compiler work
2. phase timings for diagnosis
3. `clang-c` and `full` for release-facing context

## Current Picture

After the latest sema-lookup optimization pass, the clean `RUNS=5` matrix shows:

- `stage0 compiler/main.nore emit-c`: `0.020s` average real
- `norec compiler/main.nore emit-c`: `0.020s` average real
- `norec compiler/main.nore load_ns`: `7.68 ms`
- `norec compiler/main.nore check_ns`: `5.55 ms`
- `norec compiler/main.nore codegen_ns`: `4.18 ms`
- `norec compiler/main.nore full`: `1.602s` average real

Important caveat:

- `/usr/bin/time -p` rounds aggressively, so `0.020s` vs `0.020s` means rounded wall-clock parity, not proof that the two compilers are exactly equal underneath timer precision.

The end-to-end path is still dominated by Clang work on emitted C. The compiler-only benchmark remains the right metric for self-hosted optimization decisions.

## Optimization Log

### `ccb81f8` Add compiler-only benchmark matrix

What changed:

- added `benchmark/compiler_matrix.sh`
- added self-hosted benchmark instrumentation
- added real `--emit-c` support to `norec-stage0`

What it established:

- the right primary metric is `emit-c`, not `full`
- initial compiler-only baseline on `compiler/main.nore` was roughly:
  - `norec emit-c`: `0.030s`
  - `stage0 emit-c`: `0.020s`
  - `load_ns`: `8.10 ms`
  - `check_ns`: `8.04 ms`
  - `codegen_ns`: `4.48 ms`

### `a5541cc` Cache common type lookups

What changed:

- removed the unnecessary `type_named()` reverse scan
- cached `slice-of(type)` and `table-row-of(table)` relations

What it did:

- helped a little
- did not materially move the rounded `emit-c` headline

Measured effect on `compiler/main.nore`:

- `load_ns`: `8.10 ms` -> `8.09 ms`
- `check_ns`: `8.04 ms` -> `7.91 ms`
- `codegen_ns`: `4.48 ms` -> `4.33 ms`

This was a good cleanup, but not the first major lever.

### `909282c` Specialize sema scope lookups

What changed:

- kept the existing full per-scope symbol chain
- added per-namespace chains for:
  - values
  - functions
  - types
- retargeted hot sema lookup helpers to the narrower chains

Why it helped:

- lookups no longer walk mixed symbol lists and then filter by kind
- the biggest visible win was in semantic analysis

Measured effect on `compiler/main.nore`:

- `check_ns`: `7.91 ms` -> `5.55 ms`
- `load_ns`: `8.09 ms` -> `7.68 ms`
- `codegen_ns`: `4.33 ms` -> `4.18 ms`

If you look only at the self-hosted internal pipeline before the emit tail:

- `load + check + codegen`: `20.33 ms` -> `17.41 ms`

That is roughly a `14.3%` reduction in the measured self-hosted compiler pipeline, with the biggest component coming from sema lookup specialization.

## Pattern Emerging So Far

The successful optimizations have not pointed to a new language feature yet. They have pointed to a recurring implementation pattern:

- keep one flat source-of-truth table
- add derived access paths for the queries that matter
- make those access paths cheap to maintain and cheap to validate

In concrete terms, that has meant:

- cached derived type relations
- namespace-specific symbol chains over one base symbol table

This is close in spirit to “multiple views over one base dataset”, but still much narrower and more implementation-specific than a general IndexSet/View abstraction.

## Data-Layout Directions Worth Watching

These are not commitments. They are ideas that fit the optimization pattern seen so far and may deserve future experiments.

### Secondary Indexes Over Flat Tables

Examples:

- per-scope namespace indexes
- source-id or path indexes in the loader
- declaration-name indexes inside module scopes

This is the most directly validated direction so far.

### Cached One-To-One Derived Relations

Examples:

- `type -> slice(type)`
- `table -> table.Row`
- future `symbol -> canonical lowered helper id`

This pattern already paid off and is easy to reason about when the relation really is stable and unique.

### Dense Subset Or View Projections

If repeated traversals start selecting the same filtered subsets of rows, a denser “view” representation may become worthwhile.

Examples:

- all type symbols in a scope
- all functions in a module
- all deferred checks for one category

This is conceptually closer to the View/IndexSet discussion, but it is still only a compiler-internal design direction for now.

### Name-Key Interning For Lookup

A likely next step is reducing repeated token-text comparisons in sema lookups.

That could look like:

- interned identifier keys
- symbol lookup indexed by key instead of repeated slice equality

If the current namespace-chain work still leaves lookup as the dominant sema cost, this is a strong candidate.

### Span-Or-Bucket Scope Layouts

If append-once / query-many behavior dominates, a future experiment could replace linked namespace chains with denser per-scope spans or grouped buckets.

That would be a larger structural step than the current work, so it should only happen if the benchmark keeps pointing in the same direction.

## What This Does Not Prove

These optimizations do not yet justify:

- first-class IndexSet language features
- new syntax for views
- compiler directives motivated only by elegance

What they do justify is continued work on compiler-internal derived indexes and projections.

## Next Questions

The next useful performance questions are:

1. Does sema still have a meaningful name-lookup bottleneck after namespace-chain specialization?
2. Is identifier/name interning the next best move?
3. Are there other repeated filtered traversals that would benefit from a denser derived view?

Until those answers are clear, this note should stay descriptive, not prescriptive.
