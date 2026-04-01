# Self-Hosting Bootstrap Plan

## Goal

Build a new multi-module Nore compiler in Nore, in Nore's own style:

- flat tables, indices, and slices instead of pointer-heavy trees
- arenas for lifetime grouping
- explicit modules with small responsibilities
- C as the backend IR during bootstrap

The immediate goal is **not** "port `nore.c`". The goal is to build a new compiler that:

1. compiles a useful bootstrap subset of Nore to C
2. can compile its own source code
3. gradually grows toward feature parity with the current C compiler

The current C compiler remains the stage-0 bootstrap compiler until late in the plan.

---

## Ground Rules

### What counts as success

- A Nore compiler source tree exists under a dedicated directory, split into modules.
- The compiler data model is table-based, not pointer-based.
- The compiler can compile itself before it is asked to replace `nore.c`.
- The thin "driver" problem is treated separately from the language frontend.

### What we should not do

- Do not translate `nore.c` function-by-function into Nore.
- Do not block the project on generics, hash maps, or a native driver.
- Do not chase full language parity before the first self-compile.

### Architectural stance

The compiler should lean into Nore's model:

- `Sources` table for loaded modules/files
- `Tokens` table for lexed tokens
- `Nodes` table for AST/HIR nodes
- `Types` / `Symbols` / `Scopes` tables for semantic state
- string data represented as spans into source buffers or an explicit string pool
- parent/child/sibling or range-based relationships via integer indices

The JSON parser in `std/json.nore` is the proof of concept for this style.

---

## Proposed Repository Layout

The plan assumes the Nore-written compiler sources live in a committed top-level
`compiler/` tree, with committed tests beside the existing test suite and
temporary build artifacts staying under `tmp/`.

```text
docs/
  self-hosting-bootstrap-plan.md

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
    clang_driver.nore       # later milestone

tests/
  bootstrap/
    support/
    lexer/
    parser/
    imports/
    sema/
    codegen/
    selfhost/

tmp/
  bootstrap/
    stage1/
    stage2/
    generated-c/
    bins/
```

### Why this layout

- `compiler/` is committed and versioned, unlike `tmp/`
- `support/`, `frontend/`, `sema/`, and `codegen/` match the milestone order
- `tests/bootstrap/` can grow incrementally without mixing bootstrap-specific
  tests into the mature test corpus too early
- `tmp/bootstrap/` keeps generated C, temporary binaries, and self-hosting
  stage artifacts out of the repository

### Naming recommendation

Use `compiler/` as the durable source tree name from day one. Avoid calling the
source directory `bootstrap/`, because that name becomes misleading once the
compiler starts self-compiling and moves toward default status.

---

## Gap Classification

### Closed before any serious compiler implementation

1. **User-function mutual recursion**
   Done. Codegen now emits user-function prototypes before function bodies, so same-module forward calls and mutual recursion work in generated C.

2. **Add tests for the mandatory gap**
   Done. A dedicated regression test covers forward calls and mutual recursion.

### Needed before specific milestones, not before day one

1. **Native driver support**
   Not needed to start. A bootstrap compiler can emit C and rely on a shell script or the existing stage-0 compiler workflow to invoke Clang. Driver-native work comes later.

2. **Canonical path / richer OS helpers**
   Useful for a polished driver and robust import deduplication, but not required for the first compiler. Early bootstrap can use string-based path joining plus disciplined tests.

3. **Generic collections / hash maps**
   Not required. Use hand-written typed buffers/tables and linear search first. Add better data structures only when real pain shows up.

4. **Closing the cross-function `arena_reset` hole**
   Desirable, but not a blocker if compiler code avoids the bad pattern and the rule is documented internally.

---

## Step 0: Close Mandatory Gaps

**Status:** Complete

### Gaps to close before starting implementation

- user-function mutual recursion / forward declarations in generated C

### Deliverables

1. Codegen emits user-function prototypes before function bodies.
2. New tests prove mutually recursive user functions compile and run.
3. Stale gap notes are cleaned up so the bootstrap plan is based on reality.

### Exit criteria

- `make test` passes
- dedicated regression tests for mutual recursion exist

### Why this is Step 0

Without this, the compiler source will either:

- contort itself unnaturally to avoid ordinary helper decomposition

That is the wrong foundation.

---

## Milestone 1: Freeze the Bootstrap Scope

