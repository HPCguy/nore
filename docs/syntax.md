# Nore Syntax Quick Reference

Terse lookup companion for the Nore language. For the full narrative (philosophy, type model, memory model, safety), see [nore.md](nore.md).

---

## Comments

```nore
// single-line comment
val x: i64 = 42  // inline comment
/* multi-line
   comment */
```

- `//` to end of line
- `/*` to `*/` (do not nest)

## Imports

```nore
import math "std/math.nore"
import file "std/file.nore"
import utils "../shared/utils.nore"
```

- `import alias "path"` at top level
- `std/` paths resolve relative to compiler binary; others relative to importing file
- Each file imported at most once
- No transitive visibility

## Qualified Access

```nore
val x: i64 = math.min_i64(3, 7)                    // function
val r: file.ReadResult = file.read_file(mut ref m, ref p) // type
val fd: i32 = fd_open(ref path, file.O_RDONLY)      // constant
val err: file.WriteResult = file.WriteResult.Err(-1) // enum variant
val p: types.Vec2 = types.Vec2 { x: 1.0, y: 2.0 }  // value constructor
```

- Functions: `alias.func_name(args)`
- Types: `alias.TypeName`
- Value constructors: `alias.TypeName { fields }`
- Enum variants: `alias.EnumName.Variant` or `alias.EnumName.Variant(payload)`
- Constants: `alias.CONSTANT_NAME`
- Table row types: `alias.TableName.Row`
- Match arms use unqualified variant names
- Built-ins and compiler-injected names remain unqualified

## Visibility (`pub`)

```nore
pub func add(a: i64, b: i64): i64 = { a + b }
pub val MAX_SIZE: i64 = 1024
pub value Point { x: i64, y: i64 }
pub enum Color { Red, Green, Blue }
pub table Particles { x: f64, y: f64 }
```

- Prefixes: `func`, `value`, `struct`, `table`, `enum`, `val`, `mut`
- `pub import` not allowed
- Only `pub` declarations accessible from other modules (S083, S084, S085)
- Same-file: all declarations visible regardless of `pub`

---

## Types

| Type | Description |
|------|-------------|
| `i64` | 64-bit signed integer |
| `i32` | 32-bit signed integer |
| `u8` | 8-bit unsigned integer |
| `u32` | 32-bit unsigned integer |
| `f64` | 64-bit floating-point |
| `bool` | Boolean (`true` / `false`) |
| `void` | No return value (functions only) |
| `str` | String (byte slice `[u8]`) |
| `Arena` | Heap memory arena |
| `[T; N]` | Fixed-size array |
| `[T]` | Slice |
| `value Name { ... }` | User-defined value type |
| `struct Name { ... }` | User-defined struct type |
| `enum Name { ... }` | User-defined enum / tagged union |
| `table Name { ... }` | Columnar storage (generates struct + value) |

**Internal compile-time types**: `comptime_int` (coerces to any integer or f64), `comptime_float` (coerces to f64 only).

## Literals

```nore
42              // comptime_int, coerces to any integer type or f64
-17             // negative integer
3.14            // comptime_float, coerces to f64 only
"hello\n"       // string literal (type: str / [u8])
'A'             // character literal (comptime_int, value 65)
'\n'            // escape character (value 10)
true / false    // boolean
```

**String escapes**: `\n` `\t` `\r` `\\` `\"` `\0`
**Character escapes**: `\n` `\t` `\r` `\\` `\'` `\0`

## Type Coercion

```nore
val x: i64 = 42      // comptime_int -> i64
val y: f64 = 42      // comptime_int -> f64
val z: f64 = 3.14    // comptime_float -> f64
val a: u8 = 255      // comptime_int -> u8 (range-checked)
val b: u8 = 256      // ERROR: out of range
```

- `comptime_int` coerces to any integer type (`i64`, `i32`, `u8`, `u32`) or `f64`
- `comptime_float` coerces to `f64` only
- No implicit coercion between concrete types
- Compile-time range checking for smaller types
- Negation of unsigned types is an error

## Type Casting

