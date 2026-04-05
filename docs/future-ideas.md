# Future Ideas

Ideas and directions gathered from community feedback and design discussions. Nothing here is committed. These are candidates worth exploring as the language matures.

## Associated Types (`Type.Row` syntax)

Replace auto-generated `ParticlesRow` with explicit `Particles.Row` syntax. This is more consistent with Nore's "explicit is better" philosophy and opens the door to a general associated-type pattern useful for stdlib design (e.g., `HashMap.Entry`, `Iterator.Item`).

**Source:** r/ProgrammingLanguages feedback on implicit type generation.

## Table Field Grouping (hybrid SoA/AoS)

Allow grouping fields within a table for interleaved storage, while ungrouped fields stay columnar:

```rust
table Particles {
    group { pos: Vec2, velocity: Vec2 },  // stored interleaved (AoS)
    life: i64,                             // separate column (SoA)
}
```

Same API surface. `table_get` returns a flat row, grouping is purely a storage decision. Backwards-compatible (no groups = pure SoA, today's behavior).

**Source:** r/ProgrammingLanguages feedback on SoA not fitting all access patterns.

## Table as Stdlib Instead of Keyword

If Nore eventually gets generics or metaprogramming, `table` could move from a keyword to a stdlib construct. Today it's a keyword because it generates two types from one declaration, which requires compiler support. Worth revisiting if the language grows to support this.

Index sets and table views (see [data layout and optimization ideas](data-layout-optimization.md)) reinforce this direction. They're built entirely on existing primitives (structs, slices, arenas) and need no compiler machinery. A stdlib `view_create`, `view_add`, `view_get` over a `[i64]` index slice is enough. If tables, views, and index sets are all patterns composed from core primitives, the language provides the foundation and the stdlib provides the patterns.

**Source:** r/ProgrammingLanguages feedback on keywords vs stdlib.

## Arena Flexibility

Current arenas are append-only with bulk reset. This works well for batch workloads (game loops, simulations, data pipelines) but not for long-lived heterogeneous allocations (e.g., per-connection structs in a web server). Possible directions:

- Arena-based free lists
- Typed pools
- Per-request arenas with scoped lifetimes

Related direction: allocator-style APIs and clearer allocation boundaries. Arena-first allocation is still the core model, but library and interop boundaries may benefit from more explicit allocation interfaces, similar in spirit to allocator-passing styles seen in other systems languages.

This does not necessarily imply general-purpose heap allocation as the default model. The question is whether a clearer allocation API can make arena-based design more composable without giving up explicitness.

**Source:** r/ProgrammingLanguages feedback on arena-only memory being too restrictive.

## Associated Functions / Methods

Today functions are free-standing. This keeps the core model simple, but it also makes some APIs feel more distant from the types they conceptually belong to.

A future direction is to allow associating functions with types, for example as type-scoped functions or methods. This could improve discoverability and ergonomics without necessarily moving toward an object-oriented model.

**Open questions:**

- Should this start as type-scoped functions only (`Vec2.length(ref v)`) rather than method-call syntax (`v.length()`)?
- How should `ref` / `mut ref` remain explicit at declaration and call sites?
- How does this interact with the current preference for free-standing, explicit APIs?

**Source:** r/ProgrammingLanguages feedback asking for a way to associate functions with types.

## FFI / C Interop

Nore currently has a minimal native-function mechanism for compiler/runtime boundaries, but it does not yet have a broader, intentional story for foreign-function interfaces and external library interop.

A future direction is to design a clearer FFI model for calling C libraries and exposing Nore code across that boundary. This would likely be the first practical step before considering deeper library-specific integration.

SQLite feels like a particularly natural first interop target because of Nore's table-first direction, but the right first step would still be ordinary bindings. Only after that would it make sense to evaluate whether tighter integration around tables, rows, or queries is justified.

**Open questions:**

- What should the minimal safe and explicit FFI surface look like?
- How should slices, strings, arenas, and ownership map across the C boundary?
- Should FFI stay deliberately low-level, with higher-level wrappers living in the stdlib?

**Source:** r/Compiler discussion around SQLite interop and the suggestion to start with plain bindings before language-level integration.

## Scheduling / Traversal Strategies

Scheduling languages (Halide, Exo) separate *what* you compute from *how* you traverse data. Nore focuses on data layout; scheduling focuses on iteration strategy. Could table iteration support tiling, vectorization hints, or cache-blocking annotations?

**Source:** r/ProgrammingLanguages pointer to Halide, Exo, and the Exo 2 paper.

## Column Unions (overlaid storage)

Same-sized columns could share storage and be reinterpreted based on context, similar to C#'s `StructLayout.Explicit` or C unions. Useful for variant entities, e.g., enemies and projectiles sharing a table but reinterpreting fields based on a type tag.

**Source:** r/ProgrammingLanguages feedback from game dev perspective.

## Data Layout and Optimization Ideas

Ideas around index sets, table views, and compiler directives for low-level optimization now live in [data-layout-optimization.md](data-layout-optimization.md). They were split out because they form a coherent design thread around stdlib-defined data views and explicit compiler-assisted optimization.

## Release Mode (`--release` flag)

A `--release` flag that trades safety checks for performance. Today Nore is safe by default: bounds checks on array/slice access, `memmove` for `mem_copy` (handles overlapping buffers), runtime overflow detection on casts. A release mode could relax these:

- **Bounds checks**: skip `NI_SLICE_BOUNDS_CHECK` and array index checks (biggest win)
- **`mem_copy`**: emit `memcpy` instead of `memmove` (minor, single pointer comparison saved)
- **Assert removal**: strip `assert` statements
- **Cast overflow checks**: skip R003 range validation

The pattern follows C optimization levels: debug is safe and predictable, release trusts the programmer. Each relaxation should be individually toggleable if possible, so users can keep bounds checks but drop asserts, etc.

**Not worth adding yet.** The rough shape is clear, but Nore does not yet have enough performance pressure or runtime instrumentation to justify designing this in detail. Wait until real programs reveal where safety checks are actually costly. Bounds-check removal is still the most likely driver for this flag.

**Source:** Design discussion during `mem_copy` implementation (memcpy vs memmove trade-off).
