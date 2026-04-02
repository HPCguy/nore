# Arena Safety: Escape Analysis and Slice Lifetime

## Problem

Slices are fat pointers (data + length) into arena memory. When an arena is freed (scope exit) or reset, all slices pointing into it become dangling. The compiler must prevent use of dangling slices at compile time.

## Current State

### What the compiler checks today

| Check | Error | What it catches |
|-------|-------|-----------------|
| S053 | Slice escapes local arena | Direct, indirect (struct/tagged union), and transitive (function call) escape |
| S055 | Cannot reset immutable Arena | Reset on `val` arena |
| S056 | Use of slice after arena reset | Flow-sensitive invalidation within a function |

Note: S048 (blanket ban on slice returns) was removed, replaced by the unified S053 escape analysis.

### Types tracked by escape analysis

The escape analysis covers all types that can hold arena-allocated data:

- **Bare slices** (`[i64]`, `str`, etc.)
- **Structs with slice fields** (e.g., a struct containing `items: [i64]`)
- **Slice-bearing tagged unions** (e.g., `enum ReadResult { Ok([u8]), Err(i32) }`)
- **Tables** (columnar structs with slice fields)

All of these are checked uniformly by `type_return_has_slices()` and `type_is_non_copyable()`.

### The gap

S053 only catches **direct** escape, a return statement with a struct constructor whose fields are slices from a local arena. It does not catch **indirect** escape through function calls:

```rust
func build(mut ref mem: Arena): Result = {
    mut items: [i64] = arena_alloc(mut ref mem, 10)
    return Result { items: items }   // OK, mem is ref param, safe to return
}

func bad(): Result = {
    mut mem: Arena = arena(1024)
    return build(mut ref mem)        // UNSOUND, mem dies here, Result has dangling slices
}
```

From `build`'s perspective, `mem` is a ref parameter, so the arena outlives the callee. Correct. But the **caller** passes a local arena and returns the result, so the slices escape.

S048 (blanket ban on slice returns) had a similar inconsistency: bare slices could not be returned, but the same slice could escape through a struct. The ban protected one path while leaving the other open.

### Why this matters for tables and tagged unions

Tables (the DOD payoff) are structs with slice fields. Slice-bearing tagged unions (like `ReadResult { Ok([u8]), Err(i32) }`) carry slices in their payloads. The natural pattern is a helper function that takes `mut ref Arena` and returns one of these types. The indirect escape gap moves from "edge case" to "common path".

---

## Design: Unified Escape Analysis

### Principle

A slice's safety depends on one thing: **whether the arena it was allocated from is still alive when the slice is used**. The compiler should track this uniformly, regardless of whether the slice travels bare, inside a struct, or inside a tagged union.

### Existing infrastructure

Each slice variable already tracks its arena provenance:

```c
// In Variable struct
const char *arena_source_start;   // name of source arena
size_t arena_source_length;
bool arena_is_local;              // true if arena is local (not ref param)
```

Within a single function, this is sufficient:
- **Direct slice return**: check `arena_is_local`, reject if true
- **Struct constructor return**: check each slice field's `arena_is_local` (S053, already done)
- **Reset invalidation**: match `arena_source` to invalidate slices (S056, already done)

The gap is only at **function call boundaries**, where the caller cannot see inside the callee.

### Match binding propagation

When a match expression destructures a slice-bearing tagged union, the extracted slice binding inherits the scrutinee's arena tracking. This ensures that arena reset invalidation and escape analysis work correctly through match arms:

```rust
val result: ReadResult = read_file(mut ref mem, ref path)
match (result) {
    Ok(data) = {
        // data inherits result's arena tracking
        // if mem is reset, data is invalidated correctly
    }
    Err(code) = { ... }
}
```

The scrutinee variable is looked up once before the arm loop, and each slice binding copies the arena source fields.

### Per-function return analysis

During typecheck of each function body, determine whether the function returns slices that originate from an arena parameter. Store this as a flag on the function entry:

```c
// In FunctionEntry
bool returns_arena_slices;   // true if return value contains slices from arena params
```

This flag is set when typecheck encounters a return statement where:
- The return value is a slice with `arena_is_local == false` (came from a ref param arena), or
- The return value is a struct constructor with slice fields from ref param arenas (the existing S053 logic, inverted: S053 checks local arenas, this checks ref param arenas)

At **call sites**, the check becomes:
1. Does the called function have `returns_arena_slices == true`?
2. Is the arena argument at the call site a local arena?
3. If both, error: slices in return value escape local arena

### Transitive propagation

A function may not directly return an `alloc` result but instead return the result of calling another function that does:

