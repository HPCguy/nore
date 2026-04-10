# Compiler Tests

This tree holds compiler-specific coverage for the Nore-written compiler.

Current groups:

- `support/`: low-level compiler support coverage
- `lexer/`: tokenization and lexer behavior
- `parser/`: parser behavior and AST or node-table shape
- `imports/`: module loading and import resolution
- `sema/`: semantic checks and diagnostics
- `codegen/`: generated-C regressions and codegen fixtures
- `integration/`: driver and diagnostic coverage
- `bootstrap/`: trusted-seed smoke and self-compile coverage

Naming convention:

- runnable compiler test entrypoints end with `_test.nore`
- fixtures, golden inputs, and imported support modules do not use the `_test.nore` suffix

Run these tests with:

```bash
make test-compiler
make test-compiler-fast
make test-compiler-core
make test-compiler-bootstrap
make test-compiler-all
```

`test-compiler` keeps the broad self-hosted compiler suite. `test-compiler-bootstrap` is the trusted-seed bootstrap lane; use `make qa-bootstrap` when you want it together with the stage-0 language suite.

Target intent:

- `test-compiler`: broad self-hosted compiler suite
- `test-compiler-fast`: cheap daily loop for support, lexer, parser, imports, kept sema internals, kept backend invariants, and driver coverage
- `test-compiler-core`: the kept compiler-core suite used by the normal self-hosted QA gate
- `test-compiler-bootstrap`: stage-0 bootstrap trust checks, including driver, smoke, and self-compile
- `test-compiler-all`: explicit alias for the broad self-hosted compiler suite

Example-program behavior now lives outside this tree:

```bash
make test-examples
```
