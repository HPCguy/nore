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
  operations such as `dirname`, `join`, lexical dot-segment normalization, and
  extension checks may stay here until a real non-compiler consumer justifies
  extraction. Compiler-specific rules such as bootstrap `std/` resolution
  belong here, not in the stdlib.

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
  `compiler/support/path.nore`, lexical path dedup for stable module identity,
  and multi-module lex/parse orchestration, but not name resolution,
  visibility checks, or code generation.

- `compiler/sema/symbols.nore`: symbol rows and symbol metadata only. It owns
  sema symbol kinds, per-symbol flags, and the flat symbol-table layout, but
  not lookup order, scope traversal, or diagnostics.

- `compiler/sema/scopes.nore`: lexical scope rows and scope nesting only. It
  owns parent/depth bookkeeping for module, function, block, and loop scopes,
  but not symbol lookup policy or expression checking.

- `compiler/sema/types.nore`: internal type rows and type metadata only. It
  owns builtin/bootstrap type rows plus array/slice/table-row interning, but
  not name resolution, field validation, or expression typing.

- `compiler/sema/check.nore`: semantic binding and bootstrap-subset checking
  only. It owns module-scope creation, declaration collection, local/param
  binding, module-qualified lookup, basic expression/statement typing, the
  first arena/table built-in typing rules, enum/tagged-variant construction,
  plain-enum comparisons/casts, compiler-injected `OS` / `TARGET_OS`,
  expected integer literal typing in annotated contexts, table-column slice
  access, statement-vs-expression `if`, tagged-enum/scalar `match` arm
  checking with duplicate/exhaustiveness diagnostics, first visibility/type
  diagnostics, bootstrap arena escape/reset checks (`S053`, `S055`, `S056`)
  with deferred return-slice propagation, and enough real-module coverage to
  sema-check `compiler/sema/check.nore`, but not code generation.

Current early codegen file scope:

- `compiler/codegen/c_types.nore`: lower checked Nore types into stable C-facing
  names and emit dependency-ordered typedefs for arrays, slices, values,
  structs, tables, table rows, and enums only. It should not absorb expression,
  statement, runtime, or whole-file orchestration logic.

- `compiler/codegen/c_emit_decl.nore`: emit stable C declaration spellings for
  both module-prefixed user functions and plain `ni_*` native runtime hooks. It
  owns ref-parameter pointer lowering and the shared declaration spelling used
  by later definition emitters, but not global initializers, expressions,
  statements, runtime bodies, or whole-file orchestration.

- `compiler/codegen/c_runtime.nore`: emit the minimal generated-C prelude only.
  It owns required standard includes plus builtin runtime-facing typedefs such
  as `ni_str`, `ni_Arena`, and `ni_OS`, but not bounds/cast helpers, native
  hook bodies, or orchestration.

- `compiler/codegen/c_main.nore`: assemble the current whole-file C skeleton
  only. It owns section ordering for prelude, typedefs, module-global
  definitions, generated helper functions, native/user prototypes, and the
  current function-definition slice, but not runtime helper bodies or driver
  behavior.

- `compiler/codegen/c_emit_expr.nore`: emit the current checked expression
  slice only. It owns literals, typed array literals, unary/binary operators,
  casts, identifiers, field/index/slice access, constructors, and plain
  user-function calls, including ref-aware identifier/field lowering plus
  slice-view ref-argument temporaries, but not statements, globals, `if` /
  `match` expression lowering, or helper runtimes.

- `compiler/codegen/c_emit_stmt.nore`: emit the current checked statement/body
  slice only. It owns blocks, local declarations, assignments, explicit
  `return`, expr statements, tail-value blocks lowered as implicit returns or
  local assignments, and the early statement-control-flow subset (`if`,
  `match`, `while`, `break`, `continue`), including value-position `if` /
  `match` lowered through statement codegen, but not general expression
  spelling, globals, or whole-file orchestration.

Remaining placeholder file scope:

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
   using the bootstrap compiler's current path policy, including lexical
   dot-segment normalization for loader dedup.
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

## Review Findings (Milestone 7, pending)

