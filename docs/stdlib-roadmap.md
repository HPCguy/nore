# Standard Library Roadmap

## Goal

Nore's standard library should be the thinnest possible layer that makes real programs possible. Every function should earn its place. The stdlib follows the same philosophy as the language: explicit allocation, no hidden costs, and the developer always knows what's happening.

## Core Principle

**The compiler grows only when it must. Everything else is written in Nore.**

Built-in primitives provide the minimum bridge to the operating system. The rest, string operations, formatting, file helpers, is ordinary Nore code that users could write themselves. The stdlib just saves them the trouble.

Stdlib modules should import and reuse each other to avoid code duplication and ensure consistency. Higher-level modules build on lower-level ones (e.g., `std/io.nore` imports `std/string.nore` for number formatting).

---

## What Belongs Where

### In the Compiler (built-ins)

Things that require compiler support because they cannot be expressed in Nore today:

- **I/O primitives**: writing bytes to a file descriptor, reading bytes, opening/closing files. These need syscall access that Nore source code cannot express.
- **Process control**: `exit(code)` to terminate with a status code.
- **Command-line arguments**: access to argc/argv requires compiler-level wiring.

### In the Standard Library (.nore files)

Everything that *can* be a Nore function *should* be. Once the language has the necessary primitives (I/O, casting, enums, modulo), these are all regular Nore code:

- String comparison, searching, slicing
- Number-to-string and string-to-number conversion
- Formatted output (`print`, `println`)
- File reading helpers (`read_file`, `read_lines`)
- Math utilities (`min`, `max`, `abs`, `clamp`)

### Not in the Standard Library

Things that don't belong, at least not yet:

- Networking, HTTP, JSON. These are ecosystem libraries, not core stdlib.
- Concurrency primitives. The language needs a concurrency story first.
- Generic collections (hash map, dynamic array). Need generics or code generation.

---

## Language Prerequisites

The stdlib cannot be written until certain language features exist. These come first. Each one is a compiler change, not a library.

### Layer 0: Missing Operators and Types

Small additions with outsized impact. These unblock string processing, hashing, and real algorithms.

| Feature | Why It's Needed |
|---------|----------------|
| `%` modulo operator | Number formatting, hash functions, circular buffers |
| `&` `\|` `^` `~` `<<` `>>` bitwise ops | Hash functions, flag manipulation, binary protocols |
| Character literals (`'A'`, `'\n'`) | String processing without magic numbers |
| Numeric type casting | I/O works in bytes (`u8`), lengths are `i64`. Must convert between them |

**Numeric casting design note:** Nore already has comptime coercion (a literal `42` adapts to any integer type). Runtime casting between concrete types is the gap. The syntax should be explicit (something like `x as u8` or `u8(x)`) and truncation/overflow behavior must be defined. This deserves a focused design decision before implementation.

### Layer 1: Enums (DONE)

Simple C-style enums with named integer constants, auto-numbered from 0. Dot-qualified variant access (`Color.Red`), equality comparison, casting to `i64`.

```
enum Color { Red, Green, Blue }
val c: Color = Color.Red
val n: i64 = i64(c)    // 0
```

**What shipped:** named integer constants, type-safe comparison (no cross-enum, no ordering), no arithmetic on enums. Tagged unions for `Result`/`Option` patterns are a future addition.

### Layer 2: Module System (DONE)

The stdlib is shipped as `.nore` files. The import system uses string literal paths:

```
import "std/math.nore"
import "utils.nore"
import "../shared/helpers.nore"
```

**Path resolution:** paths starting with `std/` resolve relative to the compiler binary's directory. All other paths resolve relative to the importing file's directory. Each file is imported at most once (duplicates are silently skipped). All declarations from the imported file are merged into the importing program's scope. Imports are transitive.

**What shipped:** flat file-based import, no namespaces, no visibility modifiers. Error diagnostics track the correct source file per error. First stdlib file: `std/math.nore`.

---

## The Standard Library Itself

Once the prerequisites exist, the stdlib is built in layers. Each layer depends only on the ones below it.

### Layer A: I/O Foundation

The thinnest possible bridge to the operating system. These are **built-in functions** because they require syscall access.

```
// File descriptors (predefined constants)
val STDIN: i32 = 0
val STDOUT: i32 = 1
val STDERR: i32 = 2

// Write bytes to a file descriptor. Returns bytes written.
func fd_write(fd: i32, ref data: [u8]): i64

// Read bytes from a file descriptor into a buffer. Returns bytes read (0 = EOF).
func fd_read(fd: i32, mut ref buf: [u8]): i64

// Open a file. Returns a file descriptor or negative error code.
func fd_open(ref path: str, flags: i32): i32

// Close a file descriptor.
func fd_close(fd: i32): void

// Seek in a file. Returns new position (-1 on error).
func fd_seek(fd: i32, offset: i64, whence: i32): i64

// Copy bytes between buffers. Returns bytes copied (min of lengths).
func mem_copy(mut ref dst: [u8], ref src: [u8]): i64

// Terminate the process with a status code.
func exit(code: i32): void
```

