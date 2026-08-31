# Testing and Acceptance

The acceptance suite is implementation-owned C17/POSIX code. It maps the task pack's 252 mandatory catalog IDs in `acceptance/TRACEABILITY.md`.

## Commands

```sh
make test
```

Builds `darc` and `tests/test_runner`, then runs 242 non-long mandatory cases. Output includes total passed/failed/skipped and the fixed randomized seed.

```sh
make test-stress
```

Runs the 10 long cases as separate fresh test-runner processes:

- `RST-023`: 512 MiB restore streaming;
- `SCN-029`: 512 MiB scan streaming;
- `STR-001`: 20,000 small files;
- `STR-002`: 512 MiB mixed dedup/compression fixture;
- `STR-003`: single 512 MiB streamed file snapshot+restore;
- `STR-004`: 10 overlapping snapshots;
- `STR-005`: 100,000 explicit chunk references through verify/GC;
- `STR-006`: 50 fixed-seed randomized lifecycle cases;
- `STR-007`: repair/scrub zero-mutation idempotence;
- `STR-008`: repeated-GC idempotence with retained restore.

`STRESS_TMP=/path` controls only the temporary test root, not fixture sizes/assertions. This is useful on containers where overlay-backed `/tmp` writeback is throttled.

```sh
make test-e2e
```

Runs the final multi-snapshot release scenario from `tests/release_e2e.sh`: snapshot, diff, restore, direct raw CHUNK corruption, scrub detection, parity repair, old-ref deletion, GC, retained restore, and final scrub.

```sh
make test-sanitize
```

When the compiler supports it, builds with AddressSanitizer + UndefinedBehaviorSanitizer and enables LeakSanitizer for the 242-case quick suite. On overlay-heavy containers the same fixture semantics can be run with `DARC_TEST_ROOT_BASE=/dev/shm make test-sanitize`; only the temporary root changes.

## Test layers

The suite includes algorithm/parser unit tests, fixed independent golden vectors, canonical format tests, process-level CLI tests, independent filesystem/diff reference checks, end-to-end restore comparisons, raw-byte corruption injection, deterministic crash/fault injection, cross-repository determinism, randomized fixed-seed lifecycles, and large streaming/stress fixtures.

Tests create isolated temporary repositories. Mandatory tests are never marked XFAIL. In the release evidence there are zero mandatory skips.

## Golden and corruption rules

Expected cryptographic/format outputs come from `acceptance/GOLDEN_VECTORS.md`, not from calling the same production function to generate expected values. Corruption tests modify raw repository bytes directly and then invoke normal `darc` process paths.
