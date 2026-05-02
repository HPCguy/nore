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
- [docs/nore.md](docs/nore.md) — language guide (philosophy, type model, memory model, syntax, safety)

### Code Contributions
Before writing code, open an issue to discuss what you want to change. This avoids wasted effort on things that don't fit the project direction.

When submitting code:
- **Read the existing code first.** The primary compiler source tree lives under `compiler/`, with the trusted stage-0 seed in `bootstrap/`. Match the style of the part you are changing.
- **C99, no extensions.** Keep it portable.
- **Keep it simple.** No clever tricks. If you need to explain it, simplify it.
- **Include tests.** Every new feature needs a success test in `tests/success/`. Every new error code needs an error test in `tests/errors/`.
- **Run `make qa-local` before submitting.** Run `make qa-ci` for compiler-internal or CI-facing changes, and `make qa-bootstrap` for stage-0, bootstrap, or trusted-seed changes.
- **Small changes over big ones.** A focused PR that does one thing well is easier to review than a large one that touches everything.

### What Doesn't Help Right Now
- Unsolicited refactoring or "improvements" to working code
- Adding features not discussed in an issue first
- Style changes or formatting-only PRs

## Building and Testing

```bash
make              # Build the compiler
make norec-stripped # Build ./norec-stripped with asserts stripped
make test         # Run language suites through ./norec
make test-errors  # Run error code tests only
make test-success # Run success tests only
make qa-local     # Normal local language + compiler-maintainer loop
make qa-ci        # Stronger self-hosted pre-merge gate
make qa-bootstrap # Stage-0 language + trusted-seed bootstrap gate
make qa-full      # Combined self-hosted + bootstrap + examples gate
```

## Maintainer Workflows

The normal compiler entrypoint is `./norec`. The explicit rebuild and trusted-seed paths are:

```bash
make stage0
./norec-stage0 program.nore
./bootstrap/bootstrap.sh
make norec-stripped
```

Compiler-specific verification paths:

```bash
make test-stage0
make test-compiler-fast
make test-compiler-core
make test-compiler-bootstrap
make test-compiler
make test-examples
```

`test-compiler` remains the broad self-hosted compiler gate. For stage-0 confidence, use `qa-bootstrap`; it groups `test-stage0` and `test-compiler-bootstrap`, including the stripped self-hosting check. All other compiler and QA targets run through `./norec`.

## Branch Policy

Nore currently uses a simple branch model:

- `main` is the default integration branch. It should stay close to the next stable release line.
- `release/x.y.x` branches are cut for stabilization, version bumps, release-only fixes, and post-release hotfixes for that line.
- feature branches should be short-lived and merged back into `main` promptly.
- release tags such as `v0.1.1` should be created from the corresponding release branch tip.

There is intentionally no long-lived `develop` branch right now. At the current project size it adds merge and backport overhead without enough payoff. If release work happens on a `release/x.y.x` branch, merge it back into `main` promptly so the two branches do not drift for long.

## License

By contributing, you agree that your contributions will be licensed under the same terms as the project: BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
