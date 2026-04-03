<h1>
  <img src="docs/logo/logo.svg" alt="Nore logo" width="96" height="96" valign="middle">
  Nore
</h1>

Nore is a systems programming language that makes data-oriented design the path of least resistance. 

It gives you direct control over data layout, arena-based memory management, and explicit value vs resource semantics, with compile-time safety guarantees and no runtime overhead.

## A Quick Look

```rust
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

## Current State

- [`compiler/`](compiler/) is the canonical self-hosted compiler source tree
- [`bootstrap/nore.c`](bootstrap/nore.c) remains the trusted stage-0 seed used to rebuild and verify the compiler
- the compiler emits C as its backend IR and relies on Clang for native code generation
- the language, standard library, tooling, and documentation are still evolving

## Build Requirements

- **Compiler**: Clang
- **C Standard**: C99
- **Platform**: Unix-like systems (Linux, macOS, BSD)

## Build & Usage

```bash
make
./norec program.nore
./norec program.nore -o build/program
./norec --run program.nore
./norec --lexer program.nore
./norec --parser program.nore
./norec --codegen program.nore
```

Maintainer and rebuild paths:

```bash
make stage0
./nore program.nore
./bootstrap/bootstrap.sh
```

The [examples/](examples/) directory contains real programs built on the standard library.

```bash
./norec --run examples/cat.nore -- file1.txt file2.txt
./norec --run examples/json.nore -- data.json
```

## Testing

```bash
make test          # Run language suites through ./norec
make test-stage0   # Run the same language suites through the C seed fallback
make test-errors   # Run error code tests through ./norec
make test-errors-stage0  # Run error tests through the C seed fallback
make test-success  # Run success tests through ./norec (includes stdlib)
make test-success-stage0 # Run success tests through the C seed fallback
make test-std      # Run stdlib tests through ./norec
make test-std-stage0     # Run stdlib tests through the C seed fallback
make test-compiler # Run compiler-specific tests through ./norec
make test-compiler-stage0 # Run compiler-specific tests through the C seed fallback
```
- Error tests in `tests/errors/` named by expected code (e.g., `P002_missing_rparen.nore`)
- Success tests in `tests/success/`: programs with assertions, compiled and run via `--run` flag
- Stdlib tests in `tests/std/`: test each `std/` library module (e.g., `tests/std/math.nore`)
- Compiler-specific tests live under `tests/compiler/`
- For compiler build details, rebuild paths, and the benchmark command, see [docs/compiler.md](docs/compiler.md)

## Documentation

- [docs/nore.md](docs/nore.md) for the language guide
- [docs/syntax.md](docs/syntax.md) for the syntax quick reference
- [docs/compiler.md](docs/compiler.md) for the current compiler overview
- [docs/compiler-architecture.md](docs/compiler-architecture.md) for compiler internals and module ownership
- [docs/error-codes.md](docs/error-codes.md) for diagnostic codes
- [editors/nvim/README.md](editors/nvim/README.md) for the Neovim runtime package
- [docs/history/README.md](docs/history/README.md) for archived bootstrap-era records

## Repository Layout

- [`compiler/`](compiler/) — canonical self-hosted compiler source tree
- [`bootstrap/`](bootstrap/) — trusted stage-0 seed and rebuild-from-seed tooling
- [`std/`](std/) — standard library modules
- [`tests/`](tests/) — language, stdlib, and compiler-specific coverage
- [`examples/`](examples/) — example programs built on the standard library
- [`editors/`](editors/) — editor integrations

## Roadmap

- documentation cleanup and consolidation
- compiler diagnostics, reliability, and performance work
- standard library expansion
- continued language evolution
- tooling and editor support
- packaging and release discipline

## Historical Notes

Earlier self-hosting and bootstrap planning documents are preserved in [docs/history/README.md](docs/history/README.md). They remain useful as project history, but they no longer describe the canonical current structure of the compiler or repository.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines and development workflow details.

## About the Name and the Logo

Curious about the origin of the name *Nore* and the **|~|** symbol? See [docs/logo/logo.md](docs/logo/logo.md).

## License

BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
