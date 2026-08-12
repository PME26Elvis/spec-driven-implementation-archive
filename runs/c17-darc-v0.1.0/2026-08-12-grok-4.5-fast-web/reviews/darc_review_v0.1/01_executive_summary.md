# Executive Summary

## Verdict

**FAIL / NOT RELEASE-READY.** The archive should not be described as `gates_closed` under the supplied DARC task pack.

There is genuine implementation work here: approximately 3,586 lines of C/header code plus a small test suite, real SHA-256, CRC-32C, Buzhash, LZH1, repository objects, snapshots, restore, parity, index, and CLI plumbing. However, several mandatory subsystems are either incomplete, semantically incorrect, or insufficiently tested.

## Highest-severity findings

1. **Critical memory-safety bug in configurable CDC.** `process_file()` allocates a fixed 262,144-byte chunk buffer but honors a larger configured `chunk_max`; a legal larger configuration causes heap overflow. ASan reproduced this deterministically.
2. **Full verification does not verify snapshot reachability/Merkle semantics.** Removing every live FILE object can still yield exit 0 from `verify --level full`; restore then fails.
3. **GC is intentionally incomplete.** With any snapshot ref remaining, it keeps all objects and therefore cannot reclaim objects made unreachable by deleting one historical snapshot.
4. **Hard-link model is wrong.** Two independent files with identical content are restored as hard links because restore infers hard-link topology from FILE CID equality rather than archived inode topology.
5. **Partial restore is not implemented.** `--path` is accepted, then ignored.
6. **Configuration implementation is far below contract.** Include/exclude arrays are skipped; unknown keys are accepted; duplicate JSON keys are accepted; many canonical keys do not map to implementation fields; configuration normalization/hash semantics are incomplete; several configured values are ignored by runtime behavior.
7. **Diff is a minimal CID/path comparison rather than the required semantic diff.** No A/D/M/P/T/H model, metadata field deltas, chunk reuse metric, subtree filtering, NDJSON, or SVG diff report.
8. **Crash journal does not encode transaction stages.** Recovery simply removes temporary files and deletes the journal, so the specified checkpoint recovery guarantees are not demonstrated.
9. **The delivered tests do not implement the mandatory acceptance catalog.** The project itself acknowledges partial mapping; the catalog runner contains only 22 broad checks and the stress test uses 30 small files.

## Supplied test suite result

All supplied tests were green:

- `make test`: PASS
- `tests/catalog_runner.sh`: 22 PASS / 0 FAIL
- `tests/stress.sh`: PASS

This is not contradictory with the FAIL verdict: the independent tests target behaviors the supplied tests do not cover.

## Independent black-box sample

The first targeted black-box batch produced **9 failures out of 11 cases**. Several of the two nominal passes were later found to be false confidence because diff reports only the enclosing directory CID change rather than the required leaf-level semantic classification.

## Release-gate conclusion

At least the Build strictness, Snapshot, Diff/Reporting, Restore, Integrity/Recovery, Crash Safety, GC, Configuration, Test, Documentation/traceability, and overall Definition-of-Done gates fail. The implementation should be treated as a partially functioning prototype, not a completed benchmark solution.
