# Nore Syntax Reference

## Overview

Nore is a systems programming language with a focus on simplicity and clarity. This document defines the syntax currently supported by the compiler.

## Language Features

### Comments

**Single-line comments**:
```nore
// This is a single-line comment
val x: i64 = 42  // inline comment
```

**Multi-line comments**:
```nore
/* This is a
   multi-line comment */
val y: i64 = 10
```

- Single-line: `//` to end of line
- Multi-line: `/*` to `*/` (do not nest)
- Comments are skipped by the lexer (no tokens produced)

### Imports

Import declarations from other `.nore` files:

```nore
import "std/math.nore"
import "utils.nore"
import "../shared/helpers.nore"
```

- Keyword `import` followed by a string literal path
- Must appear at top level (alongside other declarations)
- **Path resolution**: paths starting with `std/` resolve relative to the compiler binary's directory. All other paths resolve relative to the importing file's directory.
- Each file is imported at most once (duplicates are silently skipped)
- All top-level declarations from the imported file are merged into the importing program's scope
- Imports are transitive: if A imports B and B imports C, declarations from C are visible in A
- Error diagnostics show the correct source file for each error

### Types

**Currently supported**:
- `i64` - 64-bit signed integer
- `i32` - 32-bit signed integer
- `u8` - 8-bit unsigned integer
- `u32` - 32-bit unsigned integer
- `f64` - 64-bit floating-point number
- `bool` - Boolean type (`true` or `false`)
- `void` - No return value (functions only)
- `[T; N]` - Fixed-size array (see Arrays)
- `[T]` - Slice (see Slices)
- `str` - String type (byte slice, see Strings)
- `Arena` - Heap memory arena (see Arenas)
- User-defined `value` types (see Value Types)
- User-defined `struct` types (see Struct Types)
- User-defined `enum` types (see Enum Types)

**Compile-time types** (internal):
- `comptime_int` - Integer literal (coerces to any integer type or f64)
- `comptime_float` - Float literal (coerces to f64 only)

### Literals

**Integer literals**:
```nore
42      // comptime_int, coerces to any integer type or f64
-17     // negative integer
```

**Float literals**:
```nore
3.14    // comptime_float, coerces to f64 only
0.5     // decimal required before and after dot
```

**String literals**:
```nore
"hello"         // string literal (type: str / [u8])
"hello\nworld"  // with escape sequences
""              // empty string
```

Supported escape sequences: `\n` (newline), `\t` (tab), `\r` (carriage return), `\\` (backslash), `\"` (double quote), `\0` (null byte).

**Character literals**:
```nore
'A'     // comptime_int (value 65), coerces to any integer type
'\n'    // escape sequence (value 10)
```

A character literal produces a `comptime_int` equal to the byte value of the character. Supported escape sequences: `\n`, `\t`, `\r`, `\\`, `\'`, `\0`.

**Boolean literals**:
```nore
true
false
```

### Type Coercion

Literals are compile-time types that coerce to concrete types:

```nore
val x: i64 = 42      // comptime_int → i64
val y: f64 = 42      // comptime_int → f64
val z: f64 = 3.14    // comptime_float → f64
val w: i64 = 3.14    // ERROR: comptime_float cannot coerce to i64
val a: u8 = 255      // comptime_int → u8 (range-checked)
val b: i32 = -100    // comptime_int → i32 (range-checked)
val c: u32 = 42      // comptime_int → u32 (range-checked)
val d: u8 = 256      // ERROR: out of range for u8
val e: u32 = -1      // ERROR: out of range for u32
```

**Coercion rules**:
- `comptime_int` coerces to any integer type (`i64`, `i32`, `u8`, `u32`) or `f64`
- `comptime_float` coerces to `f64` only
- No implicit coercion between concrete types (`i64` ↔ `f64`, `i32` ↔ `i64`, etc.)
- Compile-time range checking when assigning `comptime_int` to smaller types
- Negation of unsigned types (`u8`, `u32`) is an error

**In expressions**:
```nore
val x: i64 = 10
val y: i64 = x + 5     // OK: comptime_int coerces to i64
val z: f64 = x + 5     // ERROR: result is i64, cannot assign to f64
val w: f64 = x + 5.0   // ERROR: cannot mix i64 and f64
```

### Type Casting

Explicit conversion between numeric types using function-call syntax:

```nore
val x: i64 = 42
val y: u8 = u8(x)          // i64 → u8 (runtime bounds check)
val z: f64 = f64(x)        // i64 → f64 (always safe)
val w: i64 = i64(3.14)     // comptime_float → i64 (compile-time truncation)
```

**Supported casts**: `u8(expr)`, `i32(expr)`, `u32(expr)`, `i64(expr)`, `f64(expr)`

**Safety rules**:
- **Narrowing / sign change**: Runtime bounds check — panics with error R003 if value is out of range
- **Widening** (e.g., `u8` → `i64`, any integer → `f64`): Always safe, no check needed
- **Identity** (same type → same type): No-op
- **Float → integer**: Runtime check for NaN, Inf, and range; value is truncated toward zero
- **Comptime values**: Range-checked at compile time (error S050 if out of range)
- **Non-numeric types**: Error S063 (e.g., `u8(true)` is not allowed)

