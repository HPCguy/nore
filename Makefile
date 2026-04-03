# Nore Compiler Makefile

# Compiler
CC ?= clang

# Compiler flags for release build
CFLAGS = -std=c99      # Use C99 standard for maximum portability
CFLAGS += -Wall        # Enable all common warning messages
CFLAGS += -Wextra      # Enable additional warning messages beyond -Wall
CFLAGS += -Werror      # Treat all warnings as errors (ensures clean code)
CFLAGS += -pedantic    # Strict ISO C compliance, reject extensions
CFLAGS += -O2          # Optimization level 2 (good balance of speed and compile time)

# Compiler flags for debug build
DEBUGFLAGS = -std=c99  # Use C99 standard
DEBUGFLAGS += -Wall    # Enable all common warnings
DEBUGFLAGS += -Wextra  # Enable extra warnings
DEBUGFLAGS += -g       # Include debug symbols for debuggers (gdb, lldb)
DEBUGFLAGS += -O0      # Disable optimizations for accurate debugging

ROOT_DIR := $(CURDIR)
BOOTSTRAP_DIR := $(ROOT_DIR)/bootstrap
STAGE0_BIN := norec-stage0
NOREC := norec
BOOTSTRAP_BIN := $(BOOTSTRAP_DIR)/bootstrap.sh
STAGE0_SOURCE := $(BOOTSTRAP_DIR)/norec-stage0.c

# Default target: rebuild the self-hosted compiler from the trusted seed.
all: $(NOREC)

# Build the stage-0 compiler from the bootstrap seed.
$(STAGE0_BIN): $(STAGE0_SOURCE) $(BOOTSTRAP_DIR)/Makefile
	$(MAKE) -C $(BOOTSTRAP_DIR) CC="$(CC)" STAGE0="$(ROOT_DIR)/$(STAGE0_BIN)" stage0

# Explicit fallback entrypoint for the trusted C seed.
stage0: $(STAGE0_BIN)

# Debug build
debug: $(STAGE0_SOURCE) $(BOOTSTRAP_DIR)/Makefile
	$(MAKE) -C $(BOOTSTRAP_DIR) CC="$(CC)" STAGE0="$(ROOT_DIR)/$(STAGE0_BIN)" debug

# Rebuild the self-hosted compiler from the bootstrap seed.
$(NOREC): $(BOOTSTRAP_DIR)/bootstrap.sh $(BOOTSTRAP_DIR)/Makefile $(STAGE0_SOURCE)
	@chmod +x $(BOOTSTRAP_DIR)/bootstrap.sh
	@"$(BOOTSTRAP_DIR)/bootstrap.sh"

# Clean build artifacts
clean:
	rm -f "$(ROOT_DIR)/$(STAGE0_BIN)" "$(ROOT_DIR)/$(NOREC)" "$(ROOT_DIR)/nore"
	rm -rf "$(ROOT_DIR)/tmp/bootstrap"

# Run error code tests through the default self-hosted path
test-errors: $(NOREC)
	@chmod +x tests/run_error_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" ./tests/run_error_tests.sh

# Run error code tests with the explicit stage-0 fallback compiler
test-errors-stage0: $(STAGE0_BIN)
	@chmod +x tests/run_error_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" ./tests/run_error_tests.sh

# Backward-compatible alias for the self-hosted default path
test-errors-norec: test-errors

# Run success tests through the default self-hosted path
test-success: $(NOREC)
	@chmod +x tests/run_success_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" ./tests/run_success_tests.sh

# Run success tests with the explicit stage-0 fallback compiler
test-success-stage0: $(STAGE0_BIN)
	@chmod +x tests/run_success_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" ./tests/run_success_tests.sh

# Backward-compatible alias for the self-hosted default path
test-success-norec: test-success

# Run all language suites through the default self-hosted path
test: $(NOREC)
	@chmod +x tests/run_error_tests.sh tests/run_success_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" ./tests/run_error_tests.sh
	@echo ""
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" ./tests/run_success_tests.sh

# Run all language suites with the explicit stage-0 fallback compiler
test-stage0: $(STAGE0_BIN)
	@chmod +x tests/run_error_tests.sh tests/run_success_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" ./tests/run_error_tests.sh
	@echo ""
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" ./tests/run_success_tests.sh

# Legacy alias for the self-hosted default language-suite path
test-parity: test

# Run stdlib tests only through the default self-hosted path
test-std: $(NOREC)
	@chmod +x tests/run_std_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" ./tests/run_std_tests.sh

# Run stdlib tests only with the explicit stage-0 fallback compiler
test-std-stage0: $(STAGE0_BIN)
	@chmod +x tests/run_std_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" ./tests/run_std_tests.sh

# Backward-compatible alias for the self-hosted default path
test-std-norec: test-std

# Run compiler-specific integration and rebuild tests through the default compiler path
test-compiler: $(NOREC)
	@chmod +x tests/run_compiler_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(NOREC)" COMPILER_TEST_MODE="norec" ./tests/run_compiler_tests.sh

# Run compiler-specific tests with the explicit stage-0 fallback compiler
test-compiler-stage0: $(STAGE0_BIN)
	@chmod +x tests/run_compiler_tests.sh
	@NORE_BIN="$(ROOT_DIR)/$(STAGE0_BIN)" COMPILER_TEST_MODE="stage0" ./tests/run_compiler_tests.sh

# Compare end-to-end compiler compile time under the stage-0 and self-hosted drivers.
bench-compiler: $(STAGE0_BIN) $(NOREC)
	@chmod +x benchmark/compile_compiler.sh
	@CC="$(CC)" ./benchmark/compile_compiler.sh

# Phony targets
.PHONY: all stage0 debug clean norec test-errors test-errors-stage0 test-errors-norec test-success test-success-stage0 test-success-norec test-std test-std-stage0 test-std-norec test-compiler test-compiler-stage0 bench-compiler test test-stage0 test-parity
