# Views, Layouts, and Policies

This note starts from the HPC source material reviewed for this topic and tries to separate the ideas that earlier notes blended together.

The goal is not to jump straight to a Nore proposal. The goal is to understand the source material first, then ask which parts could fit naturally into Nore, which parts probably belong in a stdlib layer, and which parts would only make sense with explicit compiler support.

## Sources

This note is grounded in:

- the VISTA paper on hierarchical Views
- the TALC paper on layout transformation
- the RAJA paper on execution-policy separation
- an `IndexSet` implementation reviewed during this discussion as a concrete mapping example

## The Three Questions

The first useful clarification is that the source material is really about three different questions:

1. **What subset of data am I talking about?**
   This is the View and IndexSet question.
2. **How is that data arranged in memory?**
   This is the layout question: SoA, AoS, interleaving, grouping, tiling.
3. **How is iteration mapped to hardware?**
   This is the execution-policy question: sequential loops, OpenMP-style threading, CUDA thread/block mappings, tiling, hyperplanes, and so on.

Earlier notes treated these as one topic because they interact strongly. But the papers suggest they should be kept conceptually separate.

## VISTA: The Data Model

VISTA is the strongest source for the actual View model.

Its central claim is simple: a **View** is a scoped subset of a larger dataset, and Views are organized into a hierarchy.

The paper defines a View as:

```text
View
+- IndexSet
+- Relation Table
+- Field Table
+- Parameter Table
+- View Table
```

In other words, a View is not just "a list of members". It is a node in a tree with:

- an `IndexSet` telling you which entities belong to the View
- relations to other Views
- fields holding bulk data
- parameters holding non-bulk data
- child Views

This matters because it shifts the model from "subset as a helper list" to "subset as a first-class database node".

### What VISTA clarifies

- A View encapsulates a subset of a mesh.
- Views can be arranged in a scoped hierarchy.
- Child Views are not an afterthought. They are part of the View definition.
- Recursive traversal of the View tree is a core pattern, not a convenience.
- The same View tree can be used in memory, in restart files, and in analysis tools.

That last point is especially important. VISTA argues that a good data model reduces glue code because the same structure appears consistently across:

- in-memory representation
- serialization / restart files
- whiteboard reasoning
- visualization / debugger tooling

That is a much stronger claim than "subsets are useful for iteration".

## TALC: The Layout System

TALC is the strongest source for layout transformation, not for the View tree itself.

The TALC paper we have is mainly about:

- manual layout selection
- automated layout selection
- field-specification files
- meta files that describe grouped layouts
- source-to-source rewriting of array declarations, accesses, and allocation

The key point is that TALC answers the **layout** question:

- Which arrays should be considered together?
- Which fields should be interleaved?
- Should the generated layout be `27 x 1`, `9 x 3`, `3 x 9`, and so on?

The TALC paper uses `View` and `Field` in schema files, but in the material we have that usage is closer to a layout/schema namespace than to VISTA's full runtime View tree.

So the safe reading is:

- **VISTA** explains the runtime data model.
- **TALC** explains a layout-transformation workflow that can operate over that sort of model.

That separation makes the whole topic much easier to reason about.

### What TALC contributes

- Layout can be user-specified or automatically selected.
- A field-specification file marks arrays that are eligible for transformation.
- A meta file describes which fields should be grouped into a concrete memory layout.
- A source-to-source tool can rewrite accesses and allocation to match the selected layout.
- The algorithm can stay stable while the physical layout changes.

This is the important architectural lesson: **data model** and **physical layout** are related, but they are not the same layer.

## IndexSet: The Mapping Layer

The reviewed `IndexSet` implementation is useful because it makes the View-to-parent mapping more concrete.

From that file, an `IndexSet` is a map from a compact local index space in the current View into a larger index space in the parent View.

That implies a parent-relative model:

- each View has its own local `0..N-1` index space
- an `IndexSet` explains how that local space maps into the parent
- nesting is built one parent/child edge at a time

### Two representations

The implementation reviewed during this discussion supports two different representations:

- **Unstructured:** an explicit array of mapped parent indices
- **Structured:** `dims`, `extents`, `strides`, and a `parentCornerOffset`

Structured mode is important because it shows that an IndexSet does not have to mean "arbitrary sparse indices". It can mean "a regular shape inside the parent", such as:

- a packed tile
- a slab
- a strided multi-dimensional region

For structured mode, the mapping is conceptually:

```text
parent_index =
  parentCornerOffset +
  i0 * stride[0] +
  i1 * stride[1] +
  ...
```

for local coordinates `(i0, i1, ...)` within the extents.

That means a child View can be locally packed while still referring to a regular region in the parent.

### What this changes

This makes two earlier simplifications unsafe:

- an IndexSet is not just a `[i64]` of explicit indices
- sparse-set style representations are not the whole model