**Examples**:
```nore
// Integer widening (safe)
val a: u8 = 200
val b: i64 = i64(a)         // OK: 200

// Integer narrowing (checked)
mut c: i64 = 100
val d: u8 = u8(c)           // OK: 100 fits in u8
// u8(300_as_i64) → runtime panic R003

// Float conversion
mut e: f64 = 3.7
val f: i64 = i64(e)         // OK: truncated to 3

// Cast in expressions
val g: u8 = 10
val h: u8 = 20
val sum: i64 = i64(g) + i64(h)   // 30

// Comptime casts
val k: u8 = u8(65)          // compile-time checked
```

### Constant Folding

Expressions involving only literals or comptime variables are evaluated at compile time:

```nore
val x: i64 = 3 + 5 * 2      // folded to 13 at compile time
val y: f64 = 3.0 + 5        // folded to 8.0 at compile time
val z: bool = 10 > 5        // folded to true at compile time
val w: i64 = 5 / 0          // ERROR: division by zero at compile time

val a = 42                  // comptime_int
val b = a + 1               // folded to 43 at compile time
val c = a * 3.14            // folded to 131.88 at compile time
```

### Variables

**Immutable Variables** (default):
```nore
val x: i64 = 42         // explicit type
val y = 42              // comptime: type inferred as comptime_int
val z = y + 1           // comptime: propagates through expressions
val flag: bool = true
```
- Keyword: `val`
- Type annotation: Optional when initializer is compile-time constant
- Without type: Variable becomes a comptime type (can be used in other comptime expressions)
- With explicit type: Variable becomes that concrete type (not comptime)
- Cannot be reassigned after initialization

**Mutable Variables**:
```nore
mut counter: i64 = 0
counter = counter + 1
```
- Keyword: `mut`
- Explicit type required: Must specify type with `:` annotation
- Can be reassigned after initialization

### Global Variables

Variables declared at the top level (outside any function) have program lifetime. They are visible to all functions.

**Global constants**:
```nore
val PI = 3.14159              // comptime constant (inlined)
val MAX_SIZE: i64 = 1024      // typed constant
val GREETING: str = "hello"   // string constant
val IS_DEBUG: bool = false     // boolean constant
```

**Global mutable variables**:
```nore
mut counter: i64 = 0          // mutable global
mut total: f64 = 0.0          // mutable global
```

**Global value types and arrays**:
```nore
value Vec2 { x: f64, y: f64 }

val origin: Vec2 = Vec2 { x: 0.0, y: 0.0 }
val data: [i64; 3] = [10, 20, 30]
mut pos: Vec2 = Vec2 { x: 1.0, y: 2.0 }
```

**Global arenas**:
```nore
mut mem: Arena = arena(4096)

func main(): void = {
    // Arena is initialized at the start of main
    // and freed at the end of main
    mut data: [i64] = arena_alloc(mut ref mem, 10)
    data[0] = 42
}
```
- Arena globals must be `mut`
- The arena is automatically initialized at the start of `main` and freed at the end

**Restrictions**:
- Global initializers must be constant expressions (literals, comptime constants, value constructors with constant fields, array literals with constant elements)
- `arena()` is the only non-constant initializer allowed (for arena globals)
- Function calls and `arena_alloc()` are not allowed as global initializers (error S057)
- Slice globals are not allowed (except string literals via `val`)
- Slices allocated from a global arena never "escape" (global arenas have program lifetime)

### Functions

```nore
func name(param1: type1, param2: type2): returnType = {
    body
}
```

**Syntax**:
- Keyword: `func`
- Parameters: Comma-separated with type annotations, optionally prefixed with `ref` or `mut ref`
- Return type: Required, specified after `:`
- Equals sign: `=` precedes the function body (functions are values)
- Body: Enclosed in braces `{}`

**Examples**:
```nore
func add(a: i64, b: i64): i64 = {
    return a + b
}

func greet(): void = {
    // no return needed
}

func scale(mut ref v: Vec2, factor: f64): void = {
    v.x = v.x * factor
    v.y = v.y * factor
}
```

### Function Calls

```nore
name(arg1, arg2, ...)
```

**Syntax**:
- Function name followed by parentheses
- Arguments: Comma-separated expressions
- Zero arguments: Use empty parentheses `()`
- Can be used as expressions or bare statements

**Examples**:
```nore
val sum: i64 = add(10, 20)
val nested: i64 = add(mul(2, 3), 4)
val result: i64 = 1 + add(2, 3) * 2
val value: i64 = get_value()
process()                              // bare call statement
```

### Ref Parameters

Reference parameters allow functions to access or modify the caller's data without copying.

**Read-only ref** (`ref`):
```nore
func length_sq(ref v: Vec2): f64 = {
    return v.x * v.x + v.y * v.y
}
```
- Keyword: `ref` before parameter name
- Generates `const T *` in C
- Cannot modify the referenced data

**Mutable ref** (`mut ref`):
```nore
func scale(mut ref v: Vec2, factor: f64): void = {
    v.x = v.x * factor
    v.y = v.y * factor
}
```
- Keywords: `mut ref` before parameter name
- Generates `T *` in C
- Can modify the referenced data

**Call-site syntax** (explicit):
```nore
val p: Vec2 = Vec2 { x: 3.0, y: 4.0 }
val lsq: f64 = length_sq(ref p)

mut q: Vec2 = Vec2 { x: 1.0, y: 2.0 }
scale(mut ref q, 2.0)
```
- Call site must explicitly match: `ref` for `ref` params, `mut ref` for `mut ref` params
- Argument must be addressable (variable or field-access chain)
- `mut ref` requires the root variable to be `mut`