```nore
val x: i64 = 42
val y: u8 = u8(x)          // narrowing: runtime bounds check (R003)
val z: f64 = f64(x)        // widening: always safe
val w: i64 = i64(3.14)     // truncation toward zero
val c: i64 = i64(Color.Red) // enum to integer
```

Supported: `u8()`, `i32()`, `u32()`, `i64()`, `f64()`

- Narrowing / sign change: runtime bounds check, panics R003
- Widening: always safe
- Float to integer: runtime check for NaN/Inf/range, truncates toward zero
- Comptime values: range-checked at compile time (S050)
- Non-numeric: error S063

## Constant Folding

```nore
val x: i64 = 3 + 5 * 2      // folded to 13
val y: bool = 10 > 5         // folded to true
val z: i64 = 5 / 0           // ERROR: division by zero

val a = 42                   // comptime_int
val b = a + 1                // folded to 43
val O_CREAT: i32 = if (TARGET_OS == OS.MacOS) { 512 } else { 64 }  // comptime if/else
```

---

## Variables

**Immutable** (`val`):
```nore
val x: i64 = 42         // explicit type, concrete
val y = 42              // comptime: type inferred as comptime_int
val z = y + 1           // comptime: propagates through expressions
```

**Comptime constants** (untyped `val`): stay flexible until consumed. Each use site decides the concrete type.

```nore
val width = 800
val a: i64 = width       // comptime_int -> i64
val b: i32 = width       // comptime_int -> i32
val c: f64 = width       // comptime_int -> f64
val d: u8 = width        // ERROR: 800 out of range for u8
```

**Mutable** (`mut`):
```nore
mut counter: i64 = 0    // explicit type required
counter = counter + 1
```

## Global Variables

```nore
val PI = 3.14159              // comptime constant (inlined)
val MAX_SIZE: i64 = 1024      // typed constant
val GREETING: str = "hello"   // string constant
mut counter: i64 = 0          // mutable global
val origin: Vec2 = Vec2 { x: 0.0, y: 0.0 }  // value type
val data: [i64; 3] = [10, 20, 30]            // array
mut mem: Arena = arena(4096)  // global arena (auto-init/free in main)
```

- Global initializers must be constant expressions
- `arena()` is the only non-constant initializer allowed
- Function calls and `arena_alloc()` not allowed as global initializers (S057)
- Slice globals not allowed (except string literals via `val`)

---

## Functions

```nore
func name(param1: type1, param2: type2): returnType = {
    body
}
```

- Return type required (use `void` for no return)
- `=` precedes body
- Same-module forward function calls are allowed
- Mutual recursion is supported
- Last expression in block is the block's value (implicit return)

```nore
func add(a: i64, b: i64): i64 = { a + b }
func min(a: i64, b: i64): i64 = { if (a < b) { a } else { b } }
```

## Function Calls

```nore
val sum: i64 = add(10, 20)
val nested: i64 = add(mul(2, 3), 4)
process()                              // bare call statement
```

## Ref Parameters

**Read-only** (`ref`):
```nore
func length_sq(ref v: Vec2): f64 = { v.x * v.x + v.y * v.y }
val lsq: f64 = length_sq(ref p)
```

**Mutable** (`mut ref`):
```nore
func scale(mut ref v: Vec2, factor: f64): void = { v.x = v.x * factor }
scale(mut ref q, 2.0)
```

- Call site must match: `ref` for `ref` params, `mut ref` for `mut ref` params
- Argument must be addressable (variable or field-access chain)
- `mut ref` requires root variable to be `mut`
- Cannot take ref of scalar fields or array elements
- Refs are calling convention only: cannot be stored, returned, or used as locals

---

## Value Types

```nore
value Vec2 { x: f64, y: f64 }
value Color { r: u8, g: u8, b: u8, a: u8 }
```

```nore
val p: Vec2 = Vec2 { x: 1.0, y: 2.0 }    // constructor (all fields, any order)
val x: f64 = p.x                           // field access
mut q: Vec2 = Vec2 { x: 0.0, y: 0.0 }
q.x = 3.0                                  // field assignment (mut only)
mut b: Vec2 = p                            // copy semantics
```

