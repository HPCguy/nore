# Module System Design

## Problem

The current import system merges all declarations into a flat global scope. Every function, type, and constant from every imported file is visible everywhere. This causes:

- **Name collisions**: functions use manual prefixes (`str_eq`, `min_i64`) as a workaround
- **No privacy**: internal helpers leak into every importer's scope
- **Transitive leaking**: if A imports B and B imports C, A sees everything from C

The stdlib is small today (4 modules, ~30 functions), but this won't scale.

## Design

### Import Syntax

```nore
import io "std/io.nore"
import file "std/file.nore"
import math "std/math.nore"
import str "std/string.nore"
```

- `import <alias> <path>` binds a module's public scope to a compile-time namespace
- The alias is not a variable, not a value, not a type. It is a compile-time name used only for qualified access
- The alias must be a valid identifier, unique within the file
- Path resolution rules unchanged: `std/` resolves relative to compiler binary, else relative to importing file

### Qualified Access

All access to imported names goes through the alias:

```nore
io.println(ref "hello")
fd_write(io.STDOUT, ref data)
val result: file.ReadResult = file.read_file(mut ref mem, ref path)
val n: i64 = math.min_i64(a, b)
```

There is no way to pull individual names into local scope. Always qualified, always clear where a name comes from. One mechanism, zero ambiguity.

### Visibility

Private by default. The `pub` keyword makes a declaration visible to importers.

```nore
// std/io.nore
pub val STDOUT: i32 = 1

pub func println(ref s: str): void = {
    fd_write(STDOUT, ref s)
    fd_write(STDOUT, ref "\n")
}

// Internal helper, not visible to importers
func write_bytes(fd: i32, ref data: [u8]): i64 = {
    return fd_write(fd, ref data)
}
```

`pub` applies to:
- `pub func` - function declarations
- `pub val` / `pub mut` - global variables and constants
- `pub value` - value type declarations
- `pub struct` - struct type declarations
- `pub enum` - enum type declarations (all variants are public if the enum is public)
- `pub table` - table type declarations (row type is public if the table is public)

Without `pub`, a declaration is module-private: visible within the file and not accessible through the module alias.

### Transitive Imports

Imports are not transitive. If `io.nore` imports `string.nore` internally, the importer of `io.nore` does not see `string.nore`'s declarations.

```nore
// std/io.nore
import str "std/string.nore"   // private import, used internally

pub func print_i64(n: i64): void = {
    // uses str.fmt_i64 internally
}
```

To use `string.nore` functions, the caller must import it explicitly:

```nore
import io "std/io.nore"
import str "std/string.nore"

io.println(ref "hello")
val ok: bool = str.str_eq(ref a, ref b)
```

### Function Names Stay As-Is

Since aliases are chosen by the importer, function names must be self-descriptive regardless of the alias. `str_eq` makes sense even as `foo.str_eq`. A name like `eq` alone does not.

Existing stdlib function names (`str_eq`, `min_i64`, `print_i64`) are already self-sufficient. They remain unchanged. The module prefix is redundant in `str.str_eq` but harmless, and clarity wins over brevity.

Function renaming, if ever desired, is a separate stdlib API decision, not a module system concern.

### Module Name Is Not a Type

The module alias is purely a compile-time construct. It cannot be:
- Stored in a variable
- Passed to a function
- Used as a type annotation
- Returned from a function

It exists only for `alias.name` resolution during compilation. The compiler resolves all qualified names before codegen. In the generated C code, names are mangled to include the module (e.g., `ni_io_println`, `ni_str_eq`).

### Built-in Functions

Compiler built-ins (`fd_write`, `fd_read`, `fd_open`, `fd_close`, `fd_seek`, `mem_copy`, `exit`, `arena`, `arena_alloc`, `arena_reset`) remain in the global scope for now. They are not part of any module.

Gating built-ins behind `native` declarations in stdlib modules is a separate design (see stdlib-roadmap.md, Milestone 0). It can be done after the module system ships.

## Examples

### Cat Clone (Milestone 1)

```nore
import io "std/io.nore"
import file "std/file.nore"

func main(): void = {
    mut mem: Arena = arena(65536)
    val result: file.ReadResult = file.read_file(mut ref mem, ref "input.txt")
    match (result) {
        Ok(contents) = { fd_write(io.STDOUT, ref contents) }
        Err(code) = { io.println(ref "error reading file") }
    }
}
```

### Using Multiple Modules

```nore
import io "std/io.nore"
import str "std/string.nore"
import math "std/math.nore"

func main(): void = {
    val a: i64 = 10
    val b: i64 = 20
    val smaller: i64 = math.min_i64(a, b)
    io.print_i64(smaller)
    io.println(ref "")

    val msg: str = "hello"
    assert str.eq(ref msg, ref "hello")
}
```

## Implementation Notes

### Compiler Changes

1. **Lexer**: add `pub` as a keyword
2. **Parser**: change `import` to require an alias before the path. Parse `pub` prefix on declarations.
3. **AST**: add module information to declarations. Each declaration knows which module it belongs to.
4. **Name resolution**: qualified names (`alias.name`) resolved during typecheck by looking up the alias, then finding the public declaration in that module's scope.
5. **Codegen**: mangle names with module prefix to avoid C-level collisions (e.g., `ni_io_println`, `ni_str_eq`).

### Scope Model

Each module gets its own scope during typecheck. The importing file's scope contains:
- Its own local declarations
- Module aliases (mapping alias name to module scope)
- Compiler built-ins (global)

Qualified access `io.println` resolves as: find `io` in current scope (it's a module alias), then find `println` in `io`'s scope, then check it's `pub`.

### Migration

The old `import "path"` syntax (no alias) becomes a compile error. All existing code must be updated to use `import alias "path"`. This is a clean break.

## What This Does NOT Cover

- **Re-exports**: a module cannot re-export names from its own imports. If you need something, import it yourself.
- **Selective imports**: no way to pull individual names into local scope. Always qualify.
- **Wildcard imports**: intentionally excluded. Always qualify.
- **`native` keyword for built-ins**: separate design, see stdlib-roadmap.md.
- **Circular imports**: currently prevented by "import once" semantics. Module scoping doesn't change this.