**Restrictions**:
- Cannot take ref of scalar fields (i64, i32, u8, u32, f64, bool) — just copy them
- Cannot take ref of array elements (deferred to slices)
- Refs are a calling convention only — cannot be stored, returned, or used as local variables

### Value Types

Value types are composite data with named fields, copy semantics, and no indirection.

**Declaration** (top-level only):
```nore
value Vec2 { x: f64, y: f64 }
value Color { r: i64, g: i64, b: i64 }
```
- Keyword: `value`
- Must be declared before use (textual order)
- Fields are comma-separated with type annotations (`name: Type`)
- Field types can be any concrete type (`i64`, `f64`, `bool`), arrays, or other value types

**Constructors**:
```nore
val p: Vec2 = Vec2 { x: 1.0, y: 2.0 }
val c: Color = Color { r: 255, g: 128, b: 0 }
```
- All fields must be provided (no defaults)
- Fields can be in any order
- No duplicate fields allowed

**Field Access**:
```nore
val x: f64 = p.x
val sum: f64 = p.x + p.y
```
- Dot notation: `expr.field`
- Chains allowed: `a.b.c`

**Field Assignment**:
```nore
mut p: Vec2 = Vec2 { x: 0.0, y: 0.0 }
p.x = 1.0
p.y = p.x + 2.0
```
- Only on `mut` variables (root variable must be mutable)
- `val` variables and their fields are immutable

**Value Semantics**:
```nore
val a: Vec2 = Vec2 { x: 1.0, y: 2.0 }
mut b: Vec2 = a       // copy, not reference
b.x = 99.0            // does not affect a
assert a.x == 1.0     // a is unchanged
```
- Assigned by copy (no shared references)
- Passed to functions by copy
- Returned from functions by copy

### Struct Types

Struct types are resource owners with ref-only passing semantics. Unlike value types, structs cannot be copied — they must be passed by `ref` or `mut ref`.

**Declaration** (top-level only):
```nore
struct Entity { x: f64, y: f64, health: i64 }
struct Player { pos: Vec2, score: i64 }
```
- Keyword: `struct`
- Must be declared before use (textual order)
- Fields are comma-separated with type annotations (`name: Type`)
- Field types can be any concrete type (`i64`, `f64`, `bool`), arrays, slices, or value types
- Cannot embed other struct types or Arena as fields

**Constructors**:
```nore
val e: Entity = Entity { x: 1.0, y: 2.0, health: 100 }
```
- Same syntax as value type constructors
- All fields must be provided (no defaults)
- Fields can be in any order

**Field Access and Assignment**:
```nore
val hp: i64 = e.health
mut e: Entity = Entity { x: 0.0, y: 0.0, health: 100 }
e.health = 50
```
- Same dot notation as value types
- Field assignment requires `mut` variable

**No Copy Semantics**:
```nore
val a: Entity = Entity { x: 1.0, y: 2.0, health: 100 }
val b: Entity = a    // ERROR: Cannot copy struct (S043)
```
- Structs can only be initialized from constructors or function return values
- Cannot copy a struct variable to another
- Struct variables cannot be reassigned after initialization (even from a new constructor)

**Ref-Only Passing**:
```nore
func get_health(ref e: Entity): i64 = {
    return e.health
}

func damage(mut ref e: Entity, amount: i64): void = {
    e.health = e.health - amount
}
```
- Struct parameters must use `ref` (read-only) or `mut ref` (mutable)
- Passing a struct by value is an error
- Call-site syntax matches value types: `ref e` or `mut ref e`

**Returning Structs**:
```nore
func make_entity(x: f64, y: f64, hp: i64): Entity = {
    return Entity { x: x, y: y, health: hp }
}

val e: Entity = make_entity(1.0, 2.0, 100)
```
- Functions can return struct types
- Return value can initialize a struct variable (not a copy)

### Enum Types

Enums define named integer constants grouped under a type. Variants are auto-numbered starting from 0.

**Declaration** (top-level only):
```nore
enum Color { Red, Green, Blue }
enum Direction { North, South, East, West }
```
- Keyword: `enum`
- Variants are comma-separated identifiers
- Each variant gets an integer value: 0, 1, 2, ...

**Usage**:
```nore
val c: Color = Color.Red
mut d: Color = Color.Green
d = Color.Blue
```
- Access variants with dot syntax: `EnumName.Variant`
- Type annotation required (enum types are not compile-time constants)
- Can be used as function parameters and return types

**Comparison**:
```nore
assert c == Color.Red
assert c != Color.Green
```
- Only `==` and `!=` are allowed
- Ordering (`<`, `>`, `<=`, `>=`) is not allowed
- Arithmetic (`+`, `-`, etc.) is not allowed
- Both sides must be the same enum type

**Casting to integer**:
```nore
assert i64(Color.Red) == 0
assert i64(Color.Blue) == 2
```
- Cast to any integer type with `i64()`, `i32()`, `u8()`, `u32()`

**Built-in OS enum and TARGET_OS**:

The compiler injects `enum OS { Linux, MacOS }` and a comptime constant `TARGET_OS: OS` set at compile time based on the host platform. This enables platform-specific constants using comptime if/else:

```nore
val O_CREAT: i32 = if (TARGET_OS == OS.MacOS) { 512 } else { 64 }
```

Comptime if/else expressions with enum conditions are folded at compile time.

### Arrays

Fixed-size arrays with compile-time known size, value semantics, and bounds checking.

