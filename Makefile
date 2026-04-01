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
TARGET := nore
NOREC := norec
STAGE0_SOURCE := $(BOOTSTRAP_DIR)/nore.c

# Default target
all: $(TARGET)

# Build the stage-0 compiler from the bootstrap seed.
$(TARGET): $(STAGE0_SOURCE) $(BOOTSTRAP_DIR)/Makefile
	$(MAKE) -C $(BOOTSTRAP_DIR) CC="$(CC)" STAGE0="$(ROOT_DIR)/$(TARGET)" stage0

# Debug build
debug: $(STAGE0_SOURCE) $(BOOTSTRAP_DIR)/Makefile
	$(MAKE) -C $(BOOTSTRAP_DIR) CC="$(CC)" STAGE0="$(ROOT_DIR)/$(TARGET)" debug

# Rebuild the self-hosted compiler from the bootstrap seed.
$(NOREC): $(BOOTSTRAP_DIR)/bootstrap.sh $(BOOTSTRAP_DIR)/Makefile $(STAGE0_SOURCE)
	@chmod +x $(BOOTSTRAP_DIR)/bootstrap.sh
	@"$(BOOTSTRAP_DIR)/bootstrap.sh"

# Clean build artifacts
clean:
	rm -f "$(ROOT_DIR)/$(TARGET)" "$(ROOT_DIR)/$(NOREC)"
	rm -rf "$(ROOT_DIR)/tmp/bootstrap"

# Run error code tests
test-errors: $(TARGET)
	@chmod +x tests/run_error_tests.sh
	@./tests/run_error_tests.sh

# Run success tests
test-success: $(TARGET)
	@chmod +x tests/run_success_tests.sh
	@./tests/run_success_tests.sh

# Run all tests
test: $(TARGET)
	@chmod +x tests/run_error_tests.sh tests/run_success_tests.sh
	@./tests/run_error_tests.sh
	@echo ""
	@./tests/run_success_tests.sh

# Run stdlib tests only
test-std: $(TARGET)
	@chmod +x tests/run_std_tests.sh
	@./tests/run_std_tests.sh

# Run compiler-specific bootstrap/selfhost tests
test-compiler: $(TARGET)
	@chmod +x tests/run_compiler_tests.sh
	@./tests/run_compiler_tests.sh

# Phony targets
.PHONY: all debug clean norec test-errors test-success test-std test-compiler test
