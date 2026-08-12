CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -O2 -Iinclude -D_POSIX_C_SOURCE=200809L
SRCS = src/sha256.c src/crc32c.c src/buzhash.c src/lzh1.c \
       src/object.c src/repo.c src/index.c src/snapshot.c \
       src/verify.c src/restore.c src/diff.c src/config.c src/main.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean test algtest

all: bin/darc

bin/darc: $(OBJS) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJS)
	chmod +x $@ 2>/dev/null || true

bin:
	mkdir -p bin

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) bin/darc

algtest:
	gcc $(CFLAGS) -o /tmp/test_alg tests/test_algorithms.c \
	  src/sha256.c src/crc32c.c src/buzhash.c src/lzh1.c
	/tmp/test_alg

test: bin/darc algtest
	@cp bin/darc /tmp/darc && chmod +x /tmp/darc
	@bash tests/e2e.sh
