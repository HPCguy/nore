<h1>
  <img src="docs/logo/logo.svg" alt="Nore logo" width="96" height="96" valign="middle">
  Nore
</h1>

Nore is a systems programming language that makes data-oriented design the path of least resistance. 

Instead of hiding memory layout behind objects, Nore gives you direct control over how data is organized: columnar tables, arena allocation, explicit value vs resource semantics. All with compile-time safety guarantees and zero runtime overhead.

The compiler is a self-contained, single-file C program that translates Nore source code into native executables via C as an intermediate representation.

## A Quick Look

```nore
value Vec2 { x: f64, y: f64 }

// One declaration → columnar storage (struct-of-arrays)
// Generates: Particles (struct with slice columns) and Particles.Row (value type)
table Particles {
    pos: Vec2,
    life: i64
}

func spawn(mut ref p: Particles, x: f64, y: f64): void = {
    table_insert(mut ref p, Particles.Row {
        pos: Vec2 { x: x, y: y },
        life: 100
    })
}

func main(): void = {
    // All heap memory comes from arenas. No malloc, no GC
    mut mem: Arena = arena(65536)
    mut p: Particles = table_alloc(mut ref mem, 1000)

    spawn(mut ref p, 1.0, 2.0)
    spawn(mut ref p, 3.0, 4.0)

    // Row access (returns a value copy)
    val r: Particles.Row = table_get(ref p, 0)
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

**Data layout is a first-class concern.** A single `table` declaration generates columnar storage (struct-of-arrays) with type-safe row access. The kind of layout that games, simulations, and data-heavy systems need for cache performance, without manual bookkeeping. For example, `table Particles { pos: Vec2, life: i64 }` generates:

```
Particles (struct)          Particles.Row (value)
┌─────────────────┐         ┌─────────────────┐
│ pos:  []Vec2    │         │ pos:  Vec2      │
│ life: []i64     │         │ life: i64       │
│ _len: i64       │         └─────────────────┘
└─────────────────┘         copyable, embeddable
ref-only, owns slices
```

**Two kinds of types, one clear rule.** `value` types are plain data: they live on the stack, copy freely, and compose into arrays and tables. `struct` types hold slices (pointers into arena memory), so copying one would create aliased pointers to the same allocation. That's why structs pass by reference only and cannot be copied. The distinction isn't about data shape, it's about whether a type owns heap resources.

**Arenas replace malloc/free.** All heap memory comes from arenas. The compiler tracks which slices come from which arena and rejects programs where a slice could outlive its arena. All at compile time, with no garbage collector and no runtime cost.

**Explicit is better than implicit.** Parameters are `ref` or `mut ref` at both declaration and call site. Mutability is visible everywhere. There are no hidden copies, no move semantics to reason about.

## Thinking in Nore

Most languages default to trees of objects: a node *contains* its children, each allocated somewhere on the heap. It's intuitive, it maps to how we naturally think about hierarchies. But it's also slow when you have thousands of nodes, because every child access is a pointer chase to a different memory location.

Nore nudges you toward a different shape: flat tables where relationships are indices, not pointers. A compiler AST becomes a table of nodes with a `parent_id` column. A scene graph becomes a table of entities with a `parent` index. Children aren't *inside* the parent, they're rows in the same table that *reference* it.

```
Traditional (tree of objects)       Nore (table with relationships)

  Scene                             Entities table
  ├── Player                        ┌────┬──────────┬───────────┐
  │   ├── pos: Vec2                 │ id │ name     │ parent_id │
  │   ├── health: 100               ├────┼──────────┼───────────┤
  │   └── Sword                     │  0 │ Player   │        -1 │
  │       └── damage: 50            │  1 │ Sword    │         0 │
  └── Enemy                         │  2 │ Enemy    │        -1 │
      ├── pos: Vec2                 │  3 │ Shield   │         2 │
      └── Shield                    └────┴──────────┴───────────┘
          └── armor: 30
                                    Flat, sequential, cache-friendly.
  Nested pointers, scattered        Relationships are just indices.
  across the heap.
