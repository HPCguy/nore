# Standard Library Roadmap

## Goal

Nore's standard library should be the thinnest possible layer that makes real programs possible. Every function should earn its place. The stdlib follows the same philosophy as the language: explicit allocation, no hidden costs, and the developer always knows what's happening.

## Core Principle

**The compiler grows only when it must. Everything else is written in Nore.**

Built-in primitives provide the minimum bridge to the operating system. The rest, string operations, formatting, file helpers, is ordinary Nore code that users could write themselves. The stdlib just saves them the trouble.

Stdlib modules should import and reuse each other to avoid code duplication and ensure consistency. Higher-level modules build on lower-level ones (e.g., `std/io.nore` imports `std/string.nore` for number formatting).

---

## What We Shipped (v1)

The foundation is complete. Four stdlib modules and a set of compiler built-ins provide the minimum viable platform for real programs.

### Compiler Built-ins

Things that require compiler support because they cannot be expressed in Nore:

| Built-in | Purpose |
|----------|---------|
| `fd_write(fd, ref data)` | Write bytes to a file descriptor |
| `fd_read(fd, mut ref buf)` | Read bytes from a file descriptor |
| `fd_open(ref path, flags)` | Open a file, returns fd or negative error |
| `fd_close(fd)` | Close a file descriptor |
| `fd_seek(fd, offset, whence)` | Seek within a file |
| `mem_copy(mut ref dst, ref src)` | Bulk byte copy (maps to C `memmove`) |
| `exit(code)` | Terminate the process |
| `args(mut ref mem)` | Get command-line arguments as `[str]` (arena-allocated) |

Compiler-injected constants: `TARGET_OS`.

### Standard Library Modules

| Module | What it provides |
|--------|-----------------|
| `std/math.nore` | `min_i64`, `max_i64`, `abs_i64`, `clamp_i64`, `min_f64`, `max_f64`, `abs_f64`, `clamp_f64` |
| `std/string.nore` | Character classification (`is_digit`, `is_alpha`, `is_space`), comparison (`str_eq`, `str_starts_with`, `str_ends_with`), searching (`str_find`, `str_contains`), concatenation (`str_concat`), formatting (`fmt_i64`, `i64_to_str`), parsing (`str_to_i64`) |
| `std/io.nore` | `STDIN`, `STDOUT`, `STDERR`, `print`, `println`, `print_i64` (imports `std/string.nore`). Declares native: `fd_write`, `fd_read` |
| `std/file.nore` | `read_file`, `write_file`, platform-specific I/O constants (`O_RDONLY`, `O_CREAT`, `SEEK_SET`, etc. via `TARGET_OS`). Declares native: `fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek` |
| `std/sys.nore` | `exit(code)` process termination, `get_args(mut ref mem)` command-line arguments as `[str]`. Declares native: `exit`, `args` |

All modules tested via `tests/std/`. Comprehensive success and error test suites cover the full language.

### Error Handling Debt (mostly resolved)

The v1 stdlib originally used sentinel values for errors because tagged unions did not exist yet. Tagged unions have since shipped, and most functions have been retrofitted:

| Function | Status | Current behavior |
|----------|--------|-----------------|
| `read_file` | Fixed | Returns `ReadResult.Ok([u8])` or `ReadResult.Err(i32)` |
| `write_file` | Fixed | Returns `WriteResult.Ok(i64)` or `WriteResult.Err(i32)` |
| `str_to_i64` | Fixed | Returns `ParseResult.Ok(i64)` or `ParseResult.None` |
| `fd_open` | Open | Still returns raw `i32` (negative on error). A stdlib wrapper returning a Result type is not yet implemented. |

---

## What Belongs Where

### In the Compiler (built-ins)

Things that require compiler support because they cannot be expressed in Nore today:

- **I/O primitives**: writing bytes to a file descriptor, reading bytes, opening/closing files. These need syscall access that Nore source code cannot express.
- **Process control**: `exit(code)` to terminate with a status code.
- **Command-line arguments**: access to argc/argv requires compiler-level wiring.