**Type Syntax**:
```nore
[i64; 3]          // array of 3 integers
[f64; 4]          // array of 4 floats
[bool; 2]         // array of 2 booleans
[[i64; 2]; 3]     // nested: 3 arrays of 2 integers
```
- Size must be a positive integer literal
- Element type can be any concrete type, array, or value type

**Array Literals**:
```nore
val arr: [i64; 3] = [1, 2, 3]
val floats: [f64; 2] = [1.5, 2.5]
val grid: [[i64; 2]; 3] = [[1, 2], [3, 4], [5, 6]]
```
- Elements are comma-separated expressions in brackets
- Number of elements must match the declared array size
- Element types must be compatible (comptime coercion applies)

**Indexing**:
```nore
val x: i64 = arr[0]
val y: i64 = arr[i]
val z: i64 = grid[1][0]   // nested indexing
```
- Index must be integer type (`i64`, `i32`, `u8`, `u32`, or `comptime_int`)
- Bounds checking at runtime (error R002, exits with code 2 on out-of-bounds)
- Chains with field access: `v.data[i]`

**Sub-Slicing** (produces a slice from an array):
```nore
val arr: [i64; 5] = [10, 20, 30, 40, 50]
val sub: [i64] = arr[1..4]    // slice of elements 1, 2, 3
val all: [i64] = arr[..]      // full array as slice
```
- See the Slices section for full sub-slicing syntax

**Element Assignment**:
```nore
mut arr: [i64; 3] = [1, 2, 3]
arr[0] = 99
arr[i] = arr[i] + 1
```
- Only on `mut` variables (root variable must be mutable)
- `val` arrays and their elements are immutable
- Bounds checking on assignment too

**As Value Type Fields**:
```nore
value Vec3 { data: [f64; 3] }

func main(): void = {
    mut v: Vec3 = Vec3 { data: [1.0, 2.0, 3.0] }
    v.data[0] = 10.0
    assert v.data[0] == 10.0
}
```

**Value Semantics**:
```nore
val a: [i64; 3] = [1, 2, 3]
mut b: [i64; 3] = a       // copy, not reference
b[0] = 99                 // does not affect a
assert a[0] == 1           // a is unchanged
```
- Assigned by copy (no shared references)
- Passed to functions by copy
- Returned from functions by copy

### Slices

A slice is a fat pointer (data pointer + length) that can be backed by either a fixed-size array on the stack or heap memory allocated from an Arena. Slices allow functions to operate on sequences of any size regardless of the backing storage.

**Type Syntax**:
```nore
[i64]             // slice of integers
[f64]             // slice of floats
[Vec2]            // slice of value types
```
- Drop the size from `[T; N]` to get `[T]`
- Element type can be any concrete type or value type

**Slice Parameters** (ref required):
```nore
func sum(ref data: [i64]): i64 = {
    mut total: i64 = 0
    mut i: i64 = 0
    while (i < data.len) {
        total = total + data[i]
        i = i + 1
    }
    return total
}
```
- Slice parameters must use `ref` (read-only) or `mut ref` (mutable)
- Passing a slice by value is an error

**Call-Site Coercion** (array → slice):
```nore
val a: [i64; 3] = [1, 2, 3]
val b: [i64; 5] = [10, 20, 30, 40, 50]
assert sum(ref a) == 6       // [i64; 3] → [i64]
assert sum(ref b) == 150     // [i64; 5] → [i64]
```
- Fixed-size arrays coerce to slices at `ref`/`mut ref` call sites
- Element types must be compatible

**Mutable Slices**:
```nore
func double_all(mut ref data: [i64]): void = {
    mut i: i64 = 0
    while (i < data.len) {
        data[i] = data[i] * 2
        i = i + 1
    }
}

mut arr: [i64; 3] = [1, 2, 3]
double_all(mut ref arr)
assert arr[0] == 2
```
- `mut ref` slices can modify elements through the slice
- Root variable must be `mut`

**Slice Passthrough**:
```nore
func first(ref data: [i64]): i64 = {
    return data[0]
}

func get_first(ref data: [i64]): i64 = {
    return first(ref data)    // pass slice to another function
}
```
- A slice parameter can be passed directly to another function expecting a slice

**Length Field**:
```nore
func len(ref data: [i64]): i64 = {
    return data.len
}
```
- `.len` returns the number of elements as `i64`
- This is the only field on slices

**Indexing and Bounds Checking**:
```nore
val x: i64 = data[0]         // indexing
data[i] = 42                 // element assignment (mut ref only)
```
- Same syntax as arrays
- Runtime bounds checking (error R002, exits with code 2 on out-of-bounds)

**Sub-Slicing** (`expr[start..end]`):
```nore
val sub: [i64] = data[2..5]    // elements at indices 2, 3, 4
val head: [i64] = data[..3]    // same as data[0..3]
val tail: [i64] = data[2..]    // same as data[2..data.len]
val all: [i64] = data[..]      // full slice
```
- Works on both slices and fixed arrays; result is always a slice
- Range is half-open: `[start..end)` (end-exclusive)
- Start defaults to 0 when omitted, end defaults to length when omitted
- Runtime bounds checking (error R004, exits with code 2 if `start < 0`, `end < start`, or `end > len`)
- Sub-slices borrow from the source, so mutations through a sub-slice affect the original
- Can be chained with indexing: `data[1..4][0]`
- Can be passed directly to `ref` parameters: `sum(ref arr[1..4])`

