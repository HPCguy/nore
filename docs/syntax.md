# Nore Syntax Reference

## Overview

Nore is a systems programming language with a focus on simplicity and clarity. This document defines the minimal syntax currently supported by the lexer.

## Language Features

### Variables

**Immutable Variables** (default):
```nore
val x = 42
val name = someFunction()
```
- Keyword: `val`
- Type inference: Type is inferred from the initializer
- Cannot be reassigned after initialization

**Mutable Variables**:
```nore
mut counter : i32 = 0
counter = counter + 1
```
- Keyword: `mut`
- Explicit type required: Must specify type with `:` annotation
- Can be reassigned after initialization
- Rationale: Explicit typing for mutable variables ensures consistency when values change

### Functions

```nore
func name(param1, param2) : returnType = {
    body
}
```

**Syntax**:
- Keyword: `func`
- Parameters: Comma-separated in parentheses
- Return type: Required, specified after `:`
- Equals sign: `=` precedes the function body
- Body: Enclosed in braces `{}`

**Example**:
```nore
func add(a, b) : i32 = {
    val sum = a + b
    return sum
}
```

### Return Statements

```nore
return expression
```
- Keyword: `return`
- Explicit returns only (no implicit returns from last expression)

### Types

**Currently supported**:
- `void` - No return value
- `i32` - 32-bit signed integer

### Operators

**Arithmetic**:
- `+` Addition
- `-` Subtraction
- `*` Multiplication
- `/` Division

**Assignment**:
- `=` Assignment operator

### Statement Termination

**Newline-based** (no semicolons):
```nore
val x = 5
val y = 10
return x + y
```

Statements are terminated by newlines. No semicolons required.

## Complete Example

```nore
func counter() : i32 = {
    mut count : i32 = 0
    count = count + 1
    return count
}

func main() : void = {
    val result = counter()
    return result
}
```

## Token Reference

### Keywords (6)
- `func` - Function declaration
- `val` - Immutable variable
- `mut` - Mutable variable
- `return` - Return statement
- `void` - Void type
- `i32` - 32-bit integer type

### Operators (5)
- `+` - Addition
- `-` - Subtraction
- `*` - Multiplication
- `/` - Division
- `=` - Assignment

### Punctuation (6)
- `(` `)` - Parentheses (parameters, grouping)
- `{` `}` - Braces (blocks)
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

### Why explicit types for `mut` variables?
- Ensures consistency when reassigning values
- Makes type changes visible
- Prevents subtle bugs from implicit type conversions

### Why `=` before function body?
- Functions are values (Scala/Kotlin style)
- Consistent with variable assignment syntax
- Enables potential function expressions later

### Why newline-based termination?
- Cleaner, less visual noise
- Modern language trend
- Reduces boilerplate

## Future Extensions

**Not yet implemented**:
- Comparison operators (`==`, `<`, `>`, etc.)
- Boolean type and logical operators
- Conditionals (`if`, `else`)
- Loops (`while`, `for`)
- String and character literals
- Comments (`//`, `/* */`)
- Additional types (`i64`, `u32`, `bool`, `f32`)
- Arrays and structs
- Module system

These features will be added incrementally as the parser and compiler evolve.
