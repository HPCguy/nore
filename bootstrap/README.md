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

Current wrapper mode:

- `./bootstrap/bootstrap.sh` rebuilds `./norec`
- `./bootstrap/bootstrap.sh program.nore`
- `./bootstrap/bootstrap.sh --run program.nore -- arg1 arg2`

That wrapper mode is transitional. It keeps the self-hosted compiler path usable while native driver parity is still in progress.
