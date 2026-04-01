# Compiler Tests

This tree holds compiler-specific coverage for the Nore-written compiler.

Current status:

- the stage-0 C compiler remains the default compiler
- the committed source layout lives under `compiler/`
- support, lexer, parser, imports, sema, and codegen tests cover the committed bootstrap compiler slices
- selfhost tests cover smoke compilation, self-compile stability, and the transitional wrapper path
- the trusted C seed and rebuild-from-seed flow now live under `bootstrap/`

Planned subdirectories:

- `support/`
- `lexer/`
- `parser/`
- `imports/`
- `sema/`
- `codegen/`
- `selfhost/`

Naming convention:

- runnable compiler test entrypoints end with `_test.nore`
- fixtures, golden inputs, and imported support modules do not use the `_test.nore` suffix

Run these tests with `make test-compiler` or `tests/run_compiler_tests.sh`.
