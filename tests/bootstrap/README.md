# Bootstrap Tests

This tree is reserved for the Nore-written compiler as it moves through the bootstrap milestones.

Current status:

- the stage-0 C compiler remains the default compiler
- bootstrap source layout is committed under `compiler/`
- support-library smoke tests now cover spans, typed buffers, path helpers, line maps, source loading, and diagnostic formatting
- lexer bootstrap tests now cover golden dumps, one real compiler module, and lexer diagnostics
- parser bootstrap tests now cover a stable AST golden dump, targeted parser regressions, and one real compiler support module
- import-graph bootstrap tests now cover local imports, lexical `.`/`..` path deduplication, diamond deduplication, `std/` resolution, and missing-import diagnostics
- sema bootstrap tests now cover binding, basic subset typing, arena/table built-in typing, arena escape/reset diagnostics, table-column access, `match` arm binding plus duplicate/exhaustiveness diagnostics, shape-declaration field diagnostics, enum-payload validation, compiler-injected names, native-name diagnostics, alias/ref/constant-initializer/string-literal/equality regressions, and one real compiler support module plus one real compiler sema module and four real std modules
- codegen bootstrap tests now cover the first committed Milestone 8 slices: stable C typedef emission for arrays, slices, values, structs, tables, table rows, and enums, plus stable user-function prototype emission with module prefixes, ref lowering, and native-declaration skipping
- bootstrap-specific tests continue to land here as each later milestone becomes executable

Planned subdirectories:

- `support/`
- `lexer/`
- `parser/`
- `imports/`
- `sema/`
- `codegen/`
- `selfhost/`

Naming convention:

- runnable bootstrap test entrypoints end with `_test.nore`
- fixtures, golden inputs, and imported support modules do not use the `_test.nore` suffix

These tests are intentionally not wired into `make test` yet. Run them with `tests/run_bootstrap_tests.sh`.
