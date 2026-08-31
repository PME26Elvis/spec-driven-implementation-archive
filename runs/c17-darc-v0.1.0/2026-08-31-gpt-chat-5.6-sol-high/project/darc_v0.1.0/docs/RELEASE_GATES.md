# Release Gates — DARC 0.1.0

This document records the final release-source gate results. Detailed logs live under `evidence/`; catalog-to-test mapping is `acceptance/TRACEABILITY.md`.

| Gate | Result | Evidence |
|---|---|---|
| Clean C17 build | PASS | `evidence/build-and-test-final.log` (0 compiler warnings) |
| Algorithm/parser implementations | PASS | ALG/CFG rows in `acceptance/TRACEABILITY.md`; quick log |
| Repository-format/determinism | PASS | FMT rows; golden vectors; quick log |
| Snapshot/incremental/delete | PASS | SCN/INC/DEL rows; quick + stress logs |
| Diff/reporting | PASS | DIF rows; quick log |
| Restore/safety | PASS | RST rows; quick + `RST-023` stress log |
| Integrity/parity/index recovery | PASS | VER rows; quick log + release E2E |
| Crash safety/locking | PASS | CRS rows; quick log |
| Garbage collection | PASS | GCI rows; quick log + STR-005/007/008 + release E2E |
| Config JSON/YAML | PASS | CFG rows; quick log |
| Mandatory automated catalog | PASS | 242 quick + 10 stress = 252/252, 0 mandatory skips |
| Randomized/reference model | PASS | STR-006, fixed 50-seed set |
| Stress | PASS | `evidence/stress-final/*.log` |
| ASan/UBSan/LeakSanitizer quick | PASS | `evidence/test-sanitize-final-source.log` |
| Final multi-snapshot E2E | PASS | `evidence/release-e2e-final-source.log` |
| Final scrub | PASS | `evidence/final-scrub.log` |
| Documentation/traceability | PASS | `README.md`, `docs/`, `acceptance/TRACEABILITY.md` |
| Clean archive reproducibility | PASS | `evidence/reproducibility.log` (unzip → clean build → 242 quick → release E2E, 0 warnings) |

## Final catalog totals

- quick mandatory: 242 passed, 0 failed, 0 skipped;
- stress mandatory: 10 passed, 0 failed, 0 skipped;
- overall mandatory: **252 passed, 0 failed, 0 skipped**.

## Final E2E result

The release scenario creates two snapshots, diffs them, restores the old snapshot, flips one raw CHUNK object byte, proves scrub detects it with integrity exit code 6 and the affected CID, repairs it through parity with cryptographic verification, deletes the old published ref, performs GC, scrubs to `HEALTHY`, then restores the retained snapshot with matching bytes, symlink target and hardlink inode topology.

## Known mandatory issues

None at release-gate closure. The explicit v0.1.0 non-goals are documented in `docs/LIMITATIONS.md` and do not represent omitted mandatory task-pack features.
