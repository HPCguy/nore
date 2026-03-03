# Future Ideas

Ideas and directions gathered from community feedback and design discussions. Nothing here is committed — these are candidates worth exploring as the language matures.

## Associated Types (`Type.Row` syntax)

Replace auto-generated `ParticlesRow` with explicit `Particles.Row` syntax. This is more consistent with Nore's "explicit is better" philosophy and opens the door to a general associated-type pattern useful for stdlib design (e.g., `HashMap.Entry`, `Iterator.Item`).

**Source:** r/ProgrammingLanguages feedback on implicit type generation.

## Table Field Grouping (hybrid SoA/AoS)

Allow grouping fields within a table for interleaved storage, while ungrouped fields stay columnar:

```nore
table Particles {
    group { pos: Vec2, velocity: Vec2 },  // stored interleaved (AoS)
    life: i64,                             // separate column (SoA)
}
```

Same API surface — `table_get` returns a flat row, grouping is purely a storage decision. Backwards-compatible (no groups = pure SoA, today's behavior).

**Source:** r/ProgrammingLanguages feedback on SoA not fitting all access patterns.

## Table as Stdlib Instead of Keyword

If Nore eventually gets generics or metaprogramming, `table` could move from a keyword to a stdlib construct. Today it's a keyword because it generates two types from one declaration, which requires compiler support. Worth revisiting if the language grows to support this.

Index sets and table views (see below) reinforce this direction — they're built entirely on existing primitives (structs, slices, arenas) and need no compiler machinery. A stdlib `view_create`, `view_add`, `view_get` over a `[]i64` index slice is enough. If tables, views, and index sets are all patterns composed from core primitives, the language provides the foundation and the stdlib provides the patterns.

**Source:** r/ProgrammingLanguages feedback on keywords vs stdlib.

## Arena Flexibility

Current arenas are append-only with bulk reset. This works well for batch workloads (game loops, simulations, data pipelines) but not for long-lived heterogeneous allocations (e.g., per-connection structs in a web server). Possible directions:

- Arena-based free lists
- Typed pools
- Per-request arenas with scoped lifetimes

**Source:** r/ProgrammingLanguages feedback on arena-only memory being too restrictive.

## Scheduling / Traversal Strategies

Scheduling languages (Halide, Exo) separate *what* you compute from *how* you traverse data. Nore focuses on data layout; scheduling focuses on iteration strategy. Could table iteration support tiling, vectorization hints, or cache-blocking annotations?

**Source:** r/ProgrammingLanguages pointer to Halide, Exo, and the Exo 2 paper.

## Column Unions (overlaid storage)

Same-sized columns could share storage and be reinterpreted based on context, similar to C#'s `StructLayout.Explicit` or C unions. Useful for variant entities — e.g., enemies and projectiles sharing a table but reinterpreting fields based on a type tag.

**Source:** r/ProgrammingLanguages feedback from game dev perspective.

## Index Sets and Table Views

Today's tables are flat: N rows, every column has exactly N entries. There's no way to express subsets, partitions, or hierarchical groupings. An index set model could address this.

**Core idea:** A view is a set of indices over the same underlying columns. Multiple views share the same data — no duplication.

```
// 1000 particle slots allocated
p: Particles([0, 1000))     // full index set

// Only some are active — a subset view
p.active()                   // starts empty, grows with spawns

// "Deleting" = removing from the active set, no data movement
// Iterating p.active only touches live particles
```

**What this enables:**

- **No insert/shift cost** — "deleting" means removing an index, not moving data
- **Multiple views, same data** — `p.active`, `p.dying`, `p.onscreen` all point to different subsets of the same columns
- **Hierarchical subsets** — a view can have sub-views (`p.active.nearby`), addressing the flat-cardinality limitation
- **Partitioning for parallelism** — index sets can be split across threads; each thread gets a chunk of indices over the same columns, no aliasing

**Stdlib, not compiler.** An index set is just a `[]i64` slice — Nore already has this. A view is a struct holding a table reference + an index set. All operations (`view_create`, `view_add`, `view_remove`, `view_get`) are regular functions over existing primitives. No new compiler machinery needed.

The only reasons to involve the compiler would be syntactic sugar (e.g., `foreach point in p.active`) or auto-parallelism — neither of which fits Nore's explicit philosophy. This is a stdlib feature, and reinforces the case for migrating `table` itself to the stdlib once generics or metaprogramming are available.

**Implementation direction: sparse sets.** Naive index sets (`[]i64` of indices) break cache locality — access becomes scattered instead of sequential. Sparse sets (as used by EnTT) solve this with two arrays:

```
sparse: [_, _, 1, _, _, 2, _, 0, _, _]   // indexed by entity ID → position in dense
dense:  [7, 2, 5]                         // packed, no holes — iterate this
```

- Iteration walks `dense` sequentially — cache-perfect, same as a flat table
- Insert is O(1) — append to dense, update sparse
- Remove is O(1) — swap last element into the hole, update sparse
- Lookup is O(1) — sparse[id] gives position in dense

The sparse array can be large but unused pages are never touched (virtual memory handles this). This gives subset views with no cache penalty on iteration. Implementable entirely with slices and arenas — no compiler support.

**Open questions:**
- How do views interact with arena lifetime tracking?
- What set operations are worth providing (intersection, union, difference)?
- Should views support hierarchical nesting (sub-views of views)?

**Source:** r/ProgrammingLanguages detailed feedback with pseudo-code example showing View-based model with index sets, nested views, and implicit parallelism.
