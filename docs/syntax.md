# Nore Syntax Reference

## Overview

Nore is a systems programming language with a focus on simplicity and clarity. This document defines the syntax currently supported by the compiler.

## Language Features

### Types

**Currently supported**:
- `i64` - 64-bit signed integer
- `f64` - 64-bit floating-point number
- `bool` - Boolean type (`true` or `false`)
- `void` - No return value (functions only)

**Compile-time types** (internal):
- `comptime_int` - Integer literal (coerces to i64 or f64)
- `comptime_float` - Float literal (coerces to f64 only)

### Literals

**Integer literals**:
```nore
42      // comptime_int, coerces to i64 or f64
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
```

**Coercion rules**:
- `comptime_int` coerces to `i64` or `f64`
- `comptime_float` coerces to `f64` only
- No implicit coercion between concrete types (`i64` ↔ `f64`)

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
val z = x + 1           // comptime: propagates through expressions
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
- Parameters: Comma-separated with type annotations
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
```

### Function Calls

```nore
name(arg1, arg2, ...)
```

**Syntax**:
- Function name followed by parentheses
- Arguments: Comma-separated expressions
- Zero arguments: Use empty parentheses `()`
- Can be used as expressions

**Examples**:
```nore
val sum: i64 = add(10, 20)
val nested: i64 = add(mul(2, 3), 4)
val result: i64 = 1 + add(2, 3) * 2
val value: i64 = get_value()
```

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

### Operators

**Arithmetic** (numeric operands, same type result):
- `+` Addition
- `-` Subtraction (binary and unary negation)
- `*` Multiplication
- `/` Division

Works with i64, f64, and comptime types. Both operands must be compatible (see Type Coercion).

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
- `val` - Immutable variable
- `mut` - Mutable variable
- `return` - Return statement
- `assert` - Runtime assertion
- `if` - Conditional
- `else` - Else branch
- `while` - Loop
- `break` - Exit loop
- `continue` - Next iteration
- `true` - Boolean true
- `false` - Boolean false
- `i64` - 64-bit integer type
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
- `{` `}` - Braces (blocks, function bodies)
- `:` - Type annotation separator
- `,` - Parameter separator

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
- Comments (`//`, `/* */`)
- Additional types (`i32`, `u32`, `f32`)
- Arrays and structs
- Module system
- If/while as expressions (will use `= { }` syntax)

These features will be added incrementally as the compiler evolves.
