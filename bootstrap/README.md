# Bootstrap Seed

This directory holds the trusted stage-0 compiler seed and the rebuild-from-seed entrypoint for the self-hosted compiler.

## Contents

- `norec-stage0.c`: the trusted C seed
- `Makefile`: builds the explicit stage-0 fallback compiler
- `bootstrap.sh`: rebuilds the repo-root `./norec` from the trusted seed

## Rebuild Flow

1. Build a temporary stage-0 compiler under `tmp/bootstrap/stage0/`
2. Use that temporary compiler to build a temporary stage-1 compiler under `tmp/bootstrap/stage1/`
3. Use the stage-1 compiler to emit `tmp/bootstrap/generated-c/norec.c`
4. Compile that generated C file with Clang to the repo-root `./norec`

## Usage

```bash
make stage0
./norec-stage0 program.nore
./bootstrap/bootstrap.sh
make norec-stripped
```

`./norec` is the normal compiler path. `make norec-stripped` builds a separate `./norec-stripped` binary by running the same rebuild flow with `--strip-asserts`.

For custom bootstrap outputs, set `NOREC_OUT=/path/to/bin`. Set `STRIP_ASSERTS=1` to emit the compiler with effect-free asserts omitted.
