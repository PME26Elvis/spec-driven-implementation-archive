# CVC — C17 Local Version-Control System

CVC is a local, single-repository command-line version-control system implemented in C17 for the CVC v1 task specification. It stores immutable, uncompressed content-addressed loose objects under `.cvc/objects`, uses handwritten SHA-256, JSON, glob matching, Myers diff, merge logic, UTF-8 handling, and uses only ISO C plus the permitted low-level POSIX filesystem/locking surface at runtime.

## Requirements

- a C17 compiler (`cc`, GCC, or Clang)
- POSIX filesystem APIs including `lstat`, symlinks, `fcntl` record locks, `fsync`, `rename`, and `mkstemp`
- GNU/POSIX `make`
- Python 3 only for the automated black-box test harness (not used by the production executable)
- `libdl` only for the test-only injected-I/O-failure shim

The production executable has no third-party runtime dependency and does not invoke external programs.

## Build

From a clean source extraction:

```sh
make clean
make
```

The executable is `./cvc`.

A warning-strict build used for release validation is:

```sh
make clean
make CFLAGS='-std=c17 -O2 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wstrict-prototypes -Werror -D_POSIX_C_SOURCE=200809L'
```

## Complete test command

```sh
make clean
make test
```

`make test` builds the production executable and test binaries and then runs:

1. the C core unit suite;
2. the dedicated multiple-best-ancestor merge-base test;
3. the complete black-box acceptance matrix against the real `./cvc` executable;
4. supplemental filesystem/failure-safety regression tests.

Optional sanitizer validation:

```sh
make sanitize
```

See [`docs/TEST_EVIDENCE.md`](docs/TEST_EVIDENCE.md) for the clean release run and [`docs/TRACEABILITY.md`](docs/TRACEABILITY.md) for Release Gate mapping.

## Quick start

```sh
mkdir demo && cd demo
/path/to/cvc init
printf 'hello\n' > hello.txt
/path/to/cvc status
/path/to/cvc save -m 'initial snapshot'
/path/to/cvc log
```

CVC discovers a repository by walking upward from the current directory. After discovery, commands operate on the full working tree rooted at the directory containing `.cvc`, even when invoked from a nested subdirectory.

## Commands

```text
cvc init
cvc status [--include=PATTERNS] [--exclude=PATTERNS]
cvc save -m MESSAGE [--include=PATTERNS] [--exclude=PATTERNS] [--no-diffstat]
cvc log [--max-count=N]
cvc diff [REVISION] [--include=PATTERNS] [--exclude=PATTERNS]
cvc branch
cvc branch create NAME
cvc branch delete NAME
cvc switch BRANCH
cvc restore PATH --from REVISION
cvc rollback REVISION -m MESSAGE
cvc merge BRANCH [-m MESSAGE]
cvc merge --continue [-m MESSAGE]
cvc merge --abort
cvc resolve PATH
cvc verify
cvc config show
cvc config validate
cvc help
```

### Snapshot behavior

There is no staging area. `save` scans the current working tree according to repository tracking configuration and records a complete snapshot of eligible regular files and symlinks. Directories are structural only.

A regular file is eligible only when it is at most 8,388,608 bytes and contains no NUL byte in the first `min(size, 8192)` bytes. Other regular files and unsupported special files are not versioned. Symlinks are saved as link-target bytes and are never dereferenced by repository traversal or restoration.

Command-line `--include` and `--exclude` on `save` filter diffstat presentation only; repository `tracking.include` / `tracking.exclude` determine snapshot membership.

### Branches and revisions

Branches are real refs over a shared commit/object graph. `switch` refuses an actual branch change when selected working-tree content differs from current `HEAD`, and it refuses collisions with untracked/ineligible paths that would be overwritten.

Revision operands resolve in the required precedence: existing branch name, full 64-hex commit ID, then unambiguous 8–63 hex commit-ID prefix.

### Merge

CVC supports:

- self/already-up-to-date handling;
- fast-forward merge without a redundant commit;
- divergent merge using a deterministic merge base;
- recursive path/tree merge;
- three-way text merge for eligible regular files;
- persistent conflicts and visible conflict markers;
- exact-root `resolve` records;
- re-resolution checks before `merge --continue`;
- retryable finalization state;
- `merge --abort` restoration of the pre-merge tracked tree.

A successful divergent merge commit has two parents. Merge metadata lives in `.cvc/state/` and is not part of committed history.

### Rollback

`rollback` is history preserving: it creates a new commit whose tree equals the requested historical revision and whose single parent is the pre-rollback `HEAD`. It never moves the branch pointer backward as a substitute for a commit.

### Integrity and locking

All repository commands validate `config.json`. Read-only commands take a nonblocking shared whole-file POSIX `fcntl` record lock on `.cvc/lock`; writers take the corresponding exclusive lock.

Loose objects are installed immutably and content-addressed. Ref/HEAD metadata updates use durable temporary-file + rename updates. Multi-path working-tree operations use transactional backup-renames and rollback detected runtime failures before returning failure.

`cvc verify` validates branch/HEAD structure, reachable commit/tree references, active merge-state references, canonical object encoding, object hashes, and every canonical loose-object pathname, including unreachable objects.

## Configuration

`.cvc/config.json` is parsed by the handwritten JSON parser. The initial file is:

```json
{"format_version":1}
```

Optional schema:

```json
{
  "format_version": 1,
  "save": { "show_diffstat": true },
  "tracking": {
    "include": ["**"],
    "exclude": []
  },
  "diffstat": {
    "include": ["**"],
    "exclude": []
  }
}
```

Unknown keys, duplicate decoded keys, comments, trailing commas, BOM, malformed UTF-8, unpaired surrogate escapes, invalid pattern strings, and non-integer/non-1 `format_version` are rejected.

The CVC glob grammar supports literal UTF-8 bytes, `?`, `*`, and `**`; a run of three or more `*` bytes is invalid. Matching is against canonical repository paths using `/`.

## Repository format

The committed v1 format is fixed and deterministic. See [`docs/SERIALIZATION.md`](docs/SERIALIZATION.md) for byte-level details and canonical object vectors.

## Implementation notes

- [`docs/SERIALIZATION.md`](docs/SERIALIZATION.md) — repository/object/state serialization.
- [`docs/CHOICE_POINTS.md`](docs/CHOICE_POINTS.md) — implementation choices where the specification permits freedom.
- [`docs/TEST_EVIDENCE.md`](docs/TEST_EVIDENCE.md) — clean build/test/static/sanitizer evidence.
- [`docs/MANUAL_CHECKLIST.md`](docs/MANUAL_CHECKLIST.md) — manual acceptance checklist review.
- [`docs/TRACEABILITY.md`](docs/TRACEABILITY.md) — mandatory test categories and Release Gates.
- [`docs/TEST_MATERIAL_LICENSES.md`](docs/TEST_MATERIAL_LICENSES.md) — test-only material/license note.

## Known limitations

Only features declared out of scope by the v1 task specification are omitted: network/remotes, detached HEAD, staging, compression/pack files, garbage collection, rename detection, binary diff/merge, encryption, and preservation of empty directories. These do not replace or weaken any mandatory v1 behavior.
