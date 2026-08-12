# Remediation Priority

This is not a rewrite plan; it is the shortest sensible order for turning the current prototype into a conforming implementation.

## P0 — correctness and memory safety

1. Replace fixed chunk buffer assumptions with allocation sized to validated effective `chunk_max`; validate all config before use.
2. Rebuild verifier around published-root graph traversal and semantic object validation.
3. Redesign hardlink representation so inode topology is independent from FILE content CID.
4. Restore via temporary files + chunk/full-file verification + fsync + atomic rename.
5. Implement race-resistant restore confinement using directory fds/openat-style operations.

## P1 — canonical product semantics

6. Implement one normalized typed config schema shared by JSON/YAML and all subsystems; reject unknown/duplicate/invalid keys.
7. Implement scan include/exclude/special-file/permission/race policies.
8. Add default HEAD parent and derived scan cache incremental fast path.
9. Implement full semantic diff statuses P/T/H, metadata deltas, chunk reuse, `--path`, JSON/NDJSON/SVG.
10. Implement partial restore and exact overwrite policies.
11. Bring canonical SNAPSHOT/TREE/hardlink/profile fields in line with format spec.

## P2 — durability/recovery

12. Replace current journal with staged transaction records covering objects/ref/HEAD/index/parity/GC states.
13. Move parity generation inside snapshot transaction; make stripe membership CID-sorted and deterministic.
14. Implement full GC mark/sweep from published roots plus deterministic parity repack.
15. Make persistent index sorted, checksummed, deterministic, <=70% in-memory load, and fully verified/rebuildable.
16. Validate an existing object before treating it as a dedup hit.

## P3 — evidence and completion

17. Implement every mandatory acceptance test ID and exact traceability matrix.
18. Add fault-injection wrappers and named crash checkpoints.
19. Run full randomized/reference-model suite.
20. Run mandatory stress scales, not the current 30-small-file smoke test.
21. Only after all gates pass, update README status and claim completion.
