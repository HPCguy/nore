# Standard Library Roadmap

## Goal

Nore's standard library should be the thinnest possible layer that makes real programs possible. Every function should earn its place. The stdlib follows the same philosophy as the language: explicit allocation, no hidden costs, and the developer always knows what's happening.

## Core Principle

**The compiler grows only when it must. Everything else is written in Nore.**

Built-in primitives provide the minimum bridge to the operating system. The rest — string operations, formatting, file helpers — is ordinary Nore code that users could write themselves. The stdlib just saves them the trouble.

---

## What Belongs Where

### In the Compiler (built-ins)

Things that require compiler support because they cannot be expressed in Nore today:

- **I/O primitives** — writing bytes to a file descriptor, reading bytes, opening/closing files. These need syscall access that Nore source code cannot express.
- **Process control** — `exit(code)` to terminate with a status code.
- **Command-line arguments** — access to argc/argv requires compiler-level wiring.

### In the Standard Library (.nore files)

Everything that *can* be a Nore function *should* be. Once the language has the necessary primitives (I/O, casting, enums, modulo), these are all regular Nore code:

- String comparison, searching, slicing
- Number-to-string and string-to-number conversion
- Formatted output (`print`, `println`)
- File reading helpers (`read_file`, `read_lines`)
- Math utilities (`min`, `max`, `abs`, `clamp`)

### Not in the Standard Library

Things that don't belong — at least not yet:

- Networking, HTTP, JSON — these are ecosystem libraries, not core stdlib
- Concurrency primitives — the language needs a concurrency story first
- Generic collections (hash map, dynamic array) — need generics or code generation

---

## Language Prerequisites

The stdlib cannot be written until certain language features exist. These come first — each one is a compiler change, not a library.

### Layer 0: Missing Operators and Types

Small additions with outsized impact. These unblock string processing, hashing, and real algorithms.

| Feature | Why It's Needed |
|---------|----------------|
| `%` modulo operator | Number formatting, hash functions, circular buffers |
| `&` `\|` `^` `~` `<<` `>>` bitwise ops | Hash functions, flag manipulation, binary protocols |
| Character literals (`'A'`, `'\n'`) | String processing without magic numbers |
| Numeric type casting | I/O works in bytes (`u8`), lengths are `i64` — must convert between them |

**Numeric casting design note:** Nore already has comptime coercion (a literal `42` adapts to any integer type). Runtime casting between concrete types is the gap. The syntax should be explicit — something like `x as u8` or `u8(x)` — and truncation/overflow behavior must be defined. This deserves a focused design decision before implementation.

### Layer 1: Enums

Enums unlock error handling, option types, and state machines. Without them, the stdlib has no way to report errors beyond `assert` (which aborts).

```
enum Color { Red, Green, Blue }

// Later: tagged unions for Result/Option patterns
enum Result {
    Ok { value: i64 },
    Err { code: i64 },
}
```

**Minimum viable version:** simple C-style enums (named integer constants). Tagged unions can come later but are the real prize — they enable `Result` and `Option` types that make error handling safe and explicit.

### Layer 2: Module System

The stdlib is shipped as `.nore` files. Without an import mechanism, everything lives in one file.

```
import std.io
import std.str
```

**Minimum viable version:** a flat file-based import that brings declarations into scope. No nested namespaces, no visibility modifiers, no package manager. Just `import "path.nore"` to include another source file's declarations.

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

// Terminate the process with a status code.
func exit(code: i32): void
```

**Design notes:**
- File descriptors are plain integers — no wrapper types, no handles. This matches POSIX and keeps things simple.
- `fd_write` / `fd_read` work with byte slices — Nore's natural data type for buffers.
- Error handling through return codes initially. Once enums/tagged unions exist, these can return `Result` types.
- String literals are `str` (which is `[u8]`) so printing a string literal is just `fd_write(STDOUT, ref "hello")`.

### Layer B: String Operations

Written in Nore. Depends on Layer A (I/O) and Layer 0 (casting, character literals).

```
// Comparison
func str_eq(ref a: str, ref b: str): bool
func str_starts_with(ref s: str, ref prefix: str): bool
func str_ends_with(ref s: str, ref suffix: str): bool

// Searching
func str_find(ref s: str, ref needle: str): i64       // -1 if not found
func str_contains(ref s: str, ref needle: str): bool

// Conversion (allocates into caller's arena)
func i64_to_str(mut ref mem: Arena, n: i64): str
func str_to_i64(ref s: str): i64                       // 0 on invalid input, or Result later

// Character classification
func is_digit(c: u8): bool
func is_alpha(c: u8): bool
func is_space(c: u8): bool
```

**Design notes:**
- Functions that produce strings take an `Arena` parameter — explicit allocation, no hidden malloc.
- `str` is `[u8]`, so these all work on byte slices. No separate string type.
- Character functions work on `u8` — a character is just a byte.

### Layer C: Formatted Output

Written in Nore. Depends on Layer B (string conversion) and Layer A (I/O).

```
// Print a string to stdout
func print(ref s: str): void

