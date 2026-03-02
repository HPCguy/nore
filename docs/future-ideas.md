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

**Design tension for Nore:** The full model (auto-parallelism, compiler-managed thread safety, automatic reduction in loops) is powerful but requires significant compiler machinery — index set management, lock-free mutations, thread-local accumulators. This conflicts with Nore's "explicit, no magic" philosophy.

A simpler version might fit Nore better: support subset views as explicit index sets, but let the developer manage threading. This addresses the "flat table" limitation without hidden concurrency.

**Open questions:**
- Should views be a table feature or a more general language concept?
- How do views interact with arena lifetime tracking?
- Is the simpler "explicit index sets, no auto-parallelism" version enough?

**Source:** r/ProgrammingLanguages detailed feedback with pseudo-code example showing View-based model with index sets, nested views, and implicit parallelism.
