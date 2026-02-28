# Nore Programming Language

## Overview

Nore is a systems programming language that makes data-oriented design the path of least resistance. Instead of hiding memory layout behind objects, Nore gives you direct control over how data is organized — columnar tables, arena allocation, explicit value vs resource semantics — with compile-time safety guarantees and zero runtime overhead.

The compiler is a self-contained, single-file C program that translates Nore source code into native executables via C as an intermediate representation.

## A Quick Look

```nore
value Vec2 { x: f64, y: f64 }

// One declaration → columnar storage (struct-of-arrays)
// Generates: Particles (struct with slice columns) and ParticlesRow (value type)
table Particles {
    pos: Vec2,
    life: i64
}

func spawn(mut ref p: Particles, x: f64, y: f64): void = {
    table_insert(mut ref p, ParticlesRow {
        pos: Vec2 { x: x, y: y },
        life: 100
    })
}

func main(): void = {
    // All heap memory comes from arenas — no malloc, no GC
    mut mem: Arena = arena(65536)
    mut p: Particles = table_alloc(mut ref mem, 1000)

    spawn(mut ref p, 1.0, 2.0)
    spawn(mut ref p, 3.0, 4.0)

    // Row access (returns a value copy)
    val r: ParticlesRow = table_get(ref p, 0)
    assert r.pos.x == 1.0

    // Direct column access (cache-friendly iteration)
    mut total: i64 = 0
    for i in 0..table_len(ref p) {
        total = total + p.life[i]
    }
    assert total == 200
}
```

## What Makes Nore Different

**Data layout is a first-class concern.** A single `table` declaration generates columnar storage (struct-of-arrays) with type-safe row access — the kind of layout that games, simulations, and data-heavy systems need for cache performance, without manual bookkeeping.

**Two kinds of types, one clear rule.** `value` types are plain data: they live on the stack, copy freely, and compose into arrays and tables. `struct` types own resources: they hold slices of arena-allocated memory, pass by reference only, and cannot be copied. No hidden allocations, no implicit clones.

**Arenas replace malloc/free.** All heap memory comes from arenas. The compiler tracks which slices come from which arena and rejects programs where a slice could outlive its arena — at compile time, with no garbage collector and no runtime cost.

**Explicit is better than implicit.** Parameters are `ref` or `mut ref` at both declaration and call site. Mutability is visible everywhere. There are no hidden copies, no move semantics to reason about.

## Design Documents

- [docs/data-oriented-design.md](docs/data-oriented-design.md) — Type model, tables, arenas, and memory safety approach
- [docs/arena-safety.md](docs/arena-safety.md) — Escape analysis, slice lifetime tracking, and safety guarantees
- [docs/error-codes.md](docs/error-codes.md) — All compiler error codes and error handling internals

## Architecture

The compiler follows a multi-stage pipeline:

1. **Frontend** — Lexer tokenizes source, parser builds an AST
2. **Semantic analysis** — Type checking, escape analysis, arena lifetime validation
3. **Code generation** — AST translates to C99 code
4. **Native compilation** — Clang compiles generated C to a native binary

## Project Status

**Current Phase**: Early development

The compiler is being developed as a single-file C program (`nore.c`) containing:
- Lexer implementation
- Parser implementation
- AST data structures
- C code generator
- Clang integration layer

## Build Requirements

- **Compiler**: Clang
- **C Standard**: C99
- **Platform**: Unix-like systems (Linux, macOS, BSD)

## Build & Usage

```bash
# Build the Nore compiler (optimized)
make

# Build with debug symbols
make debug

# Clean build artifacts
make clean

# Compile a Nore program (outputs ./program by default)
./nore program.nore

# Specify output path explicitly
./nore program.nore -o build/program

# Compile and run immediately (temp binary, auto-cleaned)
./nore --run program.nore

# Debug flags (inspect compiler stages)
./nore program.nore --lexer    # Print lexer tokens
./nore program.nore --parser   # Print AST structure
./nore program.nore --codegen  # Print generated C code (IR)

# Combine flags
./nore program.nore --parser --codegen -o program
```

## Language Syntax

See [docs/syntax.md](docs/syntax.md) for the complete language syntax reference.

## Error Handling

The compiler uses structured error codes (e.g., `S053`, `P014`) with source locations and collects up to 10 errors before stopping. See [docs/error-codes.md](docs/error-codes.md) for the full reference.

## Testing
```bash
make test          # Run all tests (errors + success)
make test-errors   # Run error code tests only
make test-success  # Run success tests only
```
- Error tests in `tests/errors/` named by expected code (e.g., `P002_missing_rparen.nore`)
- Success tests in `tests/success/` — programs with assertions, compiled and run via `--run` flag
- Test runners: `tests/run_error_tests.sh` and `tests/run_success_tests.sh`

## Development Roadmap

1. **Phase 1**: Lexer and basic tokenization
2. **Phase 2**: Parser and AST construction
3. **Phase 3**: C code generation for basic constructs
4. **Phase 4**: Clang integration and native compilation
5. **Phase 5**: Language feature expansion
6. **Phase 6**: Standard library development

### DOD Type System Implementation Sequence

Each step builds on the previous one. See [docs/data-oriented-design.md](docs/data-oriented-design.md) for the full design.

1. **`value` types** — composite data with named fields, stack-only, pass by copy
2. **Fixed-size arrays** `[T; N]` — stack-allocated, value-compatible
3. **`ref` parameters** — pass by reference for functions (required before structs)
4. **`struct` types** — resource owners, ref-only passing, may contain slices
5. **Slices `[]T` + Arenas** — first heap allocation, compile-time lifetime checks
6. **`str` type** — byte slice, falls out of slice implementation
7. **`table` sugar** — generates struct + value, the DOD payoff

## Technical Decisions

### Why a single-file compiler?
Simplifies building, distribution, and studying the compiler. Can be refactored into modules later if needed.

### Why C as intermediate representation?
Avoids platform-specific backends, inherits Clang's optimization passes, and enables rapid compiler development. Proven approach (used by early C++, Nim, and others).

### Why Clang?
Modern, actively maintained, with strong cross-compilation support and excellent error messages.

## Contributing

This project is in early development. Design discussions and architecture feedback are welcome.

## License

BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
