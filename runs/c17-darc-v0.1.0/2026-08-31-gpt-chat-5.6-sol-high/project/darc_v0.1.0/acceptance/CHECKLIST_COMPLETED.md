# Completed Human Acceptance Checklist — Release Verification

This is the executed copy of the task-pack checklist. Every item below was checked only after the final-source automated catalog, stress runs, release E2E, scrub, source/dependency audit, CLI/help smoke, and SVG render inspection. The unchanged task-pack checklist remains `acceptance/CHECKLIST.md`.

Evidence anchors: `evidence/build-and-test-final.log`, `evidence/stress-final/`, `evidence/test-sanitize-final-source.log`, `evidence/release-e2e-final-source.log`, `evidence/final-scrub.log`, `evidence/source-audit.log`, `evidence/runtime-deps.log`, and `evidence/svg-human-check.log`.

## Build and structure

- [x] Clean source build produces `darc`.
- [x] No prohibited third-party production dependency is required.
- [x] Source visibly contains implementations for CDC, SHA-256, CRC-32C, LZ77, Huffman, JSON, YAML, Robin Hood index, Merkle, parity.
- [x] `darc --help` and subcommand help are usable.

## Basic repository workflow

- [x] `darc init` creates a valid repository.
- [x] First snapshot of a nested directory succeeds.
- [x] Second snapshot after small insertion stores mostly reused chunks.
- [x] Snapshot list/show expose meaningful statistics.
- [x] Snapshot delete removes only the ref, dry-run is immutable, and GC performs later reclamation.
- [x] Duplicate independent files deduplicate storage but restore independently.
- [x] Hardlinked files restore as hardlinks.
- [x] Symlinks restore as symlinks.

## Diff and reports

- [x] Diff visibly distinguishes A/D/M/P/T/H.
- [x] Diff text is aligned/readable without color.
- [x] JSON output parses as JSON with no human noise.
- [x] NDJSON is line-parseable.
- [x] Diff SVG opens as a standalone SVG and is readable.
- [x] Stats SVG is meaningful and self-contained.

## Restore safety

- [x] Full restore matches original bytes and required metadata.
- [x] Partial restore works.
- [x] `overwrite=never/files/all` differ as specified.
- [x] Crafted traversal/symlink escape is rejected.

## Integrity and recovery

- [x] Healthy repo passes full/scrub verification.
- [x] Flipped chunk byte is detected.
- [x] One missing protected chunk is recoverable.
- [x] Two missing chunks in one stripe are reported unrecoverable.
- [x] Corrupt parity is regenerated only with explicit repair.
- [x] Corrupt index rebuilds without losing snapshots.

## Crash and GC

- [x] Interrupted snapshot never exposes a partial snapshot.
- [x] Published snapshot survives interruption before index update.
- [x] GC dry-run performs no mutation.
- [x] GC removes unreachable data only.
- [x] Retained historical snapshots restore after GC.
- [x] Interrupted GC does not destroy live data.

## Config

- [x] Example JSON validates.
- [x] Example YAML validates.
- [x] Both normalize to the same config hash.
- [x] Unknown keys and malformed syntax fail clearly.

## Completion

- [x] Acceptance traceability maps every mandatory TEST_CATALOG ID.
- [x] Mandatory automated tests pass.
- [x] Stress suite passes.
- [x] No mandatory feature is placeholder, mocked, or disconnected.
- [x] Final scrub verify passes.
