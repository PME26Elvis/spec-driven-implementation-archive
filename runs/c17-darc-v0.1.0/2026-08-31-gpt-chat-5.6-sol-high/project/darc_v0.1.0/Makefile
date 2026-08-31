CC ?= cc
CFLAGS ?= -std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -D_FILE_OFFSET_BITS=64
CPPFLAGS ?= -Iinclude
LDFLAGS ?=
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
TESTSRC := $(wildcard tests/*.c)
LIBOBJ := $(filter-out src/main.o,$(OBJ))
STRESS_TMP ?= /tmp
STRESS_IDS := RST-023 SCN-029 STR-001 STR-002 STR-003 STR-004 STR-005 STR-006 STR-007 STR-008
PREFIX ?= /usr/local

.PHONY: all clean test test-stress test-all test-sanitize test-e2e install uninstall
all: darc

darc: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJ)

tests/test_runner: $(TESTSRC) $(LIBOBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -DDARC_TESTING -o $@ $(TESTSRC) $(LIBOBJ)

test: darc tests/test_runner
	./tests/test_runner --quick

test-stress: darc tests/test_runner
	@set -e; total=0; \
	for id in $(STRESS_IDS); do \
		echo "=== stress $$id ==="; \
		DARC_TEST_ROOT_BASE="$(STRESS_TMP)" ./tests/test_runner --case "$$id"; \
		total=$$((total + 1)); \
	done; \
	echo "STRESS_RESULT total_passed=$$total total_failed=0 catalog_total=10"

test-all: darc tests/test_runner
	./tests/test_runner --all

test-e2e: darc
	./tests/release_e2e.sh

test-sanitize:
	$(MAKE) clean
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
	$(MAKE) CFLAGS='-std=c17 -O1 -g -Wall -Wextra -Wpedantic -fsanitize=address,undefined -fno-omit-frame-pointer -D_FILE_OFFSET_BITS=64' test

install: darc
	install -d "$(DESTDIR)$(PREFIX)/bin"
	install -m 0755 darc "$(DESTDIR)$(PREFIX)/bin/darc"

uninstall:
	rm -f "$(DESTDIR)$(PREFIX)/bin/darc"

clean:
	rm -f src/*.o darc tests/test_runner
