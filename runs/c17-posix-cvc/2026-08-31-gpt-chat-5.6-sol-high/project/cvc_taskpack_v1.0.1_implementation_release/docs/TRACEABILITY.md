# Acceptance and Release-Gate Traceability

The primary black-box harness is `tests/acceptance.py`. It invokes the real production `cvc` executable in temporary repositories and contains one mapped result for every mandatory identifier in `acceptance/01_TEST_MATRIX.md`; no mandatory identifier is skipped or expected-fail. The release run reported **202/202 PASS, 0 FAIL, 0 MISSING, 0 SKIPPED**.

## Mandatory acceptance categories

| Category | Mandatory IDs | Coverage location | Release result |
|---|---:|---|---|
| A — Build and Smoke | 9 | `tests/acceptance.py` A01–A09 | PASS |
| B — Initialization | 7 | B01–B07 | PASS |
| C — JSON Parser | 20 | C01–C20 + `tests/unit_core.c` JSON vectors | PASS |
| D — SHA-256 and Object Store | 14 | D01–D14 + canonical vectors in `tests/unit_core.c` | PASS |
| E — Eligibility and Scanning | 15 | E01–E15 | PASS |
| F — Symlinks | 8 | F01–F08 + `tests/supplemental.py` hostile structural-symlink restore | PASS |
| G — Save/Status | 11 | G01–G11 | PASS |
| H — Glob/Filtering | 14 | H01–H14 + glob vectors in `tests/unit_core.c` | PASS |
| I — Diff/Myers | 13 | I01–I13 + Myers vectors in `tests/unit_core.c` | PASS |
| J — Branch and Switch | 15 | J01–J14 and J11b | PASS |
| K — Revision Resolution and Restore | 13 | K01–K13 | PASS |
| L — Merge | 29 | L01–L28 and L07b + `tests/mergebase_unit.c` | PASS |
| M — Rollback | 9 | M01–M09 | PASS |
| N — Verification and Corruption | 15 | N01–N15 | PASS |
| O — Locking and Failure Safety | 10 | O01–O10 + test-only `tests/libfaultio.so` injection | PASS |

`tests/supplemental.py` adds two regressions beyond the fixed matrix: a locally type-changed structural directory represented as a hostile symlink is restored without dereferencing the outside target, and a post-install-rename parent-directory `fsync` failure rolls the just-installed path back while leaving HEAD unchanged.

## Core unit mapping

`tests/unit_core.c` runs 50 direct C tests against production modules:

- SHA-256 known vectors, multi-block and incremental update boundaries;
- JSON object/array/string/number handling, UTF-8, escapes, surrogate pairs, decoded duplicate keys, BOM/comments/trailing data rejection, length-aware NUL;
- handwritten glob `?`, `*`, `**`, zero-directory `**/`, invalid triple-star, UTF-8 literals;
- Myers shortest-edit behavior including repeats, CRLF, unterminated final line, UTF-8 bytes;
- three-way text merge for disjoint, identical, overlapping, and insertion-boundary cases;
- all fixed canonical object-ID vectors.

`tests/mergebase_unit.c` constructs a real commit DAG in a temporary CVC repository and verifies deterministic selection when more than one best common ancestor exists.

## Release Gates

| Gate | Evidence | Status |
|---|---|---|
| R1 — Build | Clean normal build/test; warning-strict `-Werror` build | PASS |
| R2 — Core Snapshot Integrity | A/B/D/E/G/I/K categories; canonical vectors; `verify`; serialization notes | PASS |
| R3 — Parser and Filter Correctness | C/H plus JSON/glob unit tests; source/prohibited audit confirms no delegated parser/glob APIs | PASS |
| R4 — History and Branches | J category plus real shared object/commit graph exercised by log/revision tests | PASS |
| R5 — Merge | L category, merge-base graph unit, three-way unit tests, persistent conflict/finalization black-box cases | PASS |
| R6 — Rollback | M01–M09 verify new single-parent rollback commit with exact target tree and preserved history | PASS |
| R7 — Safety and Verification | F/N/O plus supplemental symlink/fsync rollback tests; `fcntl`, atomic metadata/object writes, corruption checks | PASS |
| R8 — Test Completeness | 202/202 mapped mandatory IDs, 0 missing, 0 skipped; real production binary | PASS |
| R9 — Prohibited Implementations | Production-source API/dependency audit and normal-binary dependency/symbol inspection; see `TEST_EVIDENCE.md` | PASS |

The Gate statuses above are tied to the clean final evidence recorded in `docs/TEST_EVIDENCE.md`; they are not inferred solely from source inspection.
