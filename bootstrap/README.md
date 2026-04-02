# Bootstrap Seed

This directory holds the trusted stage-0 compiler seed and the rebuild-from-seed entrypoint.

Contents:

- `nore.c`: the current C bootstrap compiler source
- `Makefile`: builds a temporary stage-0 compiler binary
- `bootstrap.sh`: rebuilds the self-hosted compiler as `./norec`

Bootstrap flow:

1. Build a temporary stage-0 compiler under `tmp/bootstrap/stage0/`
2. Use that temporary compiler to build a temporary bootstrap compiler under `tmp/bootstrap/stage1/`
3. Use the bootstrap compiler to emit `tmp/bootstrap/generated-c/norec.c`
4. Compile that C file with Clang to the repo-root `./norec`

Important:

- `tmp/bootstrap/stage1/bootstrap-compiler` and `./norec` are different build artifacts from different bootstrap stages
- `bootstrap-compiler` is the stage-1 native binary produced directly by the trusted C seed
- `./norec` is the later native binary produced from C emitted by that stage-1 compiler
- matching native binary size is not an expected invariant
- the current fixed-point check is emitted-C stability (`nore_stage2.c` vs `nore_stage3.c`), not native-binary identity

Current wrapper mode:

- `make` rebuilds `./norec` as the default compiler artifact
- `make stage0` rebuilds the explicit C-seed fallback at `./nore`
- `./bootstrap/bootstrap.sh` rebuilds `./norec`
- `./bootstrap/bootstrap.sh program.nore`
- `./bootstrap/bootstrap.sh --run program.nore -- arg1 arg2`

That wrapper mode is transitional. It keeps the self-hosted compiler path usable while native driver parity is still in progress.
