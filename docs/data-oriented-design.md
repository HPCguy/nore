# Data-Oriented Design in Nore

## Goal

Nore should naturally guide developers toward data locality and cache-friendly patterns while remaining general-purpose. The language is opinionated: it makes the DOD path the path of least resistance, and makes the cost of indirection always visible.

## Core Principle

**The developer should always know where data lives and what it costs to access.**

No hidden allocations, no implicit indirection, no false sense of locality.

---

## The Type Model: `value`, `struct`, and `enum`

Every composite type in Nore falls into one of three categories: `value`, `struct`, or `enum`. Tables are built on top of `value` and `struct` as syntactic sugar, not as a separate concept.

### `value` — The Composable Building Block

A `value` is a fixed-size, fully inline data type. It contains only bytes, no pointers, no slices, no indirection of any kind. Values are the atoms of data layout.

```nore
value Vec2 { x: f64, y: f64 }
value Color { r: u8, g: u8, b: u8, a: u8 }
value Matrix4 { elements: [f64; 16] }
```

**Rules:**
- Fields can be: scalars, fixed-size arrays, or other `value` types
- Slices, references, and any form of indirection are compile errors
- Copy semantics, always safe to memcpy
- Can be passed by value or by `ref` to functions
- Can be embedded anywhere: in other values, in structs, in arrays

A `value` is what you reach for when you want data that composes, inlines, and iterates efficiently.

### `struct` — The Top-Level Resource Owner

A `struct` is for types that manage resources or contain indirection. It may hold slices pointing into arenas, dynamic data handles, or other indirect state. Structs are not building blocks. They are top-level containers.

```nore
struct Mesh {
    vertices: [f64],      // slice into an arena
    indices: [u32],       // slice into an arena
    vertex_count: i64,
}

struct Arena { ... }      // manages a block of heap memory
```

**Rules:**
- Fields can be: scalars, fixed-size arrays, `value` types, and slices
- **Cannot contain other structs** or non-copyable types
- **Cannot be passed by value**, only by `ref`
- **Cannot be stored as a `ref`**, references are not storable (see References)
- Exists only as: local variable, global variable, or function `ref` parameter

### `enum` — Tagged Values and Tagged Unions

An `enum` is a named set of variants, optionally carrying data. Plain enums (no payloads) are pure value types. Tagged unions (variants with payloads) can be either value-like or struct-like depending on their payloads.

```nore
// Plain enum: pure value type, copyable
enum Color { Red, Green, Blue }

// Tagged union with value payloads: still a value type, copyable
enum Option { Some(i64), None }

// Tagged union with slice payload: non-copyable (like a struct with slices)
enum ReadResult { Ok([u8]), Err(i32) }
```

**Rules:**
- Plain enums and value-payload tagged unions follow value semantics (copyable, passable by value)
- Tagged unions with any slice payload become non-copyable: must use `ref` for parameters (S044), cannot be copied from another variable (S043), cannot be embedded in value types (S045)
- Payload types must be value-compatible: scalars, fixed arrays, value types, slices, or plain enums. Structs and Arena are not allowed as payloads (S082)
- Destructured via `match` expressions, which provide exhaustive pattern matching

Slice-bearing tagged unions fill an important role: they let functions return success-or-error results that carry arena-allocated data, without needing a separate struct definition.

```nore
enum ReadResult { Ok([u8]), Err(i32) }

func read_file(mut ref mem: Arena, ref path: str): ReadResult = {
    // ... returns ReadResult.Ok(data) or ReadResult.Err(code)
}

val result: ReadResult = read_file(mut ref mem, ref path)
val content: [u8] = match (result) {
    Ok(data) = { data }
    Err(code) = { empty }
}
```

### Why Structs Don't Compose

If structs could be nested inside other structs, you'd get:
- Hidden resource ownership chains (who owns the arena inside the struct inside the struct?)
- Implicit copy semantics for types that shouldn't be copied (copying an Arena aliases its memory)
- Complex lifetime dependencies between nested containers

