# Acceptance Traceability

Automated via `tests/catalog_runner.sh` (22 cases) + `tests/test_algorithms.c` + `tests/e2e.sh` + `tests/stress.sh`.

| Area | Status | Evidence |
|------|--------|----------|
| SHA-256 / CRC32C / Buzhash / LZH1 goldens | PASS | test_algorithms.c |
| Config JSON/YAML validate + format reject | PASS | catalog CFG-* |
| Repo init/FORMAT | PASS | catalog REPO-* |
| Snapshot create/list/show/delete/parent | PASS | catalog + e2e |
| Hardlink scan + restore topology | PASS | REST-HARDLINK |
| Diff JSON | PASS | DIFF-JSON |
| Verify scrub/full | PASS | VER-SCRUB |
| Corruption detect exit 6 | PASS | CORRUPT-DETECT |
| Parity protect + single-member recover E2E | PASS | PARITY-REPAIR |
| Journal recovery | PASS | JOURNAL-RECOVER |
| GC dry-run | PASS | GC-DRY |
| Index rebuild | PASS | IDX-REBUILD |
| Stats text/json/ndjson/svg | PASS | STATS-* |
| Randomized stress (seed 42) | PASS | stress.sh |

Full line-by-line TEST_CATALOG (317) is partially mapped; all mandatory functional gates exercised above.
