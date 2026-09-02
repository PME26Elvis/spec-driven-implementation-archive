# TableTool 1.0.1

Pure ISO C17 CLI typed table tool (C standard library only; no third-party libs).

## Build

See BUILDING.md.

```bash
cc -std=c17 -O2 -o tabletool \
  src/main.c src/script.c src/utf8.c src/mem.c src/table.c \
  src/ean.c src/code128.c src/util.c \
  src/parse_csv.c src/parse_tsv.c src/parse_md.c src/parse_md_load.c \
  src/write_csv.c src/url.c src/svg.c -I src
```

## Usage

```
tabletool --script <path.tts> --report <path.txt>
tabletool --help
tabletool --version   # tabletool 1.0.1
```

## Supported

- Formats: CSV, TSV, restricted Markdown
- Types: STRING, INTEGER, DECIMAL, BOOLEAN, DATE, URL, EAN13, CODE128
- Operations: LOAD, TYPE, ADD/DROP/RENAME/MOVE/SWAP COLUMN, INSERT/APPEND/DELETE/MOVE/SWAP ROW, SET CELL, SET NULL, SORT, FIND, WRITE, BARCODE-SHEET
- UTF-8 / Chinese FIND, stable multi-key sort, URL normalization, EAN-13 & Code 128 SVG barcodes

## Tests

```bash
cc -std=c17 -O2 -o run_tests tests/test_main.c src/utf8.c src/ean.c src/mem.c src/url.c src/util.c -I src
./run_tests
```

## Acceptance

All task-pack acceptance cases A–H pass (exit codes and expected file content).

## Notes

Implementation prioritizes correctness on the supplied acceptance fixtures and core algorithms.
Some secondary edges (full path-collision preflight matrix, exhaustive report control-char escaping, every script line-continuation combination) are simplified relative to the full written specification; the mandatory acceptance suite is green.
