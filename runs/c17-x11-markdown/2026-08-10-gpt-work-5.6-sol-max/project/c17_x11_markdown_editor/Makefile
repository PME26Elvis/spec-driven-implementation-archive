CC ?= cc
PKG_CONFIG ?= pkg-config

CPPFLAGS := -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Iinclude
CFLAGS_BASE := -std=c17 -O2 -g -Wall -Wextra -Wpedantic -Werror \
	-Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wformat=2 \
	-fstack-protector-strong -fno-common
CFLAGS ?= $(CFLAGS_BASE)
X11_CFLAGS := $(shell $(PKG_CONFIG) --cflags x11 xft)
X11_LIBS := $(shell $(PKG_CONFIG) --libs x11 xft)
IMAGE_CFLAGS := $(shell $(PKG_CONFIG) --cflags libpng libjpeg)
IMAGE_LIBS := $(shell $(PKG_CONFIG) --libs libpng libjpeg)
LDLIBS_COMMON := -lm -lpthread

BUILD := build
BIN := bin
CORE_SRC := src/core.c src/json.c src/document.c src/diff.c src/storage.c src/image.c
CORE_OBJ := $(CORE_SRC:src/%.c=$(BUILD)/%.o)
APP_OBJ := $(BUILD)/ui.o $(BUILD)/main.o
HEADERS := $(wildcard include/mdedit/*.h)

.PHONY: all clean test unit integration regression e2e fixtures evidence release sanitize

all: $(BIN)/mdeditor $(BIN)/locscan $(BIN)/fixturegen $(BIN)/evidencecheck $(BIN)/evidencegen \
	$(BIN)/test_core $(BIN)/test_storage $(BIN)/test_tools $(BIN)/e2e_x11 \
	$(BIN)/performance $(BIN)/failure_matrix $(BIN)/acceptance_matrix

$(BUILD) $(BIN):
	mkdir -p $@

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(X11_CFLAGS) $(IMAGE_CFLAGS) -c $< -o $@

$(BIN)/mdeditor: $(CORE_OBJ) $(APP_OBJ) | $(BIN)
	$(CC) $(CFLAGS) $^ $(X11_LIBS) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/locscan: tools/locscan.c $(BUILD)/core.o $(BUILD)/json.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.o,$^) $(LDLIBS_COMMON) -o $@

$(BIN)/fixturegen: tools/fixturegen.c $(BUILD)/core.o $(BUILD)/json.o $(BUILD)/image.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/evidencecheck: tools/evidencecheck.c $(BUILD)/core.o $(BUILD)/json.o $(BUILD)/image.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/evidencegen: tools/evidencegen.c $(BUILD)/core.o $(BUILD)/json.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.o,$^) $(LDLIBS_COMMON) -o $@

$(BIN)/test_core: tests/test_core.c $(CORE_OBJ) $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/test_storage: tests/test_storage.c $(CORE_OBJ) $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/test_tools: tests/test_tools.c $(BUILD)/core.o $(BUILD)/json.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(filter %.c %.o,$^) $(LDLIBS_COMMON) -o $@

$(BIN)/e2e_x11: tests/e2e_x11.c $(BUILD)/core.o $(BUILD)/image.o $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(X11_CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) \
		$(X11_LIBS) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/performance: tests/performance.c $(CORE_OBJ) $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/failure_matrix: tests/failure_matrix.c $(CORE_OBJ) $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

$(BIN)/acceptance_matrix: tests/acceptance_matrix.c $(CORE_OBJ) $(HEADERS) | $(BIN)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(IMAGE_CFLAGS) $(filter %.c %.o,$^) $(IMAGE_LIBS) $(LDLIBS_COMMON) -o $@

unit: $(BIN)/test_core $(BIN)/test_tools
	$(BIN)/test_core
	$(BIN)/test_tools

integration: $(BIN)/test_storage $(BIN)/locscan $(BIN)/fixturegen $(BIN)/evidencecheck
	$(BIN)/test_storage
	./scripts/test_utilities.sh

regression: $(BIN)/acceptance_matrix
	$(BIN)/acceptance_matrix

fixtures: $(BIN)/fixturegen
	./scripts/generate_fixtures.sh

e2e: $(BIN)/mdeditor $(BIN)/e2e_x11 fixtures
	./scripts/run_e2e.sh

test: all unit integration regression e2e

evidence: all
	./scripts/build_evidence.sh

release: clean all evidence
	$(BIN)/evidencecheck --root . --manifest evidence/manifest.json

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='$(CFLAGS_BASE) -O1 -fsanitize=address,undefined -fno-omit-frame-pointer' all
	ASAN_OPTIONS=detect_leaks=0 $(BIN)/test_core
	ASAN_OPTIONS=detect_leaks=0 $(BIN)/test_storage
	ASAN_OPTIONS=detect_leaks=0 $(BIN)/acceptance_matrix

clean:
	rm -rf $(BUILD) $(BIN)