**Slice Local Variables**:
```nore
mut mem: Arena = arena(4096)
val data: [i64] = arena_alloc(mut ref mem, 10)
mut pts: [Vec2] = arena_alloc(mut ref mem, 100)
val result: [i64] = get_data(mut ref mem, 5)   // from function call
val sub: [i64] = data[2..5]                     // from sub-slicing
```
- Slice locals must be initialized via `arena_alloc()`, a function call returning a slice, or a sub-slice expression

**Slice Struct Fields**:
```nore
struct Mesh { vertices: [f64], count: i64 }
```
- Slices are allowed as fields in `struct` types
- Slices are NOT allowed as fields in `value` types

**No Copy Semantics**:
```nore
mut mem: Arena = arena(4096)
val a: [i64] = arena_alloc(mut ref mem, 10)
val b: [i64] = a    // ERROR: slice local must use arena_alloc or function call (S046)
```
- Slice locals can only be initialized via `arena_alloc()`, a function call, or a sub-slice expression
- To share access, pass slices by `ref` or `mut ref`

**Restrictions** (current):
- Slice parameters must use `ref` or `mut ref`
- Slice local variables must be initialized via `arena_alloc()`, a function call returning a slice, or a sub-slice expression

### Arenas

Arenas provide heap allocation for slices. An Arena is a contiguous block of memory from which slices can be allocated sequentially.

**Creating an Arena**:
```nore
mut mem: Arena = arena(4096)    // 4096-byte arena
```
- `arena(capacity)` creates a new arena with the given byte capacity
- Arena variables should be `mut` since `arena_alloc()` and `arena_reset()` require mutability

**Allocating Slices**:
```nore
val data: [i64] = arena_alloc(mut ref mem, 10)       // 10 zero-initialized i64s
mut pts: [Vec2] = arena_alloc(mut ref mem, 100)      // 100 zero-initialized Vec2s
```
- `arena_alloc(mut ref arena, count)` allocates `count` elements from the arena
- Element type is inferred from the declaration type
- Memory is zero-initialized
- Arena argument must be `mut ref` (allocation mutates the arena)
- Aborts at runtime if arena runs out of memory

**Passing Arenas to Functions**:
```nore
func make_arena(): Arena = {
    return arena(4096)
}

func fill(mut ref mem: Arena, n: i64): void = {
    mut data: [i64] = arena_alloc(mut ref mem, n)
    data[0] = 42
}
```
- Arena parameters must use `ref` (read-only) or `mut ref` (for allocation)
- Arena can be returned from functions (like structs, via constructor or function call)

**Automatic Cleanup**:
- Arena memory is automatically freed when the arena goes out of scope
- On `return`, all local arenas in the function scope chain are freed before returning
- On `break`/`continue`, arenas declared inside the loop body are freed
- Arena ref parameters are NOT freed by the callee (the caller owns them)

**Arena Reset**:
```nore
arena_reset(mut ref mem)
```
- `arena_reset(mut ref arena)` reclaims all arena memory at once (resets offset to zero)
- Arena argument must be `mut ref` (reset mutates the arena)
- All slices previously allocated from the arena are **invalidated** — using them after reset is a compile-time error (S056)
- After reset, new slices can be allocated from the arena with fresh variables
- Invalidation is conservative: once a slice is invalidated, it stays invalidated for its entire scope

