# edb — C17 / X11 Embedded Database Workbench

Self-contained embedded relational database engine in **ISO C17**, with CLI and X11 workbench.

> **Honesty notice:** This tree is a working engine with substantial features and tests, but it does **not** fully meet every mandatory clause of the v1.0.0 product specification. See `docs/STATUS.md`.

## Build

```bash
make          # all binaries under build/bin/
make test     # partial automated suite
make clean
```

Requires: `gcc`, `make`, X11 development headers (`libx11-dev`) for `edb-gui`.

If the build directory is on a noexec mount, copy binaries to `/tmp` before running.

## Binaries

| Binary | Role |
|--------|------|
| `edb` | CLI |
| `edb-gui` | X11 workbench |
| `edb-check` | Integrity checker |
| `edb-fixture` | Fixture generator |
| `edb-crashrun` | Crash harness |
| `locscan` | Line counter |

## Quick start

```bash
./edb -c demo.edb -e "CREATE TABLE t (id INTEGER PRIMARY KEY, score INTEGER);"
./edb demo.edb -e "INSERT INTO t VALUES (1, 10);"
./edb demo.edb -e "SELECT SUM(score) FROM t;"
./edb-check demo.edb
./edb-fixture --scenario tasks_small --seed 1 --out tasks.edb
```

Encrypted: `edb -c -p secret enc.edb -e "..."`

## Documentation

- `docs/format.md` — file format draft
- `docs/STATUS.md` — completion status vs DoD

## License / provenance

Project-authored C17 sources; no third-party database or GUI toolkit dependencies beyond system libc and X11.
