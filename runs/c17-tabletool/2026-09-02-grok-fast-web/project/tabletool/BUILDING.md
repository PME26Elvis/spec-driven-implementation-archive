# Building TableTool

ISO C17, no third-party libraries, standard C library only.

## Compile product

```bash
cc -std=c17 -Wall -Wextra -O2 -o tabletool \
  src/main.c src/script.c src/utf8.c src/mem.c src/table.c \
  src/ean.c src/code128.c src/util.c \
  src/parse_csv.c src/parse_tsv.c src/parse_md.c src/parse_md_load.c \
  src/write_csv.c src/url.c src/svg.c -I src
```

## Tests

```bash
cc -std=c17 -O2 -o run_tests tests/test_main.c src/utf8.c src/ean.c src/mem.c -I src
./run_tests
```

## Acceptance

From the task-pack root (or with adjusted paths):

```bash
./tabletool --script acceptance/scripts/case_a_people.tts --report /tmp/r.txt
# compare people_actual.md with acceptance/expected/people_expected.md
```

All cases A–H are expected to pass.
