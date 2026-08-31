CC ?= cc
CFLAGS ?= -std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -D_POSIX_C_SOURCE=200809L
CPPFLAGS += -Iinclude
LDFLAGS ?=
SRC := $(wildcard src/*.c)
OBJ := $(SRC:.c=.o)
CORE_OBJ := $(filter-out src/main.o,$(OBJ))
.PHONY: all clean unit supplemental test sanitize
all: cvc
cvc: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) $(LDFLAGS) -o $@

tests/unit_core: $(CORE_OBJ) tests/unit_core.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/unit_core.c $(CORE_OBJ) $(LDFLAGS) -o $@
unit: tests/unit_core

tests/mergebase_unit: $(CORE_OBJ) tests/mergebase_unit.c
	$(CC) $(CFLAGS) $(CPPFLAGS) tests/mergebase_unit.c $(CORE_OBJ) $(LDFLAGS) -o $@

tests/libfaultio.so: tests/faultio.c
	$(CC) -std=c17 -O2 -g -Wall -Wextra -Wpedantic -fPIC -shared tests/faultio.c -ldl -o $@

clean:
	rm -f src/*.o cvc tests/unit_core tests/mergebase_unit tests/libfaultio.so

supplemental: all tests/libfaultio.so
	python3 tests/supplemental.py ./cvc

test: all unit tests/mergebase_unit tests/libfaultio.so
	./tests/unit_core
	./tests/mergebase_unit
	python3 tests/acceptance.py ./cvc
	python3 tests/supplemental.py ./cvc

sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c17 -O1 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -D_POSIX_C_SOURCE=200809L -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined' all unit tests/mergebase_unit
	ASAN_OPTIONS=detect_leaks=1 ./tests/unit_core
	ASAN_OPTIONS=detect_leaks=1 ./tests/mergebase_unit
