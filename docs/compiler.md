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
./norec --help
./norec --version
./norec program.nore
./norec --run program.nore
./norec --lexer program.nore
./norec --parser program.nore
./norec --codegen program.nore
./norec --emit-c program.nore build/program.c
```

Maintainer and fallback paths:

```bash
make stage0
./norec-stage0 program.nore
./bootstrap/bootstrap.sh
```

`./norec` is the normal compiler interface. `./norec-stage0` and `bootstrap/bootstrap.sh` remain explicit trusted-seed and rebuild paths.

## Emit-C Mode

`--emit-c` runs the normal frontend, semantic analysis, and C lowering pipeline, writes the generated C file you asked for, and stops before invoking Clang.

```bash
./norec --emit-c program.nore build/program.c
./norec --emit-c program.nore build/program.c /path/to/compiler-root
```

The optional third argument is `compiler_root`. It is only used when resolving `std/...` imports, which are loaded relative to the compiler's own root directory. Ordinary relative imports are still resolved from the importing source file.

When omitted, `compiler_root` defaults to the directory that contains the running `norec` executable. In normal use you should not need to pass it manually. It is mainly useful for bootstrap, installed-tool, and debugging workflows where you want to override which `std/` tree the compiler uses.

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

For the compiler-only matrix, phase timings, and optimization log, see [compiler-performance.md](compiler-performance.md).

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
- [compiler-performance.md](compiler-performance.md)
- [../compiler/README.md](../compiler/README.md)
