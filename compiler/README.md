# Compiler Source Tree

This directory contains the canonical self-hosted compiler implementation.

See [../docs/compiler.md](../docs/compiler.md) for the current compiler overview and [../docs/compiler-architecture.md](../docs/compiler-architecture.md) for module ownership and dependency direction.

## Layout

- `main.nore`: top-level orchestration of the compiler pipeline
- `support/`: shared low-level buffers, spans, paths, source storage, line mapping, and diagnostics
- `frontend/`: tokenization, parsing, and module loading
- `sema/`: symbols, scopes, types, and semantic checking
- `codegen/`: C lowering and emission
- `driver/`: CLI entrypoints and driver-specific logic

## Notes

- keep module responsibilities narrow
- move shared primitives downward into `support/`
- keep semantic policy in `sema/` and C output logic in `codegen/`