### In the Standard Library (.nore files)

Everything that *can* be a Nore function *should* be:

- String comparison, searching, slicing, splitting
- Number-to-string and string-to-number conversion
- Formatted output (`print`, `println`, and future format functions)
- File reading helpers (`read_file`, `read_lines`)
- Math utilities (`min`, `max`, `abs`, `clamp`)

### Not in the Standard Library

Things that don't belong, at least not yet:

- Networking, HTTP. These are ecosystem libraries, not core stdlib.
- Concurrency primitives. The language needs a concurrency story first.
- Generic collections (hash map, dynamic array). Need generics or code generation.

---

## Next: Program-Driven Development

The foundation phase was built bottom-up: language features first, then stdlib layers on top. The next phase combines both approaches. Some language features are needed to fix what already exists (module namespaces, native declarations). Others will be driven top-down by attempting real programs.

### Milestone 0: Fix the Foundation

Before writing new programs, fix the existing stdlib. This is real work that validates new language features against existing code.

**Move I/O constants to stdlib:** Done. `STDIN`, `STDOUT`, `STDERR` are now defined in `std/io.nore` as plain `i32` constants, just like `O_RDONLY` and `SEEK_SET` in `std/file.nore`. Removed from compiler injection.

**Tagged unions:** Done. The language supports tagged unions with `match` expressions, exhaustiveness checking, and slice payloads. The stdlib has been retrofitted:
- `read_file` returns `ReadResult { Ok([u8]), Err(i32) }`
- `write_file` returns `WriteResult { Ok(i64), Err(i32) }`
- `str_to_i64` returns `ParseResult { Ok(i64), None }`
- `fd_open` stdlib wrapper returning Result: not yet done

**Module namespaces and visibility:** Done. The import system now provides proper module boundaries:
- `import alias "path"` syntax with mandatory module alias
- Qualified access: `alias.name` for functions, types, constants, and enum variants
- `pub` visibility modifier enforced: only `pub` declarations are accessible from other modules
- No transitive visibility: each file must import what it uses directly
- Name mangling in generated C code prevents cross-module collisions (`ni_alias__name`)
- Three-level dot access for enum variants: `file.ReadResult.Ok(data)`
- Built-in functions (`fd_write`, `arena`, `exit`, etc.) remain in global scope

**Gate built-ins behind `native` declarations:** Done (Category A). The `native func` keyword lets `.nore` modules declare built-in function signatures. The compiler validates the name against a known list and requires the declaration before the built-in can be used. Eight native functions are gated: `fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek`, `mem_copy`, `exit`, `args`. Each stdlib module declares only the natives it uses:
- `std/io.nore`: `native func fd_write(...)`, `native func fd_read(...)`
- `std/file.nore`: `native func fd_write(...)`, `native func fd_read(...)`, `native func fd_open(...)`, `native func fd_close(...)`, `native func fd_seek(...)`
- `std/string.nore`: `native func mem_copy(...)`
- `std/sys.nore`: `native func exit(...)`, `native func args(...)` with `pub` wrappers

Arena/table built-ins (`arena`, `arena_alloc`, `arena_reset`, `table_alloc`, `table_len`, `table_get`, `table_insert`) remain in global scope (Category B, deferred).

**Remaining requires:** None for Milestone 0 (foundation complete).

### Milestone 1: Cat Clone

Read a file, write it to stdout. The simplest useful program.

```nore
import io "std/io.nore"
import file "std/file.nore"

func main(): void = {
    mut mem: Arena = arena(65536)
    val result: file.ReadResult = file.read_file(mut ref mem, ref "input.txt")
    match (result) {
        Ok(contents) = { io.print(ref contents) }
        Err(code) = { io.println(ref "error reading file") }
    }
}
```

**What it needs:**
- Read a filename from command-line arguments
- Read file contents, write to stdout
- Handle errors properly (file not found, read failure)

**Known gaps:** None. Command-line arguments shipped via `sys.get_args()`.