**Status:** Complete

Frozen in `docs/compiler-bootstrap-architecture.md` and the committed `compiler/`
tree.

### Gaps to close before starting

- Step 0 complete

### Objective

Define the **bootstrap subset** and the compiler architecture before writing modules.

### Deliverables

1. A source tree layout for the new compiler, for example:
   - `compiler/main.nore`
   - `compiler/support/source.nore`
   - `compiler/support/path.nore`
   - `compiler/support/diag.nore`
   - `compiler/frontend/token.nore`
   - `compiler/frontend/lexer.nore`
   - `compiler/frontend/node.nore`
   - `compiler/frontend/parser.nore`
   - `compiler/sema/symbols.nore`
   - `compiler/sema/types.nore`
   - `compiler/sema/check.nore`
   - `compiler/codegen/c_main.nore`
   - `compiler/driver/clang_driver.nore` later

2. A written "compiler coding subset" rule set:
   - allowed language features in compiler sources
   - temporary bans on features not yet supported by the new compiler
   - capacity and arena discipline conventions

3. A concrete table-based IR design for:
   - sources/modules
   - tokens
   - syntax nodes
   - diagnostics

### Exit criteria

- architecture note is stable enough that modules can be implemented independently
- no unresolved argument about "tree of structs" vs "tables of nodes"

### Notes

This is where we commit to a Nore-shaped compiler, not a C-shaped compiler wearing Nore syntax.

---

## Milestone 2: Build the Compiler Support Library

**Status:** Complete

### Gaps to close before starting

- Milestone 1 complete

### Objective

Create the typed storage and utility modules needed by the compiler before the lexer/parser work starts.

### Deliverables

1. Typed growable buffers implemented in Nore style, likely via chunked or pre-reserved arena-backed storage:
   - `ByteBuf`
   - `I64Buf`
   - `TokenBuf`
   - `NodeBuf`
   - `DiagBuf`
   - optional `SpanBuf`, `ModuleBuf`, `ScopeBuf`

2. Utility modules:
   - string/span helpers
   - path join / dirname / extension checks
   - source text slicing helpers
   - small formatting helpers for diagnostics / C emission

3. Conventions for allocation:
   - long-lived compiler state arena
   - per-module parse arena if useful
   - optional scratch arena with explicit reset discipline

### Exit criteria

- support-library tests pass
- no compiler module needs raw ad hoc storage tricks outside these helpers

### Why this milestone exists

The current C compiler relies heavily on `malloc`/`realloc`. The Nore compiler needs explicit replacements before frontend work can move cleanly.

---

## Milestone 3: Source Manager and Diagnostics

**Status:** Complete

### Gaps to close before starting

- Milestone 2 complete

### Objective

Build the shared infrastructure that every stage will use.

### Deliverables

1. `Sources` module:
   - load file bytes
   - track module path, display path, source length
   - own imported source buffers for compiler lifetime

2. `Diagnostics` module:
   - store code, source, line, column, message spans
   - collect multiple errors
   - stable formatting compatible with current compiler style where practical

3. `LineMap` or equivalent helper:
   - convert byte offsets to line/column
   - reused by lexer, parser, typechecker

### Exit criteria

- a small smoke program can load a file and emit a formatted error at a given byte offset

### Deferred gaps

- canonical path handling can stay simple here
- no driver native needed

---

## Milestone 4: Lexer for the Bootstrap Subset

**Status:** Complete

Committed in `compiler/frontend/token.nore`, `compiler/frontend/lexer.nore`, and
`tests/bootstrap/lexer/`.

### Gaps to close before starting

- Milestone 3 complete

### Objective

Build a lexer that can tokenize the bootstrap subset plus current compiler source conventions.

### Deliverables

1. Token kinds and keyword handling
2. Token table storing:
   - kind
   - source id
   - byte start
   - byte length
   - literal payload where needed
3. Lexer diagnostics for invalid characters and unterminated literals/comments
4. `--lexer`-style debug output for bootstrap testing

### Exit criteria

- lexer golden tests for representative compiler modules
- ability to lex at least one bootstrap compiler module end to end

### Notes

Keep tokens as rows in a table, not heap-allocated linked structures.

---

## Milestone 5: Parser for a Single Module

**Status:** Complete