Sparse sets may still be useful for highly dynamic unstructured subsets, but they are only one possible backend.

### What is still unclear

The material we have does **not** clearly document a maximum subview nesting depth.

What it does show is:

- one IndexSet represents one current-to-parent mapping step
- view nesting is therefore naturally recursive
- the visible fixed-size arrays in `createUnstructuredFromStructured()` suggest a practical limit of 10 structured dimensions in that code path

That last point is about dimensionality, not View-tree depth.

## RAJA: The Execution Policy Layer

RAJA is the strongest source here for the third question: how iteration is mapped to hardware.

The RAJA paper shows a useful separation:

- the numerical kernel stays conceptually the same
- the execution policy decides whether it runs as sequential loops, OpenMP-style parallelism, or CUDA kernels
- changing the policy changes the mapping to hardware without forcing a rewrite of the algorithm

That is a different concern from both:

- View hierarchy
- physical data layout

This is useful for Nore because it suggests a cleaner split:

- View / IndexSet model: what subset of data exists
- layout controls: how that data is stored
- execution policy: how loops over that data run

If those are kept separate, each layer can stay explicit without being overloaded.

## Distilled Conceptual Model

Putting the source material together suggests this stack:

### 1. View / subset layer

- A View is a first-class node, not just a helper range
- Each View has a compact local index space
- Each child View maps into its parent via an IndexSet
- Views may own child Views and associated data/metadata

### 2. Layout layer

- Field grouping and physical arrangement are separate decisions
- The same logical View/data model can admit multiple layouts
- Layout selection can be manual or automatic

### 3. Execution layer

- The same logical algorithm can be mapped differently to hardware
- Tiling, hyperplanes, vectorization, GPU launch structure, and threading belong here

This layered reading is the clearest thing the source material gives us.

## What Could Fit in Nore

This section is intentionally conservative. It is not trying to force a final language design yet.

### A. What could plausibly be stdlib-level

The baseline View model feels like a stdlib or library-level concept:

- root Views over existing table-backed or slice-backed data
- child Views with parent-relative IndexSets
- explicit APIs for child creation, traversal, and serialization
- structured and unstructured IndexSet representations behind one semantic abstraction

This matches Nore's preference for explicitness and avoids baking one specific HPC model directly into the compiler.

### B. What may eventually need compiler help

The source material also suggests limits to a pure library approach:

- layout rewriting is much easier when the compiler can reason about grouped fields
- some transformations want compiler-visible intent, not just opaque function calls
- optimization hints around layout, tiling, and vectorization may eventually justify a small explicit directive surface

But that does **not** imply that Views themselves should become native syntax.

A more plausible path is:

1. keep Views and IndexSets as ordinary types and functions
2. add explicit compiler hooks only when a concrete optimization gap appears
3. keep those hooks general rather than special-casing one data structure family

### C. What should probably stay separate

The source material argues strongly against collapsing everything into one feature.

Nore should probably avoid treating these as one mechanism:

- subset hierarchy
- memory layout
- loop scheduling / hardware mapping

They influence one another, but they are not the same design problem.

## A Plausible Nore Path

If these ideas ever move from research into implementation, a staged path seems safer than a single feature push.

### Iteration 1: Clarify the data model

- Define what a Nore View would mean semantically
- Decide whether a View owns fields or references existing table storage
- Decide how parent-relative IndexSets are represented

### Iteration 2: Add structured IndexSets

- Explicit map form
- Dense-range form
- Structured slab / tile / stride form
- Maybe dynamic sparse-set backends where they actually help

### Iteration 3: Decide layout control boundaries

- Which layout choices should remain library-level?
- Which ones, if any, deserve explicit compiler directives?
- How much automation fits Nore's "explicit is better" philosophy?

### Iteration 4: Consider execution policies separately

- sequential vs parallel
- tiling and hyperplanes
- vectorization hints
- future device-specific mappings

This should only happen after the data model is clear. Otherwise the design risks mixing storage, subset semantics, and execution mapping too early.

## Open Questions for Nore

- What is the smallest useful View abstraction in Nore?
- Should a View be defined over a `table`, over generic slice-backed storage, or both?
- How should relations between Views be modeled?
- How do arena lifetimes interact with child Views and parent-relative IndexSets?
- Do Fields conceptually belong to the View node, or should they remain separate table-backed data with the View as an index-space lens?
- What is the minimal structured IndexSet API that is worth supporting?
- If compiler support is eventually added, what is the smallest explicit surface that gives real value without turning into a second language?

## Summary

The source material suggests a cleaner picture than the earlier future-ideas notes:

- **VISTA** gives the View tree and subset data model.
- **TALC** gives layout transformation and layout-selection workflow.
- **RAJA** gives explicit execution-policy control.
- **IndexSet mechanics** clarify how a child View can map into a parent either structurally or explicitly.

That layered interpretation is probably the right starting point for any future Nore design work in this area.
