# Compiler Tests

This tree holds compiler-specific coverage for the Nore-written compiler.

Current groups:

- `support/`: low-level compiler support coverage
- `lexer/`: tokenization and lexer behavior
- `parser/`: parser behavior and AST or node-table shape
- `imports/`: module loading and import resolution
- `sema/`: semantic checks and diagnostics
- `codegen/`: generated-C regressions and codegen fixtures
- `integration/`: end-to-end rebuild, self-compile, diagnostics, and driver coverage

Naming convention:

- runnable compiler test entrypoints end with `_test.nore`
- fixtures, golden inputs, and imported support modules do not use the `_test.nore` suffix

Run these tests with:

```bash
make test-compiler
make test-compiler-stage0
```

The default path goes through `./norec`. The `-stage0` variant forces the explicit trusted-seed fallback compiler.