- Fields: scalars, fixed arrays, other value types
- No slices, no indirection
- Copy semantics, pass by value or `ref`

## Struct Types

```nore
struct Entity { x: f64, y: f64, health: i64 }
```

```nore
val e: Entity = Entity { x: 1.0, y: 2.0, health: 100 }
val hp: i64 = e.health
```

- Fields: scalars, fixed arrays, value types, slices
- Cannot embed other structs or Arena
- Cannot copy (S043), cannot pass by value (S044)
- `ref` or `mut ref` only for parameters
- Can be returned from functions (direct init)

## Enum Types

```nore
enum Color { Red, Green, Blue }
val c: Color = Color.Red
assert c == Color.Red           // == and != only
assert i64(Color.Blue) == 2     // cast to integer
```

- Variants auto-numbered from 0
- Type annotation required
- No ordering (`<` `>`) or arithmetic
- Both sides must be same enum type

**Built-in**: `enum OS { Linux, MacOS }` and `TARGET_OS: OS` (compiler-injected).

## Tagged Unions

```nore
enum Option { Some(i64), None }
enum ReadResult { Ok([u8]), Err(i32) }
```

**Construction**:
```nore
val x: Option = Option.Some(42)
val y: Option = Option.None
```

**Match** (statement and expression):
```nore
enum Color { Red, Green, Blue }
match (Color.Green) {
    Red = { }
    Green = { }
    Blue = { }
}

match (opt) {
    Some(n) = { result = n }
    None = { result = 0 }
}

val x: i64 = match (opt) {
    Some(n) = { n }
    None = { 0 }
}
```

- `match (scrutinee) { arms }` with parentheses
- Scrutinee may be a plain enum or a tagged enum
- Plain enum arms: `VariantName = { body }`
- Tagged enum arms: `VariantName(binding) = { body }` or `VariantName = { body }`
- `_` wildcard: `Some(_) = { ... }`
- Exhaustiveness required
- Match expression arms must produce compatible types (S076)
- Slice-payload tagged unions are non-copyable (S043, S044, S045)
- Payload types: scalars, fixed arrays, value types, slices, plain enums. No structs/Arena (S082)
- No `==`/`!=` on tagged unions (use `match`)
- Plain enum arms cannot bind payloads

---

## Arrays

```nore
val arr: [i64; 3] = [1, 2, 3]
val grid: [[i64; 2]; 3] = [[1, 2], [3, 4], [5, 6]]
```

- Size must be a positive integer literal
- Indexing: `arr[i]`, bounds-checked (R002)
- Element assignment: `arr[0] = 99` (mut only)
- Value semantics: copy on assignment
- Sub-slicing: `arr[1..4]` produces a slice

## Slices

```nore
mut mem: Arena = arena(4096)
val data: [i64] = arena_alloc(mut ref mem, 10)
val result: [i64] = get_data(mut ref mem, 5)
val sub: [i64] = data[2..5]
```

- Fat pointer: `{data, length}`
- `.len` returns element count as `i64`
- Indexing: `data[i]`, bounds-checked (R002)
- Parameters must use `ref` or `mut ref`
- Locals initialized via `arena_alloc()`, function call, or sub-slice only (S046)

**Sub-slicing** (`expr[start..end]`):
```nore
val sub: [i64] = data[2..5]    // elements 2, 3, 4
val head: [i64] = data[..3]    // same as data[0..3]
val tail: [i64] = data[2..]    // same as data[2..data.len]
val all: [i64] = data[..]      // full slice
```

- Half-open range `[start..end)`
- Bounds-checked (R004)
- Works on slices and arrays, always produces a slice
- Can be chained: `data[1..4][0]`
- Can be passed to `ref` params: `sum(ref arr[1..4])`

## Strings

```nore
val greeting: str = "hello"    // static memory, zero cost
val h: u8 = greeting[0]       // 104 (ASCII 'h')
assert greeting.len == 5
```

- `str` is `[u8]` (interchangeable)
- String literals: `val` only (S054 for `mut`)
- Parameters: `ref` or `mut ref` (same as slices)
- Escapes: `\n` `\t` `\r` `\\` `\"` `\0`

