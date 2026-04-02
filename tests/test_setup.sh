# Shared compiler entrypoint setup for test runners.
# Expects SCRIPT_DIR to be set by the caller.

DEFAULT_NORE_BIN="${SCRIPT_DIR}/../bootstrap/bootstrap.sh"
if [ ! -x "$DEFAULT_NORE_BIN" ]; then
    DEFAULT_NORE_BIN="${SCRIPT_DIR}/../nore"
fi
NORE_BIN="${NORE_BIN:-$DEFAULT_NORE_BIN}"

if [ ! -x "$NORE_BIN" ]; then
    echo "Error: compiler entrypoint not found at $NORE_BIN"
    echo "Run 'make' first to build the compiler"
    exit 1
fi