Committed in `compiler/frontend/node.nore`, `compiler/frontend/parser.nore`, and
`tests/bootstrap/parser/`.

### Gaps to close before starting

- Milestone 4 complete
- Step 0 complete

### Objective

Parse one source file into a table-based syntax tree.

### Deliverables

1. Node representation:
   - one `Nodes` table with `kind`, `type`, `span`, and index/range fields
   - auxiliary child/range tables as needed
2. Parsing of the bootstrap subset:
   - top-level declarations
   - blocks/statements
   - expressions
   - types
3. Debug dump similar to `--parser`
4. Parse-only tests for bootstrap modules

### Exit criteria

- parser can parse its own low-level support modules
- AST dump is stable enough to diff in tests

### Non-goals

- no typechecking yet
- no multi-module import graph yet

---

## Milestone 6: Multi-Module Frontend Loading

**Status:** Complete

Committed in `compiler/frontend/loader.nore` and `tests/bootstrap/imports/`.

### Gaps to close before starting

- Milestone 5 complete

### Objective

Extend the frontend from one module to a whole import graph.

### Deliverables

1. Module registry:
   - source path
   - alias
   - parsed root node
   - import edges

2. Import resolution:
   - relative path handling
   - `std/` handling via a bootstrap rule compatible with current behavior
   - duplicate-import suppression

3. Visibility and qualified-name metadata carried into semantic analysis

4. Integration tests:
   - simple imports
   - diamond imports
   - std imports

### Exit criteria

- the bootstrap compiler can parse a small multi-module Nore program

### Gaps explicitly deferred

- executable-path native for a polished stdlib path policy
- canonical realpath deduplication

---

## Milestone 7: Semantic Analysis for the Bootstrap Subset

**Status:** Complete

Started in `compiler/sema/` and `tests/bootstrap/sema/`.
The current committed slice binds module scopes, top-level declarations,
locals/params, module-qualified lookups, and now typechecks the first
user-declared subset: literals, field access, constructors, calls,
assignments, block/if values, return/condition diagnostics, and the first
compiler-built-in slice for `arena`/`arena_alloc`/`arena_reset` and
`table_*`, plus native declaration validation. The current enum slice now
handles variant construction, plain-enum comparisons/casts, typed array
literals in annotated contexts, and slice/string-literal ref arguments.
Compiler-injected `OS` / `TARGET_OS` names and expected integer literal
typing in annotated contexts are now wired through the same module-scope and
expected-type machinery. Table field access now produces slice-typed column
views, statement `if` no longer forces expression branch unification, and the
current `match` slice binds tagged-enum payload names, checks scalar literal
and wildcard arms, and now emits duplicate/non-exhaustive diagnostics.
Lexical import-path normalization now keeps transitive module
identity stable enough for broader compiler coverage. One real compiler
support module (`compiler/support/byte_buf.nore`), one real compiler sema
module (`compiler/sema/check.nore`), and four real std modules
(`std/string.nore`, `std/file.nore`, `std/json.nore`, `std/io.nore`) now
sema-check cleanly. The arena-safety slice now covers local provenance,
direct/indirect return escape diagnostics (`S053`), immutable reset rejection
(`S055`), and in-function reset invalidation (`S056`). The documented
cross-function reset hole remains a non-blocking limitation, but Milestone 7
itself is now closed.

### Gaps to close before starting

- Milestone 6 complete

### Objective

Typecheck enough of Nore to compile the compiler itself.

### Deliverables

1. Name resolution:
   - locals
   - globals
   - module-qualified lookups
   - visibility checks

2. Type system support for bootstrap compiler code:
   - scalars, arrays, slices, strings
   - values, structs, tables
   - enums / tagged unions
   - refs and mut refs

3. Statement/expression checks:
   - declarations, assignment, control flow
   - function calls
   - `match`
   - built-ins used by the compiler and stdlib

4. Arena safety support required by compiler code:
   - enough of existing escape analysis to safely compile the compiler sources

5. Diagnostic coverage for the subset

### Exit criteria

- the bootstrap compiler can typecheck its own support modules and frontend modules

### Important rule

Do not try to reach full parity here. Reach "enough to compile the compiler source we actually wrote".

---

## Milestone 8: C Codegen for the Bootstrap Subset

**Status:** Complete

