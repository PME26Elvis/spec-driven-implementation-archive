# Recovery, GC, Integrity, and Security

## Writer and crash model

Mutating operations use a non-blocking `flock` on `locks/writer.lock`. The kernel lock is authoritative; acquiring a healthy lock does not rewrite the file, preserving repair/GC idempotence. Writes use temporary files, fsync, rename, and directory fsync at publication boundaries. Journals/checkpoints allow deterministic crash/fault tests around object, ref, HEAD, index and parity transitions.

On reopen before a write, DARC recovers stale `journal/` and `tmp/` transient state while preserving already-published immutable objects and refs. A crash can leave safe orphan objects, but must not expose a partial new snapshot or make an older published snapshot invalid.

## Verification levels

- `quick`: repository/framing and derived integrity appropriate to quick validation;
- `full`: semantic object decode/CID verification plus snapshot/Merkle consistency;
- `scrub`: full verification plus active parity checks and transient-state inspection.

Integrity output reports affected object CIDs. `--repair` can rebuild derived index state, remove stale transient state, repair one protected bad/missing CHUNK via parity after CID verification, and regenerate bad/missing parity when source data is healthy. It does not rewrite refs/history to hide unrecoverable loss.

## Restore safety

Archive TREE components are validated as raw path components; absolute names, embedded NUL, `/`, `..`, and crafted traversal entries are rejected. Destination traversal is component-by-component with `openat(..., O_DIRECTORY|O_NOFOLLOW)` so a pre-existing symlink cannot redirect writes outside the restore root.

Regular files are streamed to a temporary path, every chunk is CID-verified/recovered as needed, the FILE full hash is checked, metadata is applied in the required order, and the path is atomically renamed final. Corrupt or unrecoverable bytes are never published as a successful file.

## Garbage collection

GC computes reachability from all currently published snapshot refs and their Merkle object graphs. Snapshot parent linkage is soft historical metadata and does not by itself keep an unreferenced ancestor alive. `gc --dry-run` performs zero repository mutation and reports proposed reclamation.

When a live parity stripe contains dead data members and repacking is enabled, replacement parity is created and made durable before obsolete protection/data is removed. GC then deletes unreachable canonical objects, rebuilds the derived index, and leaves retained snapshots verifiable/restorable. A second GC on the same live set is idempotent.

## Security boundaries

DARC is a local archive engine, not a sandbox for hostile concurrent filesystem mutation. It does, however, enforce the task-required path, length, parser, overflow, symlink-escape, object-version, codec, corruption and allocation checks. Source scanning does not follow symlinks by default and automatically excludes the repository itself when it lies under a source root.