**Lifetime Safety**:
- Slices allocated from a local arena cannot escape the function — neither directly via return nor indirectly through a function call (error S053)
- Slices allocated from a ref-param arena are safe to return (the arena lives in the caller's scope)
- Escape analysis propagates transitively through call chains
- Known limitation: functions with multiple arena parameters may produce false positives (the analysis uses a single per-function flag, not per-parameter tracking)

**Restrictions**:
- Arena is not copyable (like structs)
- Arena cannot be a field in value or struct types
- Arena parameters must use `ref` or `mut ref`

### Tables

Tables provide columnar (struct-of-arrays) storage as syntactic sugar. A `table` declaration generates two types: a `struct` for columnar storage and a `value` for row access.

**Declaration** (top-level only):
```nore
value Vec2 { x: f64, y: f64 }

table Particles {
    pos: Vec2,
    life: i64
}
```
- Keyword: `table`
- Generates a struct `Particles` with slice columns (`pos: [Vec2]`, `life: [i64]`, `_len: i64`)
- Generates a value `Particles.Row` with the original fields (`pos: Vec2`, `life: i64`)
- Row type name is `Name.Row` (e.g., `Particles.Row`)

**Table Field Constraints**:
- Fields must be value-compatible: scalars, fixed arrays, or value types
- Slices, structs, and Arena are not allowed as table fields (error S059)

**Allocating a Table**:
```nore
mut mem: Arena = arena(65536)
mut p: Particles = table_alloc(mut ref mem, 100)
```
- `table_alloc(mut ref arena, count)` allocates column storage for `count` rows
- Table type is inferred from the declaration context (like `arena_alloc`)
- Arena argument must be `mut ref`

**Getting the Row Count**:
```nore
val n: i64 = table_len(ref p)
```
- `table_len(ref table)` returns the current number of inserted rows
- Table argument must be `ref`

**Inserting Rows**:
```nore
table_insert(mut ref p, Particles.Row { pos: Vec2 { x: 1.0, y: 2.0 }, life: 100 })
```
- `table_insert(mut ref table, row)` appends a row to the table
- Table argument must be `mut ref`
- Row must match the generated row type
- Runtime error if capacity is exceeded

**Getting Rows**:
```nore
val row: Particles.Row = table_get(ref p, 0)
assert row.pos.x == 1.0
```
- `table_get(ref table, index)` returns a row value at the given index
- Table argument must be `ref`
- Index must be an integer type
- Runtime bounds check (error R002, exits with code 2 on out-of-bounds)

**Direct Column Access**:
```nore
// Read column elements (slice indexing)
val life: i64 = p.life[i]
val pos: Vec2 = p.pos[0]

// Write column elements (requires mut table)
p.life[i] = 50
```
- Columns are regular slices — standard slice indexing and bounds checking apply

**Tables Follow Struct Rules**:
- Cannot be copied (use `ref` or `mut ref` to pass)
- Cannot be embedded in other types
- Can be passed as `ref` (read-only) or `mut ref` (for insert) parameters

**Example**:
```nore
func count_alive(ref p: Particles): i64 = {
    mut alive: i64 = 0
    for i in 0..table_len(ref p) {
        if (table_get(ref p, i).life > 0) {
            alive = alive + 1
        }
    }
    return alive
}
```

### Strings

The `str` type is syntactic sugar for `[u8]` (a byte slice). String literals create fat pointers pointing to static C string data at zero cost.

**Declaration**:
```nore
val greeting: str = "hello"
val empty: str = ""
val escaped: str = "line1\nline2"
```
- `str` is equivalent to `[u8]`
- String literals must be bound with `val` (immutable) — `mut` is an error (S054)
- `.len` gives the byte count (excludes null terminator)

**Indexing**:
```nore
val s: str = "hello"
val h: u8 = s[0]       // 104 (ASCII 'h')
assert s.len == 5
```
- Same indexing and bounds checking as slices

**Passing to Functions** (ref required):
```nore
func length(ref s: str): i64 = {
    return s.len
}

val msg: str = "hello"
assert length(ref msg) == 5
```
- `str` parameters follow slice rules: must use `ref` or `mut ref`
- `str` and `[u8]` are interchangeable

**Escape Sequences**:
- `\n` — newline
- `\t` — tab
- `\r` — carriage return
- `\\` — backslash
- `\"` — double quote
- `\0` — null byte

**Restrictions** (current):
- String literals cannot be mutable (`mut` binding is error S054)
- Multiline strings are not yet supported

### Predefined Constants

**Compiler-injected constants** are available in every program without imports:

| Name | Type | Value |
|------|------|-------|
| `TARGET_OS` | `OS` | `OS.Linux` or `OS.MacOS` (set at compile time) |
| `STDIN` | `comptime_int` | 0 |
| `STDOUT` | `comptime_int` | 1 |
| `STDERR` | `comptime_int` | 2 |

**I/O constants from `std/file.nore`** (require `import "std/file.nore"`):

| Name | Value |
|------|-------|
| `O_RDONLY` | 0 |
| `O_WRONLY` | 1 |
| `O_RDWR` | 2 |
| `O_CREAT` | platform-specific (via `TARGET_OS`) |
| `O_TRUNC` | platform-specific (via `TARGET_OS`) |
| `O_APPEND` | platform-specific (via `TARGET_OS`) |
| `SEEK_SET` | 0 |
| `SEEK_CUR` | 1 |
| `SEEK_END` | 2 |

Flags can be combined with bitwise OR: `O_WRONLY | O_CREAT | O_TRUNC`

### I/O Built-in Functions

Low-level I/O built-ins provide the thinnest possible bridge to POSIX syscalls. These are the only way to interact with the operating system.

**fd_write(fd, ref data)** — write bytes to a file descriptor:
```nore
val n: i64 = fd_write(STDOUT, ref "Hello, World!\n")
assert n == 14

val data: [u8; 3] = [65, 66, 67]
fd_write(STDOUT, ref data)    // writes "ABC"
```
- `fd` must be an integer type (file descriptor)
- `data` must be `[]u8` (slice) or `[u8; N]` (array), passed with `ref`
- Returns `i64`: bytes written (negative on error)
- Can be used as a bare statement or expression

**fd_read(fd, mut ref buf)** — read bytes from a file descriptor:
```nore
mut buf: [u8; 256] = [0, 0, ...]
val n: i64 = fd_read(STDIN, mut ref buf)
```
- `fd` must be an integer type (file descriptor)
- `buf` must be `[]u8` or `[u8; N]`, passed with `mut ref` (buffer must be mutable)
- Returns `i64`: bytes read (0 = EOF, negative on error)
- Can be used as a bare statement or expression

**fd_open(ref path, flags)** — open a file:
```nore
val fd: i32 = fd_open(ref "file.txt", O_RDONLY)
assert fd >= 0

val wfd: i32 = fd_open(ref "out.txt", O_WRONLY | O_CREAT | O_TRUNC)
```
- `path` must be `[]u8` or string literal, passed with `ref`
- `flags` must be an integer type (use predefined `O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_TRUNC`, etc.)
- Returns `i32`: file descriptor (negative on error)
- Path is null-terminated internally (max 4095 bytes)
- File permissions default to 0644

**fd_close(fd)** — close a file descriptor:
```nore
fd_close(fd)
```
- `fd` must be an integer type
- Returns `void`
- Used as a bare statement

**fd_seek(fd, offset, whence)** — reposition file offset:
```nore
val size: i64 = fd_seek(fd, 0, SEEK_END)   // get file size
fd_seek(fd, 0, SEEK_SET)                    // seek back to start
```
- `fd` must be an integer type (file descriptor)
- `offset` must be an integer type (byte offset)
- `whence` must be an integer type (`SEEK_SET`, `SEEK_CUR`, or `SEEK_END`)
- Returns `i64`: new file position (negative on error)
- Can be used as a bare statement or expression

**exit(code)** — terminate the process:
```nore
exit(0)      // success
exit(1)      // failure
```
- `code` must be an integer type
- Returns `void` (never returns)
- Used as a bare statement

**mem_copy(mut ref dst, ref src)** — copy bytes between byte buffers:
```nore
mut buf: [u8; 4] = [0, 0, 0, 0]
val src: [u8; 3] = [65, 66, 67]
val n: i64 = mem_copy(mut ref buf, ref src)
assert n == 3    // min(4, 3)
```
- `dst` must be `[u8]` or `[u8; N]`, passed with `mut ref` (must be mutable)
- `src` must be `[u8]` or `[u8; N]`, passed with `ref`
- Copies `min(dst.len, src.len)` bytes from src to dst
- Returns `i64`: number of bytes copied
- Uses `memmove` internally (safe with overlapping buffers)
- Can be used as a bare statement or expression

**Type errors**:
- Non-integer fd: error S064
- Non-byte-buffer data/buf/path/dst/src: error S065
- Immutable buffer for fd_read/mem_copy dst: error S066

### Control Flow

**Conditional Statements**:
```nore
if (condition) {
    // then branch
}

if (condition) {
    // then branch
} else {
    // else branch
}
```
- Condition must be `bool` type
- Braces required

**If Expressions**:
```nore
val x: i64 = if (cond) { 1 } else { 2 }

val y: i64 = if (flag) {
    val a: i64 = 10
    a + 5
} else {
    val b: i64 = 20
    b - 5
}

// if/else as function body value expression (implicit return)
func min(a: i64, b: i64): i64 = {
    if (a < b) { a } else { b }
}

// return if/else
func abs(x: i64): i64 = {
    return if (x < 0) { 0 - x } else { x }
}
```
- Both branches must have compatible types
- Each branch's value is its last expression
- Works as block value expression (last expression before `}`) in any block, including function bodies
- Works after `return` keyword
- Comptime if conditions are folded at compile time

**While Loops**:
```nore
while (condition) {
    // loop body
}
```
- Condition must be `bool` type
- `break` exits the loop
- `continue` skips to next iteration

**For Loops** (range-based):
```nore
for i in 0..n {
    // loop body — i goes from 0 to n-1
}
```
- Exclusive upper bound (`0..5` iterates 0, 1, 2, 3, 4)
- Loop variable is implicit `val i64` (immutable, cannot be reassigned)
- Range bounds must be integer types (`i64`, `i32`, `u8`, `u32`, or `comptime_int`)
- End expression is evaluated once before the loop starts
- Empty ranges (start >= end) simply skip the loop body
- `break` exits the loop
- `continue` skips to next iteration

**Blocks**:
```nore
{
    val x: i64 = 10
    // x is scoped to this block
}
```
- Create new scope
- Allow variable shadowing

**Expression Blocks**:
```nore
val x: i64 = { 42 }

val y: i64 = {
    val a: i64 = 10
    val b: i64 = 20
    a + b
}
```
- Blocks can be used as expressions
- The last expression (without trailing statement) is the block's value
- Block ending with a statement has type `void`
- Simple blocks (only value expression) are comptime if the value is comptime

### Operators

**Arithmetic** (numeric operands, same type result):
- `+` Addition
- `-` Subtraction (binary and unary negation)
- `*` Multiplication
- `/` Division
- `%` Modulo (integer types only, not supported on `f64`)

Works with all numeric types (i64, i32, u8, u32, f64) and comptime types. Both operands must be the same concrete type (after coercion). Negation (`-x`) is not allowed on unsigned types (`u8`, `u32`). Modulo follows C truncation semantics (result sign matches dividend).

**Bitwise** (integer operands only, same type result):
- `&` Bitwise AND
- `|` Bitwise OR
- `^` Bitwise XOR
- `<<` Left shift
- `>>` Right shift
- `~` Bitwise NOT (unary)

Works with all integer types (i64, i32, u8, u32) and comptime int. Not supported on `f64` (error S062). Bitwise operators bind **tighter** than comparisons, so `a & mask == 0` means `(a & mask) == 0`.

**Comparison** (numeric operands, bool result):
- `==` Equal
- `!=` Not equal
- `<` Less than
- `<=` Less than or equal
- `>` Greater than
- `>=` Greater than or equal

Both operands must be the same concrete numeric type (after coercion). Comparison of `bool` values is not supported.

**Logical** (bool operands, bool result):
- `&&` Logical AND
- `||` Logical OR
- `!` Logical NOT (unary)

**Operator Precedence** (highest to lowest):
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

**Assignment**:
- `=` Assignment operator (mutable variables only)

### Statements

- `val name: type = expr` - Immutable variable declaration
- `mut name: type = expr` - Mutable variable declaration
- `name = expr` - Assignment (mutable only)
- `expr.field = expr` - Field assignment (root variable must be mutable)
- `expr[index] = expr` - Array element assignment (root variable must be mutable)
- `name(args...)` - Bare function call statement
- `return expr` - Return from function
- `assert expr` - Runtime assertion (bool expr, error R001, exits with code 2 on failure)
- `break` - Exit innermost loop
- `continue` - Skip to next loop iteration

## Complete Example

```nore
func factorial(n: i64): i64 = {
    mut result: i64 = 1
    mut i: i64 = 1
    while (i <= n) {
        result = result * i
        i = i + 1
    }
    return result
}

func main(): void = {
    val n: i64 = 5
    val result: i64 = factorial(n)
    assert result == 120
}
```

## Token Reference

### Keywords
- `func` - Function declaration
- `value` - Value type declaration
- `struct` - Struct type declaration
- `table` - Table type declaration (columnar storage sugar)
- `str` - String type (byte slice `[u8]`)
- `Arena` - Arena type (heap memory)
- `ref` - Reference parameter/argument
- `val` - Immutable variable
- `mut` - Mutable variable/reference
- `return` - Return statement
- `assert` - Runtime assertion
- `if` - Conditional
- `else` - Else branch
- `while` - While loop
- `for` - For loop (range-based)
- `in` - For-loop range keyword
- `break` - Exit loop
- `continue` - Next iteration
- `true` - Boolean true
- `false` - Boolean false
- `i64` - 64-bit signed integer type
- `i32` - 32-bit signed integer type
- `u8` - 8-bit unsigned integer type
- `u32` - 32-bit unsigned integer type
- `f64` - 64-bit floating-point type
- `bool` - Boolean type
- `void` - Void type

### Operators
- `+` `-` `*` `/` `%` - Arithmetic
- `&` `|` `^` `~` `<<` `>>` - Bitwise
- `==` `!=` `<` `<=` `>` `>=` - Comparison
- `&&` `||` `!` - Logical
- `=` - Assignment

### Built-in Functions
- `arena(capacity)` - Create a new Arena with the given byte capacity
- `arena_alloc(mut ref arena, count)` - Allocate a slice of `count` elements from an Arena
- `arena_reset(mut ref arena)` - Reclaim all arena memory (invalidates existing slices)
- `table_alloc(mut ref arena, count)` - Allocate columnar storage for `count` rows from an Arena
- `table_len(ref table)` - Get the current row count of a table
- `table_get(ref table, index)` - Get a row value at the given index (bounds checked)
- `table_insert(mut ref table, row)` - Insert a row into the table
- `fd_write(fd, ref data)` - Write bytes to a file descriptor, returns `i64` bytes written
- `fd_read(fd, mut ref buf)` - Read bytes from a file descriptor, returns `i64` bytes read
- `fd_open(ref path, flags)` - Open a file, returns `i32` file descriptor
- `fd_close(fd)` - Close a file descriptor
- `fd_seek(fd, offset, whence)` - Seek in a file, returns `i64` new position
- `mem_copy(mut ref dst, ref src)` - Copy bytes between buffers, returns `i64` bytes copied
- `exit(code)` - Terminate the process

### Standard Library Functions

Available via `import "std/io.nore"`:
- `print(ref s)` - Print bytes to stdout
- `println(ref s)` - Print bytes + newline to stdout
- `print_i64(n)` - Print integer as decimal to stdout

Available via `import "std/math.nore"`:
- `min_i64(a, b)`, `max_i64(a, b)`, `abs_i64(a)`, `clamp_i64(x, lo, hi)`
- `min_f64(a, b)`, `max_f64(a, b)`, `abs_f64(a)`, `clamp_f64(x, lo, hi)`

Available via `import "std/file.nore"`:
- `read_file(mut ref mem, ref path)` - Read entire file into arena, returns `[u8]`
- `write_file(ref path, ref data)` - Write bytes to file (create/overwrite), returns `bool`

### Predefined Constants
- `STDIN` - Standard input (0)
- `STDOUT` - Standard output (1)
- `STDERR` - Standard error (2)
- `O_RDONLY`, `O_WRONLY`, `O_RDWR`, `O_CREAT`, `O_TRUNC`, `O_APPEND` - POSIX open flags
- `SEEK_SET`, `SEEK_CUR`, `SEEK_END` - Seek whence constants

### Punctuation
- `(` `)` - Parentheses (parameters, grouping, conditions)
- `{` `}` - Braces (blocks, function bodies, value type declarations, constructors)
- `[` `]` - Brackets (array types, array literals, indexing, sub-slicing)
- `:` - Type annotation separator
- `;` - Array type size separator (`[T; N]`)
- `,` - Parameter/field/element separator
- `.` - Field access
- `..` - Range operator (for loops, sub-slicing)

Note: `"` delimits string literals but is consumed by the lexer during scanning, not emitted as a standalone token.

## Design Decisions

### Why `func` instead of `fn`?
- More explicit and readable
- Familiar to developers from many languages
- Clear intent without abbreviation

### Why `val` and `mut`?
- `val` emphasizes immutability by default (safe)
- `mut` makes mutability explicit and visible
- Similar to Kotlin/Scala conventions

### Why explicit types for runtime variables?
- Clear, predictable code at runtime boundaries
- No type inference surprises
- Easier to read and maintain
- Exception: `val x = 42` infers comptime types for compile-time constants

### Why `=` before function body?
- Functions are values (Scala/Kotlin style)
- Consistent with variable assignment syntax
- Enables function expressions later (if/while expressions will follow same pattern)

## Future Extensions

**Not yet implemented**:
- Multiline strings
- Additional types (`f32`)
- Module system
- While as expressions
- Early exit from expression blocks (`yield` keyword)

These features will be added incrementally as the compiler evolves.
