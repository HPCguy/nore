# Bootstrap Compiler Architecture

This document freezes Milestone 1 from [self-hosting-bootstrap-plan.md](self-hosting-bootstrap-plan.md): the bootstrap subset, the module layout, and the table-oriented data model for the Nore-written compiler.

The purpose of this document is to let support, frontend, semantic-analysis, and codegen work proceed without reopening the same architectural arguments on every module.

## Status

- Stage-0 remains the C compiler in `nore.c`.
- The Nore-written compiler source tree lives in `compiler/`.
- Milestone 1 is complete once this document and that tree are in place.

## Non-Goals for the Bootstrap Compiler

- Do not port `nore.c` function-by-function.
- Do not build pointer-rich recursive object graphs.
- Do not depend on generics, hash maps, or native driver support.
- Do not chase full language parity before the first self-compile.

## Source Tree

The committed bootstrap compiler source tree is:

```text
compiler/
  main.nore
  support/
    byte_buf.nore
    i64_buf.nore
    span.nore
    source.nore
    path.nore
    line_map.nore
    diag.nore
  frontend/
    token.nore
    lexer.nore
    node.nore
    parser.nore
  sema/
    symbols.nore
    scopes.nore
    types.nore
    check.nore
  codegen/
    c_types.nore
    c_emit_expr.nore
    c_emit_stmt.nore
    c_emit_decl.nore
    c_runtime.nore
    c_main.nore
  driver/
    cli.nore
```

Module responsibilities are intentionally narrow:

- `support/`: typed buffers, spans, paths, source ownership, line/column mapping, diagnostics
- `frontend/`: token kinds/tables, lexing, node tables, parsing
- `sema/`: symbols, scopes, types, checking
- `codegen/`: C lowering and emission
- `driver/`: CLI glue only; external Clang invocation stays out of scope until later milestones

## Bootstrap Coding Subset

Compiler sources in `compiler/` should stay inside this subset until self-compile is working.

### Allowed language features

- modules, `import`, `pub`
- `val`, `mut`, global constants, and small mutable globals when there is a clear ownership reason
- `func` with normal same-module calls and mutual recursion
- scalar types, fixed arrays, slices, `str`
- `value`, `struct`, `table`, `enum`, tagged unions
- `ref` and `mut ref`
- `if`, `while`, `for`, `match`
- string and character literals
- built-ins already required by compiler-style code:
  - `arena`
  - `arena_alloc`
  - `arena_reset`
  - `table_alloc`
  - `table_insert`
  - `table_get`
  - `table_len`
  - `assert`
  - `mem_copy`

### Temporary bans

- no platform or driver `native` declarations inside `compiler/`
- no dependence on process spawning, temporary-file APIs, or canonical path natives
- no generic collections or hash maps
- no ad hoc storage patterns outside `support/` helpers
- no pointer-shaped AST emulation through webs of slices-to-singletons
- no compiler features used in the source tree unless they are part of the frozen subset

Exception:

- a support module may carry a declaration shim for an already-approved built-in such as `mem_copy` when the stage-0 compiler still requires an explicit declaration

### Style constraints for compiler code

- use integer ids and row indices instead of nested ownership graphs
- prefer explicit tables and typed buffers over clever helper abstractions
- keep modules small and single-purpose
- prefer linear search first; only introduce faster lookup structures after profiling pain is real
- comments should explain invariants or ownership, not restate syntax

## Data Model

The compiler is table-oriented. Relationships are represented with ids, sibling links, or index ranges, not recursive heap objects.

### Common conventions

- all ids are `i64`
- `-1` is the invalid id sentinel unless a table documents a different sentinel
- strings are represented as spans into source bytes or spans into a compiler-owned string pool
- long-lived compiler data must never point into scratch storage

### Core shared value types

The support layer owns a few small value types used everywhere:

- `Span`: `start`, `len`
- `SourceSpan`: `source_id`, `start`, `len`
- `Range`: `start`, `count`

These are value types because they are copied freely and stored inside tables.

### Sources

`Sources` owns loaded module text and display-path metadata for the lifetime of the compilation.

Minimum columns:

- `path: Span`
- `display_path: Span`
- `text_start: i64`
- `text_len: i64`
- `line_starts: Range`
- `importer_source: i64`

Supporting storage:

- one byte buffer for concatenated source text
- one byte buffer for pooled paths/display strings
- one `I64Buf` for line-start offsets

### Tokens

`Tokens` is a flat token table, one row per token.

Minimum columns:

- `kind`
- `source_id`
- `start`
- `len`
- `data0`
- `data1`

`data0` and `data1` are generic payload slots. They cover cases like:

- interned identifier span
- parsed integer payload
- string literal payload span
- keyword-specific extra data where useful

There is no token object graph and no linked list.

### Nodes

`Nodes` is the single syntax tree table.

Minimum columns:

- `kind`
- `source_id`
- `start`
- `len`
- `parent`
- `first_child`
- `next_sibling`
- `type_id`
- `symbol_id`
- `data0`
- `data1`
- `data2`

Rules:

- use child/sibling links for irregular shapes
- use compact ranges only where nodes naturally own ordered lists
- semantic passes annotate nodes by id instead of rewriting tree shapes

### Diagnostics

`Diagnostics` is also a table, not a stream of formatted strings.

Minimum columns:

- `severity`
- `code: Span`
- `source_id`
- `start`
- `len`
- `message: Span`

Formatting happens late from structured data. The diagnostic table may grow across phases; rendering should stay compatible with the current stage-0 compiler style where practical.

### Later semantic tables

The semantic layer will add tables for:

- `Symbols`
- `Scopes`
- `Types`
- module/import metadata

Those tables follow the same rule: rows plus ids, never pointer-linked ownership trees.

## Allocation Discipline

Memory ownership must stay obvious.

### Arenas

- `state_mem`: long-lived arena for compilation state that survives the whole compile
- `module_mem`: optional per-module arena for work that can be discarded after a module is lowered into stable tables
- `scratch_mem`: optional resettable arena for formatting, small temporary scans, and transient work

### Rules

- only `support/` storage modules should call raw allocation helpers directly in normal cases
- frontend/sema/codegen modules should request capacity changes through typed helpers
- never store spans or slices into `scratch_mem` inside long-lived tables
- reset scratch storage only at explicit phase boundaries owned by one caller
- when a table can estimate capacity, reserve early and grow geometrically after that

## Capacity Conventions

- start with explicit initial capacities near the owner module
- grow typed buffers by doubling or another simple monotonic rule
- do not hide capacity growth inside unrelated modules
- avoid tiny repeated allocations in hot loops

## Strings and Text

- source text stays in compiler-owned byte buffers for the full compile
- identifiers, paths, and rendered diagnostic strings should use spans into owned buffers
- avoid creating one-off string wrapper types with independent ownership

## Milestone Boundaries

This document freezes what later milestones may assume:

- Milestone 2 builds the typed buffers and utility modules described here
- Milestone 3 builds `Sources`, line mapping, and diagnostics on top of those helpers
- Milestone 4 and later consume the same `Tokens` and `Nodes` model instead of inventing alternate trees

If a later milestone needs to change this document, the change should be deliberate and small, not a quiet local workaround.
