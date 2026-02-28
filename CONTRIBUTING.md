# Contributing to Nore

Nore is in early development with a single author. Contributions are welcome, but the project is at a stage where direction and design coherence matter more than velocity.

## How to Help

### Bug Reports
Found a compiler crash, wrong codegen, or a misleading error message? Open an issue with:
- The Nore source code that triggers the problem
- Expected vs actual behavior
- Compiler output (use `--codegen` or `--parser` flags to narrow it down)

### Design Feedback
Nore makes opinionated choices (arenas over GC, value/struct split, tables as columnar sugar). If you have experience with data-oriented design, systems programming, or compiler implementation, feedback on the design documents is valuable:
- [docs/data-oriented-design.md](docs/data-oriented-design.md) — type model and memory approach
- [docs/arena-safety.md](docs/arena-safety.md) — escape analysis and lifetime tracking

### Code Contributions
Before writing code, open an issue to discuss what you want to change. This avoids wasted effort on things that don't fit the project direction.

When submitting code:
- **Read the existing code first.** The compiler is a single C file with consistent patterns. Match the style.
- **C99, no extensions.** Keep it portable.
- **Keep it simple.** No clever tricks. If you need to explain it, simplify it.
- **Include tests.** Every new feature needs a success test in `tests/success/`. Every new error code needs an error test in `tests/errors/`.
- **Run `make test` before submitting.** All existing tests must still pass.
- **Small changes over big ones.** A focused PR that does one thing well is easier to review than a large one that touches everything.

### What Doesn't Help Right Now
- Unsolicited refactoring or "improvements" to working code
- Adding features not discussed in an issue first
- Style changes or formatting-only PRs

## Building and Testing

```bash
make              # Build the compiler
make test         # Run all tests
make test-errors  # Run error code tests only
make test-success # Run success tests only
```

## License

By contributing, you agree that your contributions will be licensed under the same terms as the project: BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