By forbidding struct nesting, Nore keeps ownership flat and explicit. A struct lives where it's declared (local, global, or parameter) and its lifetime is always obvious.

### Why Structs Can't Be Passed by Value

A struct may own resources (an Arena owns a heap block, a file handle owns an OS resource). Copying such types is either expensive or semantically wrong. Forcing `ref`-only parameter passing means:
- No accidental copies of resource owners
- The caller always knows the struct lives somewhere specific
- The struct's lifetime is always tied to the declaring scope

### The Type Roles

| Capability              | `value`                  | `struct`                 | `enum` (plain/value)     | `enum` (slice-bearing)   |
|-------------------------|--------------------------|--------------------------|--------------------------|--------------------------|
| Embed in `value`        | yes                      | **no**                   | yes                      | **no**                   |
| Embed in `struct`       | yes                      | **no**                   | yes                      | **no**                   |
| Pass by value           | yes                      | **no** (ref only)        | yes                      | **no** (ref only)        |
| Pass by `ref`           | yes                      | yes                      | yes                      | yes                      |
| Store as local/global   | yes                      | yes                      | yes                      | yes                      |
| May contain slices      | **no**                   | yes                      | **no**                   | yes (as payloads)        |
| Copy semantics          | memcpy (always safe)     | not copyable             | memcpy (always safe)     | not copyable             |

`value` is what data is made of. `struct` is what manages data. `enum` is for choices and result types.

---

## `table` — Syntactic Sugar for Columnar Storage

A `table` is **not** a separate fundamental type. It is syntactic sugar that generates a `struct` (for columnar storage) plus a `value` (for row access). The model remains: `value`, `struct`, and `enum`.

### What a Table Declaration Generates

```nore
table Particles {
    pos: Vec2,
    life: i64,
    color: Color,
}
```

The compiler generates two types:

**1. A `struct` for columnar storage:**

```nore
struct Particles {
    pos: [Vec2],          // column slice
    life: [i64],          // column slice
    color: [Color],       // column slice
    _len: i64,            // row count
}
```

**2. A `value` for single-row access:**

```nore
value Particles.Row {
    pos: Vec2,
    life: i64,
    color: Color,
}
```

### Why This Is Unified

- `Particles` is a `struct`, passed by `ref`, not nestable, not copyable
- `Particles.Row` is a `value`, copyable, composable, can be embedded in other values

No extra concept. The `table` keyword is convenience that emits the two types we already have.

### Table Field Constraints

Table fields must be `value`-compatible: scalars, fixed arrays, or other value types. This falls directly from the model: the generated `.Row` type must be a valid `value`, so its fields cannot contain indirection.

```nore
table Enemies {
    health: i64,          // ok: scalar
    pos: Vec2,            // ok: value type
    name: str,            // error: str is a slice, not allowed
}
```

For strings and other indirect data, use indices:

```nore
table Enemies {
    health: i64,
    pos: Vec2,
    name_id: u32,         // index into a string table
}
```

### Using Tables

```nore
table Particles {
    pos: Vec2,
    life: i64,
    color: Color,
}

mut mem: Arena = arena(1024 * 1024)
mut particles: Particles = table_alloc(mut ref mem, 1000)    // allocates 3 columns

// Column access, contiguous and cache-friendly
for i in 0..table_len(ref particles) {
    particles.life[i] = particles.life[i] - 1
}

// Row access, returns Particles.Row (a value)
val row: Particles.Row = table_get(ref particles, 0)

// Insert a row from a value
table_insert(mut ref particles, Particles.Row { pos: Vec2 { x: 0.0, y: 0.0 }, life: 100, color: ... })
```

### Tables Follow Struct Rules

Because a table instance is a struct:

- **Cannot be embedded in another struct or value**, struct rule
- **Cannot be passed by value**, struct rule, only `ref`
- **Exists as local, global, or `ref` parameter**, struct rule

```nore
func update_particles(ref p: Particles): void = {
    // ...
}

struct Bad {
    particles: Particles,    // error: cannot embed struct in struct
}
```

### The Mental Model

