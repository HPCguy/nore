# Nore Syntax Reference

## Overview

Nore is a systems programming language with a focus on simplicity and clarity. This document defines the syntax currently supported by the compiler.

## Language Features

### Types

**Currently supported**:
- `i64` - 64-bit signed integer
- `bool` - Boolean type (`true` or `false`)
- `void` - No return value (functions only)

### Variables

**Immutable Variables** (default):
```nore
val x: i64 = 42
val flag: bool = true
```
- Keyword: `val`
- Explicit type required: Must specify type with `:` annotation
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

**Arithmetic** (i64 operands, i64 result):
- `+` Addition
- `-` Subtraction (binary and unary negation)
- `*` Multiplication
- `/` Division

**Comparison** (i64 operands, bool result):
- `==` Equal
- `!=` Not equal
- `<` Less than
- `<=` Less than or equal
- `>` Greater than
- `>=` Greater than or equal

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
- Function calls
- String and character literals
- Comments (`//`, `/* */`)
- Additional types (`i32`, `u32`, `f64`)
- Arrays and structs
- Module system
- If/while as expressions (will use `= { }` syntax)

These features will be added incrementally as the compiler evolves.
