# The Nore Programming Language

Nore is a systems programming language that makes data-oriented design the path of least resistance. Instead of hiding memory layout behind objects, Nore gives you direct control over how data is organized: columnar tables, arena allocation, explicit value vs resource semantics. All with compile-time safety guarantees and zero runtime overhead.

The compiler is a self-contained, single-file C program that translates Nore source code into native executables via C as an intermediate representation.

```nore
value Vec2 { x: f64, y: f64 }

table Particles {
    pos: Vec2,
    life: i64
}

func main(): void = {
    mut mem: Arena = arena(65536)
    mut p: Particles = table_alloc(mut ref mem, 1000)

    table_insert(mut ref p, Particles.Row {
        pos: Vec2 { x: 1.0, y: 2.0 },
        life: 100
    })

    for i in 0..table_len(ref p) {
        p.life[i] = p.life[i] - 1
    }
}
```

For the complete syntax quick-reference, see [syntax.md](syntax.md).

---

## Philosophy

### The Core Principle

**The developer should always know where data lives and what it costs to access.**

No hidden allocations, no implicit indirection, no false sense of locality. Nore is opinionated: it makes the data-oriented path the path of least resistance, and makes the cost of indirection always visible.

### Thinking in Tables, Not Trees

Most languages default to trees of objects: a node *contains* its children, each allocated somewhere on the heap. It maps to how we naturally think about hierarchies. But it scatters data across memory, and every child access is a pointer chase.

Nore nudges you toward a different shape: flat tables where relationships are indices, not pointers. A compiler AST becomes a table of nodes with a `parent_id` column. A scene graph becomes a table of entities with a `parent` index. Children are not *inside* the parent; they are rows in the same table that *reference* it.

```
Traditional (tree of objects)       Nore (table with relationships)

  Scene                             Entities table
  +-- Player                        +----+----------+-----------+
  |   +-- pos: Vec2                 | id | name     | parent_id |
  |   +-- health: 100               +----+----------+-----------+
  |   +-- Sword                     |  0 | Player   |        -1 |
  |       +-- damage: 50            |  1 | Sword    |         0 |
  +-- Enemy                         |  2 | Enemy    |        -1 |
      +-- pos: Vec2                 |  3 | Shield   |         2 |
      +-- Shield                    +----+----------+-----------+
          +-- armor: 30
                                    Flat, sequential, cache-friendly.
  Nested pointers, scattered        Relationships are just indices.
  across the heap.
```

The mind shift: instead of "this object contains its data," you think "data lives in tables, and relationships are just columns." The flat layout gives you sequential memory access, easy serialization, and straightforward parallelism.

If your natural model is trees of objects and your dataset is small, Nore will feel like unnecessary ceremony. It is not the right tool for everything, and that is okay.

### Design Commitments

- **Explicit allocation.** Every dynamic allocation names its arena. No global heap, no hidden malloc.
- **Visible mutability.** `ref` and `mut ref` appear at both declaration and call site. You see what can change.
- **No hidden copies.** Types that own resources cannot be copied. The type system enforces this.
- **Compile-time safety.** Slices cannot outlive their arena. The compiler checks this, not a runtime garbage collector.

---

## The Type Model

Every composite type in Nore falls into one of three categories: `value`, `struct`, or `enum`. Tables are built on top of `value` and `struct` as syntactic sugar, not as a separate concept.

### Values: The Composable Building Block

A `value` is a fixed-size, fully inline data type. It contains only bytes: no pointers, no slices, no indirection of any kind. Values are the atoms of data layout.

```nore
value Vec2 { x: f64, y: f64 }
value Color { r: u8, g: u8, b: u8, a: u8 }
value Matrix4 { elements: [f64; 16] }
```

**Rules:**
- Fields can be: scalars, fixed-size arrays, or other `value` types
- Slices, references, and any form of indirection are compile errors
- Copy semantics: always safe to memcpy
- Can be passed by value or by `ref` to functions
- Can be embedded anywhere: in other values, in structs, in arrays, in tables

A `value` is what you reach for when you want data that composes, inlines, and iterates efficiently.

**Why explicit `value`?** The compiler could infer which types are "plain data." But implicit inference creates fragility: adding a `str` field to a type months later silently breaks every usage, and errors appear far from the change. With explicit `value`, the guarantee lives where the data is defined. Reading the code, you see the contract without checking every field.

**Constructors, field access, assignment:**

```nore
val p: Vec2 = Vec2 { x: 1.0, y: 2.0 }    // all fields required, any order
val x: f64 = p.x                           // dot notation
mut q: Vec2 = Vec2 { x: 0.0, y: 0.0 }
q.x = 3.0                                  // field assignment on mut variables
```

**Value semantics:** values are assigned by copy. Copies are independent:

```nore
val a: Vec2 = Vec2 { x: 1.0, y: 2.0 }
mut b: Vec2 = a       // copy, not reference
b.x = 99.0            // does not affect a
assert a.x == 1.0     // a is unchanged
```

### Structs: Top-Level Resource Owners

