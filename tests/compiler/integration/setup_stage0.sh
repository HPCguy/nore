# Shared stage-0 bootstrap setup. Source from integration test scripts.
# Expects ROOT_DIR to be set by the caller.

BOOTSTRAP_DIR="$ROOT_DIR/bootstrap"
STAGE0_DIR="$ROOT_DIR/tmp/bootstrap/stage0"
STAGE0="$STAGE0_DIR/nore-stage0"
CC_BIN="${CC:-clang}"
CLANG_FLAGS=(-std=c99 -O2 -fwrapv -Werror)

mkdir -p "$STAGE0_DIR"

if [ ! -x "$STAGE0" ] || [ "$BOOTSTRAP_DIR/nore.c" -nt "$STAGE0" ] || [ "$BOOTSTRAP_DIR/Makefile" -nt "$STAGE0" ]; then
    make -C "$BOOTSTRAP_DIR" CC="$CC_BIN" STAGE0="$STAGE0" stage0 >/dev/null
fi
if [ ! -L "$STAGE0_DIR/std" ] || [ "$(readlink "$STAGE0_DIR/std")" != "$ROOT_DIR/std" ]; then
    rm -rf "$STAGE0_DIR/std"
    ln -sfn "$ROOT_DIR/std" "$STAGE0_DIR/std"
fi