---

## Arenas

```nore
mut mem: Arena = arena(4096)
val data: [i64] = arena_alloc(mut ref mem, 10)
arena_reset(mut ref mem)       // invalidates all slices from mem (S056)
```

- `arena(capacity)` creates with byte capacity
- `arena_alloc(mut ref arena, count)` allocates zero-initialized elements
- `arena_reset(mut ref arena)` reclaims all memory
- Arena params: `ref` or `mut ref` only
- Auto-freed on scope exit, `return`, `break`/`continue`
- Ref-param arenas not freed by callee
- Slice lifetime: cannot outlive arena (S053)
- Escape analysis: tracks direct, indirect, and transitive escape

**Global arenas**: `mut` at top level, auto-init at start of `main`, auto-free at end.

## Tables

```nore
value Vec2 { x: f64, y: f64 }
table Particles { pos: Vec2, life: i64 }
```

Generates: struct `Particles` (`pos: [Vec2]`, `life: [i64]`, `_len: i64`) + value `Particles.Row` (`pos: Vec2`, `life: i64`).

```nore
mut mem: Arena = arena(65536)
mut p: Particles = table_alloc(mut ref mem, 100)
table_insert(mut ref p, Particles.Row { pos: Vec2 { x: 1.0, y: 2.0 }, life: 100 })
val row: Particles.Row = table_get(ref p, 0)
val n: i64 = table_len(ref p)
p.life[0] = 50                             // direct column access
```

- Fields must be value-compatible (S059)
- Table is a struct: no copy, `ref` only, no nesting
- Row is a value: copyable, embeddable

---

## Control Flow

**If/else**:
```nore
if (condition) { ... }
if (condition) { ... } else { ... }
if (condition) { ... } else if (other) { ... } else { ... }
val x: i64 = if (a < b) { a } else { b }   // expression
return if (x < 0) { 0 - x } else { x }     // after return
```

- Condition must be `bool`
- `else if` chains are allowed in statements and expressions
- Expression form: both branches must have compatible types
- Comptime conditions folded at compile time

**While**:
```nore
while (condition) { ... }
```

- `break` exits, `continue` skips to next iteration

**For** (range-based):
```nore
for i in 0..n { ... }
```

- Exclusive upper bound (`0..5` = 0,1,2,3,4)
- Loop variable is `val i64` (immutable)
- End expression evaluated once
- Empty ranges skip the body
- `break` and `continue` work

**Expression blocks**:
```nore
val y: i64 = {
    val a: i64 = 10
    a + 5       // last expression is the block's value
}
```

---

## Operators

**Arithmetic** (numeric): `+` `-` `*` `/` `%` (modulo: integers only)
**Bitwise** (integers): `&` `|` `^` `~` `<<` `>>`
**Comparison** (numeric, produces bool): `==` `!=` `<` `<=` `>` `>=`
**Logical** (bool): `&&` `||` `!`
**Assignment**: `=` `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=` (mutable only)

**Compound assignment**:
```nore
counter += 1
mask &= 15
arr[i] <<= 1
```

- Same type rules as `target = target op value`
- Works for variables, fields, and index expressions
- Target must be rooted in a mutable variable

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

- Bitwise binds tighter than comparison: `a & mask == 0` means `(a & mask) == 0`
- Both operands must be same concrete type (after coercion)
- Modulo follows C truncation semantics
- No comparison of `bool` values

## Statements

- `val name: type = expr` / `mut name: type = expr`
- `name = expr` (assignment, mutable only)
- `name op= expr` (`+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`)
- `expr.field = expr` / `expr[index] = expr`
- `expr.field op= expr` / `expr[index] op= expr`
- `name(args...)` (bare function call)
- `return expr`
- `assert expr` (R001, exits with code 2)
- `break` / `continue`

---

## Native Declarations

```nore
native func fd_write(fd: i32, ref data: [u8]): i64
native func exit(code: i32): void
```

