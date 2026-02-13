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
- Multi-line: `/*` to `*/`
- Comments are skipped by the lexer (no tokens produced)

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
- User-defined `value` types (see Value Types)
- User-defined `struct` types (see Struct Types)

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
- Field types can be any concrete type (`i64`, `f64`, `bool`), arrays, or value types
- Cannot embed other struct types as fields

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
val b: Entity = a    // ERROR: Cannot copy struct
b = a                // ERROR: Cannot assign to struct variable
```
- Structs can only be initialized from constructors or function return values
- Cannot assign one struct variable to another

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
- Bounds checking at runtime (exits with error code 2 on out-of-bounds)
- Chains with field access: `v.data[i]`

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

Slices are views into existing fixed-size arrays. A slice is a fat pointer containing a data pointer and length, allowing functions to operate on arrays of any size.

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
- Runtime bounds checking (exits with error code 2 on out-of-bounds)

**Restrictions** (current):
- Slices are parameter-only — no local slice variables, return types, or fields
- These restrictions will lift when arenas are introduced

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
```
- Both branches must have compatible types
- Each branch's value is its last expression
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

Works with all numeric types (i64, i32, u8, u32, f64) and comptime types. Both operands must be the same concrete type (after coercion). Negation (`-x`) is not allowed on unsigned types (`u8`, `u32`).

**Comparison** (numeric operands, bool result):
- `==` Equal
- `!=` Not equal
- `<` Less than
- `<=` Less than or equal
- `>` Greater than
- `>=` Greater than or equal

Both operands must be the same concrete type (after coercion).

**Logical** (bool operands, bool result):
- `&&` Logical AND
- `||` Logical OR
- `!` Logical NOT (unary)

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
- `assert expr` - Runtime assertion (bool expr, exits with code 2 on failure)
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
- `ref` - Reference parameter/argument
- `val` - Immutable variable
- `mut` - Mutable variable/reference
- `return` - Return statement
- `assert` - Runtime assertion
- `if` - Conditional
- `else` - Else branch
- `while` - Loop
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
- `+` `-` `*` `/` - Arithmetic
- `==` `!=` `<` `<=` `>` `>=` - Comparison
- `&&` `||` `!` - Logical
- `=` - Assignment

### Punctuation
- `(` `)` - Parentheses (parameters, grouping, conditions)
- `{` `}` - Braces (blocks, function bodies, value type declarations, constructors)
- `[` `]` - Brackets (array types, array literals, indexing)
- `:` - Type annotation separator
- `;` - Array type size separator (`[T; N]`)
- `,` - Parameter/field/element separator
- `.` - Field access

## Design Decisions

### Why `func` instead of `fn`?
- More explicit and readable
- Familiar to developers from many languages
- Clear intent without abbreviation

### Why `val` and `mut`?
- `val` emphasizes immutability by default (safe)
- `mut` makes mutability explicit and visible
- Similar to Kotlin/Scala conventions

### Why explicit types for all variables?
- Clear, predictable code
- No type inference surprises
- Easier to read and maintain

### Why `=` before function body?
- Functions are values (Scala/Kotlin style)
- Consistent with variable assignment syntax
- Enables function expressions later (if/while expressions will follow same pattern)

## Future Extensions

**Not yet implemented**:
- String and character literals
- Additional types (`f32`)
- Module system
- While as expressions
- Early exit from expression blocks (`yield` keyword)

These features will be added incrementally as the compiler evolves.