Predefined constants for I/O:
- `STDIN=0`, `STDOUT=1`, `STDERR=2` (file descriptors)
- `O_RDONLY=0`, `O_WRONLY=1`, `O_RDWR=2`, `O_CREAT`, `O_TRUNC`, `O_APPEND` (open flags, platform-specific values for create/trunc/append)
- `SEEK_SET=0`, `SEEK_CUR=1`, `SEEK_END=2` (seek whence)

**Design notes:**
- File descriptors are plain integers. No wrapper types, no handles. This matches POSIX and keeps things simple.
- `fd_write` / `fd_read` work with byte slices, Nore's natural data type for buffers.
- Error handling through return codes initially. Once enums/tagged unions exist, these can return `Result` types.
- String literals are `str` (which is `[u8]`) so printing a string literal is just `fd_write(STDOUT, ref "hello")`.
- Open flag constants use platform-specific values (`#ifdef __APPLE__` in the compiler) since generated C is always compiled on the same platform.

### Layer B: String Operations (DONE)

Shipped as `std/string.nore`. Depends on Layer 0 (casting, character literals).

```nore
import "std/string.nore"

assert str_eq(ref "hello", ref "hello")
assert str_find(ref "abcdef", ref "cd") == 2
assert is_digit('5')

mut mem: Arena = arena(256)
val s: str = i64_to_str(mut ref mem, 42)
assert str_eq(ref s, ref "42")
val hw: str = str_concat(mut ref mem, ref "hello", ref " world")
```

Provides:
- **Character classification:** `is_digit`, `is_alpha`, `is_space`
- **Comparison:** `str_eq`, `str_starts_with`, `str_ends_with`
- **Searching:** `str_find` (returns -1 if not found), `str_contains`
- **Concatenation:** `str_concat` (arena-allocated)
- **Formatting:** `fmt_i64` (writes into caller-provided buffer, no allocation)
- **Conversion:** `i64_to_str` (arena-allocated, uses `fmt_i64`), `str_to_i64` (returns 0 on invalid input)

**Design notes:**
- Functions that produce strings take an `Arena` parameter, explicit allocation, no hidden malloc.
- `str` is `[u8]`, so these all work on byte slices. No separate string type.
- Character functions work on `u8`, a character is just a byte.
- `str_to_i64` returns 0 for both empty/invalid input and actual "0". Revisit with `Result` type when tagged unions exist.
- Tested via `tests/std/string.nore`.

### Layer C: Formatted Output (DONE)

Shipped as `std/io.nore`. Imports `std/string.nore` for shared formatting. Moved from compiler built-ins to regular Nore functions.

```nore
import "std/io.nore"

print(ref "Hello, ")
println(ref "World!")
print_i64(42)
```

Provides: `print`, `println`, `print_i64`.

**Design notes:**
- `print` and `println` are thin wrappers around `fd_write(STDOUT, ...)`.
- `print_i64` calls `fmt_i64` from `std/string.nore` into a stack buffer, then writes via `fd_write`. No allocation, no hidden costs, no duplicated formatting logic.
- No format strings. Call the function that matches your type.
- `eprint`/`eprintln` (stderr variants) can be added to the same file.
- Tested via `tests/std/io.nore`. Tests verify the functions compile and run without crashing (exit 0). Conversion correctness is covered by `tests/std/string.nore`.

### Layer D: File Operations (DONE)

Shipped as `std/file.nore`. Depends on Layer A (I/O built-ins, including `fd_seek`) and Layer B (strings).

```nore
import "std/file.nore"

mut mem: Arena = arena(4096)
val contents: [u8] = read_file(mut ref mem, ref "data.txt")
val ok: bool = write_file(ref "output.txt", ref contents)
```

Provides:
- **`read_file(mut ref mem, ref path)`** - Read entire file into arena-allocated `[u8]`. Uses `fd_seek` to determine file size, allocates exactly, reads in a loop. Returns empty slice on error.
- **`write_file(ref path, ref data)`** - Write `[u8]` to file (create/overwrite via `O_WRONLY | O_CREAT | O_TRUNC`). Returns `true` on success, `false` on error.