The `/simplify` review of the sema milestone (check.nore and supporting sema
modules) produced findings in three categories. Items marked **fixed** are
already applied; the rest are deferred for later milestones.

### Code Reuse

| # | Finding | Status |
|---|---------|--------|
| R1 | `flag_has` appears 3x (parser, check, symbols). check.nore uses it for node flags while symbols.nore uses it for symbol flags, so the implementations are semantically distinct despite identical bodies. | skipped (different domains) |
| R2 | `node_child_at` duplicated in parser.nore and check.nore (117 call sites in check.nore alone). Canonical home would be `node.nore` as a pub function. | **fixed** (moved to node.nore as pub, all call sites use node.node_child_at) |
| R3 | `name_text` in check.nore is a one-line alias for `token_text_or_synthetic`. | skipped (provides diagnostic-intent clarity) |
| R4 | Duplicate variant-name S069 scan appeared in two branches of `check_type_decl`, one for payload variants and one for plain variants. | **fixed** (hoisted before the has_type branch) |
| R5 | `check_enum_variant_access` and `check_enum_variant_call` share an identical visibility/S084 block (~10 lines each). | deferred (extracting a 15-parameter helper is marginal) |
| R6 | `check_global_decl` and `check_local_decl` share type-annotation resolution, init checking, S006, and S054 logic. | deferred (interleaved differences make extraction hard) |
| R7 | Five scope-find functions follow the same pattern but differ in SymbolKind filter. | deferred (language lacks closures or tagged-union predicates) |

### Code Quality

| # | Finding | Status |
|---|---------|--------|
| Q1 | Three arena escape functions (`check_return_identifier_arena_escape`, `check_return_constructor_arena_escape`, `check_return_variant_arena_escape`) share the same local-vs-propagate decision block with slight message variations. | deferred (13-parameter helper saves ~4 lines per site) |
| Q2 | Comments on small helpers sometimes restate WHAT rather than WHY. Most carry useful intent or grouping context per the project guideline ("add a short essential comment to newly introduced functions"). | skipped (no clear removals after closer review) |

### Efficiency

| # | Finding | Status |
|---|---------|--------|
| E1 | `module_scope_id` does an O(N) linear scan of all scopes, called ~14 times on hot paths (every type reference, every function call, every declaration). | **fixed** (O(1) direct index, module scopes are first N scopes in order) |
| E2 | Type interning functions (`type_named`, `type_array`, `type_slice`, `type_table_row`) do full linear scans of the types table on each call. | **fixed** (reverse scan, most recent types found first) |
| E3 | `check_match_enum_coverage` has O(V * A * V) behavior due to repeated `enum_find_variant` calls inside the variant loop. | **fixed** (store resolved variant_id in nodes.data2 during pattern check, compare directly in coverage) |
| E4 | `check_match_scalar_coverage` has O(arms^2) duplicate detection with repeated `str_to_i64` re-parsing. | deferred (precompute ScalarPatternInfo once, then compare) |
| E5 | `arena_dep_add` and `deferred_arena_check_add` do linear duplicate scans on each insert. Table is over-allocated to `node_count + 8` but actual entries are typically small. | deferred (low priority, practical sizes are small) |
| E6 | Recursive type-property queries (`type_has_arena_data`, `type_is_non_copyable`, etc.) are called repeatedly on the same types without memoization. Budget parameter bounds worst case. | **fixed** (per-type cached flags in types.data2 using bit-packed tri-state) |
| E7 | `builtin_call_kind` does up to 7 sequential string comparisons per non-user call expression. | skipped (only on the non-user-function path, short lists) |

### Priority for later milestones

All high-impact efficiency items (E1, E2, E3, E6) are now fixed. The remaining
deferred items (E4, E5) are low priority with small practical input sizes.

## Milestone Boundaries

This document freezes what later milestones may assume:

- Milestone 2 builds the typed buffers and utility modules described here
- Milestone 3 builds `Sources`, line mapping, and diagnostics on top of those helpers
- Milestone 4 and later consume the same `Tokens` and `Nodes` model instead of inventing alternate trees

If a later milestone needs to change this document, the change should be deliberate and small, not a quiet local workaround.