- Top level, no body (P060 if `=` follows)
- Name must match known built-in (S086)
- Always module-private (`pub native` is an error)
- Wrapper pattern: `native func exit(...)` + `pub func exit(...)` (native takes precedence inside module)
- Required before using the built-in (S087)

| Native | Signature | Used by |
|--------|-----------|---------|
| `fd_write` | `(fd: i32, ref data: [u8]): i64` | `std/io.nore`, `std/file.nore` |
| `fd_read` | `(fd: i32, mut ref buf: [u8]): i64` | `std/io.nore`, `std/file.nore` |
| `fd_open` | `(ref path: str, flags: i32): i32` | `std/file.nore` |
| `fd_close` | `(fd: i32): void` | `std/file.nore` |
| `fd_seek` | `(fd: i32, offset: i64, whence: i32): i64` | `std/file.nore` |
| `mem_copy` | `(mut ref dst: [u8], ref src: [u8]): i64` | `std/string.nore` |
| `exit` | `(code: i32): void` | `std/sys.nore` |
| `args` | `(mut ref mem: Arena): [str]` | `std/sys.nore` |

## I/O Built-in Functions

**fd_write(fd, ref data)**: write bytes to fd. Returns `i64` bytes written (negative on error).
```nore
val n: i64 = fd_write(io.STDOUT, ref "Hello\n")
```

**fd_read(fd, mut ref buf)**: read bytes from fd. Returns `i64` bytes read (0 = EOF).
```nore
mut buf: [u8; 256] = [0, 0, ...]
val n: i64 = fd_read(io.STDIN, mut ref buf)
```

**fd_open(ref path, flags)**: open file. Returns `i32` fd (negative on error). Null-terminated, max 4095 bytes, perms 0644.
```nore
val fd: i32 = fd_open(ref "file.txt", file.O_RDONLY)
```

**fd_close(fd)**: close fd. Returns `void`.

**fd_seek(fd, offset, whence)**: seek. Returns `i64` new position.
```nore
val size: i64 = fd_seek(fd, 0, file.SEEK_END)
```

**exit(code)**: terminate process. Returns `void` (never returns).

**mem_copy(mut ref dst, ref src)**: copy `min(dst.len, src.len)` bytes. Returns `i64` bytes copied. Uses `memmove`.

**Type errors**: S064 (fd not integer), S065 (data/buf not byte buffer), S066 (buf immutable).

## Built-in Functions (always available)

- `arena(capacity)` - Create Arena with byte capacity
- `arena_alloc(mut ref arena, count)` - Allocate `count` zero-initialized elements
- `arena_reset(mut ref arena)` - Reclaim all arena memory (invalidates slices, S056)
- `table_alloc(mut ref arena, count)` - Allocate columnar storage for `count` rows
- `table_len(ref table)` - Get current row count
- `table_get(ref table, index)` - Get row value at index (R002)
- `table_insert(mut ref table, row)` - Insert a row

## Predefined Constants

- `TARGET_OS` - `OS.Linux` or `OS.MacOS` (compiler-injected, no import)
- `io.STDIN` (0), `io.STDOUT` (1), `io.STDERR` (2) - from `std/io.nore`
- `file.O_RDONLY` (0), `file.O_WRONLY` (1), `file.O_RDWR` (2) - from `std/file.nore`
- `file.O_CREAT`, `file.O_TRUNC`, `file.O_APPEND` - platform-specific, from `std/file.nore`
- `file.SEEK_SET` (0), `file.SEEK_CUR` (1), `file.SEEK_END` (2) - from `std/file.nore`
- Flags combine with `|`: `file.O_WRONLY | file.O_CREAT | file.O_TRUNC`

---

## Standard Library

**`std/io.nore`** (import as `io`):
- `io.print(ref s)`, `io.println(ref s)` - stdout
- `io.eprint(ref s)`, `io.eprintln(ref s)` - stderr
- `io.print_i64(n)` - integer to stdout
- `io.Writer` struct, `io.writer_new(mut ref mem, capacity)` - buffered writer
- `io.write_str(mut ref w, ref s)`, `io.write_byte(mut ref w, b)`, `io.write_i64(mut ref w, n)`
- `io.write_i64_padded(mut ref w, n, width)` - right-aligned with leading spaces
- `io.flush(ref w, fd)`, `io.writer_reset(mut ref w)`

