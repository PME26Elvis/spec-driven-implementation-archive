CC ?= cc
CFLAGS ?= -O2 -std=c17 -Wall -Wextra -Wpedantic -Werror
CPPFLAGS ?= -Iinclude
LDLIBS ?= -lm
SRC = src/util.c src/parse.c src/config.c src/trace.c src/sim.c src/output.c
OBJ = $(SRC:.c=.o)
all: elevsim
elevsim: $(OBJ) src/main.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
tests/test_elevsim: tests/test_main.o $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
test: elevsim tests/test_elevsim
	./tests/test_elevsim
clean:
	rm -f $(OBJ) src/main.o tests/test_main.o elevsim tests/test_elevsim
.PHONY: all test clean