**Stdlib additions:** None expected. `read_file` and `fd_write` already exist. Error handling already fixed in Milestone 0.

### Milestone 2: Word Count

Read a file, count lines, words, and characters. Print formatted results.

```nore
// Desired output: "  42  108  723 input.txt"
```

**What it needs:**
- Everything from Milestone 1
- Split text on whitespace boundaries
- Count lines, words, characters
- Print multiple values in a formatted line

**Known gaps:**
- **Formatted output** (stdlib or language): printing `"  42  108  723 input.txt"` currently requires multiple `print`/`print_i64` calls with manual padding. This is where varargs or a format/writer pattern becomes necessary.

**Stdlib additions likely needed:**
- String splitting or tokenization (iterate words in a string)
- Number padding/formatting for aligned output
- Possibly `eprint`/`eprintln` for stderr output

### Milestone 3: JSON Parser

Parse a JSON string into a structured representation. This is the big jump: it requires recursive data structures and non-trivial string processing.

```nore
// Desired: parse '{"name": "nore", "version": 1}' into a tree
```

**What it needs:**
- Everything from Milestone 2
- Recursive data types (a JSON value can contain other JSON values)
- Tagged unions for variant types: already shipped (a JSON value is one of: string, number, bool, null, array, object)
- Error reporting (line/column of parse failure)

**Known gaps:**
- **Recursive data structures** (compiler feature): self-referential types for trees
- Possibly dynamic arrays or growable buffers for unknown-size JSON arrays/objects

**Stdlib additions likely needed:**
- String escaping/unescaping
- More string manipulation (trim, split on delimiter)

---

## Language Prerequisites

Features the compiler needs, ordered by priority. Each is a compiler change, not a library.

### ~~1. Tagged Unions~~ (done)

Shipped. Enums carry associated data. `match` expressions with exhaustiveness checking. Slice payloads supported (makes the tagged union non-copyable, like structs with slice fields). Inline layout (max size of all variants). The stdlib has been retrofitted with `ReadResult`, `WriteResult`, and `ParseResult`.

### ~~2. Module Namespaces and Visibility~~ (done)

Shipped. The import system now uses `import alias "path"` with mandatory module aliases and qualified access (`alias.name`). The `pub` keyword controls visibility: only `pub` declarations are accessible from other modules. No transitive visibility. Name mangling in generated C prevents cross-module collisions.

### ~~2b. Native Keyword for Built-in Declarations~~ (done, Category A)

Shipped. The `native func` keyword lets `.nore` modules declare built-in function signatures. The compiler validates the name against a known list and requires the declaration before the built-in can be used. Seven functions are gated: `fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek`, `mem_copy`, `exit`. Native declarations are always module-private (`pub native` is an error). To expose a native to importers, a module declares the native privately and provides a `pub` wrapper function with the same name. Inside the module, the native name takes precedence over the wrapper, preventing infinite recursion (e.g., `std/sys.nore` wraps `exit` this way). Arena/table built-ins remain in global scope (Category B, to be gated in a future phase).

### ~~3. Command-Line Arguments~~ (done)

Shipped. The `native func args(mut ref mem: Arena): [str]` built-in copies C's `argc`/`argv` into arena-allocated Nore slices. Exposed via `std/sys.nore` as `pub func get_args(mut ref mem: Arena): [str]`. The `--run` mode forwards arguments after `--` separator (e.g. `nore --run prog.nore -- arg1 arg2`).

### 4. Varargs / Formatted Output

**Priority: needed for Milestone 2. Needs design.**

The current model (one function per type: `print`, `print_i64`) does not scale. Printing `"lines: 42 words: 108"` requires six separate calls. Real programs need a better pattern.

**Blocks:** Milestone 2 (word count needs formatted output).

**Design space:** This needs careful thought. Options include:
- **Varargs with type dispatch**: `println(ref "count: ", n, ref " items")`. Requires variadic parameters and some form of runtime or comptime type dispatch. May depend on tagged unions.
- **Writer/buffer pattern**: `write(mut ref w, ref "count: ") ; write_i64(mut ref w, n)`. No new language feature, but verbose.
- **String interpolation**: `println(ref f"count: {n}")`. Compiler desugars into format calls. Powerful but a significant compiler addition.

