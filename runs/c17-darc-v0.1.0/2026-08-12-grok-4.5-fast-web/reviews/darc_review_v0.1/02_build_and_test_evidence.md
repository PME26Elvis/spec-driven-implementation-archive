# Build and Test Evidence

## Input

Reviewed archive: `darc_gates_closed_v0.1.zip`.

The exact input SHA-256 is recorded in `evidence/input_sha256.txt`.

## Build

Commands executed from a clean source tree:

```text
make clean
make -j2
```

Result: executable built successfully, but GCC emitted **17 warnings**. Important warnings include misleading indentation in `src/config.c` and multiple path truncation warnings around fixed `PATH_MAX` buffers.

Raw output: `evidence/build.log`.

## Supplied automated tests

### `make test`

Result: PASS.

What it actually runs:

- algorithm smoke/golden test binary;
- `tests/e2e.sh`.

It does **not** run the supplied catalog runner or stress script.

Raw output: `evidence/make_test.log`.

### `tests/catalog_runner.sh ./bin/darc`

Result:

```text
PASSED=22 FAILED=0
```

Raw output: `evidence/catalog_runner.log`.

### `tests/stress.sh ./bin/darc`

Result: PASS.

This script generates only 30 random files, each 0–8,000 bytes, then creates two snapshots. This is far below the mandatory stress scale in the task pack.

Raw output: `evidence/stress.log`.

## Independent sanitizer test

I rebuilt the program with:

```text
-fsanitize=address,undefined -fno-omit-frame-pointer
```

Then supplied a valid configuration with a chunk maximum larger than the built-in 256 KiB buffer and a file large enough to reach that configured boundary.

Result: deterministic **heap-buffer-overflow** at `src/snapshot.c:67`.

ASan identifies the write immediately after the 262,144-byte heap allocation created at `src/snapshot.c:57`.

Raw report: `evidence/asan_heap_overflow.log`.

## Independent black-box tests

Script: `evidence/custom_blackbox.py`.

Observed failures included:

- independent duplicate files restored to the same inode;
- `--path` partial restore ignored;
- overwrite `never` silently succeeds rather than reporting destination conflict;
- omitted `--parent` does not default to HEAD;
- `--timestamp 0` is treated as “use current clock,” preventing the specified fixed-zero timestamp behavior;
- unknown configuration keys accepted;
- duplicate JSON keys accepted;
- configured exclude rule ignored;
- FIFO silently skipped under default behavior.

Raw results: `evidence/custom_blackbox_results.txt`.

## Independent integrity/GC tests

Script: `evidence/custom_integrity.sh`, plus a focused live-FILE-object deletion test.

Key results:

- after deleting one historical snapshot while another ref remains, `gc` reports `Actually reclaimed 0 objects` even when old-only data exists;
- deleting a live FILE object from a one-snapshot repository still allows `verify --level full` to return **exit 0**;
- restore of that same snapshot then fails, proving the missing FILE object was semantically required;
- deleting the current HEAD ref leaves `HEAD` pointing to the deleted snapshot instead of moving to the deterministic remaining snapshot.

Raw output: `evidence/custom_integrity_results.txt` plus the source-review evidence described in `04_findings.md`.

## Scope note

This review did not attempt to write the hundreds of missing acceptance tests on behalf of the submitter. The observed failures are sufficient to reject Release Gate closure; additional exhaustive testing would likely identify more defects.
