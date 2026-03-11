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

Compiler-injected constants: `TARGET_OS`.

### Standard Library Modules

| Module | What it provides |
|--------|-----------------|
| `std/math.nore` | `min_i64`, `max_i64`, `abs_i64`, `clamp_i64`, `min_f64`, `max_f64`, `abs_f64`, `clamp_f64` |
| `std/string.nore` | Character classification (`is_digit`, `is_alpha`, `is_space`), comparison (`str_eq`, `str_starts_with`, `str_ends_with`), searching (`str_find`, `str_contains`), concatenation (`str_concat`), formatting (`fmt_i64`, `i64_to_str`), parsing (`str_to_i64`) |
| `std/io.nore` | `STDIN`, `STDOUT`, `STDERR`, `print`, `println`, `print_i64` (imports `std/string.nore`) |
| `std/file.nore` | `read_file`, `write_file`, platform-specific I/O constants (`O_RDONLY`, `O_CREAT`, `SEEK_SET`, etc. via `TARGET_OS`) |

All modules tested via `tests/std/`. Comprehensive success and error test suites cover the full language.

### Known Error Handling Debt

The v1 stdlib uses sentinel values for errors because tagged unions did not exist yet:

| Function | Current error behavior | Problem |
|----------|----------------------|---------|
| `read_file` | Returns empty `[u8]` on error | Indistinguishable from reading an actual empty file |
| `write_file` | Returns `false` on error | No error detail (permission denied? disk full?) |
| `str_to_i64` | Returns `0` on invalid input | Indistinguishable from parsing the string `"0"` |
| `fd_open` | Returns negative error code | Caller must check `< 0`, easy to forget |

This debt is addressed in Milestone 0 below.

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

The foundation phase was built bottom-up: language features first, then stdlib layers on top. The next phase combines both approaches. Some language features are needed immediately to fix what already exists (tagged unions, modules). Others will be driven top-down by attempting real programs.

### Milestone 0: Fix the Foundation

Before writing new programs, fix the existing stdlib. This is real work that validates new language features against existing code.

**Move I/O constants to stdlib:** Done. `STDIN`, `STDOUT`, `STDERR` are now defined in `std/io.nore` as plain `i32` constants, just like `O_RDONLY` and `SEEK_SET` in `std/file.nore`. Removed from compiler injection.

**Tagged unions:** Add `Result` and `Option` types, then retrofit all stdlib modules:
- `read_file` returns `Result` instead of empty slice on error
- `write_file` returns `Result` with error detail instead of bare bool
- `str_to_i64` returns `Option` to distinguish "0" from parse failure
- `fd_open` wrapped in a stdlib helper returning `Result`

**Module namespaces and visibility:** Refactor the import system so modules have proper boundaries. The problems are clear (name collisions from flat scope, no way to hide internal helpers), but the solution needs design. Starting points for discussion:
- Visibility: explicit exports (`pub func`) vs explicit privacy (`private func`) vs unexported-by-default
- Namespace access: dot-qualified (`string.eq`) vs current prefix convention
- Backward compatibility: can existing code migrate incrementally?

**Gate built-ins behind `native` declarations:** Currently, compiler built-ins (`fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek`, `mem_copy`, `exit`) are injected into every program's global namespace. Instead, a `native` keyword would let any `.nore` module declare a built-in's signature, and the compiler provides the implementation only when the signature matches a known built-in. For example, `std/io.nore` would declare `native func fd_write(fd: i32, ref data: [u8]): i64` and the function becomes available only to code that imports that module. This means built-ins are not a prerogative of any specific compiler path; they live in whatever domain module makes sense. The `.nore` file becomes the documentation, the compiler just has a table of known native signatures to match against.

