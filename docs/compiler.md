# The Nore Compiler

Nore's primary compiler is the self-hosted implementation in `compiler/`, built as `./norec`. It compiles Nore source to C and relies on Clang for native code generation. The trusted C seed in `bootstrap/` remains the rebuild and verification path for the self-hosted compiler.

## Current Overview

- `./norec` is the default compiler path used by `make` and the normal test flow
- `compiler/` is the canonical self-hosted compiler source tree
- `bootstrap/norec-stage0.c` is the trusted stage-0 seed
- `bootstrap/bootstrap.sh` rebuilds `./norec` from the trusted seed
- C remains the current backend IR

## Compiler Components

- `compiler/` contains the self-hosted compiler implementation
- `bootstrap/` contains the trusted seed and rebuild-from-seed tooling
- `std/` contains standard library modules used by programs and compiler tests
- `tests/compiler/` contains compiler-specific regression and integration coverage

## Build And Rebuild Paths

Normal compiler path:

```bash
make
./norec program.nore
./norec --run program.nore
./norec --lexer program.nore
./norec --parser program.nore
./norec --codegen program.nore
```

Maintainer and fallback paths:

```bash
make stage0
./norec-stage0 program.nore
./bootstrap/bootstrap.sh
```

`./norec` is the normal compiler interface. `./norec-stage0` and `bootstrap/bootstrap.sh` remain explicit trusted-seed and rebuild paths.

## Testing

```bash
make test
make test-errors
make test-success
make test-std
make test-compiler

make test-stage0
make test-errors-stage0
make test-success-stage0
make test-std-stage0
make test-compiler-stage0
```

The default targets run through `./norec`. The `*-stage0` targets keep the trusted-seed fallback path available for comparison and verification.

## Benchmarking

A simple compiler benchmark is available for comparing the stage-0 seed and the self-hosted compiler on the same workload.

```bash
make bench-compiler
RUNS=5 make bench-compiler
```

The benchmark measures end-to-end compile time for building `compiler/main.nore` with `./norec-stage0` and `./norec`.

## Source Tree

- `compiler/main.nore`: top-level compiler orchestration
- `compiler/support/`: low-level buffers, spans, paths, sources, diagnostics, and line mapping
- `compiler/frontend/`: tokens, lexer, parser, and module loading
- `compiler/sema/`: symbols, scopes, types, and semantic checks
- `compiler/codegen/`: C lowering and C emission
- `compiler/driver/`: CLI and driver-specific logic

For deeper module ownership and dependency direction, see [compiler-architecture.md](compiler-architecture.md).

## Related Documents

- [compiler-architecture.md](compiler-architecture.md)
- [../compiler/README.md](../compiler/README.md)
