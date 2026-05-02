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
./norec --strip-asserts program.nore
./norec --emit-c program.nore build/program.c
```

Maintainer and fallback paths:

```bash
make stage0
./norec-stage0 program.nore
./bootstrap/bootstrap.sh
make norec-stripped
```

`./norec` is the normal compiler interface. `./norec-stage0` and `bootstrap/bootstrap.sh` remain explicit trusted-seed and rebuild paths. `make norec-stripped` builds a separate `./norec-stripped` binary with effect-free asserts omitted; it does not replace the default assert-enabled compiler.

## Emit-C Mode

`--emit-c` runs the normal frontend, semantic analysis, and C lowering pipeline, writes the generated C file you asked for, and stops before invoking Clang.

```bash
./norec --emit-c program.nore build/program.c
./norec --emit-c program.nore build/program.c /path/to/compiler-root
```

The optional third argument is `compiler_root`. It is only used when resolving `std/...` imports, which are loaded relative to the compiler's own root directory. Ordinary relative imports are still resolved from the importing source file.

When omitted, `compiler_root` defaults to the directory that contains the running `norec` executable. In normal use you should not need to pass it manually. It is mainly useful for bootstrap, installed-tool, and debugging workflows where you want to override which `std/` tree the compiler uses.

## Testing

The recommended workflow targets are:

```bash
make qa-local     # normal local loop: test + test-compiler-fast
make qa-ci        # self-hosted pre-merge gate: test + test-compiler-core
make qa-bootstrap # stage-0 confidence: test-stage0 + test-compiler-bootstrap
make qa-full      # combined gate: qa-ci + qa-bootstrap + test-examples
```

The stage-0 boundary is explicit: the `*-stage0` language targets run through `./norec-stage0`, and `test-compiler-bootstrap` starts from the trusted seed before exercising bootstrap-built compilers. The normal language, example, compiler, and self-hosted QA targets run through `./norec`.

Language and stdlib suite targets:

```bash
make test
make test-errors
make test-success
make test-std
make test-stage0
make test-errors-stage0
make test-success-stage0
make test-std-stage0
```

Compiler and example suite targets:

```bash
make test-compiler-fast
make test-compiler-core
make test-compiler-bootstrap
make test-compiler
make test-compiler-all
make test-examples
```

Compiler-specific suite targets now split by intent:

- `test-compiler`: broad self-hosted compiler suite
- `test-compiler-fast`: cheap daily loop for parser, imports, kept sema internals, kept backend invariants, and driver coverage
- `test-compiler-core`: the kept compiler-core suite used by the normal self-hosted QA gate
- `test-compiler-bootstrap`: stage-0 bootstrap trust checks, including driver, smoke, self-compile, and stripped self-hosting
- `test-compiler-all`: explicit alias for the broad self-hosted compiler suite
- `test-examples`: example-program behavior, kept separate from compiler integration

Workflow targets group the normal commands:

- `qa-local`: `test` plus `test-compiler-fast`
- `qa-ci`: `test` plus `test-compiler-core`
- `qa-bootstrap`: `test-stage0` plus `test-compiler-bootstrap`
- `qa-full`: `qa-ci` plus `qa-bootstrap` plus `test-examples`

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