```rust
func level2(mut ref mem: Arena): [i64] = {
    return arena_alloc(mut ref mem, 10)       // direct: returns_arena_slices = true
}

func level1(mut ref mem: Arena): [i64] = {
    return level2(mut ref mem)          // transitive, should also be true
}
```

Without propagation, `level1.returns_arena_slices` stays `false` because its return value comes from a function call, not directly from `alloc`. A caller passing a local arena to `level1` would not be caught.

**Solution**: during typecheck, when a function returns the result of a call that passes through an arena ref parameter, record a **dependency**:

```c
// Dependency record
{ size_t func_index, size_t callee_index }
// "func's returns_arena_slices depends on callee's returns_arena_slices"
```

After all functions are typechecked, **propagate to fixpoint** before checking call sites:

```
repeat until no changes:
    for each dependency (A depends on B):
        if B.returns_arena_slices -> set A.returns_arena_slices = true
```

This converges in O(chain depth) iterations, bounded by function count. In practice one or two passes over a small list.

### Deferred call-site checks

**Problem**: functions may be called before they are typechecked (forward references). When typechecking `main`, a helper defined later hasn't been analyzed yet, so `returns_arena_slices` is unknown.

**Solution**: defer the call-site arena escape checks. During typecheck, when a call site passes a local arena to a function that returns a type containing slices, record the check:

```c
// Deferred check record
{ SourceLoc loc, function_name, arena_name, arena_is_local }
```

After all function bodies are typechecked, run the transitive propagation (above), then process the deferred list. Each entry looks up the called function's flag and emits a diagnostic if needed.

This avoids two-pass typechecking and handles any definition order.

---

## Changes

### 1. Lift S048, allow bare slice returns

Remove the blanket ban on slice return types. Instead, check at return statements:
- If returning a slice variable whose `arena_is_local == true`, error (direct escape)
- If returning a slice variable whose `arena_is_local == false`, allowed (arena outlives callee)

This makes bare slices and struct-with-slices consistent.

### 2. Strengthen S053, catch indirect escape

Current S053 only checks return statements with struct constructors. Extend it to also catch function call returns via the deferred check mechanism described above.

The same error code (S053) can cover both cases with different messages:
- Direct: "Slice field '%s' escapes local arena" (existing)
- Indirect: "Return value of '%s' contains slices that escape local arena"

### 3. Per-function `returns_arena_slices` flag

Add the flag to `FunctionEntry`. Set it during `typecheck_function` when processing return statements. A function is marked `true` if any return path yields slices from arena ref parameters.

### 4. Deferred check list

A simple dynamic array of pending checks, processed after all functions are typechecked, before codegen.

### 5. Reset invalidation