Started in `compiler/codegen/c_types.nore`, `compiler/codegen/c_emit_decl.nore`,
`compiler/codegen/c_runtime.nore`, `compiler/codegen/c_main.nore`,
`compiler/codegen/c_emit_expr.nore`, `compiler/codegen/c_emit_stmt.nore`, and
`tests/bootstrap/codegen/`. The current committed slice lowers sema types to
stable C names, emits typedefs for arrays, slices, values, structs, tables,
table rows, and enums, emits stable user-function prototypes in module order
plus plain `ni_*` native hook prototypes with ref-parameter pointer lowering,
emits stable module-prefixed C definitions for the first top-level global slice
(`val` / `mut` bindings with currently supported initializer expressions), and
now assembles the first whole-file C file with a minimal core prelude
(`stdint.h`, `stdio.h`, `stdlib.h`, `ni_str`, `ni_Arena`, `ni_OS`, and compile-time
`NI_TARGET_OS`) plus native/user prototype sections and a broader early
function-body subset: empty bodies, explicit returns, stage-0-compatible
`assert` diagnostics (`R001`, `stderr`, exit code 2), tail-value blocks, local
declarations, assignments, literals, unary/binary/cast expressions, typed
array literals, field/index/slice access, constructors,
same-translation-unit user-function calls, injected `TARGET_OS` / `OS.*`
names, ref arguments over slice-view temporaries, statement-form `if` /
`match` / `while` with `break` / `continue`, and value-position `if` /
`match` lowered through statement codegen for returns and local initializers.
The current runtime/codegen slice now also lowers the first builtin calls
(`arena`, `arena_alloc`, `arena_reset`, `table_alloc`, `table_get`,
`table_insert`, `table_len`) to generated C helpers, emits the first shared
arena/native runtime bodies (`ni_arena_*`, `ni_fd_*`, `ni_mem_copy`, `ni_exit`,
`ni_args`), handles expected-type view coercions between `str`, `[u8]`, and
`[u8; N]` in returns/locals/assignments/calls, emits a root `main(...)` wrapper
when the graph defines a zero-arg Nore `main`, and materializes per-table
alloc/get/insert helpers only for table builtins used by the current module
graph.
Dedicated smoke tests now prove one real compiler support module
(`compiler/support/byte_buf.nore`), one real compiler frontend module
(`compiler/frontend/node.nore`), one real compiler parser module globals
slice, a dedicated `assert`-lowering regression, and two real std modules
(`std/io.nore`, `std/file.nore`) plus a focused builtin-runtime fixture lower
through the current whole-file codegen without asserting. A real end-to-end
bootstrap smoke now also emits C for `tests/success/print_hello.nore`, builds it
with Clang, and runs the resulting binary successfully.

### Gaps to close before starting

- Milestone 7 complete

### Objective

Emit C for the subset used by the bootstrap compiler.

### Deliverables

1. C type lowering for bootstrap subset types
2. C emission for:
   - globals
   - functions
   - control flow
   - expressions
   - tables
   - tagged enums
3. Runtime helpers needed by generated C
4. **User-function prototypes emitted first**
   This is now part of the baseline, not an optional polish item.

### Exit criteria

- bootstrap compiler can compile non-trivial subset programs to C
- generated C builds with Clang through an external wrapper script or manual step

### Deferred gaps

- direct native driver integration
- polished CLI behavior beyond what is needed for tests

---

## Milestone 9: First End-to-End Bootstrap Compiler

**Status:** Complete

Committed in `compiler/main.nore`, `tests/run_bootstrap_tests.sh`, and
`tests/bootstrap/selfhost/`.

### Gaps to close before starting

- Milestone 8 complete

### Objective

Produce the first usable Nore-written compiler binary, even if it is still subset-limited and still depends on external scripts for the last driver step.

### Deliverables

1. `compiler/main.nore` that:
   - loads the root module
   - parses the import graph
   - typechecks
   - emits C

2. Build/test glue:
   - compile the bootstrap compiler with stage-0 `nore`
   - run the bootstrap compiler to emit C for sample programs
   - compile emitted C with Clang from the shell

3. A dedicated bootstrap test target

### Exit criteria

- a Nore-written compiler can compile sample programs from the bootstrap subset

### Notes