```

The mind shift is real: instead of "this object contains its data," you think "data lives in tables, and relationships are just columns." Once it clicks, you start seeing that most "tree" problems are actually "table with relationships" problems. And the flat layout gives you sequential memory access, easy serialization, and straightforward parallelism for free.

That said, if your natural model is trees of objects and your dataset is small, Nore will feel like unnecessary ceremony. It's not the right tool for everything, and that's okay.

## Architecture

The compiler follows a multi-stage pipeline:

1. **Frontend**: Lexer tokenizes source, parser builds an AST
2. **Semantic analysis**: Type checking, escape analysis, arena lifetime validation
3. **Code generation**: AST translates to C99 code
4. **Native compilation**: Clang compiles generated C to a native binary

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

## Language Guide

See [docs/nore.md](docs/nore.md) for the holistic language guide (philosophy, type model, memory model, syntax, safety). For a terse quick-reference, see [docs/syntax.md](docs/syntax.md).

## Editor Support

Editor integrations live in [`editors/`](editors/) so they can evolve alongside the language syntax.

Current support:
- [`editors/nvim/`](editors/nvim/) — Neovim runtime package with filetype detection for `*.nore`, syntax highlighting, basic comment settings, and simple brace-based indentation

Quick start with `lazy.nvim`:

```lua
{
  dir = "/absolute/path/to/nore/editors/nvim",
  name = "nore.nvim",
}
```

Replace the path with your local checkout. For manual installation and package layout details, see [`editors/nvim/README.md`](editors/nvim/README.md).

## Error Handling

The compiler uses structured error codes (e.g., `S053`, `P014`) with source locations and collects up to 10 errors before stopping. See [docs/error-codes.md](docs/error-codes.md) for the full reference.

## Examples

The [examples/](examples/) directory contains real programs built on the standard library. 

```bash
# Run the cat clone
./nore --run examples/cat.nore -- file1.txt file2.txt

# Parse and print a JSON file as an indented tree
./nore --run examples/json.nore -- data.json
```

## Testing
```bash
make test          # Run all tests (errors + success + stdlib)
make test-errors   # Run error code tests only
make test-success  # Run success tests only (includes stdlib)
make test-std      # Run stdlib tests only
```
- Error tests in `tests/errors/` named by expected code (e.g., `P002_missing_rparen.nore`)
- Success tests in `tests/success/`: programs with assertions, compiled and run via `--run` flag
- Stdlib tests in `tests/std/`: test each `std/` library module (e.g., `tests/std/math.nore`)
- Test runners: `tests/run_error_tests.sh`, `tests/run_success_tests.sh`, `tests/run_std_tests.sh`

## Development Roadmap

1. **Phase 1**: Lexer and basic tokenization
2. **Phase 2**: Parser and AST construction
3. **Phase 3**: C code generation for basic constructs
4. **Phase 4**: Clang integration and native compilation
5. **Phase 5**: Language feature expansion
6. **Phase 6**: Standard library development
7. **Phase 7**: Self-hosting (writing the Nore compiler in Nore)

## Technical Decisions

### Why a single-file compiler?
Simplifies building, distribution, and studying the compiler. Can be refactored into modules later if needed.

### Why C as intermediate representation?
Avoids platform-specific backends, inherits Clang's optimization passes, and enables rapid compiler development. Proven approach (used by early C++, Nim, and others).

### Why Clang?
Modern, actively maintained, with strong cross-compilation support and excellent error messages.

## Contributing

This project is in early development. Design discussions and architecture feedback are welcome.

## About the Name and the Logo

Curious about the origin of the name *Nore* and the **|~|** symbol? See [docs/logo/logo.md](docs/logo/logo.md).

## License

BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
