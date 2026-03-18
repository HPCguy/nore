# Standard Library Design

Design principles and forward-looking guidance for Nore's standard library. For the current API reference, see [nore.md](nore.md) and [syntax.md](syntax.md).

## Core Principle

**The compiler grows only when it must. Everything else is written in Nore.**

Built-in primitives provide the minimum bridge to the operating system. The rest (string operations, formatting, file helpers, JSON parsing) is ordinary Nore code that users could write themselves. The stdlib just saves them the trouble.

Stdlib modules should import and reuse each other to avoid code duplication and ensure consistency. Higher-level modules build on lower-level ones (e.g., `std/io.nore` imports `std/string.nore` for number formatting, `std/json.nore` imports `std/string.nore` for character classification and float parsing).

---

## What Belongs Where

### In the Compiler (built-ins)

Things that require compiler support because they cannot be expressed in Nore today:

- **I/O primitives**: writing bytes to a file descriptor, reading bytes, opening/closing files. These need syscall access that Nore source code cannot express.
- **Process control**: `exit(code)` to terminate with a status code.
- **Command-line arguments**: access to argc/argv requires compiler-level wiring.

Each built-in requires a `native func` declaration in the module that uses it. Arena/table built-ins (`arena`, `arena_alloc`, `arena_reset`, `table_alloc`, `table_len`, `table_get`, `table_insert`) remain in global scope.

### In the Standard Library (.nore files)

Everything that *can* be a Nore function *should* be:

- String comparison, searching, slicing, splitting
- Number-to-string and string-to-number conversion (including `str_to_f64`)
- Formatted output (`print`, `println`, buffered `Writer`)
- File reading helpers (`read_file`, `write_file`)
- Math utilities (`min`, `max`, `abs`, `clamp`)
- Data format parsers (`json_parse`)

### Not in the Standard Library

Things that don't belong, at least not yet:

- Networking, HTTP. These are ecosystem libraries, not core stdlib.
- Concurrency primitives. The language needs a concurrency story first.
- Generic collections (hash map, dynamic array). Need generics or code generation.

---

## Arena-Aware Design

The stdlib follows Nore's memory philosophy: **the caller owns the memory**.

Any stdlib function that allocates takes an `Arena` parameter:

```nore
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

## Known Gaps

Language limitations observed while building real programs on the stdlib:

- **No `else if` syntax**: chains must use nested `else { if (...) { } }` or flat `if/return` patterns
- **No mutual recursion**: codegen doesn't emit C forward declarations for user functions. Self-recursion works. Workaround: merge mutually recursive functions into one
- **Codegen bug with string literal `ref` args**: string literals passed directly as `ref` to user-defined functions with many parameters can produce wrong C types. Workaround: bind to a `val` first
- **Arena reuse for multiple files**: large files could fill the arena. Resetting between files is tricky when argv slices share the arena

---

## North Star: The Self-Hosting Test

The stdlib is "done enough" when Nore can write a non-trivial program. The completed milestones form the progression:

1. **"Hello, World"**: `print(ref "hello")` works.
2. **Foundation**: tagged unions, module system, native declarations, visibility.
3. **Cat clone**: `examples/cat.nore` proves file I/O, args, error handling.
4. **Word count**: `examples/wc.nore` proves string processing, formatted output via the Writer pattern.
5. **JSON parser**: `std/json.nore` + `examples/json.nore` proves trees work without recursive types.

The JSON parser was a key inflection point: it proved trees work without recursive types, validating the data-oriented design. The ultimate test is self-hosting (writing the Nore compiler in Nore), which will likely need generics and possibly recursive types, but the DOD approach may push that boundary further than expected.