**Requires:** Tagged unions (language prereq #1), Module system v2 (language prereq #2), Built-in gating (language prereq #2b).

### Milestone 1: Cat Clone

Read a file, write it to stdout. The simplest useful program.

```nore
import "std/io.nore"
import "std/file.nore"

mut mem: Arena = arena(65536)
val contents: [u8] = read_file(mut ref mem, ref "input.txt")
fd_write(STDOUT, ref contents)
```

**What it needs:**
- Read a filename from command-line arguments
- Read file contents, write to stdout
- Handle errors properly (file not found, read failure)

**Known gaps:**
- **Command-line arguments** (compiler built-in): access to argc/argv. Without this, the filename is hardcoded.

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
- Tagged unions for variant types (a JSON value is one of: string, number, bool, null, array, object)
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

### 1. Tagged Unions

**Priority: immediate.** The existing stdlib already needs this.

Extend enums to carry associated data. Required for `Result`, `Option`, and any variant type.

```
enum Result { Ok(i64), Err(i32) }
enum Option { Some(i64), None }
```

**Blocks:** Milestone 0 (stdlib error handling retrofit), Milestone 3 (JSON value variants).

**Design space:** Pattern matching (`match` expression), exhaustiveness checking, memory layout. This is the biggest language addition on the horizon.

### 2. Module Namespaces and Visibility

**Priority: immediate, right after tagged unions. Needs design.**

The current import system merges all declarations into a flat global scope. Every function in every imported module is visible to everyone. This creates two problems that will only get worse:

- **Name collisions:** Functions use manual prefixes (`str_eq`, `min_i64`) as a workaround for missing namespaces. As more modules are added, collisions become inevitable.
- **No privacy:** A module cannot have internal helper functions. Every helper leaks into the importer's scope. A JSON parser's `skip_whitespace` would be visible to every file that imports it.

**Blocks:** Milestone 0 (stdlib cleanup), and any non-trivial module with internal helpers.

**Design space:** The problems are well understood, but the solution needs careful thought:
- Visibility: explicit exports (`pub func`) vs explicit privacy (`private func`) vs unexported-by-default
- Namespace access: dot-qualified (`string.eq`) vs current prefix convention
- Backward compatibility: can existing code migrate incrementally?
- Interaction with transitive imports: if A imports B imports C, what does A see from C?

### 2b. Native Keyword for Built-in Declarations

**Priority: right after module system v2. Needs design.**

A `native` keyword lets `.nore` modules declare built-in function signatures. The compiler maintains a table of known native functions and provides the implementation when a declaration's signature matches. No match means a compile error.

```nore
// in std/io.nore
native func fd_write(fd: i32, ref data: [u8]): i64
native func fd_read(fd: i32, mut ref buf: [u8]): i64

// in std/file.nore
native func fd_open(ref path: str, flags: i32): i32
native func fd_close(fd: i32): void
native func fd_seek(fd: i32, offset: i64, whence: i32): i64
```

**Blocks:** Milestone 0 (clean global namespace).

**Design space:**
- Built-ins are not a prerogative of any specific module. They live in whatever domain module makes sense. `fd_write` belongs in `std/io.nore`, `fd_open` in `std/file.nore`, etc.
- Where do `exit` and `mem_copy` live? Possibly `std/sys.nore` or `std/core.nore`, or in existing modules where they're most relevant.
- The `.nore` file becomes the documentation for each built-in. The signature is visible, the `native` keyword signals compiler-provided implementation.
- Interaction with module v2: native declarations follow the same namespace/visibility rules as regular functions. If a module doesn't export a native function, importers don't see it.

### 3. Command-Line Arguments

**Priority: needed for Milestone 1.**

Access to argc/argv. Required for any program that takes input from the command line.

**Blocks:** Milestone 1 (cat clone), Milestone 2 (word count).

**Design space:** Could be a built-in function returning a slice of strings, or compiler-injected globals. Should follow the same pattern as `TARGET_OS`: simple, no magic.

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
2. **Milestone 0**: fix existing stdlib with tagged unions and proper modules.
3. **Cat clone**: file I/O end-to-end, command-line arguments.
4. **Word count**: string processing, counting, formatted output.
5. **JSON parser**: recursive data, tagged unions, error reporting.

Each milestone proves the stdlib supports more real work. The ultimate test is self-hosting (writing the Nore compiler in Nore), but that requires language features (recursive data structures, tagged unions, generics) beyond what the stdlib alone provides.

---

## Open Questions

- **Tagged union memory layout**: how are variants stored? Inline (max size of all variants) or heap-allocated? Affects arena interaction.
- **Pattern matching syntax**: `match` expression with exhaustiveness checking? How verbose should it be?
- **Module visibility default**: export-by-default (add `private`) or private-by-default (add `pub`)? Nore's explicit philosophy suggests private-by-default.
- **Command-line arguments API**: built-in function vs compiler-injected globals? Slice of strings or raw argc/argv?
- **Varargs design**: language feature or stdlib pattern? Depends on tagged unions? Let Milestone 2 drive this.
- **Error model migration**: when `Result`/`Option` arrive, how do existing stdlib APIs evolve? Breaking change or parallel APIs?
- **String builder pattern**: arena-allocated growing buffer? Or a fixed-size buffer with explicit flush? The right pattern depends on how programs actually use it.
