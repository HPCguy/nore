# Nore Compiler Architecture

This document describes the current architecture of the Nore self-hosted compiler in `compiler/`. It defines the main pipeline, module ownership boundaries, and dependency direction used by the current implementation.

Historical bootstrap planning and milestone documents live under [history/](history/README.md).

## Pipeline Overview

1. Source loading and module discovery
2. Lexing into flat token tables
3. Parsing into flat node tables
4. Semantic analysis and type checking
5. C lowering and emission
6. Host C compilation through the driver path

## Core Design Principles

- flat tables and ids over pointer-rich recursive structures
- arena-backed compiler storage
- narrow module ownership boundaries
- late formatting of diagnostics from structured data
- simple dependency direction between phases
- boring, inspectable code over abstraction-heavy frameworks

## Module Ownership

### `compiler/main.nore`

- owns top-level orchestration of the compiler pipeline
- wires phases together and manages long-lived compiler state
- should not absorb lexer, sema, or codegen internals

### `compiler/support/`

- owns shared low-level storage, spans, source storage, line mapping, diagnostics, and path helpers
- provides compiler-wide primitives used by higher phases
- should not absorb parse control flow, semantic policy, or code generation

### `compiler/frontend/`

- owns token tables, lexing, node tables, parsing, and module loading
- turns source files into parsed compiler data structures
- should not absorb type checking or C emission

### `compiler/sema/`

- owns symbols, scopes, internal type rows, name resolution, type checking, and semantic diagnostics
- computes the checked program state consumed by later phases
- should not absorb parsing or host-tool orchestration

### `compiler/codegen/`

- owns lowering checked programs to C and assembling generated C output
- depends on semantic information rather than re-deriving language rules
- should not absorb semantic validation or CLI behavior

### `compiler/driver/`

- owns the CLI-facing compiler entrypoints and host-tool invocation boundary
- turns compiler output into the normal user-facing build and run flow
- should not absorb frontend, sema, or codegen internals

## Dependency Direction

Allowed dependency direction is intentionally simple:

- `main` depends on all compiler phases
- `driver` depends on stable compiler entrypoints and output boundaries
- `codegen` depends on checked sema data
- `sema` depends on frontend data and support helpers
- `frontend` depends on support helpers
- `support` stays at the bottom and should not depend on higher-level phases

Shared primitives should move downward into lower-level modules. Higher-level phases should not reimplement or own data that already belongs to lower layers.

## Compiler Data Model

The compiler follows the same data-oriented bias as the language itself:

- sources are stored in stable compiler-owned tables
- tokens are stored as flat token rows
- syntax is stored as flat node rows with ids and relationships
- semantic state is stored in symbol, scope, and type tables
- diagnostics are collected as structured rows and formatted late

The implementation favors ids, spans, ranges, and side arrays over recursive owned object graphs.

## Driver And Rebuild Boundary

- `./norec` is the normal compiler entrypoint
- `compiler/` is the primary implementation
- `bootstrap/` is the trusted seed and rebuild path
- rebuild-from-seed is operationally important, but outside most compiler module ownership boundaries

## Invariants

- module responsibilities stay narrow
- shared primitives move downward, not upward
- diagnostics are stored structurally and formatted late
- semantic facts are computed in sema, not reconstructed in codegen
- source identity and module identity stay stable across the pipeline

## Related Documents

- [compiler.md](compiler.md)
- [../compiler/README.md](../compiler/README.md)
- [history/compiler-bootstrap-architecture.md](history/compiler-bootstrap-architecture.md)
- [history/self-hosting-bootstrap-plan.md](history/self-hosting-bootstrap-plan.md)
