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

### File-Level Responsibilities

The directory split above is not enough on its own. Each committed source file
should have a narrow ownership boundary so later milestones do not smear logic
across unrelated helpers.

Current committed file scope:

- `compiler/main.nore`: bootstrap compiler entry point and top-level stage
  orchestration only. This file should wire modules together, not absorb lexer,
  parser, sema, or codegen logic.

- `compiler/support/span.nore`: tiny shared value types and half-open range
  helpers. It owns `Span`, `SourceSpan`, `Range`, and their trivial constructor /
  end helpers. It should not grow parsing, path, or source-loading policy.

- `compiler/support/byte_buf.nore`: arena-backed growable `[u8]` storage for
  compiler-owned pooled bytes. It owns capacity growth, append, and clear
  semantics for byte data. It should not know about paths, diagnostics, source
  files, or token structure.

- `compiler/support/i64_buf.nore`: arena-backed growable `i64` side-array
  storage. It exists for flat tables and line-start arrays. It should remain a
  boring typed buffer, not a generic collection framework.

- `compiler/support/path.nore`: simple path manipulation needed by bootstrap
  imports plus the bootstrap compiler's import-resolution policy. Generic path
  operations such as `dirname`, `join`, and extension checks may stay here until
  a real non-compiler consumer justifies extraction. Compiler-specific rules
  such as bootstrap `std/` resolution belong here, not in the stdlib.

- `compiler/support/line_map.nore`: byte-offset to line/column translation over
  precomputed line-start offsets. It owns lookup math and line-start recording,
  but not file I/O, source storage, or diagnostic text formatting.

- `compiler/support/source.nore`: the `Sources` table plus pooled ownership of
  paths, display paths, source bytes, and per-source line-start ranges. It owns
  file-loading ingestion into stable compiler storage. It should not take on
  import-graph policy, parsing, or diagnostic rendering.

- `compiler/support/diag.nore`: the `Diagnostics` table and late formatting of
  structured diagnostics from stable compiler-owned data. It owns severity/code/
  message storage and `file:line:col` rendering. It should not decide when to
  stop compilation, which phase emits what, or how diagnostics are transported
  to a CLI.

- `compiler/frontend/token.nore`: token kinds, token-row layout, token naming,
  and token/source slice helpers only. It owns the flat token table contract and
  token payload/text access, but not scanning state, diagnostics, or parser
  logic.

- `compiler/frontend/lexer.nore`: source-to-token scanning plus lexer-only
  diagnostics and debug dumps only. It owns keyword recognition, comment and
  literal scanning, and token-table append order. It should not absorb parsing,
  node creation, or source-loading policy.

- `compiler/frontend/node.nore`: node kinds plus the shared flat node-table
  layout only. It owns child/sibling link policy and parser-facing node
  metadata slots, but not parse control flow or semantic meaning.

- `compiler/frontend/parser.nore`: parse token rows into node rows only. It owns
  single-module recursive descent, parser diagnostics, and stable parser debug
  dumps, but not import-graph loading, semantic checks, or code generation.

- `compiler/frontend/loader.nore`: load one root module into a parsed import
  graph only. It owns module rows, import-edge rows, path resolution on top of
  `compiler/support/path.nore`, and multi-module lex/parse orchestration, but
  not name resolution, visibility checks, or code generation.

Remaining placeholder file scope:

- `compiler/sema/symbols.nore`: symbol rows and symbol metadata only.

- `compiler/sema/scopes.nore`: lexical scope rows and scope nesting only.

- `compiler/sema/types.nore`: internal type rows and type metadata only.

- `compiler/sema/check.nore`: name resolution, type checking, and bootstrap
  arena-safety enforcement only.

- `compiler/codegen/c_types.nore`: lower checked Nore types into C-facing type
  descriptions only.

- `compiler/codegen/c_emit_expr.nore`: C emission for expressions only.

- `compiler/codegen/c_emit_stmt.nore`: C emission for statements and control
  flow only.

- `compiler/codegen/c_emit_decl.nore`: C emission for declarations, globals,
  and function prototypes only.

- `compiler/codegen/c_runtime.nore`: runtime helper snippets required by
  generated C only.

- `compiler/codegen/c_main.nore`: codegen orchestration across checked modules
  only.

- `compiler/driver/cli.nore`: CLI parsing and stage orchestration at the driver
  boundary only. It should not absorb frontend or codegen internals.

Rule for future files:

- add a short module header at the top of each new committed compiler source
  file
- the header should state responsibility, owned scope, and explicit non-scope
- document both what the file owns and what it intentionally does not own
- prefer moving shared primitives downward into `support/` or `std/` instead of
  letting orchestration files accumulate low-level helpers

### Support Workflow

The support modules are intended to be used in a fixed direction, from generic
storage upward toward compiler-facing reporting:

1. `compiler/main.nore` owns the top-level pipeline and long-lived arenas.
2. `compiler/support/path.nore` resolves the root file path and import paths
   using the bootstrap compiler's current path policy.
3. `compiler/support/source.nore` loads file contents, assigns `source_id`
   values, and stores source bytes plus display-path metadata in stable pooled
   storage.
4. `compiler/support/line_map.nore` records line starts for each loaded source
   so later phases can map byte offsets to 1-based line and column positions.
5. frontend, sema, and codegen phases read stable source text from
   `compiler/support/source.nore` and refer to text by ids and offsets rather
   than ad hoc copied strings.
6. when a phase needs to report a problem, it records a structured row in
   `compiler/support/diag.nore` using source ids and byte offsets.
7. formatting happens late: `compiler/support/diag.nore` combines diagnostic
   rows with `Sources` and line maps to render `file:line:col` output.

The dependency direction should stay simple:

- `span.nore`, `byte_buf.nore`, and `i64_buf.nore` are the low-level storage and
  value helpers
- `path.nore` adds bootstrap path policy
- `source.nore` builds stable source ownership on top of buffers and line maps
- `diag.nore` sits above source ownership and line mapping for late formatting

Higher layers should depend on this stack. The support stack should not grow
upward dependencies on frontend, sema, codegen, or CLI orchestration.

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
- `payload_start`
- `payload_len`

`payload_start` and `payload_len` cover the bootstrap lexer needs directly. They
carry cases like:

- identifier text spans
- numeric literal text spans
- string and char literal inner payload spans

There is no token object graph and no linked list.

### Nodes

`Nodes` is the single syntax tree table.

Minimum columns:

- `kind`
- `source_id`
- `start`
- `span_len`
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
