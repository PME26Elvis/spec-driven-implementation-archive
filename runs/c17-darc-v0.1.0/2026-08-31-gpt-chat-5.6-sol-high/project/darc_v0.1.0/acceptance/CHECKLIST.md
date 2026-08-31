# Human Acceptance Checklist

Use this after automated tests pass. It is intentionally simple and practical.

## Build and structure

- [ ] Clean source build produces `darc`.
- [ ] No prohibited third-party production dependency is required.
- [ ] Source visibly contains implementations for CDC, SHA-256, CRC-32C, LZ77, Huffman, JSON, YAML, Robin Hood index, Merkle, parity.
- [ ] `darc --help` and subcommand help are usable.

## Basic repository workflow

- [ ] `darc init` creates a valid repository.
- [ ] First snapshot of a nested directory succeeds.
- [ ] Second snapshot after small insertion stores mostly reused chunks.
- [ ] Snapshot list/show expose meaningful statistics.
- [ ] Snapshot delete removes only the ref, dry-run is immutable, and GC performs later reclamation.
- [ ] Duplicate independent files deduplicate storage but restore independently.
- [ ] Hardlinked files restore as hardlinks.
- [ ] Symlinks restore as symlinks.

## Diff and reports

- [ ] Diff visibly distinguishes A/D/M/P/T/H.
- [ ] Diff text is aligned/readable without color.
- [ ] JSON output parses as JSON with no human noise.
- [ ] NDJSON is line-parseable.
- [ ] Diff SVG opens as a standalone SVG and is readable.
- [ ] Stats SVG is meaningful and self-contained.

## Restore safety

- [ ] Full restore matches original bytes and required metadata.
- [ ] Partial restore works.
- [ ] `overwrite=never/files/all` differ as specified.
- [ ] Crafted traversal/symlink escape is rejected.

## Integrity and recovery

- [ ] Healthy repo passes full/scrub verification.
- [ ] Flipped chunk byte is detected.
- [ ] One missing protected chunk is recoverable.
- [ ] Two missing chunks in one stripe are reported unrecoverable.
- [ ] Corrupt parity is regenerated only with explicit repair.
- [ ] Corrupt index rebuilds without losing snapshots.

## Crash and GC

- [ ] Interrupted snapshot never exposes a partial snapshot.
- [ ] Published snapshot survives interruption before index update.
- [ ] GC dry-run performs no mutation.
- [ ] GC removes unreachable data only.
- [ ] Retained historical snapshots restore after GC.
- [ ] Interrupted GC does not destroy live data.

## Config

- [ ] Example JSON validates.
- [ ] Example YAML validates.
- [ ] Both normalize to the same config hash.
- [ ] Unknown keys and malformed syntax fail clearly.

## Completion

- [ ] Acceptance traceability maps every mandatory TEST_CATALOG ID.
- [ ] Mandatory automated tests pass.
- [ ] Stress suite passes.
- [ ] No mandatory feature is placeholder, mocked, or disconnected.
- [ ] Final scrub verify passes.