**`std/math.nore`** (import as `math`):
- `math.min_i64`, `math.max_i64`, `math.abs_i64`, `math.clamp_i64`
- `math.min_f64`, `math.max_f64`, `math.abs_f64`, `math.clamp_f64`

**`std/string.nore`** (import as `string`):
- `string.is_digit(c)`, `string.is_alpha(c)`, `string.is_space(c)` - char classification
- `string.str_eq(ref a, ref b)`, `string.str_starts_with(...)`, `string.str_ends_with(...)` - comparison
- `string.str_find(ref s, ref needle)`, `string.str_find_byte(ref s, needle)`, `string.str_rfind_byte(ref s, needle)`, `string.str_contains(...)` - searching
- `string.str_copy(mut ref mem, ref s)`, `string.str_concat(mut ref mem, ref a, ref b)`, `string.str_concat3(mut ref mem, ref a, ref b, ref c)` - copy/concatenation
- `string.fmt_i64(mut ref buf, n)`, `string.i64_to_str(mut ref mem, n)`, `string.str_to_i64(ref s)`, `string.str_to_f64(ref s)`
- `string.ParseResult` - `Ok(i64)` or `None`
- `string.ParseFloatResult` - `Ok(f64)` or `None`

**`std/file.nore`** (import as `file`):
- `file.read_file(mut ref mem, ref path)` - returns `file.ReadResult` (`Ok([u8])` / `Err(i32)`)
- `file.write_file(ref path, ref data)` - returns `file.WriteResult` (`Ok(i64)` / `Err(i32)`)

**`std/sys.nore`** (import as `sys`):
- `sys.exit(code)` - terminate process
- `sys.get_args(mut ref mem)` - command-line args as `[str]` (`argv[0]` is program name, `--run` forwards args after `--`)

**`std/json.nore`** (import as `json`):
- `json.json_parse(mut ref nodes, ref src)` - parse JSON, returns `json.JsonResult` (`Ok(i64)` root index / `Err(JsonError)`)
- `json.json_kind(ref nodes, node)` - node kind (`json.JsonKind`: Null, Bool, Number, Str, Array, Object)
- `json.json_child(ref nodes, parent)` / `json.json_next(ref nodes, node)` - tree traversal
- `json.json_str(ref src, ref nodes, node)` / `json.json_key(ref src, ref nodes, node)` - string/key access
- `json.json_num(ref nodes, node)` / `json.json_bool(ref nodes, node)` - value access
- `json.json_len(ref nodes, node)` / `json.json_count(ref nodes)` - child count / total nodes
- `json.json_find(ref src, ref nodes, parent, ref key)` - find child by key

---

## Token Reference

### Keywords

`func` `value` `struct` `enum` `table` `match` `import` `pub` `native` `str` `Arena` `ref` `val` `mut` `return` `assert` `if` `else` `while` `for` `in` `break` `continue` `true` `false` `i64` `i32` `u8` `u32` `f64` `bool` `void`

### Operators

`+` `+=` `-` `-=` `*` `*=` `/` `/=` `%` `%=` `&` `&=` `|` `|=` `^` `^=` `~` `<<` `<<=` `>>` `>>=` `==` `!=` `<` `<=` `>` `>=` `&&` `||` `!` `=`

### Punctuation

| Token | Usage |
|-------|-------|
| `(` `)` | Parameters, grouping, conditions |
| `{` `}` | Blocks, bodies, declarations, constructors |
| `[` `]` | Array types, literals, indexing, sub-slicing |
| `:` | Type annotation separator |
| `;` | Array type size separator (`[T; N]`) |
| `,` | Parameter/field/element separator |
| `.` | Field access, module access, enum variant access |
| `..` | Range operator (for loops, sub-slicing) |

`"` delimits string literals (consumed by lexer, not a standalone token).

---

## Future Extensions

- Multiline strings
- Additional types (`f32`)
- While as expressions
- Early exit from expression blocks (`yield`)