// Print a string to stdout followed by a newline
func println(ref s: str): void

// Print an integer to stdout
func print_i64(n: i64): void

// Print to stderr
func eprint(ref s: str): void
func eprintln(ref s: str): void
```

**Design notes:**
- No format strings. Format strings require either variadic functions or generics — both are complex features Nore doesn't have. Instead, call the function that matches your type.
- A general-purpose `format` function that builds a string in an arena can come later when the pattern is clear.
- This is deliberately primitive. It prints values. That's it.

### Layer D: File Operations

Written in Nore. Depends on Layer A (I/O built-ins) and Layer B (strings).

```
// Read an entire file into a byte slice (allocated from the arena)
func read_file(mut ref mem: Arena, ref path: str): [u8]

// Write a byte slice to a file (creates/overwrites)
func write_file(ref path: str, ref data: [u8]): bool
```

**Design notes:**
- `read_file` takes an arena — the caller controls where the file contents live and how long they survive.
- Error handling through return values initially (empty slice / false). Migrate to `Result` when enums exist.

### Layer E: Math and Utilities

Written in Nore. No dependencies beyond the base language.

```
func min_i64(a: i64, b: i64): i64
func max_i64(a: i64, b: i64): i64
func abs_i64(a: i64): i64
func clamp_i64(x: i64, lo: i64, hi: i64): i64

func min_f64(a: f64, b: f64): f64
func max_f64(a: f64, b: f64): f64
func abs_f64(a: f64): f64
func clamp_f64(x: f64, lo: f64, hi: f64): f64
```

**Design notes:**
- Type-suffixed names because Nore has no generics or overloading. This is ugly but honest.
- When generics arrive, these become `min(a: T, b: T): T`. Until then, explicit names.
- `abs`, `min`, `max`, `clamp` cover the vast majority of math utility needs.

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

Phase 1: Language completeness (compiler work)
  7. Enums                          ← unblocks error handling, Result/Option
  8. Module system                  ← unblocks shipping stdlib as .nore files

Phase 2: Standard library (.nore files, importable)
  9.  String operations             ← Layer B
  10. File operations               ← Layer D
  11. Math utilities                ← Layer E
```

Phase 0 is complete. Steps 1-6 are all done, meaning Nore programs can do real I/O.

Phase 1 groups the two remaining compiler prerequisites together. Without enums, the stdlib has no error handling story. Without modules, stdlib code has no home. Both must land before the stdlib can be written as proper importable `.nore` files. Enums come first because they are a smaller, more self-contained change, and the module system design benefits from knowing what the stdlib needs to export (including `Result` and `Option` from enums).

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
- No surprise OOM from a stdlib call — the arena's capacity is set by the caller
- Batch deallocation works naturally — reset the arena, free all stdlib-produced data at once
- The stdlib cannot "leak" memory because arenas have scoped lifetimes

Functions that don't allocate (comparisons, searches, math) take no arena parameter.

---

## What About Generics?

Many stdlib designs lean on generics: `Vec<T>`, `HashMap<K, V>`, `sort<T>()`. Nore doesn't have generics and won't for a while.

The pragmatic path:
- **Write type-specific functions now** (`min_i64`, `str_eq`, `sort_i64`)
- **Keep the API surface small** — don't write 4 variants of everything just because you might need them
- **Let real programs reveal which variants matter** — if nobody needs `sort_u8`, don't write it
- **Replace with generics later** when the language supports it

Generics are not a prerequisite for a useful stdlib. They're a cleanup that comes after real programs have validated the API.

---

## North Star: The Self-Hosting Test

The stdlib is "done enough" when Nore can write a non-trivial program. Good intermediate milestones:

1. **"Hello, World"** — `print("hello")` works (Layer A + C)
2. **Cat clone** — read file, write to stdout (Layer A + D)
3. **Word count** — read file, split on whitespace, count (Layer B + D)
4. **Brainfuck interpreter** — parsing, state machine, I/O (all layers)
5. **JSON parser** — string processing, recursive data, error reporting

Each milestone proves the stdlib supports more real work. The ultimate test is self-hosting — writing the Nore compiler in Nore — but that requires language features (recursive data structures, tagged unions) beyond what the stdlib alone provides.

---

## Open Questions

- **Casting syntax**: resolved. Function-call style `u8(x)`, `i64(x)`, etc. Reads naturally, parses cleanly.
- **Overflow behavior on cast**: currently truncates (C-style). Revisit when enums/`Result` exist.
- **I/O error model before enums**: return negative error codes? Return a boolean + out-parameter? Accept the limitation and add proper error handling when enums land?
- **String builder pattern**: arena-allocated growing buffer? Or a fixed-size buffer with explicit flush? The right pattern depends on how programs actually use it.
- **Module system scope**: textual inclusion (like C `#include`) vs. proper modules with scoping? The former is simpler to implement, the latter is better long-term.
- **`print`/`println` are built-in for now.** They are compiler built-ins until the module system exists, at which point they can move to stdlib `.nore` files.