The committed `compiler/main.nore` is now a real pipeline instead of a stub:
it loads the graph, prints loader/sema diagnostics, emits C, wires `args()`
through the generated runtime, and `tests/bootstrap/selfhost/smoke_test.sh`
now proves the full stage-0 -> bootstrap compiler -> emitted C -> Clang ->
native binary path for representative sample programs using the same basic
Clang mode that stage-0 already uses for compiled Nore programs.

This is the first real milestone where "compiler in Nore" exists, but it is not self-hosted yet.

---

## Milestone 10: Self-Compile

**Status:** Complete

Committed in `tests/bootstrap/selfhost/self_compile_test.sh`.

### Gaps to close before starting

- Milestone 9 complete
- bootstrap compiler can compile every module in its own source tree

### Objective

Have the Nore-written compiler compile itself.

### Deliverables

1. Self-compile script:
   - stage-0 `nore` compiles bootstrap compiler -> `nore1`
   - `nore1` compiles the bootstrap compiler source again -> `nore2`

2. Stability checks:
   - compare behavior of `nore1` and `nore2`
   - optionally compare generated C for representative modules

3. Expand subset support only as needed to get self-compile over the line

### Exit criteria

- `nore1` successfully compiles the compiler source tree
- the produced compiler can repeat the build

The committed selfhost test now proves:

- stage-0 `nore` compiles the bootstrap compiler to `nore1`
- `nore1` compiles `compiler/main.nore` to `nore_stage2.c`, which Clang builds to `nore2`
- `nore2` compiles `compiler/main.nore` again to `nore_stage3.c`, which Clang builds to `nore3`
- `nore_stage2.c` and `nore_stage3.c` compare identical for the current compiler tree

### This is the actual "start of self-hosting"

At this point the project is self-compiling, but not yet a drop-in replacement for the C compiler.

---

## Milestone 11: Close the Driver Gap

### Gaps to close before starting

- Milestone 10 complete

### Objective

Replace the remaining external-driver dependency when it is worth doing.

### Candidate approaches

1. **Thin wrapper approach**
   Keep a tiny external shell/C wrapper that invokes the Nore compiler and Clang.

2. **New native approach**
   Add focused natives for:
   - temporary file creation
   - spawning Clang / process execution
   - maybe path canonicalization if needed

### Recommendation

Prefer the thin wrapper first. Add new natives only when the wrapper becomes a real liability.

### Exit criteria

- building ordinary Nore programs no longer depends on hand-run shell steps

### Why this is not Step 0

The frontend is the hard part. The driver is plumbing.

---

## Milestone 12: Grow from Bootstrap Subset to Full Parity

### Gaps to close before starting

- Milestone 10 complete

### Objective

Turn the self-compiling compiler into the default compiler.

### Deliverables

1. Close remaining frontend/typecheck/codegen gaps against the current C compiler
2. Port the current test suite to run against the Nore compiler
3. Add new regression tests for bootstrap-only failures found along the way
4. Keep the C compiler as a fallback until parity is credible

### Exit criteria

- full test suite passes under the Nore compiler
- C compiler is no longer the default path

---

## Recommended Work Order Summary

1. Step 0: close mandatory language/codegen gaps
2. Milestone 1: freeze subset and architecture
3. Milestone 2: support library
4. Milestone 3: sources + diagnostics
5. Milestone 4: lexer
6. Milestone 5: single-module parser
7. Milestone 6: multi-module loading
8. Milestone 7: semantic analysis for subset
9. Milestone 8: C codegen for subset
10. Milestone 9: first end-to-end Nore compiler
11. Milestone 10: self-compile
12. Milestone 11: driver gap
13. Milestone 12: parity and switchover

---

## Honest Risk Assessment

### Likely hard parts

1. **Representing the compiler cleanly without pointer-rich data structures**
   This is the main design challenge, not parsing or C emission.

2. **Living without generic collections**
   This is survivable, but it forces discipline and more hand-written typed storage modules.

3. **Avoiding accidental scope creep**
   Full parity is a trap before the first self-compile.

4. **Keeping bootstrap sources inside the supported subset**
   The compiler source must not outrun the compiler.

### Probably not real blockers

1. Generics
2. Recursive types
3. Hash maps on day one
4. Native driver support on day one

---

## Recommendation

Treat the **driver** as a late-stage integration task.

Step 0 is done. Start the compiler implementation at Milestone 1, and then stay disciplined:

- subset first
- tables first
- self-compile before parity
