# Nore Programming Language

## Overview

Nore is a systems programming language that makes data-oriented design the path of least resistance. Instead of hiding memory layout behind objects, Nore gives you direct control over how data is organized — columnar tables, arena allocation, explicit value vs resource semantics — with compile-time safety guarantees and zero runtime overhead.

The compiler is a self-contained, single-file C program that translates Nore source code into native executables via C as an intermediate representation.

## What Makes Nore Different

**Data layout is a first-class concern.** A single `table` declaration generates columnar storage (struct-of-arrays) with type-safe row access — the kind of layout that games, simulations, and data-heavy systems need for cache performance, without manual bookkeeping.

**Two kinds of types, one clear rule.** `value` types are plain data: they live on the stack, copy freely, and compose into arrays and tables. `struct` types own resources: they hold slices of arena-allocated memory, pass by reference only, and cannot be copied. No hidden allocations, no implicit clones.

**Arenas replace malloc/free.** All heap memory comes from arenas. The compiler tracks which slices come from which arena and rejects programs where a slice could outlive its arena — at compile time, with no garbage collector and no runtime cost.

**Explicit is better than implicit.** Parameters are `ref` or `mut ref` at both declaration and call site. Mutability is visible everywhere. There are no hidden copies, no move semantics to reason about.

## Design Documents

- [docs/data-oriented-design.md](docs/data-oriented-design.md) — Type model, tables, arenas, and memory safety approach
- [docs/arena-safety.md](docs/arena-safety.md) — Escape analysis, slice lifetime tracking, and safety guarantees

## Architecture

The compiler follows a multi-stage pipeline:

1. **Frontend** — Lexer tokenizes source, parser builds an AST
2. **Semantic analysis** — Type checking, escape analysis, arena lifetime validation
3. **Code generation** — AST translates to C99 code
4. **Native compilation** — Clang compiles generated C to a native binary

## Project Status

**Current Phase**: Early development

The compiler is being developed as a single-file C program (`nore.c`) containing:
- Lexer implementation
- Parser implementation
- AST data structures
- C code generator
- Clang integration layer

## Build Requirements

- **Compiler**: Clang
- **C Standard**: C99
- **Platform**: Unix-like systems (Linux, macOS, BSD)

## Build & Usage

```bash
# Build the Nore compiler (optimized)
make

# Build with debug symbols
make debug

# Clean build artifacts
make clean

# Compile a Nore program (outputs ./program by default)
./nore program.nore

# Specify output path explicitly
./nore program.nore -o build/program

# Compile and run immediately (temp binary, auto-cleaned)
./nore --run program.nore

# Debug flags (inspect compiler stages)
./nore program.nore --lexer    # Print lexer tokens
./nore program.nore --parser   # Print AST structure
./nore program.nore --codegen  # Print generated C code (IR)

# Combine flags
./nore program.nore --parser --codegen -o program
```

## Language Syntax

See [docs/syntax.md](docs/syntax.md) for the complete language syntax reference.

### Code Generation Details
- Variables tracked in linked scope chain with mutability and type
- `i64` generates `int64_t` in C
- `i32` generates `int32_t` in C
- `u8` generates `uint8_t` in C
- `u32` generates `uint32_t` in C
- `f64` generates `double` in C
- `bool` generates `int` in C
- `val` declarations generate `const` prefix
- `true` generates `1`, `false` generates `0`
- Integer literals generate `42L` suffix
- Float literals generate `3.14` (no suffix)
- Constant folding evaluates literal-only expressions at compile time
- Type checking runs before code generation
- All expressions and statements compile to C99 code

## Error Handling

### Error Code System
Errors use numeric codes with phase prefixes for stable testing:

| Prefix | Group | Description |
|--------|-------|-------------|
| I | Internal | Bugs, OOM (exit 101) |
| D | Driver | CLI, files, backend (exit 1) |
| L | Lexer | Invalid characters (exit 1) |
| P | Parser | Syntax errors (exit 1) |
| S | Semantic | Type/scope errors (exit 1) |
| R | Runtime | Assertion failures (exit 2) |

