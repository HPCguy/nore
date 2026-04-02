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

Index sets and table views (see below) reinforce this direction. They're built entirely on existing primitives (structs, slices, arenas) and need no compiler machinery. A stdlib `view_create`, `view_add`, `view_get` over a `[i64]` index slice is enough. If tables, views, and index sets are all patterns composed from core primitives, the language provides the foundation and the stdlib provides the patterns.

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

Same-sized columns could share storage and be reinterpreted based on context, similar to C#'s `StructLayout.Explicit` or C unions. Useful for variant entities, e.g., enemies and projectiles sharing a table but reinterpreting fields based on a type tag.

**Source:** r/ProgrammingLanguages feedback from game dev perspective.

## Index Sets and Table Views

Today's tables are flat: N rows, every column has exactly N entries. There's no way to express subsets, partitions, or hierarchical groupings. An index set model could address this.

**Core idea:** A view is a set of indices over the same underlying columns. Multiple views share the same data, no duplication.

```
// 1000 particle slots allocated
p: Particles([0, 1000))     // full index set

// Only some are active, a subset view
p.active()                   // starts empty, grows with spawns

// "Deleting" = removing from the active set, no data movement
// Iterating p.active only touches live particles
```

**What this enables:**

- **No insert/shift cost.** "Deleting" means removing an index, not moving data.
- **Multiple views, same data.** `p.active`, `p.dying`, `p.onscreen` all point to different subsets of the same columns.
- **Hierarchical subsets.** A view can have sub-views (`p.active.nearby`), addressing the flat-cardinality limitation.
- **Partitioning for parallelism.** Index sets can be split across threads; each thread gets a chunk of indices over the same columns, no aliasing.

**Stdlib, not compiler.** An index set is just a `[i64]` slice. Nore already has this. A view is a struct holding a table reference + an index set. All operations (`view_create`, `view_add`, `view_remove`, `view_get`) are regular functions over existing primitives. No new compiler machinery needed.

The only reasons to involve the compiler would be syntactic sugar (e.g., `foreach point in p.active`) or auto-parallelism, neither of which fits Nore's explicit philosophy. This is a stdlib feature, and reinforces the case for migrating `table` itself to the stdlib once generics or metaprogramming are available.

**Implementation direction: sparse sets.** Naive index sets (`[i64]` of indices) break cache locality because access becomes scattered instead of sequential. Sparse sets (as used by EnTT) solve this with two arrays:

```
sparse: [_, _, 1, _, _, 2, _, 0, _, _]   // indexed by entity ID → position in dense
dense:  [7, 2, 5]                         // packed, no holes. iterate this
```

- Iteration walks `dense` sequentially, cache-perfect, same as a flat table
- Insert is O(1): append to dense, update sparse
- Remove is O(1): swap last element into the hole, update sparse
- Lookup is O(1): sparse[id] gives position in dense

The sparse array can be large but unused pages are never touched (virtual memory handles this). This gives subset views with no cache penalty on iteration. Implementable entirely with slices and arenas, no compiler support.