- `table` declares a schema
- The compiler generates a `struct` (columnar container) + `value` (row type)
- You write one declaration, get both SoA storage and AoS row access
- Everything follows from the `value`/`struct` distinction

---

## Arrays and Slices

The type model determines where arrays and slices can appear.

### Fixed-Size Arrays — Inline, Everywhere

Fixed-size arrays are `value`-compatible. They have a known size at compile time and contain no indirection.

```nore
value Matrix4 { elements: [f64; 16] }     // in a value: ok
val positions: [Vec2; 100] = ...           // standalone: ok, stack-allocated
```

Fixed arrays can appear in values, structs, and as standalone variables.

### Slices — Indirect, Struct-Only (and Tagged Union Payloads)

A slice (`[T]`) is a fat pointer: `{pointer, length}`. It contains indirection, the data lives elsewhere (in an arena). Therefore:

```nore
struct Image {
    pixels: [u8],        // ok: structs allow indirection
    width: i64,
    height: i64,
}

value Bad {
    data: [f64],         // error: slices not allowed in value
}
```

Slices are allowed in structs, as tagged union payloads, and as standalone variables. They are banned from values because values guarantee no indirection.

---

## Strings

Strings follow the same rules as arrays, because a string is a byte slice.

### `str` Is a Slice

`str` is `{pointer, length}`, a slice of bytes. It contains indirection, so:

- Allowed in `struct` fields, tagged union payloads, and standalone variables
- **Not allowed** in `value` types
- Fixed byte buffers `[u8; N]` are allowed everywhere (they're fixed arrays, not slices)

### String Literals Are Static

```nore
val greeting: str = "hello"    // points to static memory, zero cost
```

String literals live in static memory for the program's lifetime. No arena needed.

### Dynamic Strings Need Arenas

```nore
mut mem: Arena = arena(4096)
mut name: [u8] = arena_alloc(mut ref mem, 11)
// fill name bytes...
```

The developer always sees where string data comes from. No hidden heap allocation.

### Strings in Tables: Use Indices

Since `str` is a slice and table fields must be `value`-compatible, strings cannot be table columns. Use a scalar index:

```nore
table Enemies {
    health: i64,
    name_id: u32,            // index into a string table
}

// Resolve the name when needed:
val name: str = table_get(ref name_table, enemies.name_id[i])
```

---

## References

### Design: Calling Convention, Not Data Modeling Tool

References exist **only as function parameters**. They cannot be stored, not in variables, not in values, not in structs.

```nore
// Allowed: reference as function parameter
func normalize(mut ref v: Vec2): void = {
    val len: f64 = sqrt(v.x * v.x + v.y * v.y)
    v.x = v.x / len
    v.y = v.y / len
}

// NOT allowed:
val r: ref Vec2 = ...            // error: cannot store reference
struct Bad { v: ref Vec2 }       // error: cannot store reference
```

This applies to all type kinds:
- `value` types can be passed by value or by `ref` (developer's choice based on size)
- `struct` types can **only** be passed by `ref` (enforced by the language)
- Slice-bearing tagged unions can **only** be passed by `ref` (same rule as structs)
- Plain enums can be passed by value or by `ref`

### Why This Restriction

References as a calling convention means:
- Zero-copy function parameters (no 128-byte Matrix4 copies in hot loops)
- Mutation through functions without return-and-reassign boilerplate
- No aliasing in data structures, ownership is always clear
- No lifetime annotations, no borrow checker: references can't escape the call
- Values and structs stay clean by construction

**For graphs/trees:** use indices into tables, the DOD-canonical approach.
**For C interop:** a separate `unsafe` pointer type, explicitly marked, not mixed with the safe system.

### The Mental Model

References are a **performance optimization for function calls**, not a way to model relationships between data. Relationships are modeled with indices.

---

## Arenas: Compile-Time Safe Dynamic Allocation

### The Problem

Dynamic data (strings, variable-length arrays, table columns) needs heap memory. Most languages hide this behind implicit allocation. Nore makes allocation explicit: **every dynamic allocation names its arena.**

### Arena Is a Struct

An arena is a `struct`. It manages a heap-allocated block of memory. Because it's a struct, all struct rules apply automatically:

- **Cannot be embedded in another struct**, struct rule
- **Cannot be passed by value**, struct rule, only `ref`
- **Cannot be stored as a ref**, reference rule

These aren't special Arena restrictions. They're the natural consequences of the type model.

### Where Arenas Can Live

Since structs exist only as local variables, global variables, or function `ref` parameters, arenas have exactly three lifetimes:

| Arena location       | Lifetime           |
|----------------------|--------------------|
| Local variable       | Enclosing scope    |
| Function `ref` param | Caller's scope     |
| Global variable      | Program            |

No other cases exist. No nested ownership. No lifetime chains.

### Arena Basics

```nore
// Create an arena with explicit capacity
mut scratch: Arena = arena(4096)

// Allocate dynamic data from it
mut buf: [u8] = arena_alloc(mut ref scratch, 100)
mut points: [f64] = arena_alloc(mut ref scratch, 100)

// Free everything at once
arena_reset(mut ref scratch)
```

Every slice knows which arena it came from. There is no global heap, no invisible `malloc`.

### Compile-Time Slice Validity

Slices cannot outlive their arena. The compiler enforces this by tracking which arena a slice belongs to and whether that arena is still alive.

This is **not** a full borrow checker. There are no per-reference lifetimes, no mutable/immutable borrow distinctions. Just one rule: **slices die when their arena dies.** And since arenas can only be locals, parameters, or globals, lifetime checking reduces to scope nesting, something the compiler already does.

For the full escape analysis design, including cross-function tracking, transitive propagation, and safety guarantees, see [arena-safety.md](arena-safety.md).

#### Local Arenas — Slices Bound to Scope

```nore
func process(): void = {
    mut scratch: Arena = arena(4096)
    mut s: [u8] = arena_alloc(mut ref scratch, 32)
    // s is valid here, scratch is alive
}   // scratch dies, s dies, compiler enforces this
```

#### Returning Slices From Local Arenas — Compile Error

```nore
func bad(): [u8] = {
    mut tmp: Arena = arena(256)
    mut data: [u8] = arena_alloc(mut ref tmp, 10)
    return data    // error: slice outlives arena 'tmp'
}
```

#### Passing Arenas to Functions

A function receives an arena by `ref` (because arenas are structs). Slices allocated from it are valid in the caller's scope:

```nore
func build_greeting(mut ref a: Arena): str = {
    mut buf: [u8] = arena_alloc(mut ref a, 11)
    // fill buf...
    return buf
}

func main(): void = {
    mut mem: Arena = arena(4096)
    val greeting: str = build_greeting(mut ref mem)
    // greeting is valid, mem is still alive in this scope
}
```

The compiler tracks: `greeting` came from `mem` (passed as `ref a`), and `mem` outlives `greeting`.

#### Global Arenas — Program Lifetime

For long-lived data that outlives any single function:

```nore
// Module-level: alive for the entire program
mut level_mem: Arena = arena(1024 * 1024)
mut name_mem: Arena = arena(4096)

func load_level(): void = {
    mut geometry: [f64] = arena_alloc(mut ref level_mem, 10000)
    // geometry valid for the program, level_mem is global
}
```

### Arena Reset Invalidation

When `arena_reset()` is called on an arena, the compiler treats all slices from that arena as invalid from that point forward. This also covers tables and slice-bearing tagged unions allocated from that arena:

```nore
mut a: Arena = arena(1024)
mut s: [u8] = arena_alloc(mut ref a, 32)
// use s...
arena_reset(mut ref a)
// using s here is a compile error: slice 's' invalidated by arena reset
```

This is a flow-sensitive check: the compiler tracks `arena_reset()` calls and rejects use of any slice sourced from that arena after the reset.

### Tables and Arenas

Since tables are structs that hold column slices, they need arena memory:

```nore
table Particles {
    pos: Vec2,
    life: i64,
}

mut game_mem: Arena = arena(1024 * 1024)
mut particles: Particles = table_alloc(mut ref game_mem, 1000)
// particles.pos and particles.life are slices into game_mem
```

The table's slices are valid as long as the arena is alive. Same rule as any other slice.

### Multiple Arenas, Clear Lifetimes

Different arenas for different lifetimes, the developer decides:

```nore
// Global: long-lived data
mut level_mem: Arena = arena(1024 * 1024)
mut name_mem: Arena = arena(4096)

func game_loop(): void = {
    // Local: per-frame temporary data
    mut frame_mem: Arena = arena(8192)
    mut debug_label: [u8] = arena_alloc(mut ref frame_mem, 64)
    // ... process ...
    arena_reset(mut ref frame_mem)
    // using debug_label here would be a compile error: arena was reset
}
```

### The Allocator Hierarchy

| Layer          | What lives here                        | Lifetime        |
|----------------|----------------------------------------|-----------------|
| Stack          | `value` types, fixed arrays, locals    | Function scope  |
| Arena (local)  | Slices, strings, table columns         | Enclosing scope |
| Arena (global) | Slices, strings, table columns         | Program         |
| Static         | String literals, program-lifetime data | Program         |

No general-purpose heap by default. For `malloc`-style allocation: use a global arena that never resets, or an explicit `unsafe` allocator for C interop.

### Design Rationale

**Why compile-time over runtime?** Runtime checks (like Zig's debug allocator) catch bugs during testing but miss them in production. Compile-time enforcement means dangling slices are impossible, not just unlikely.

**Why arenas over GC or borrow checker?** Arenas match DOD workloads (batch allocate, batch free). A garbage collector hides allocation cost. A full borrow checker (Rust) adds language complexity disproportionate to the problem when arenas are the primary allocation pattern.

**Why not a global heap?** A global heap makes it easy to scatter allocations across memory, defeating locality. Arenas group related data together by construction. If every slice points into a known contiguous region, even indirect access has better cache behavior than random heap allocations.

---

## Design Rationale Summary

### Why `value` Must Be Explicit

The compiler could infer which types are "plain data." But implicit inference creates fragility:

- Adding a `str` field to a type months later silently breaks every usage
- Errors appear far from the change

With explicit `value`:
- Adding `str` to a `value` produces an immediate compile error **at the definition**
- The guarantee lives where the data is defined
- Reading the code, you see the contract without checking every field

### Why `table` Is Sugar, Not a Primitive

If `table` were a separate fundamental type, we'd need to define:
- How it composes (it doesn't, same as struct)
- How it's passed (by ref, same as struct)
- Where it can live (local, global, param, same as struct)

These answers are identical to struct rules. Rather than duplicate the rules, `table` generates a struct. The model stays simple.

### Why Tagged Unions Bridge the Gap

Slice-bearing tagged unions fill a role that neither `value` nor `struct` covers well: returning success-or-error results that may carry arena-allocated data. Without them, every function returning dynamic data needs a separate struct type plus an out-parameter for error status. Tagged unions like `enum ReadResult { Ok([u8]), Err(i32) }` express this naturally while following the same non-copyable rules as structs.

### Why the Type Model Works

Three type kinds, clear roles:
- **`value`**: composable, copyable, inline, no indirection. The building block.
- **`struct`**: top-level, not composable, ref-only, may own resources. The container.
- **`enum`**: choices and result types. Value-like when payloads are simple, struct-like when payloads contain slices.

Every other construct in the language (tables, arenas, slices, function passing) follows from these distinctions. The `table` keyword is convenience, not complexity.

---

## Open Questions

- Table operations beyond `table_alloc`, `table_len`, `table_get`, `table_insert` (remove, iterate, filter)
- `unsafe` pointer type design for C interop
- Whether `value` types should support methods or only free functions
- Small-string optimization: should `str` have an inline variant?
- Arena growth policy: fixed size and panic, or growable?
- Typed arenas (`Arena<f64>`) vs generic byte arenas
- Table capacity management: fixed at creation, or growable within arena?