**Design notes:**
- `read_file` takes an arena. The caller controls where the file contents live and how long they survive.
- Error handling through return values (empty slice / false). Migrate to `Result` when tagged unions exist.
- `fd_seek` built-in added to support size detection. Maps to POSIX `lseek()`.
- Predefined constants (`O_RDONLY`, `O_WRONLY`, `O_CREAT`, `O_TRUNC`, `O_APPEND`, `O_RDWR`, `SEEK_SET`, `SEEK_CUR`, `SEEK_END`) make flag usage readable and portable.
- Tested via `tests/std/file.nore`.

### Layer E: Math and Utilities (DONE)

Shipped as `std/math.nore`. No dependencies beyond the base language.

```nore
import "std/math.nore"

val x: i64 = min_i64(3, 5)          // 3
val y: f64 = clamp_f64(x, 0.0, 1.0) // 1.0
```

Provides: `min_i64`, `max_i64`, `abs_i64`, `clamp_i64`, `min_f64`, `max_f64`, `abs_f64`, `clamp_f64`.

**Design notes:**
- Type-suffixed names because Nore has no generics or overloading.
- When generics arrive, these become `min(a: T, b: T): T`. Until then, explicit names.
- Tested via `tests/std/math.nore`.

---

## Implementation Sequence

The order matters. Each step unlocks the next.

```
Phase 0: Primitives (DONE)
  1. ✓ Modulo operator (%)          ← unblocks hashing/formatting
  2. ✓ Bitwise operators            ← unblocks binary operations
  3. ✓ Character literals           ← unblocks string processing
  4. ✓ Numeric type casting         ← unblocks I/O (u8 ↔ i64)
  5. ✓ I/O built-ins (fd_write..)  ← THE unlock for real programs
  6. ✓ print / println / print_i64 ← first programs can print output

Phase 1: Language completeness (DONE)
  7. ✓ Enums                        ← unblocks error handling, Result/Option
  8. ✓ Module system (import)       ← unblocks shipping stdlib as .nore files

Phase 2: Standard library (.nore files, importable)
  9.  ✓ Math utilities              ← std/math.nore (first stdlib file)
  10. ✓ Move print/println/print_i64 ← from compiler built-ins to std/io.nore
  11. ✓ String operations            ← std/string.nore (Layer B)
  12. ✓ mem_copy built-in            ← efficient bulk byte copy (memcpy)
  13. ✓ fd_seek built-in + constants ← file size detection, portable open flags
  14. ✓ File operations              ← std/file.nore (Layer D)
```

All phases complete. Four stdlib modules are shipped and tested: `std/math.nore` (Layer E), `std/string.nore` (Layer B), `std/io.nore` (Layer C), and `std/file.nore` (Layer D). The modules reuse each other where it makes sense: `std/io.nore` imports `std/string.nore` so `print_i64` delegates formatting to `fmt_i64` while keeping its own stack buffer (no hidden allocation). The fd_write/fd_read/fd_open/fd_close/fd_seek/mem_copy primitives stay as compiler built-ins (they need syscall or C library access).

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

The stdlib is "done enough" when Nore can write a non-trivial program. Good intermediate milestones:

1. **"Hello, World"**: `print("hello")` works (Layer A + C)
2. **Cat clone**: read file, write to stdout (Layer A + D)
3. **Word count**: read file, split on whitespace, count (Layer B + D)
4. **Brainfuck interpreter**: parsing, state machine, I/O (all layers)
5. **JSON parser**: string processing, recursive data, error reporting

Each milestone proves the stdlib supports more real work. The ultimate test is self-hosting (writing the Nore compiler in Nore), but that requires language features (recursive data structures, tagged unions) beyond what the stdlib alone provides.

---

## Open Questions

- **Casting syntax**: resolved. Function-call style `u8(x)`, `i64(x)`, etc.
- **Overflow behavior on cast**: runtime error (R003) on out-of-range values. Safe by default.
- **I/O error model before enums**: return negative error codes for now. Revisit with tagged unions.
- **String builder pattern**: arena-allocated growing buffer? Or a fixed-size buffer with explicit flush? The right pattern depends on how programs actually use it.
- **Module system scope**: resolved. Textual inclusion via `import "path.nore"` with `std/` prefix for stdlib. Proper modules with scoping/visibility can come later.
- **`print`/`println`/`print_i64` migration**: resolved. Moved to `std/io.nore`. `print_i64` reuses `fmt_i64` from `std/string.nore` with a stack buffer (no allocation).
- **Bulk memory operations**: resolved. `mem_copy(mut ref dst, ref src): i64` built-in added, maps to C `memmove`. Copies `min(dst.len, src.len)` bytes, returns count. Safe with overlapping buffers. Available for `std/file.nore` and can be adopted by `str_concat`/`i64_to_str` to replace byte-by-byte loops.
