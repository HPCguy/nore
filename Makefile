# Nore Compiler Makefile

# Compiler
CC = clang

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

TARGET = nore
SOURCE = nore.c

# Default target
all: $(TARGET)

# Build the compiler
$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

# Debug build
debug: $(SOURCE)
	$(CC) $(DEBUGFLAGS) -o $(TARGET) $(SOURCE)

# Clean build artifacts
clean:
	rm -f $(TARGET)

# Run error code tests
test-errors: $(TARGET)
	@chmod +x tests/run_error_tests.sh
	@./tests/run_error_tests.sh

# Phony targets
.PHONY: all debug clean test-errors