The right answer may emerge from attempting Milestone 2. Listed as "needs design" until then.

### 5. Recursive Data Structures

**Priority: needed for Milestone 3.**

Self-referential types for trees, linked lists, and other recursive structures.

**Blocks:** Milestone 3 (JSON tree).

**Design space:** Requires heap-allocated nodes (arena or explicit). Interacts with tagged unions (a JSON value contains a slice of JSON values).

---

## Arena-Aware Design

The stdlib follows Nore's memory philosophy: **the caller owns the memory**.

Any stdlib function that allocates takes an `Arena` parameter:

```
// The caller decides where the string lives
val result: str = i64_to_str(mut ref my_arena, 42)

// The caller decides how big the buffer is
val contents: [u8] = read_file(mut ref file_arena, ref "data.txt")
```

This means:
- No global allocator hidden inside the stdlib
- No surprise OOM from a stdlib call. The arena's capacity is set by the caller.
- Batch deallocation works naturally. Reset the arena, free all stdlib-produced data at once.
- The stdlib cannot "leak" memory because arenas have scoped lifetimes

Functions that don't allocate (comparisons, searches, math) take no arena parameter.

---

## What About Generics?

Many stdlib designs lean on generics: `Vec<T>`, `HashMap<K, V>`, `sort<T>()`. Nore doesn't have generics and won't for a while.

The pragmatic path:
- **Write type-specific functions now** (`min_i64`, `str_eq`, `sort_i64`)
- **Keep the API surface small**. Don't write 4 variants of everything just because you might need them.
- **Let real programs reveal which variants matter**. If nobody needs `sort_u8`, don't write it.
- **Replace with generics later** when the language supports it

Generics are not a prerequisite for a useful stdlib. They're a cleanup that comes after real programs have validated the API.

---

## North Star: The Self-Hosting Test

The stdlib is "done enough" when Nore can write a non-trivial program. The milestones form the progression:

1. **"Hello, World"**: done. `print(ref "hello")` works.
2. **Milestone 0**: done. Tagged unions shipped, stdlib retrofitted, module system complete, native declarations shipped (Category A).
3. **Cat clone**: file I/O end-to-end, command-line arguments (shipped).
4. **Word count**: string processing, counting, formatted output.
5. **JSON parser**: recursive data, tagged unions, error reporting.

Each milestone proves the stdlib supports more real work. The ultimate test is self-hosting (writing the Nore compiler in Nore), but that requires language features (recursive data structures, generics) beyond what the stdlib alone provides.

---

## Open Questions

- ~~**Tagged union memory layout**: how are variants stored?~~ Resolved: inline layout (max size of all variants). Slice payloads make the union non-copyable.
- ~~**Pattern matching syntax**: `match` expression with exhaustiveness checking?~~ Resolved: `match (scrutinee) { Variant(binding) = { body } }` with exhaustiveness checking. See `docs/syntax.md`.
- ~~**Error model migration**: when `Result`/`Option` arrive, how do existing stdlib APIs evolve?~~ Resolved: breaking change. `read_file`, `write_file`, `str_to_i64` all use new tagged union return types.
- **Module visibility default**: export-by-default (add `private`) or private-by-default (add `pub`)? Nore's explicit philosophy suggests private-by-default.
- ~~**Command-line arguments API**: built-in function vs compiler-injected globals? Slice of strings or raw argc/argv?~~ Resolved: `native func args(mut ref mem: Arena): [str]` built-in, arena-allocated, exposed via `std/sys.nore`.
- **Varargs design**: language feature or stdlib pattern? Depends on tagged unions? Let Milestone 2 drive this.
- **String builder pattern**: arena-allocated growing buffer? Or a fixed-size buffer with explicit flush? The right pattern depends on how programs actually use it.
