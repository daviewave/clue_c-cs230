# Makefile for the Clue text adventure (CS230 project 2).
#
# Targets (docs/conventions.md section 3):
#   all      release build of build/adventure (default)
#   debug    same binary with -O0 -g3 (run `make clean` first to force a rebuild)
#   test     builds, then runs test/run_tests.sh (unit + end-to-end)
#   check    gcc -fanalyzer over every file in src/
#   run      builds and runs the game interactively
#   dist     flat Gradescope bundle in dist/ with a generated Makefile, then builds it
#   clean    removes build/ and dist/

# ---- toolchain and flags -----------------------------------------------------
# The spec demands -std=c99. The warning set is the conventions' mandatory
# development set and is treated as errors so nothing slips through.
# -MMD -MP writes a .d file next to each object so header edits trigger rebuilds.
# make predefines CC=cc, so ?= would never apply; only a command-line or
# environment CC overrides this default.
ifeq ($(origin CC),default)
CC := gcc
endif
STD := -std=c99
WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes \
            -Wmissing-prototypes -Wconversion -Wvla -Werror
OPTIMISE := -O2
CFLAGS ?= $(STD) $(WARNINGS) $(OPTIMISE) -MMD -MP
LDFLAGS ?=

# ---- layout ------------------------------------------------------------------
BUILD := build
SRC_DIR := src
BIN := $(BUILD)/adventure
SOURCES := $(wildcard $(SRC_DIR)/*.c)
HEADERS := $(wildcard $(SRC_DIR)/*.h)
OBJECTS := $(patsubst $(SRC_DIR)/%.c,$(BUILD)/obj/%.o,$(SOURCES))
# Unit tests link against the data modules only; test_adventure includes
# adventure.c directly (the include trick) to reach its static functions.
MODULE_OBJECTS := $(filter-out $(BUILD)/obj/adventure.o,$(OBJECTS))
UNIT_SOURCES := $(wildcard test/unit/test_*.c)
UNIT_BINARIES := $(patsubst test/unit/%.c,$(BUILD)/test/%,$(UNIT_SOURCES))
DIST_FILES := $(SOURCES) $(HEADERS) README.txt

.PHONY: all debug test unit-binaries check run dist clean

# ---- builds ------------------------------------------------------------------
all: $(BIN)

debug: OPTIMISE := -O0 -g3
debug: $(BIN)

$(BIN): $(OBJECTS) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS)

$(BUILD)/obj/%.o: $(SRC_DIR)/%.c | $(BUILD)/obj
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD) $(BUILD)/obj $(BUILD)/test:
	mkdir -p $@

# ---- tests -------------------------------------------------------------------
# Test binaries are always built with debug flags so a failure is debuggable.
unit-binaries: OPTIMISE := -O0 -g3
unit-binaries: $(UNIT_BINARIES)

$(BUILD)/test/%: test/unit/%.c $(MODULE_OBJECTS) | $(BUILD)/test
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(MODULE_OBJECTS)

test: all
	test/run_tests.sh

# ---- static analysis ---------------------------------------------------------
# -fanalyzer needs code generation to run, so each file is compiled to a
# throwaway object under build/analyze/ with the same standard and warnings.
check: | $(BUILD)
	mkdir -p $(BUILD)/analyze
	for source in $(SOURCES); do \
	    echo "analyzing $$source"; \
	    $(CC) $(STD) $(WARNINGS) -O2 -fanalyzer -c -o $(BUILD)/analyze/$$(basename $$source .c).o $$source || exit 1; \
	done

# ---- run ---------------------------------------------------------------------
run: $(BIN)
	./$(BIN)

# ---- submission bundle -------------------------------------------------------
# Gradescope wants flat files, so the bundle gets its own Makefile that builds
# in place with the course's `gcc -std=c99 -Wall`. The template is kept here
# (exported through the environment) so the flags live in one file.
define DIST_MAKEFILE
# Flat build of the Clue text adventure: `make` produces ./adventure.
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2
OBJECTS = adventure.o rooms.o items.o characters.o

all: adventure

adventure: $$(OBJECTS)
	$$(CC) $$(CFLAGS) -o $$@ $$(OBJECTS)

adventure.o: adventure.c rooms.h items.h characters.h
rooms.o: rooms.c rooms.h items.h
items.o: items.c items.h
characters.o: characters.c characters.h rooms.h items.h

%.o: %.c
	$$(CC) $$(CFLAGS) -c $$<

clean:
	rm -f $$(OBJECTS) adventure

.PHONY: all clean
endef
export DIST_MAKEFILE

dist: $(DIST_FILES)
	rm -rf dist
	mkdir -p dist
	cp $(DIST_FILES) dist/
	printf '%s\n' "$$DIST_MAKEFILE" > dist/Makefile
	$(MAKE) -C dist
	@echo "dist/ is ready: $$(ls dist | tr '\n' ' ')"

# ---- clean -------------------------------------------------------------------
clean:
	rm -rf $(BUILD) dist

-include $(OBJECTS:.o=.d) $(UNIT_BINARIES:=.d)