A `struct` is for types that manage resources or contain indirection. It may hold slices pointing into arenas, dynamic data handles, or other indirect state. Structs are not building blocks. They are top-level containers.

```nore
struct Mesh {
    vertices: [f64],      // slice into an arena
    indices: [u32],       // slice into an arena
    vertex_count: i64,
}
```

**Rules:**
- Fields can be: scalars, fixed-size arrays, `value` types, and slices
- Cannot contain other structs or non-copyable types
- Cannot be passed by value, only by `ref` or `mut ref`
- Cannot be stored as a `ref` (references are not storable)
- Exists only as: local variable, global variable, or function `ref` parameter

**Why structs cannot be nested.** If structs could contain other structs, you would get hidden resource ownership chains, implicit copy semantics for types that should not be copied, and complex lifetime dependencies between nested containers. By forbidding struct nesting, Nore keeps ownership flat and explicit.

**Why structs cannot be passed by value.** A struct may own resources (an Arena owns a heap block). Copying such types is either expensive or semantically wrong. Forcing `ref`-only passing means no accidental copies of resource owners, and the struct's lifetime is always tied to the declaring scope.

**No copy semantics:**

```nore
val a: Entity = Entity { x: 1.0, y: 2.0, health: 100 }
val b: Entity = a    // ERROR S043: Cannot copy struct
```

Structs can only be initialized from constructors or function return values. They cannot be copied from another variable or reassigned after initialization.

**Ref-only passing:**

```nore
func get_health(ref e: Entity): i64 = { return e.health }
func damage(mut ref e: Entity, amount: i64): void = {
    e.health = e.health - amount
}
```

Passing a struct by value is an error (S044). Call site must match: `ref e` or `mut ref e`.

**Returning structs** (direct init, not copy):

```nore
func make_entity(x: f64, y: f64, hp: i64): Entity = {
    return Entity { x: x, y: y, health: hp }
}
val e: Entity = make_entity(1.0, 2.0, 100)
```

### Enums: Choices and Result Types

An `enum` is a named set of variants, optionally carrying data. Plain enums (no payloads) are pure value types. Tagged unions (variants with payloads) can be value-like or struct-like depending on their payloads.

**Plain enums** are named integer constants:

```nore
enum Color { Red, Green, Blue }

val c: Color = Color.Red
assert c == Color.Red       // == and != only, no ordering
assert i64(Color.Blue) == 2 // cast to integer
```

**Tagged unions** carry data payloads. Plain and data variants can be mixed:

```nore
enum Option { Some(i64), None }
enum ReadResult { Ok([u8]), Err(i32) }
enum Mixed { A(bool), B, C(f64) }
```

**Construction:**

```nore
val x: Option = Option.Some(42)     // data variant: requires (expr)
val y: Option = Option.None          // non-data variant: bare syntax
```

Type annotation is required. Tagged unions can be assigned to mutable variables and used as function parameters and return types.

**Semantics by payload type:**
- Value-payload tagged unions (like `Option`) follow value semantics: copyable, passable by value.
- Slice-payload tagged unions (like `ReadResult`) become non-copyable (S043), must use `ref` for parameters (S044), and cannot be embedded in value types (S045).
- Payload types must be value-compatible: scalars, fixed arrays, value types, slices, or plain enums. Structs and Arena are not allowed (S082).
- Comparison (`==`, `!=`) is not allowed on tagged unions (use `match` instead).

**Match expressions** destructure tagged unions with exhaustive pattern matching:

```nore
// As statement
match (opt) {
    Some(n) = { result = n }
    None = { result = 0 }
}

// As expression (val/mut initializer or function body value)
val x: i64 = match (opt) {
    Some(n) = { n }
    None = { 0 }
}

func unwrap_or(opt: Option, default: i64): i64 = {
    match (opt) {
        Some(n) = { n }
        None = { default }
    }
}
```

Match rules:
- `match (scrutinee) { arms }` with parentheses around the scrutinee
- Each arm: `VariantName(binding) = { body }` or `VariantName = { body }` for non-data variants
- `=` separates pattern from body (consistent with function syntax)
- Variant names are unqualified (the scrutinee type determines the enum)
- `_` wildcard for unused bindings: `Some(_) = { ... }`
- Exhaustiveness required: all variants must be covered
- Binding variables are scoped to the arm body
- All arms must produce values of compatible types when used as expression (S076)

**Why tagged unions bridge the gap.** Slice-bearing tagged unions fill a role that neither `value` nor `struct` covers well: returning success-or-error results that may carry arena-allocated data. Without them, every function returning dynamic data needs a separate struct type plus an out-parameter for error status. `ReadResult { Ok([u8]), Err(i32) }` expresses this naturally.

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

### Tables: Columnar Storage as Sugar

A `table` is not a separate fundamental type. It is syntactic sugar that generates a `struct` (for columnar storage) plus a `value` (for row access).

```nore
table Particles {
    pos: Vec2,
    life: i64,
}
```

This generates two types:

