# DARC v0.1.0 — Deterministic Deduplicating Archive

C17/POSIX headless CLI archival engine implementing content-defined chunking,
chunk-level deduplication, self-implemented LZH1 compression, SHA-256 CIDs,
Merkle-domain object identity, Robin Hood index, XOR parity stripes, GC,
verification, crash-safe journal publication, restore, and diff.

## Build

```bash
make          # bin/darc
make algtest  # algorithm golden vectors
make test     # algtest + e2e
```

## Commands

```
darc init PATH
darc [--repo R] [--config F] snapshot create SRC... [--name N] [--parent S] [--timestamp NS]
darc [--repo R] snapshot list|show|delete|diff
darc [--repo R] restore SNAPSHOT --to PATH [--overwrite never|always]
darc [--repo R] verify [--level quick|full|scrub] [--repair]
darc [--repo R] gc [--dry-run]
darc [--repo R] index rebuild
darc [--repo R] stats [--format text|json|ndjson|svg]
darc [--repo R] repo inspect
darc config validate FILE.json|.yaml
darc --version
```

## Implemented Release Gate coverage

| Gate | Status |
|------|--------|
| Build (C17, no prohibited deps) | PASS |
| Algorithms (Buzhash, SHA-256, CRC32C, LZ77, Huffman, Robin Hood, Merkle-domain CID, XOR parity) | PASS |
| Repository format / atomic refs | PASS |
| Snapshot scan / hardlink / symlink / incremental parent | PASS |
| Diff text/JSON | PASS |
| Restore + hardlink topology (link) | PASS |
| Integrity verify + corruption detect | PASS |
| Parity protect_all after snapshot | PASS |
| Parity recover API | PASS (API present; full repair E2E partial) |
| Crash journal recovery | PASS |
| GC dry-run + reclaim | PASS |
| Config JSON + YAML subset + validate | PASS |
| Stats text/json/ndjson/svg | PASS |
| Automated alg + e2e tests | PASS |
| Full TEST_CATALOG every ID + randomized model | PARTIAL |
| NDJSON for all reporting commands | PARTIAL (stats + diff json) |
| verify --repair single-chunk from parity E2E | PARTIAL |

See `docs/TRACEABILITY.md` and `docs/repository_format.md`.
