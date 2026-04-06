# Data Layout and Optimization Ideas

Focused future directions around table views, index sets, layout control, and low-level optimization. Nothing here is committed. These ideas are grouped separately because they describe a coherent path: richer stdlib-defined data views backed by explicit compiler support for optimization when needed.

For a source-grounded reading of the underlying HPC material before jumping into Nore-specific future ideas, see [views-layout-and-policies.md](views-layout-and-policies.md).

## Index Sets and Table Views

Today's tables are flat: N rows, every column has exactly N entries. There's no way to express subsets, partitions, or hierarchical groupings. An index set model could address this.

**Core idea:** A view has its own compact local index space `0..N-1`, and an `IndexSet` maps those local indices into the parent view's index space. Views form a hierarchy of subsets: each child view refers only to entities already present in its parent view.

```
// root view over all rows
p.all()

// subset of the root view
p.active()

// subset of a subset
p.active.nearby()
```

**What this enables:**

- **Hierarchical subsets.** A view can have zero or more child views, and each child is a subset of its parent view rather than a fresh global index list.
- **Local compactness.** Each view is locally packed and stride-1 in its own index space, which keeps iteration efficient.
- **Cheap subset maintenance.** In the subset-view model, "deleting" can mean removing an index from a view rather than moving base rows.
- **Multiple views, same entities.** `p.active`, `p.dying`, and `p.onscreen` can describe different subsets of the same underlying table entities.
- **Partitioning for parallelism.** Index sets can be split across threads; each thread gets a chunk of indices over the same columns, no aliasing.

Sparse memory access is only required when a child view maps sparsely into a parent or ancestor view. A lot of useful cases are much cheaper than that: if the child-to-parent mapping is stride-1 or a regular slab, parent access is still packed.

**TALC's `IndexSet` model is richer than "just a slice of indices".** TALC's implementation makes the representation more concrete:

- **Unstructured mode:** an explicit map from local view indices to parent-view indices.
- **Structured mode:** extents, strides, and a parent-corner offset, enough to describe a hyperslab or packed n-dimensional region inside the parent view.
- **Materialization on demand:** TALC can turn a structured index set into an explicit map when needed.

For example, in a row-major flattened `8x8` parent grid, a `4x3` tile starting at row 2, column 1 could be described as:

```text
dims = 2
extents = [4, 3]
strides = [1, 8]
parentCornerOffset = 17
```

That means:

- local `(0, 0)` maps to parent `17`
- local `(1, 0)` maps to parent `18`
- local `(0, 1)` maps to parent `25`

So the structured index set describes this parent region without storing an explicit index list:

```text
17 18 19 20
25 26 27 28
33 34 35 36
```

In other words, structured mode stores a local shape plus the stepping rule needed to walk that shape inside the parent view.

That suggests Nore should separate the **semantic model** from the **chosen representation**. Semantically, a view is a parent-relative compact subset. Representation-wise, it might be a dense range, a structured slab, an explicit map, or another internal form.

**Stdlib, not compiler.** The baseline model still looks like a stdlib feature: a view is a struct over a table plus some parent-relative index-set representation, and operations such as `view_create`, `view_add`, `view_remove`, and `view_get` are regular functions over existing primitives. The only reasons to involve the compiler would be syntactic sugar (e.g. `foreach point in p.active`) or deeper optimization support.

That said, this only addresses the functional baseline. If Nore later wants TALC-style layout control, cache hints, or other deeper optimization support for stdlib-defined data structures, that may justify separate compiler directives without making views or index sets native language features.

**One possible backend: sparse sets.** Sparse sets are still interesting for highly dynamic unstructured subsets such as "active entities" or "dirty rows". They may be a useful implementation strategy for one class of views, but they are not the whole model. TALC's view hierarchy also covers structured, parent-relative subsets where sparse indirection is unnecessary.

For the dynamic-unstructured case, a sparse set (as used by EnTT) looks like this:

```
sparse: [_, _, 1, _, _, 2, _, 0, _, _]   // indexed by entity ID -> position in dense
dense:  [7, 2, 5]                         // packed, no holes. iterate this
```

- Iteration over membership walks `dense` sequentially
- Insert is O(1): append to dense, update sparse
- Remove is O(1): swap last element into the hole, update sparse
- Lookup is O(1): sparse[id] gives position in dense

The sparse array can be large but unused pages are never touched (virtual memory handles this). This is a plausible backend for dynamic subsets in Nore, but it should be presented as one representation choice among several, not as the definition of views or index sets.

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
- How should Nore model structured index sets such as slabs and tiles, instead of only explicit unstructured maps?
- How much of the TALC model (layout-algorithm separation, parallel partitioning) can be expressed as stdlib vs requiring compiler support?

**Source:** r/ProgrammingLanguages detailed feedback with pseudo-code example showing View-based model with index sets, nested views, and implicit parallelism. TALC reference from same commenter.

## Compiler Directives for Low-Level Optimization

The native-vs-stdlib tension in the view/index-set model reveals a deeper architectural question: how does the stdlib get the performance wins that require compiler awareness, without making the compiler special-case every data structure?

**The problem:** If IndexSets and views are pure stdlib (opaque structs and function calls), the compiler can't optimize through them: no layout rewriting, no prefetching, no tiling. But making them native compiler features contradicts Nore's philosophy and ties optimization to specific data structures.

**Proposed direction: a two-layer architecture.**

```
User code          ->  friendly API (stdlib)
                       ↓ uses
Stdlib internals   ->  compiler directives (native, low-level, powerful)
                       ↓ understood by
Compiler/codegen   ->  transforms to optimized C
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
