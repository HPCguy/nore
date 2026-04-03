# Build the trusted stage-0 seed compiler from bootstrap/norec-stage0.c.

CC ?= clang

CFLAGS = -std=c99
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -Werror
CFLAGS += -pedantic
CFLAGS += -O2

DEBUGFLAGS = -std=c99
DEBUGFLAGS += -Wall
DEBUGFLAGS += -Wextra
DEBUGFLAGS += -g
DEBUGFLAGS += -O0

STAGE0 ?= $(CURDIR)/../tmp/bootstrap/stage0/norec-stage0
SOURCE := norec-stage0.c

stage0: $(STAGE0)

$(STAGE0): $(SOURCE)
	@mkdir -p "$(dir $@)"
	$(CC) $(CFLAGS) -o "$@" "$(SOURCE)"

debug: $(SOURCE)
	@mkdir -p "$(dir $(STAGE0))"
	$(CC) $(DEBUGFLAGS) -o "$(STAGE0)" "$(SOURCE)"

clean:
	rm -f "$(STAGE0)"

.PHONY: stage0 debug clean
