# DARC v0.1.0 — Deterministic Deduplicating Archive Benchmark Task Pack

DARC is a specification-driven software implementation assignment. The required deliverable is a production-style, headless, command-line archival engine written in C17 for Linux/POSIX systems. It creates versioned snapshots of directory trees using content-defined chunking, chunk-level deduplication, self-implemented compression, cryptographic content identifiers, Merkle integrity structures, deterministic repository encodings, garbage collection, verification, crash-safe publication, and bounded corruption recovery.

This task pack defines the product behavior, engineering constraints, data formats, validation requirements, delivery contents, and completion criteria.

## Required capability summary

The implementation MUST include all of the following as real, wired functionality:

1. Recursive directory scanning.
2. Metadata manifests.
3. Content-defined chunking.
4. Rolling hash boundary detection.
5. Chunk-level deduplication.
6. Cryptographic content IDs.
7. Merkle tree integrity.
8. Snapshots, including safe published-ref deletion.
9. Incremental backup.
10. Snapshot diff.
11. Restore.
12. Hard-link and duplicate handling.
13. Self-implemented LZ77 + canonical Huffman compression.
14. Chunk index.
15. Garbage collection.
16. Corrupted-block detection.
17. Partial corruption recovery using repository parity stripes.
18. Interrupted-write recovery.
19. Atomic repository updates.
20. Repository verification.
21. Deterministic archive format.
22. JSON and YAML parameter/configuration files.
23. High-quality human CLI output plus machine-readable output; selected reporting commands also emit standalone SVG.
24. A complete implementation-owned automated test suite covering the acceptance catalog in this task pack.

## Canonical executable

The produced CLI executable is named `darc`.

## Document map

- `spec/00_scope_and_constraints.md` — scope, platform, required implementation boundaries, forbidden substitutes.
- `spec/01_cli_contract.md` — commands, options, exit codes, stdout/stderr rules, presentation quality.
- `spec/02_config_json_yaml.md` — shared parameter model and JSON/YAML behavior.
- `spec/03_repository_format.md` — repository layout, object framing, canonical encoding, IDs, publication rules.
- `spec/04_chunking_hash_compression.md` — exact CDC, rolling hash, SHA-256 and compression contracts.
- `spec/05_scan_snapshot_incremental.md` — filesystem scan, metadata, hard links, snapshots, incremental behavior.
- `spec/06_diff_restore_reporting.md` — diff semantics, restore rules, text/JSON/NDJSON/SVG reports.
- `spec/07_index_gc_integrity_recovery.md` — index, verification, parity recovery, GC, crash recovery.
- `spec/08_errors_edge_cases_security.md` — error taxonomy, hostile/corner cases, safety rules.
- `spec/09_test_and_acceptance.md` — required automated testing strategy and test obligations.
- `spec/10_delivery_dod_release_gates.md` — delivery contents, Definition of Done, stop conditions, release gates.
- `acceptance/CHECKLIST.md` — concise human acceptance checklist.
- `acceptance/TEST_CATALOG.md` — explicit required test cases.
- `acceptance/GOLDEN_VECTORS.md` — fixed algorithm/format vectors that tests must not derive from production code.
- `examples/config.json`, `examples/config.yaml` — semantically equivalent example configurations.

## Normative wording

`MUST`, `MUST NOT`, `REQUIRED`, `SHALL`, and `SHALL NOT` are mandatory. `SHOULD` is strongly recommended and may only be omitted with a documented engineering reason. `MAY` is optional.

If documents appear to conflict, precedence is:

1. `spec/10_delivery_dod_release_gates.md`
2. the feature-specific specification document
3. `spec/00_scope_and_constraints.md`
4. examples and non-normative notes

The implementation MUST NOT claim completion while any mandatory Release Gate remains failing.