1. **A struct** `Particles` with slice columns: `pos: [Vec2]`, `life: [i64]`, `_len: i64`
2. **A value** `Particles.Row` with the original fields: `pos: Vec2`, `life: i64`

Table field constraints: fields must be value-compatible (scalars, fixed arrays, or value types). No slices, no structs, no Arena. For strings and other indirect data, use scalar indices into a separate table.

```nore
mut mem: Arena = arena(65536)
mut p: Particles = table_alloc(mut ref mem, 1000)

// Insert rows
table_insert(mut ref p, Particles.Row { pos: Vec2 { x: 1.0, y: 2.0 }, life: 100 })

// Row access (returns a value copy)
val row: Particles.Row = table_get(ref p, 0)

// Column access (cache-friendly iteration)
for i in 0..table_len(ref p) {
    p.life[i] = p.life[i] - 1
}
```

Because the table instance is a struct, all struct rules apply: cannot be copied, cannot be nested, must be passed by `ref`. The `.Row` type is a value: copyable, composable, embeddable. No extra concept. The `table` keyword is convenience, not complexity.

---

## Memory Model

Nore has four places where data can live:

| Layer          | What lives here                        | Lifetime        |
|----------------|----------------------------------------|-----------------|
| Stack          | `value` types, fixed arrays, locals    | Function scope  |
| Arena (local)  | Slices, strings, table columns         | Enclosing scope |
| Arena (global) | Slices, strings, table columns         | Program         |
| Static         | String literals, program-lifetime data | Program         |

No general-purpose heap by default.

### Arrays: Inline, Fixed Size

Fixed-size arrays are value-compatible. They have a known size at compile time and contain no indirection.

```nore
val arr: [i64; 3] = [1, 2, 3]
val grid: [[i64; 2]; 3] = [[1, 2], [3, 4], [5, 6]]
value Matrix4 { elements: [f64; 16] }
```

- Size must be a positive integer literal
- Indexing: `arr[i]`, `grid[1][0]` (nested), bounds-checked at runtime (R002)
- Chains with field access: `v.data[i]`
- Element assignment: `arr[0] = 99` (root variable must be `mut`)
- Value semantics: assigned by copy, passed by copy
- Can appear in values, structs, and as standalone variables
- Sub-slicing: `arr[1..4]` produces a slice

```nore
val a: [i64; 3] = [1, 2, 3]
mut b: [i64; 3] = a       // copy, not reference
b[0] = 99                 // does not affect a
assert a[0] == 1           // a is unchanged
```

### Slices: Fat Pointers into Arenas

A slice (`[T]`) is a fat pointer: `{pointer, length}`. The data lives elsewhere, in an arena. Because slices contain indirection:

```nore
struct Image {
    pixels: [u8],        // ok: structs allow indirection
    width: i64,
}

value Bad {
    data: [f64],         // ERROR: slices not allowed in value
}
```

Slices are allowed in structs, as tagged union payloads, and as standalone variables. They are banned from values because values guarantee no indirection.

**Slice parameters** must use `ref` or `mut ref`:

```nore
func sum(ref data: [i64]): i64 = {
    mut total: i64 = 0
    for i in 0..data.len { total = total + data[i] }
    return total
}
```

**Call-site coercion** (array to slice): fixed-size arrays coerce to slices at `ref`/`mut ref` call sites:

```nore
val a: [i64; 3] = [1, 2, 3]
val b: [i64; 5] = [10, 20, 30, 40, 50]
assert sum(ref a) == 6       // [i64; 3] coerces to [i64]
assert sum(ref b) == 150     // [i64; 5] coerces to [i64]
```

**Mutable slices:**

```nore
func double_all(mut ref data: [i64]): void = {
    for i in 0..data.len { data[i] = data[i] * 2 }
}
mut arr: [i64; 3] = [1, 2, 3]
double_all(mut ref arr)
assert arr[0] == 2
```

**Slice passthrough:** a slice parameter can be passed directly to another function expecting a slice:

```nore
func first(ref data: [i64]): i64 = { return data[0] }
func get_first(ref data: [i64]): i64 = { return first(ref data) }
```

**Slice operations:**
- `.len` returns the element count as `i64`
- Indexing: `data[i]`, bounds-checked at runtime (R002)
- Sub-slicing: `data[2..5]`, `data[..3]`, `data[2..]`, `data[..]` (always produces a slice, half-open range, bounds-checked R004)
- Sub-slices can be chained: `data[1..4][0]`
- Sub-slices can be passed directly: `sum(ref arr[1..4])`

**Slice locals** must be initialized via `arena_alloc()`, a function call returning a slice, or a sub-slice expression (S046). Copying a slice variable to another is an error:

```nore
mut mem: Arena = arena(4096)
val data: [i64] = arena_alloc(mut ref mem, 10)    // from arena
val result: [i64] = get_data(mut ref mem, 5)       // from function call
val sub: [i64] = data[2..5]                        // from sub-slicing
val bad: [i64] = data                              // ERROR S046
```

### Strings

`str` is a byte slice (`[u8]`). String literals create fat pointers to static memory at zero cost.