### Current Error Codes
- **L001**: Invalid character
- **L002**: Unterminated block comment
- **L003**: Unterminated string literal
- **L004**: Invalid escape sequence
- **P006**: Integer literal out of range (overflow)
- **P007**: Expected '}' to close block
- **P008**: Expected '(' after 'if'
- **P009**: Expected ')' after if condition
- **P010**: Expected '{' for if/else body
- **P011**: Expected '(' after 'while'
- **P012**: Expected ')' after while condition
- **P013**: Expected '{' for while body
- **P014**: Expected ':' after identifier (type annotation required)
- **P015**: Expected type (i64, i32, u8, u32, f64, bool, str, or Arena)
- **P016**: Expected declaration at top level
- **P017**: Expected function name after 'func'
- **P018**: Expected '(' after function name
- **P019**: Expected ')' after parameters
- **P020**: Expected ':' before return type
- **P021**: Expected return type (i64, i32, u8, u32, f64, bool, str, Arena, or void)
- **P022**: Expected '=' before function body
- **P023**: Expected '{' for function body
- **P024**: Expected value type name after 'value'
- **P025**: Expected '{' after value type name
- **P026**: Expected field name in value type
- **P027**: Expected '}' to close value type
- **P028**: Expected field name in constructor
- **P029**: Expected ':' after field name in constructor
- **P030**: Expected '}' to close constructor
- **P031**: Expected element type in array type
- **P032**: Expected ';' in array type
- **P033**: Expected array size
- **P034**: Expected ']'
- **P035**: Expected ']' to close array literal
- **P036**: Expected 'ref' after 'mut' in parameter
- **P037**: Expected 'ref' after 'mut' in argument
- **P038**: Expected 'in' after loop variable in for-loop
- **P039**: Expected '..' in for-loop range
- **P040**: Expected '{' for for-loop body
- **S001**: Duplicate variable declaration
- **S002**: Undeclared variable
- **S003**: Cannot assign to immutable variable
- **S004**: 'break' outside of loop
- **S005**: 'continue' outside of loop
- **S006**: Type mismatch
- **S007**: Condition must be bool (if/while/assert)
- **S008**: Duplicate function declaration
- **S009**: No 'main' function defined
- **S010**: Invalid main signature (must be `(): void`)
- **S011**: Duplicate parameter name
- **S012**: void is not valid as parameter type
- **S013**: Return type mismatch
- **S014**: Void function cannot return a value
- **S015**: Non-void function must return a value
- **S016**: Undefined function
- **S017**: Wrong number of arguments
- **S018**: Argument type mismatch
- **S019**: Division by zero in constant expression
- **S020**: Type annotation required (expression is not compile-time constant)
- **S021**: If/else branches have incompatible types
- **S022**: Duplicate field in value type declaration
- **S023**: Invalid type for value type field
- **S024**: Unknown field in constructor
- **S025**: Missing field in constructor
- **S026**: Duplicate field in constructor
- **S027**: Not a value type (constructor on non-value)
- **S028**: Field access on non-value type
- **S029**: Unknown field access
- **S030**: Cannot assign to field of immutable variable
- **S031**: Array size must be positive integer
- **S032**: Array literal wrong number of elements
- **S033**: Array element type mismatch
- **S034**: Index must be integer
- **S035**: Cannot index non-array type
- **S036**: Cannot assign to element of immutable variable
- **S037**: ref not allowed on value type fields
- **S038**: ref/mut ref mismatch between call site and parameter
- **S039**: Cannot take reference of non-addressable expression
- **S040**: Cannot pass mut ref to immutable variable
- **S041**: Cannot take reference of scalar field (just copy it)
- **S042**: Cannot take reference of array element
- **S043**: Cannot copy struct/Arena variable (not copyable)
- **S044**: Struct/Arena parameter must use 'ref' or 'mut ref'
- **S045**: Cannot embed struct/Arena type as field
- **S046**: Slice type not allowed as local variable (use arena_alloc or function call)
- **S047**: Slice type not allowed as field (value types only; allowed in structs)
- **S048**: (no longer emitted — replaced by S053 escape analysis)
- **S049**: Slice parameter must use 'ref' or 'mut ref'
- **S050**: Literal out of range for target type / integer overflow in constant expression
- **S051**: arena_alloc() requires Arena type
- **S052**: Cannot arena_alloc from immutable Arena (use 'mut')
- **S053**: Slice escapes local arena (direct, indirect via struct, or transitive via function call)
- **S054**: String literals cannot be mutable (use 'val')
- **S055**: Cannot arena_reset immutable Arena (use 'mut')
- **S056**: Use of slice invalidated by arena reset
- **S057**: Global variable initializer must be a constant expression (or arena constructor)
- **S058**: For-loop range bound must be integer type
- **S059**: Table field must be value-compatible (no slices, structs, or Arena)
- **S060**: table_len/table_get/table_insert requires a table type
- **R001**: Assertion failed
- **R002**: Array index out of bounds