The existing reset invalidation (S056) is per-function and flow-sensitive. It covers bare slices, tables, and slice-bearing tagged unions. The cross-function limitation (resetting a ref-param arena doesn't invalidate slices in the caller) is the same class of problem but less critical. The caller would need to use `arena_reset()` on its own local arena, which invalidates its own slices correctly. A callee resetting a ref-param arena is an unusual pattern and can remain a documented limitation.

---

## Examples

### Direct slice return (allowed after S048 removal)

```rust
func get_data(mut ref mem: Arena, n: i64): [i64] = {
    mut data: [i64] = arena_alloc(mut ref mem, n)
    return data   // OK, mem is ref param
}

func bad(): [i64] = {
    mut mem: Arena = arena(1024)
    mut data: [i64] = arena_alloc(mut ref mem, 10)
    return data   // ERROR, mem is local, slice escapes
}
```

### Indirect escape via function call (caught by deferred check)

```rust
func build(mut ref mem: Arena): Table = {
    mut keys: [i64] = arena_alloc(mut ref mem, 100)
    return Table { keys: keys }   // marks build as returns_arena_slices=true
}

func ok(mut ref mem: Arena): Table = {
    return build(mut ref mem)     // OK, mem is ref param in caller too
}

func bad(): Table = {
    mut mem: Arena = arena(8192)
    return build(mut ref mem)     // ERROR, mem is local, build returns arena slices
}
```

### Transitive chain (caught via propagation)

```rust
func level2(mut ref mem: Arena): [i64] = {
    return arena_alloc(mut ref mem, 10)           // direct: returns_arena_slices = true
}

func level1(mut ref mem: Arena): [i64] = {
    return level2(mut ref mem)              // dependency recorded: level1 depends on level2
}                                           // after propagation: returns_arena_slices = true

func bad(): [i64] = {
    mut mem: Arena = arena(1024)
    return level1(mut ref mem)              // ERROR, mem is local, level1 returns arena slices
}
```

### Slice-bearing tagged union escape

```rust
enum ReadResult { Ok([u8]), Err(i32) }

func read_data(mut ref mem: Arena): ReadResult = {
    mut buf: [u8] = arena_alloc(mut ref mem, 1024)
    ReadResult.Ok(buf)                      // marks read_data as returns_arena_slices=true
}

func ok(mut ref mem: Arena): ReadResult = {
    read_data(mut ref mem)                  // OK, mem is ref param
}

func bad(): ReadResult = {
    mut mem: Arena = arena(4096)
    return read_data(mut ref mem)           // ERROR, mem is local
}
```

### False negative that becomes a true negative

```rust
func analyze(mut ref mem: Arena, ref data: [f64]): Stats = {
    mut tmp: [f64] = arena_alloc(mut ref mem, 100)  // scratch, not returned
    return Stats { samples: data, count: 50 } // slices from data, not mem
}

func main(): void = {
    mut mem: Arena = arena(8192)
    mut big: Arena = arena(65536)
    mut raw: [f64] = arena_alloc(mut ref big, 1000)
    val s = analyze(mut ref mem, ref raw)     // OK, analyze.returns_arena_slices=false
}
```

---

## Known Limitation: Multiple Arena Parameters

With a single boolean per function, the analysis cannot distinguish *which* arena parameter the return slices come from:

```rust
func build(mut ref scratch: Arena, mut ref storage: Arena): Table = {
    mut tmp: [i64] = arena_alloc(mut ref scratch, 100)    // scratch only, not returned
    mut keys: [i64] = arena_alloc(mut ref storage, 100)   // returned
    return Table { keys: keys }
}

func caller(mut ref storage: Arena): Table = {
    mut scratch: Arena = arena(1024)                 // local
    return build(mut ref scratch, mut ref storage)
    // returns_arena_slices=true + scratch is local -> false positive
    // The returned slices come from storage (ref param), not scratch
}
```

Fixing this requires per-parameter flow tracking (mapping each arena parameter to whether it flows into the return value). This is significantly more complex and not needed for the common case of functions taking a single arena. If this pattern becomes common in practice, per-parameter tracking can be added later without breaking existing code. It only makes the analysis more precise (fewer false positives), never less safe.

---

## Safety Scope

With this spec fully implemented, Nore is **memory-safe for arena-scoped allocation**: dangling slices (scope exit and reset), buffer overflows, and use-after-free through the arena lifecycle are all caught at compile time or runtime.

### What Nore covers

| Guarantee | Mechanism | When |
|-----------|-----------|------|
| Dangling slices (scope exit) | Escape analysis, S053 | Compile time |
| Dangling slices (reset) | Flow-sensitive invalidation, S056 | Compile time |
| Buffer overflows | Bounds checking, R002 | Runtime |
| Double free | Arena auto-cleanup, no manual free | By design |
| Null pointers | Slices always from `arena_alloc`, no null concept | By design |
| Uninitialized memory | Arena allocs are zero-initialized, variables require initializers | By design |
| Reference escape | Refs exist only as function params, cannot be stored | By design |
| Integer overflow | `-fwrapv` flag, two's complement wrapping guaranteed | By design |

### Gaps compared to Rust

**1. Cross-function reset.** A callee that receives both `mut ref Arena` and a slice from that arena as separate parameters can reset the arena and invalidate the slice without the compiler noticing. This is a genuine use-after-free:

```rust
func process(mut ref mem: Arena, ref data: [i64]): void = {
    arena_reset(mut ref mem)   // invalidates data, compiler doesn't know
    val x = data[0]      // use-after-free
}
```

Closable with a conservative call-site check: warn when both a `mut ref Arena` and a slice allocated from that arena are passed to the same function.

**2. Mutable aliasing.** Two `mut ref` parameters can point to the same data. Rust prevents this entirely (one `&mut` OR many `&`, never both). Nore doesn't check:

```rust
func bad(mut ref a: [i64], mut ref b: [i64]): void = {
    a[0] = b[0]   // if a and b alias, this is surprising but not caught
}
bad(mut ref data, mut ref data)   // aliased mut refs
```

This is a **correctness** issue (surprising behavior) rather than a **safety** issue (no memory corruption or undefined behavior in Nore's model). Rust prevents it for both correctness and to enable `noalias` optimizations.

### Bottom line

Nore's safety model is **domain-specific**: it covers the class of bugs that arena-based systems languages face in practice. This includes dangling pointers, buffer overflows, and use-after-free through scope and reset. It is substantially safer than C, with a much simpler mental model than Rust. It is not a universal aliasing and lifetime proof like Rust's borrow checker, but it targets the problems that matter most for data-oriented design.