```nore
val greeting: str = "hello"    // points to static memory, no arena needed
val h: u8 = greeting[0]       // 104 (ASCII 'h')
assert greeting.len == 5
```

- String literals must be bound with `val` (immutable). `mut` is an error (S054).
- Dynamic strings need arenas (allocate `[u8]` and fill it).
- `str` and `[u8]` are interchangeable.
- Strings in tables: use scalar indices into a string table (since `str` is a slice, it cannot be a table field).

### References: A Calling Convention

References exist **only as function parameters**. They cannot be stored: not in variables, not in values, not in structs.

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

- `ref` (read-only): generates `const T *` in C
- `mut ref` (read-write): generates `T *` in C
- Call-site syntax must match: `ref p` for `ref` params, `mut ref q` for `mut ref` params
- Argument must be addressable (variable or field-access chain)
- `mut ref` requires the root variable to be `mut`
- Cannot take ref of scalar fields (`i64`, `i32`, `u8`, `u32`, `f64`, `bool`). Just copy them.
- Cannot take ref of array elements (use slices instead)
- `value` types can be passed by value or by `ref` (developer's choice based on size)
- `struct` types can only be passed by `ref` (enforced by the language)
- Slice-bearing tagged unions can only be passed by `ref` (same rule as structs)

**References are a performance optimization for function calls, not a way to model relationships between data.** For graphs and trees: use indices into tables.

### Arenas: Explicit Dynamic Allocation

Dynamic data (strings, variable-length arrays, table columns) needs heap memory. Most languages hide this behind implicit allocation. Nore makes allocation explicit: every dynamic allocation names its arena.

An arena is a `struct`. It manages a heap-allocated block of memory. Because it is a struct, all struct rules apply: cannot be nested, cannot be passed by value, cannot be stored as a ref.

**Where arenas can live:**

| Arena location       | Lifetime           |
|----------------------|--------------------|
| Local variable       | Enclosing scope    |
| Function `ref` param | Caller's scope     |
| Global variable      | Program            |

No other cases exist. No nested ownership. No lifetime chains.

**Arena basics:**

```nore
// Create with explicit capacity
mut scratch: Arena = arena(4096)

// Allocate from it (zero-initialized)
mut buf: [u8] = arena_alloc(mut ref scratch, 100)
mut points: [f64] = arena_alloc(mut ref scratch, 100)

// Free everything at once
arena_reset(mut ref scratch)
// Using buf or points here is a compile error (S056)
```

**Passing arenas to functions:**

```nore
func build_greeting(mut ref a: Arena): str = {
    mut buf: [u8] = arena_alloc(mut ref a, 11)
    // fill buf...
    return buf    // OK: a is ref param, arena outlives callee
}

func main(): void = {
    mut mem: Arena = arena(4096)
    val greeting: str = build_greeting(mut ref mem)
    // greeting is valid: mem is still alive
}
```

**Global arenas** have program lifetime:

```nore
mut level_mem: Arena = arena(1024 * 1024)

func load_level(): void = {
    mut geometry: [f64] = arena_alloc(mut ref level_mem, 10000)
    // geometry valid for the program
}
```

**Automatic cleanup:** Arena memory is freed when the arena goes out of scope. On `return`, all local arenas are freed. On `break`/`continue`, arenas inside the loop body are freed. Arena ref parameters are not freed by the callee (the caller owns them).

**Why arenas over GC or borrow checker?** Arenas match data-oriented workloads (batch allocate, batch free). A garbage collector hides allocation cost. A full borrow checker adds language complexity disproportionate to the problem when arenas are the primary allocation pattern. Arenas group related data together by construction, so even indirect access has better cache behavior than random heap allocations.

---

## Functions and Modules

### Functions

```nore
func name(param1: type1, param2: type2): returnType = {
    body
}
```

- Keyword: `func`, followed by name, parameters, return type, `=`, body
- Return type is required (use `void` for no return value)
- The last expression in a block (without trailing statement) becomes the block's value, enabling implicit return:

```nore
func min(a: i64, b: i64): i64 = {
    if (a < b) { a } else { b }
}
```

### Imports and Qualified Access

```nore
import math "std/math.nore"
import file "std/file.nore"
```

- `import alias "path"` with a mandatory module alias
- Paths starting with `std/` resolve relative to the compiler binary's directory; all others resolve relative to the importing file
- Each file is imported at most once
- Imported declarations are accessed through the alias:

```nore
val x: i64 = math.min_i64(3, 7)                           // function
val r: file.ReadResult = file.read_file(mut ref mem, ref p) // type + function
val fd: i32 = fd_open(ref path, file.O_RDONLY)              // constant
val err: file.WriteResult = file.WriteResult.Err(-1)        // enum variant
```

No transitive visibility: if A imports B and B imports C, A cannot access C's declarations.

### Visibility (`pub`)

```nore
pub func add(a: i64, b: i64): i64 = { a + b }
pub val MAX_SIZE: i64 = 1024
pub value Point { x: i64, y: i64 }
pub enum Color { Red, Green, Blue }
```

- `pub` can prefix: `func`, `value`, `struct`, `table`, `enum`, `val`, `mut`
- Only `pub` declarations are accessible via qualified access from other modules
- Within the same file, all declarations are visible regardless of `pub`

### Native Declarations

The `native` keyword declares a function whose implementation is provided by the compiler. Each module must declare the native functions it uses:

```nore
native func fd_write(fd: i32, ref data: [u8]): i64
native func exit(code: i32): void
```

- Must be at top level, no body (the compiler provides the implementation)
- The name must match a known compiler-provided function
- Always module-private. To expose to importers, write a `pub` wrapper function
- Inside the module, the native name takes precedence over the wrapper, preventing infinite recursion

Known native functions: `fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek`, `mem_copy`, `exit`, `args`.

---

## Syntax Reference

This section covers the remaining syntax features. For the complete quick-reference with every edge case, see [syntax.md](syntax.md).

### Primitive Types

| Type | Description |
|------|-------------|
| `i64` | 64-bit signed integer |
| `i32` | 32-bit signed integer |
| `u8` | 8-bit unsigned integer |
| `u32` | 32-bit unsigned integer |
| `f64` | 64-bit floating-point |
| `bool` | Boolean (`true` / `false`) |
| `void` | No return value (functions only) |

Internal compile-time types: `comptime_int` (integer literal, coerces to any integer type or f64) and `comptime_float` (float literal, coerces to f64 only).

### Literals

```nore
42              // comptime_int
-17             // negative integer
3.14            // comptime_float
"hello\n"       // string literal (type: str / [u8])
'A'             // character literal (comptime_int, value 65)
true            // boolean
```

String escape sequences: `\n`, `\t`, `\r`, `\\`, `\"`, `\0`. Character escape sequences: `\n`, `\t`, `\r`, `\\`, `\'`, `\0`.

### Type Coercion and Casting

**Coercion** (implicit, compile-time types only):
- `comptime_int` coerces to any integer type (`i64`, `i32`, `u8`, `u32`) or `f64`
- `comptime_float` coerces to `f64` only
- No implicit coercion between concrete types
- Compile-time range checking when assigning `comptime_int` to smaller types

**In expressions**, coercion follows the same rules:

```nore
val x: i64 = 10
val y: i64 = x + 5     // OK: comptime_int 5 coerces to i64
val z: f64 = x + 5     // ERROR: result is i64, cannot assign to f64
val w: f64 = x + 5.0   // ERROR: cannot mix i64 and f64
```

**Casting** (explicit, function-call syntax):

```nore
val x: i64 = 42
val y: u8 = u8(x)          // narrowing: runtime bounds check (R003)
val z: f64 = f64(x)        // widening: always safe
val w: i64 = i64(3.14)     // truncation toward zero
val c: i64 = i64(Color.Red) // enum to integer
```

Supported casts: `u8()`, `i32()`, `u32()`, `i64()`, `f64()`.

**Cast safety rules:**
- Narrowing / sign change: runtime bounds check, panics R003 if out of range
- Widening (e.g., `u8` to `i64`, any integer to `f64`): always safe
- Identity (same type to same type): no-op
- Float to integer: runtime check for NaN/Inf/range, truncates toward zero
- Comptime values: range-checked at compile time (S050)
- Non-numeric types: error S063

### Variables

**Immutable** (`val`):

```nore
val x: i64 = 42         // explicit type, concrete i64
val y = 42              // no type: comptime constant
val z = y + 1           // comptime: folded to 43
```

**Comptime constants** (untyped `val`): when you omit the type annotation, the variable stays comptime, meaning it behaves like a named literal. The compiler inlines its value at every use site, and the concrete type is decided at the point of consumption, not at the point of definition:

```nore
val width = 800
val height = 600
val area = width * height    // folded to 480000 at compile time

// Each use site decides the concrete type independently
val a: i64 = area            // comptime_int coerces to i64
val b: i32 = width           // comptime_int coerces to i32
val c: u32 = height          // comptime_int coerces to u32
val d: f64 = area            // comptime_int coerces to f64
val e: u8 = width            // ERROR: 800 out of range for u8
```

Compare with a typed declaration, where the concrete type is locked at definition:

```nore
val width: i64 = 800         // concrete i64, not comptime
val ratio: f64 = width       // ERROR: i64 cannot coerce to f64
val small: i32 = width       // ERROR: i64 cannot coerce to i32
```

In short: untyped `val` gives you a type-safe named constant that stays flexible until consumed. Adding an explicit type pins it to that type immediately.

**Mutable** (`mut`):

```nore
mut counter: i64 = 0    // explicit type required
counter = counter + 1    // reassignable
```

Mutable variables must have an explicit type annotation. They can be reassigned after initialization.

**Global variables** are declared at the top level and have program lifetime:

```nore
val PI = 3.14159              // comptime constant (inlined)
val MAX_SIZE: i64 = 1024      // typed constant
val GREETING: str = "hello"   // string constant
mut counter: i64 = 0          // mutable global
val origin: Vec2 = Vec2 { x: 0.0, y: 0.0 }  // value type
val data: [i64; 3] = [10, 20, 30]            // array
mut mem: Arena = arena(4096)  // global arena (auto-init in main, auto-free at end)
```

**Global restrictions:**
- Global initializers must be constant expressions (literals, comptime constants, value constructors with constant fields, array literals with constant elements)
- `arena()` is the only non-constant initializer allowed (for arena globals)
- Function calls and `arena_alloc()` are not allowed as global initializers (S057)
- Slice globals are not allowed (except string literals via `val`)
- Arena globals must be `mut`
- Slices allocated from a global arena never "escape" (global arenas have program lifetime)

### Constant Folding

Expressions involving only literals or comptime variables are evaluated at compile time:

```nore
val x: i64 = 3 + 5 * 2      // folded to 13
val y: bool = 10 > 5         // folded to true
val w: i64 = 5 / 0           // ERROR: division by zero at compile time
```

Comptime if/else expressions with enum conditions are also folded:

```nore
val O_CREAT: i32 = if (TARGET_OS == OS.MacOS) { 512 } else { 64 }
```

### Operators

**Arithmetic** (numeric types): `+`, `-`, `*`, `/`, `%` (modulo: integers only)

**Bitwise** (integers only): `&`, `|`, `^`, `~`, `<<`, `>>`

**Comparison** (numeric types, produces `bool`): `==`, `!=`, `<`, `<=`, `>`, `>=`

**Logical** (bool types): `&&`, `||`, `!`

**Precedence** (highest to lowest):

| Level | Operators | Category |
|-------|-----------|----------|
| 8 | `*` `/` `%` | Multiplicative |
| 7 | `+` `-` | Additive |
| 6 | `<<` `>>` | Shift |
| 5 | `&` | Bitwise AND |
| 4 | `^` | Bitwise XOR |
| 3 | `\|` | Bitwise OR |
| 2 | `==` `!=` `<` `>` `<=` `>=` | Comparison |
| 1 | `&&` | Logical AND |
| 0 | `\|\|` | Logical OR |

Bitwise operators bind tighter than comparisons: `a & mask == 0` means `(a & mask) == 0`. Both operands must be the same concrete type (after coercion). Modulo follows C truncation semantics (result sign matches dividend). No comparison of `bool` values.

### Control Flow

**If/else** (statement and expression):

```nore
if (condition) {
    // then
} else {
    // else
}

// As expression (val/mut initializer)
val x: i64 = if (a < b) { a } else { b }

// As function body value expression (implicit return)
func min(a: i64, b: i64): i64 = {
    if (a < b) { a } else { b }
}

// After return
func abs(x: i64): i64 = {
    return if (x < 0) { 0 - x } else { x }
}
```

- Conditions must be `bool`
- Both branches must have compatible types when used as an expression
- Comptime if conditions are folded at compile time

**While loops:**

```nore
while (condition) {
    // body
}
```

`break` exits the loop, `continue` skips to the next iteration.

**For loops** (range-based):

```nore
for i in 0..n {
    // i goes from 0 to n-1
}
```

- Exclusive upper bound (`0..5` iterates 0, 1, 2, 3, 4)
- Loop variable is implicit `val i64` (immutable, cannot be reassigned)
- Range bounds must be integer types
- End expression is evaluated once before the loop starts
- Empty ranges (start >= end) simply skip the loop body
- `break` and `continue` work as expected

**Expression blocks:**

```nore
val y: i64 = {
    val a: i64 = 10
    val b: i64 = 20
    a + b       // last expression is the block's value
}
```

### Statements

- `val name: type = expr` / `mut name: type = expr`
- `name = expr` (assignment, mutable only)
- `expr.field = expr` / `expr[index] = expr`
- `name(args...)` (bare function call)
- `return expr`
- `assert expr` (runtime assertion, error R001)
- `break` / `continue`

---

## Built-in Functions

### I/O Built-ins

Low-level I/O primitives provide the thinnest possible bridge to POSIX syscalls. These require `native` declarations in the module that uses them.

| Function | Signature | Description |
|----------|-----------|-------------|
| `fd_write` | `(fd: i32, ref data: [u8]): i64` | Write bytes to fd. Returns bytes written, negative on error |
| `fd_read` | `(fd: i32, mut ref buf: [u8]): i64` | Read bytes from fd. Returns bytes read, 0 = EOF, negative on error |
| `fd_open` | `(ref path: str, flags: i32): i32` | Open file. Returns fd, negative on error. Path null-terminated internally (max 4095 bytes), permissions 0644 |
| `fd_close` | `(fd: i32): void` | Close fd |
| `fd_seek` | `(fd: i32, offset: i64, whence: i32): i64` | Seek within file. Returns new position, negative on error |
| `mem_copy` | `(mut ref dst: [u8], ref src: [u8]): i64` | Copy `min(dst.len, src.len)` bytes. Uses `memmove` (safe with overlapping buffers) |
| `exit` | `(code: i32): void` | Terminate process (never returns) |
| `args` | `(mut ref mem: Arena): [str]` | Get command-line arguments as arena-allocated `[str]` |

Type errors: S064 (fd not integer), S065 (data/buf/path not byte buffer), S066 (buf immutable for fd_read/mem_copy).

### Arena and Table Built-ins

These are always available without `native` declarations.

| Function | Description |
|----------|-------------|
| `arena(capacity)` | Create a new Arena with given byte capacity |
| `arena_alloc(mut ref arena, count)` | Allocate `count` zero-initialized elements from arena |
| `arena_reset(mut ref arena)` | Reclaim all arena memory (invalidates existing slices) |
| `table_alloc(mut ref arena, count)` | Allocate columnar storage for `count` rows |
| `table_len(ref table)` | Get current row count |
| `table_get(ref table, index)` | Get row value at index (bounds-checked) |
| `table_insert(mut ref table, row)` | Insert a row |

### Predefined Constants

| Name | Type | Source |
|------|------|--------|
| `TARGET_OS` | `OS` | Compiler-injected (no import needed) |
| `io.STDIN`, `io.STDOUT`, `io.STDERR` | `i32` | `std/io.nore` |
| `file.O_RDONLY`, `file.O_WRONLY`, etc. | `i32` | `std/file.nore` |
| `file.SEEK_SET`, `file.SEEK_CUR`, `file.SEEK_END` | `i32` | `std/file.nore` |

The compiler injects `enum OS { Linux, MacOS }` so that `TARGET_OS` comparisons work without imports.

---

## Standard Library

The standard library follows Nore's memory philosophy: **the caller owns the memory**. Any function that allocates takes an `Arena` parameter. Functions that do not allocate (comparisons, searches, math) take no arena parameter.

### `std/io.nore`

Print and buffered output. Import as `import io "std/io.nore"`.

- `io.print(ref s)`, `io.println(ref s)` for stdout
- `io.eprint(ref s)`, `io.eprintln(ref s)` for stderr
- `io.print_i64(n)` for integer output
- Buffered `io.Writer` struct: `io.writer_new`, `io.write_str`, `io.write_byte`, `io.write_i64`, `io.write_i64_padded`, `io.flush`, `io.writer_reset`

### `std/math.nore`

Math utilities. Import as `import math "std/math.nore"`.

- `math.min_i64`, `math.max_i64`, `math.abs_i64`, `math.clamp_i64`
- `math.min_f64`, `math.max_f64`, `math.abs_f64`, `math.clamp_f64`

### `std/string.nore`

String processing. Import as `import string "std/string.nore"`.

- Character classification: `string.is_digit`, `string.is_alpha`, `string.is_space`
- Comparison: `string.str_eq`, `string.str_starts_with`, `string.str_ends_with`
- Searching: `string.str_find`, `string.str_contains`
- Concatenation: `string.str_concat(mut ref mem, ref a, ref b)`
- Formatting: `string.fmt_i64`, `string.i64_to_str`, `string.str_to_i64`, `string.str_to_f64`
- `string.ParseResult` enum: `Ok(i64)` or `None`
- `string.ParseFloatResult` enum: `Ok(f64)` or `None`

### `std/file.nore`

File I/O helpers. Import as `import file "std/file.nore"`.

- `file.read_file(mut ref mem, ref path)` returns `file.ReadResult` (`Ok([u8])` or `Err(i32)`)
- `file.write_file(ref path, ref data)` returns `file.WriteResult` (`Ok(i64)` or `Err(i32)`)
- Platform-specific constants: `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND`, `SEEK_SET`, `SEEK_CUR`, `SEEK_END`

### `std/sys.nore`

Process control. Import as `import sys "std/sys.nore"`.

- `sys.exit(code)` terminates the process
- `sys.get_args(mut ref mem)` returns command-line arguments as `[str]`

### `std/json.nore`

JSON parser. Import as `import json "std/json.nore"`.

Parses JSON strings into a flat `JsonNodes` table with index-based tree relationships (parent/child/sibling). No recursive types. String values are zero-copy offsets into the source buffer.

- `json.json_parse(mut ref nodes, ref src)` returns `json.JsonResult` (`Ok(i64)` root index or `Err(JsonError)`)
- `json.json_kind(ref nodes, node)` returns `json.JsonKind` (Null, Bool, Number, Str, Array, Object)
- `json.json_child(ref nodes, parent)` returns first child index (-1 if none)
- `json.json_next(ref nodes, node)` returns next sibling index (-1 if last)
- `json.json_str(ref src, ref nodes, node)` returns string value as sub-slice of source
- `json.json_key(ref src, ref nodes, node)` returns object key as sub-slice of source
- `json.json_num(ref nodes, node)` returns number value as `f64`
- `json.json_bool(ref nodes, node)` returns boolean value
- `json.json_len(ref nodes, node)` returns child count
- `json.json_count(ref nodes)` returns total node count
- `json.json_find(ref src, ref nodes, parent, ref key)` finds child by key name (-1 if not found)

---

## Safety Guarantees

Nore is memory-safe for arena-scoped allocation. Dangling slices, buffer overflows, and use-after-free through the arena lifecycle are all caught at compile time or runtime.

### Compile-Time Slice Lifetime

The rule: **slices cannot outlive their arena.** Since arenas can only be locals, parameters, or globals, lifetime checking reduces to scope nesting.

**Local arenas:** slices are bound to the enclosing scope.

```nore
func process(): void = {
    mut scratch: Arena = arena(4096)
    mut s: [u8] = arena_alloc(mut ref scratch, 32)
    // s is valid here
}   // scratch dies, s dies
```

**Returning slices from local arenas is a compile error:**

```nore
func bad(): [u8] = {
    mut tmp: Arena = arena(256)
    mut data: [u8] = arena_alloc(mut ref tmp, 10)
    return data    // ERROR S053: slice outlives arena 'tmp'
}
```

**Slices from ref-param arenas are safe to return** (the arena lives in the caller's scope):

```nore
func get_data(mut ref mem: Arena, n: i64): [i64] = {
    mut data: [i64] = arena_alloc(mut ref mem, n)
    return data   // OK: mem is ref param, outlives callee
}
```

**Arena reset invalidation:** after `arena_reset()`, all slices from that arena are invalid:

```nore
mut a: Arena = arena(1024)
mut s: [u8] = arena_alloc(mut ref a, 32)
arena_reset(mut ref a)
// using s here is a compile error S056
```

### Cross-Function Escape Analysis

The compiler tracks whether functions return slices that originate from arena parameters. This catches indirect escape through function calls:

```nore
func build(mut ref mem: Arena): Mesh = {
    mut verts: [f64] = arena_alloc(mut ref mem, 100)
    return Mesh { vertices: verts }   // marks build as returning arena slices
}

func ok(mut ref mem: Arena): Mesh = {
    return build(mut ref mem)         // OK: mem is ref param
}

func bad(): Mesh = {
    mut mem: Arena = arena(8192)
    return build(mut ref mem)         // ERROR S053: slices escape local arena
}
```

This analysis propagates transitively through call chains. A function that returns the result of calling another arena-returning function is itself marked.

### The Safety Table

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

### Known Limitations

**Cross-function reset.** A callee that receives both `mut ref Arena` and a slice from that arena as separate parameters can reset the arena and invalidate the slice without the compiler noticing. This is a genuine gap, closable with a conservative call-site check in the future.

**Mutable aliasing.** Two `mut ref` parameters can point to the same data. Nore does not prevent this. This is a correctness issue (surprising behavior) rather than a safety issue (no memory corruption in Nore's model).

**Multiple arena parameters.** The per-function escape flag does not distinguish which arena parameter the return slices come from. Functions taking multiple arenas may see false positives. Per-parameter tracking can be added later without breaking existing code.

Nore's safety model is domain-specific: it covers the class of bugs that arena-based systems face in practice. This is substantially safer than C, with a much simpler mental model than Rust. It is not a universal aliasing and lifetime proof, but it targets the problems that matter most for data-oriented design.

---

## Design Decisions

### Why Arenas Over GC or Borrow Checker?

Arenas match DOD workloads: batch allocate, batch free. A garbage collector hides allocation cost and introduces unpredictable pauses. A full borrow checker (Rust-style) adds language complexity disproportionate to the problem when arenas are the primary allocation pattern.

### Why No Global Heap?

A global heap makes it easy to scatter allocations across memory, defeating locality. Arenas group related data together by construction. If every slice points into a known contiguous region, even indirect access has better cache behavior than random heap allocations.

### Why Explicit `value` and `struct`?

The compiler could infer which types contain indirection. But implicit inference is fragile: adding a slice field to a "value" type months later silently breaks all downstream code. With explicit keywords, the contract is visible at the definition site, and errors appear where the change happened.

### Why `func` Instead of `fn`?

More explicit and readable. Clear intent without abbreviation.

### Why `val` and `mut`?

`val` emphasizes immutability by default (safe). `mut` makes mutability explicit and visible. Similar to Kotlin/Scala conventions.

### Why Explicit Types for Runtime Variables?

Clear, predictable code at runtime boundaries. No type inference surprises. Exception: `val x = 42` infers comptime types for compile-time constants.

### Why `=` Before Function Body?

Functions are values (Scala/Kotlin style). Consistent with variable assignment syntax. Enables future function expressions.

### Why C as Intermediate Representation?

C is the universal systems language. Every platform has a C compiler. Using C as IR gives Nore access to mature optimization passes, platform-specific code generation, and easy debugging (the generated C is readable). The tradeoff is compile speed (two compilation steps), which is acceptable for a bootstrapping compiler.

---

## Further Reading

- [syntax.md](syntax.md) for the complete syntax quick-reference
- [arena-safety.md](arena-safety.md) for compiler-internal escape analysis details
- [error-codes.md](error-codes.md) for all compiler error codes
- [stdlib-design.md](stdlib-design.md) for standard library design principles
- `examples/` directory for real programs (cat clone, word count, JSON parser)
