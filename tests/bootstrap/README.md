# Bootstrap Tests

This tree is reserved for the Nore-written compiler as it moves through the bootstrap milestones.

Current status:

- the stage-0 C compiler remains the default compiler
- bootstrap source layout is committed under `compiler/`
- support-library smoke tests now cover spans, typed buffers, path helpers, line maps, source loading, and diagnostic formatting
- lexer bootstrap tests now cover golden dumps, one real compiler module, and lexer diagnostics
- parser bootstrap tests now cover a stable AST golden dump, targeted parser regressions, and one real compiler support module
- bootstrap-specific tests continue to land here as each later milestone becomes executable

Planned subdirectories:

- `support/`
- `lexer/`
- `parser/`
- `imports/`
- `sema/`
- `codegen/`
- `selfhost/`

These tests are intentionally not wired into `make test` yet. A dedicated bootstrap runner belongs to the later end-to-end milestones.
