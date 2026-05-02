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

Nore's design is no longer just theoretical. The compiler is now written in Nore itself, supported by a small but functional standard library. Self-hosting is an important validation milestone for the language: it shows that Nore can express and sustain a non-trivial systems codebase, including its own implementation.

## Current State

- [`compiler/`](compiler/) is the canonical self-hosted compiler source tree
- [`bootstrap/norec-stage0.c`](bootstrap/norec-stage0.c) remains the trusted stage-0 seed used to rebuild and verify the compiler
- the compiler emits C as its backend IR and relies on Clang for native code generation
- the language, standard library, tooling, and documentation are still evolving

## Build Requirements

- **Compiler**: Clang
- **C Standard**: C99
- **Platform**: Unix-like systems (Linux, macOS, BSD)

## Build & Usage

```bash
make
./norec --help
./norec --version
./norec program.nore
./norec program.nore -o build/program
./norec --run program.nore
./norec --lexer program.nore
./norec --parser program.nore
./norec --codegen program.nore
./norec --strip-asserts program.nore
```

For rebuild-from-seed and maintainer workflows, see [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/compiler.md](docs/compiler.md).

The [examples/](examples/) directory contains real programs built on the standard library.

```bash
./norec --run examples/cat.nore -- file1.txt file2.txt
./norec --run examples/json.nore -- data.json
```

## Testing

```bash
make test                  # Language behavior through ./norec
make test-stage0           # Language behavior through ./norec-stage0
make test-errors           # Error tests through ./norec
make test-success          # Success + stdlib tests through ./norec
make test-std              # Stdlib tests through ./norec
make test-compiler-fast    # Cheap compiler-internal regression loop
make test-compiler-core    # Kept compiler-core coverage
make test-compiler-bootstrap # Trusted-seed and stripped self-host checks
make test-examples         # Example programs through ./norec
make norec-stripped        # Build ./norec-stripped with asserts stripped

make test-compiler         # Broad self-hosted compiler suite
make test-compiler-all     # Explicit alias for the broad self-hosted suite

make qa-local              # test + test-compiler-fast
make qa-ci                 # test + test-compiler-core
make qa-bootstrap          # test-stage0 + test-compiler-bootstrap
make qa-full               # qa-ci + qa-bootstrap + test-examples
```
- Error tests in `tests/errors/` named by expected code (e.g., `P002_missing_rparen.nore`)
- Success tests in `tests/success/`: programs with assertions, compiled and run via `--run` flag
- Stdlib tests in `tests/std/`: test each `std/` library module (e.g., `tests/std/math.nore`)
- Compiler-specific tests live under `tests/compiler/`
- `test-compiler` keeps the broad self-hosted compiler suite, with `test-compiler-all` as an explicit alias
- `test-stage0` and `test-compiler-bootstrap` are the explicit stage-0 lanes; `test-compiler-bootstrap` also verifies stripped self-hosting
- `qa-local` is the normal local loop, `qa-ci` is the self-hosted pre-merge gate, and `qa-full` combines self-hosted, bootstrap, and example confidence
- For compiler build details, rebuild paths, and the benchmark command, see [docs/compiler.md](docs/compiler.md)

## Editor Support

Nore currently includes a first Vim/Neovim runtime package with:

- filetype detection for `*.nore`
- syntax highlighting
- basic comment settings
- simple brace-based indentation

See [editors/vim/README.md](editors/vim/README.md) for installation and usage.

## Documentation

- [docs/nore.md](docs/nore.md) for the language guide
- [docs/syntax.md](docs/syntax.md) for the syntax quick reference
- [docs/compiler.md](docs/compiler.md) for the current compiler overview
- [docs/compiler-architecture.md](docs/compiler-architecture.md) for compiler internals and module ownership
- [docs/compiler-performance.md](docs/compiler-performance.md) for benchmark-driven compiler optimization notes
- [docs/data-layout-optimization.md](docs/data-layout-optimization.md) for focused future ideas around views, layout control, and optimization directives
- [docs/error-codes.md](docs/error-codes.md) for diagnostic codes
- [editors/vim/README.md](editors/vim/README.md) for the Vim/Neovim runtime package

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

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines and development workflow details.

## About the Name and the Logo

Curious about the origin of the name *Nore* and the **|~|** symbol? See [docs/logo/logo.md](docs/logo/logo.md).

## License

BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