**Prior art: TALC (Topologically-Aware Layout in C).** The View + IndexSet model has been validated by [TALC](https://www.osti.gov/biblio/1108924), a research project from Lawrence Livermore National Laboratory (~2007). TALC is a source-to-source C translator that uses Views (data layout schemas), IndexSets (traversal definitions), and Fields (arrays eligible for layout transformation) to separate data layout from algorithm. Key results:

- Same code, different layouts: switching between SoA, AoS, hybrid without changing the algorithm
- Up to 200% speedups by just changing the layout schema
- Designed for HPC workloads (mesh traversal, physics simulations)

TALC proved the model works, but it's a preprocessor bolted onto C. The underlying language doesn't understand Views or IndexSets. Nore already has tables, value/struct distinction, arenas, and slices as native concepts. What TALC needed a separate schema file + source-to-source transformation + runtime system to achieve could potentially be expressed in Nore as stdlib types with the compiler's existing type checking.

**References:**
- [TALC paper (OSTI)](https://www.osti.gov/biblio/1108924)
- [TALC on ResearchGate](https://www.researchgate.net/publication/228936928_TALC_A_Simple_C_Language_Extension_For_Improved_Performance_and_Code_Maintainability)
- [Data Layout Optimization for Portable Performance (follow-up)](https://link.springer.com/chapter/10.1007/978-3-662-48096-0_20)

**Open questions:**
- How do views interact with arena lifetime tracking?
- What set operations are worth providing (intersection, union, difference)?
- Should views support hierarchical nesting (sub-views of views)?
- How much of the TALC model (layout-algorithm separation, parallel partitioning) can be expressed as stdlib vs requiring compiler support?

**Source:** r/ProgrammingLanguages detailed feedback with pseudo-code example showing View-based model with index sets, nested views, and implicit parallelism. TALC reference from same commenter.

## Compiler Directives for Low-Level Optimization

The native-vs-stdlib tension (see Index Sets above) reveals a deeper architectural question: how does the stdlib get the performance wins that require compiler awareness, without making the compiler special-case every data structure?

**The problem:** If IndexSets and views are pure stdlib (opaque structs and function calls), the compiler can't optimize through them: no layout rewriting, no prefetching, no tiling. But making them native compiler features contradicts Nore's philosophy and ties optimization to specific data structures.

**Proposed direction: a two-layer architecture.**

```
User code          →  friendly API (stdlib)
                       ↓ uses
Stdlib internals   →  compiler directives (native, low-level, powerful)
                       ↓ understood by
Compiler/codegen   →  transforms to optimized C
```

General-purpose compiler directives, not tied to any specific data structure, that the stdlib uses internally. Users *can* use them directly but mostly wouldn't need to.

**Candidate directives (raw, needs design):**

- `@layout(SoA)` / `@layout(AoS)`: control field arrangement in memory. Stdlib uses this inside table/view implementations. This is the key to TALC-style layout-algorithm separation.
- `@prefetch(data, stride)`: hint to insert prefetch instructions. Stdlib uses this inside iteration helpers.
- `@tile(loop, size)`: restructure a loop for cache blocking.
- `@vectorize(loop)`: hint that a loop body has no dependencies and can be SIMD-vectorized.
- `@inline`: force function inlining (critical for stdlib wrappers to have zero overhead).

**Why this fits Nore:**

- **Explicit, not magic.** Directives are visible in source, deliberately chosen. No hidden transformations.
- **Power without user-facing complexity.** End users call `table_get`, `view_foreach`. Only stdlib authors touch `@prefetch` and `@tile`.
- **Not tied to specific types.** Directives are general. They work for tables, views, index sets, or any future data structure. Contributors can use them without the compiler needing to special-case each new concept.
- **Future-proof.** New directives can be added as optimization needs emerge, without changing the language syntax.
- **Resolves the TALC question.** The optimization pull requests would target directives in the stdlib, not compiler internals. The compiler understands optimization *intent* through explicit directives rather than pattern-matching on magic types.

**The C analogy:** Similar to `restrict`, `inline`, `_Alignas`, and `__builtin_prefetch`. Low-level, not user-friendly, but they let library authors squeeze out performance. Nore would make them cleaner and more integrated.

**Risk:** Designing good directives is hard. Too few and they're useless, too many and they become a second language. The TALC papers and the [LLNL data layout optimization report](https://www.osti.gov/servlets/purl/1084701) (showing 1.1x to 22x speedups from automatic layout selection) are a good guide for what the minimal useful set looks like.

**Graceful degradation of control.** Any automatic optimization must have a manual override path. This applies to both layout decisions and parallelism. The principle: the user is always in control, the compiler only helps as much as the user allows.

Concrete model:
- **Compiler flags** control how aggressively the compiler rearranges user decisions. For example: `--layout=source` (emit exactly what the user wrote, for debugging), `--layout=auto` (let the compiler or architecture plug-in optimize).
- **In-source overrides** let the user pin a specific decision. For example: `@layout(AoS)` on a specific table overrides the automatic choice for that one case.
- **Default is source layout** (explicit, no rearrangement), until the user opts in. This preserves Nore's "explicit is better" philosophy while allowing progressive optimization.

This is analogous to C compiler optimization levels: `-O0` gives you what you wrote, `-O2` lets the compiler rearrange, and `volatile` or `__attribute__((noinline))` override specific decisions. The key insight from RAJA's evolution: a complete inability to direct the system step by step is a barrier to adoption. Users need to be able to debug, override, and fall back to direct control at any granularity.

**Source:** Design discussion following r/ProgrammingLanguages feedback on native vs stdlib optimization capabilities. Manual override principle from TALC/RAJA experience (Figure 6 in the TALC paper shows only one override was needed for the full functionality covered).

## Release Mode (`--release` flag)

A `--release` flag that trades safety checks for performance. Today Nore is safe by default: bounds checks on array/slice access, `memmove` for `mem_copy` (handles overlapping buffers), runtime overflow detection on casts. A release mode could relax these:

- **Bounds checks**: skip `NI_SLICE_BOUNDS_CHECK` and array index checks (biggest win)
- **`mem_copy`**: emit `memcpy` instead of `memmove` (minor, single pointer comparison saved)
- **Assert removal**: strip `assert` statements
- **Cast overflow checks**: skip R003 range validation

The pattern follows C optimization levels: debug is safe and predictable, release trusts the programmer. Each relaxation should be individually toggleable if possible, so users can keep bounds checks but drop asserts, etc.

**Not worth adding yet.** Today there is exactly one case (`mem_copy`) where this matters, and the performance difference is negligible. Wait until real programs reveal actual bottlenecks from safety checks. The bounds-check removal will be the real driver for this flag.

**Source:** Design discussion during `mem_copy` implementation (memcpy vs memmove trade-off).
