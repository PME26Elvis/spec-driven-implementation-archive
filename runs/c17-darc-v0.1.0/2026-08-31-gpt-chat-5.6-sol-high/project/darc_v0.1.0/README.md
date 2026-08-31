# DARC 0.1.0 — Deterministic Deduplicating Archive

DARC is a headless Linux/POSIX archival engine written in C17. It creates versioned snapshots of directory trees using content-defined chunking, SHA-256 content identifiers, chunk-level deduplication, self-implemented LZ77 + canonical Huffman compression, deterministic Merkle objects, derived Robin Hood indexing, XOR parity recovery, crash-safe publication, verification, restore, diff, snapshot deletion, and garbage collection.

The production executable is `darc`. Production code has no third-party runtime/library dependency beyond the C/POSIX system boundary.

## Build

Requirements: a C17 compiler, POSIX/Linux libc/interfaces, and `make`.

```sh
make clean
make
./darc --version
```

The default build uses strict warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes`.

Optional installation:

```sh
make install PREFIX="$HOME/.local"
# or system-wide, if permitted:
# sudo make install
```

## Quick start

```sh
./darc init ./repo
./darc --repo ./repo snapshot create ./data --name first
./darc --repo ./repo snapshot list
./darc --repo ./repo snapshot show HEAD
./darc --repo ./repo restore HEAD --to ./restored
./darc --repo ./repo verify --level scrub
```

Create another snapshot and compare:

```sh
FIRST=$(cat ./repo/HEAD)
# modify ./data, then create the next snapshot
./darc --repo ./repo snapshot create ./data --name second
SECOND=$(cat ./repo/HEAD)
./darc --repo ./repo snapshot diff "$FIRST" "$SECOND"
./darc --repo ./repo snapshot list
```

Selectors accepted by snapshot-taking commands are `HEAD`, a unique full/abbreviated snapshot CID (minimum 8 hex digits), or an unambiguous snapshot name. Duplicate names and ambiguous CID prefixes are rejected.

## Test and release commands

```sh
make test          # 242 mandatory quick catalog cases
make test-stress   # 10 mandatory large/stress cases
make test-e2e      # final release scenario
make test-sanitize # ASan + UBSan + LeakSanitizer quick suite when supported
```

`make test-all` runs the complete 252-case catalog in one test-runner process. `make test-stress STRESS_TMP=/dev/shm` may be useful on containers whose overlay-backed `/tmp` is heavily throttled; fixture sizes and assertions are unchanged.

The final acceptance mapping is in `acceptance/TRACEABILITY.md`; evidence from the release run is under `evidence/`.

## Documentation

- `docs/CLI.md` — commands, selectors, output and exit codes.
- `docs/CONFIG.md` — JSON/YAML subset, precedence and schema.
- `docs/REPOSITORY_FORMAT.md` — repository layout and object encodings.
- `docs/ALGORITHMS.md` — CDC, hashing, compression, Merkle/index/parity behavior.
- `docs/RECOVERY_GC_SECURITY.md` — crash safety, restore safety, recovery and GC.
- `docs/TESTING.md` — test architecture, stress fixtures and sanitizer target.
- `docs/LIMITATIONS.md` — explicit v0.1 non-goals.
- `docs/RELEASE_GATES.md` — final Definition-of-Done evidence summary.

## Repository source layout

```text
README.md
Makefile
src/                 production C17 sources
include/             public/internal headers
examples/            JSON/YAML configurations
acceptance/          task catalog, golden vectors, final traceability
tests/               C17/POSIX acceptance tests + release E2E script
tests/fixtures/      checked-in parser/config fixtures
docs/                implementation documentation
evidence/            final release verification logs
```

DARC repository data created by the executable is separate from this source tree; see `docs/REPOSITORY_FORMAT.md`.
