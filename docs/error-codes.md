# Error Codes

Errors use numeric codes with phase prefixes for stable testing:

| Prefix | Group | Description |
|--------|-------|-------------|
| I | Internal | Bugs, OOM (exit 101) |
| D | Driver | CLI, files, backend (exit 1) |
| L | Lexer | Invalid characters (exit 1) |
| P | Parser | Syntax errors (exit 1) |
| S | Semantic | Type/scope errors (exit 1) |
| R | Runtime | Assertion failures (exit 2) |

## Error Format
```
# Source errors (with location)
file.nore:3:15: error[P002]: Expected ')' after expression

# Driver errors (no location)
error[D002]: Unknown flag: --invalid
```

## All Error Codes

- **L001**: Invalid character
- **L002**: Unterminated block comment
- **L003**: Unterminated string literal
- **L004**: Invalid escape sequence
- **L005**: Empty character literal
- **L006**: Character literal must contain exactly one character
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
- **P041**: Expected enum type name after 'enum'
- **P042**: Expected '{' after enum name
- **P043**: Expected '}' to close enum
- **P044**: Expected variant name in enum
- **P045**: Unknown enum variant
- **P046**: Expected import path after module alias
- **P047**: Expected ']' after slice range
- **P048**: Expected ')' after variant payload type
- **P049**: Expected '(' after 'match'
- **P050**: Expected ')' after match scrutinee
- **P051**: Expected '{' for match body
- **P052**: Expected variant name in match arm
- **P053**: Expected '=' after match pattern
- **P054**: Expected '}' to close match
- **P055**: Expected ')' after match binding
- **P056**: 'pub' must be followed by a declaration
- **P057**: 'pub' cannot be used on imports
- **P058**: Expected module alias after 'import'
- **D007**: Cannot find imported file

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
- **S043**: Cannot copy struct/Arena/slice-bearing tagged union variable (not copyable)
- **S044**: Struct/Arena/slice-bearing tagged union parameter must use 'ref' or 'mut ref'
- **S045**: Cannot embed struct/Arena/slice-bearing tagged union type as field
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
- **S061**: modulo operator '%' is not supported on floating-point types
- **S062**: bitwise operators are not supported on floating-point types
- **S063**: Cannot cast non-numeric type (only numeric types can be cast)
- **S064**: I/O built-in fd argument must be integer type
- **S065**: I/O built-in data/buffer/path argument must be `[]u8` or `[u8; N]`
- **S066**: `fd_read()` buffer must be mutable
- **S067**: Arithmetic or ordering not allowed on enum type
- **S068**: Cannot compare different enum types
- **S069**: Duplicate variant name in enum
- **S070**: Duplicate type name (enum name already used)
- **S071**: Slice range bound must be integer type
- **S072**: Cannot sub-slice non-array/slice type
- **S073**: Match scrutinee is not a tagged enum type
- **S074**: Non-exhaustive match (missing variants)
- **S075**: Duplicate variant in match arms
- **S076**: Match arm types incompatible (expression form)
- **S077**: Payload type mismatch in tagged union variant construction
- **S078**: Unexpected payload for non-data variant
- **S079**: Missing payload for data variant
- **S080**: Cannot compare tagged enum (use match instead)
- **S081**: Cannot cast tagged enum to numeric type
- **S082**: Variant payload type not value-compatible (no structs or Arena)
- **R001**: Assertion failed
- **R002**: Array index out of bounds
- **R003**: Cast overflow (value out of range for target type at runtime)
- **R004**: Slice bounds out of range

## Error Functions

- `panic(code, fmt, ...)` - Internal errors, exits 101
- `error(code, fmt, ...)` - User errors without location
- `diagnostic(code, line, col, fmt, ...)` - Collects user errors with location
- `report_errors_and_exit()` - Prints all collected errors and exits
- `too_many_errors()` - Returns true when error limit (10) is reached

## Error Recovery

The compiler uses panic mode recovery to collect multiple errors:
- On parse error, skips to next statement boundary and continues
- Collects up to 10 errors before stopping
- Reports all errors at the end of compilation
- Semantic analysis runs even if there were parse errors