### Error Format
```
# Source errors (with location)
file.nore:3:15: error[P002]: Expected ')' after expression

# Driver errors (no location)
error[D002]: Unknown flag: --invalid
```

### Error Functions
- `panic(code, fmt, ...)` - Internal errors, exits 101
- `error(code, fmt, ...)` - User errors without location
- `diagnostic(code, line, col, fmt, ...)` - Collects user errors with location
- `report_errors_and_exit()` - Prints all collected errors and exits
- `too_many_errors()` - Returns true when error limit (10) is reached

### Error Recovery
The compiler uses panic mode recovery to collect multiple errors:
- On parse error, skips to next statement boundary and continues
- Collects up to 10 errors before stopping
- Reports all errors at the end of compilation
- Semantic analysis runs even if there were parse errors

### Testing
```bash
make test          # Run all tests (errors + success)
make test-errors   # Run error code tests only
make test-success  # Run success tests only
```
- Error tests in `tests/errors/` named by expected code (e.g., `P002_missing_rparen.nore`)
- Success tests in `tests/success/` — programs with assertions, compiled and run via `--run` flag
- Test runners: `tests/run_error_tests.sh` and `tests/run_success_tests.sh`

## Development Roadmap

1. **Phase 1**: Lexer and basic tokenization
2. **Phase 2**: Parser and AST construction
3. **Phase 3**: C code generation for basic constructs
4. **Phase 4**: Clang integration and native compilation
5. **Phase 5**: Language feature expansion
6. **Phase 6**: Standard library development

### DOD Type System Implementation Sequence

Each step builds on the previous one. See [docs/data-oriented-design.md](docs/data-oriented-design.md) for the full design.

1. **`value` types** — composite data with named fields, stack-only, pass by copy
2. **Fixed-size arrays** `[T; N]` — stack-allocated, value-compatible
3. **`ref` parameters** — pass by reference for functions (required before structs)
4. **`struct` types** — resource owners, ref-only passing, may contain slices
5. **Slices `[]T` + Arenas** — first heap allocation, compile-time lifetime checks
6. **`str` type** — byte slice, falls out of slice implementation
7. **`table` sugar** — generates struct + value, the DOD payoff

## Technical Decisions

### Why a single-file compiler?
Simplifies building, distribution, and studying the compiler. Can be refactored into modules later if needed.

### Why C as intermediate representation?
Avoids platform-specific backends, inherits Clang's optimization passes, and enables rapid compiler development. Proven approach (used by early C++, Nim, and others).

### Why Clang?
Modern, actively maintained, with strong cross-compilation support and excellent error messages.

## Contributing

This project is in early development. Design discussions and architecture feedback are welcome.

## License

BSD 3-Clause License with patent grant. See [LICENSE](LICENSE) and [PATENTS](PATENTS).
