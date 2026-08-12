# 07 — Index, Verification, Parity Recovery, GC, and Crash Recovery

## 1. Chunk/object index objective

The index accelerates existence and metadata lookup by CID. Canonical object files remain authoritative.

The implementation MUST contain a project-owned Robin Hood hash table for active in-memory lookup.

## 2. In-memory Robin Hood hash requirements

- key: 32-byte CID;
- value: object type, codec, stored length, uncompressed length, health/flags as needed;
- open addressing;
- Robin Hood probe-distance insertion;
- deletion/tombstone handling or rebuild strategy;
- maximum steady-state load factor <= 0.70;
- resizing without key loss;
- deterministic key equality and hash extraction from CID bytes;
- unit tests for collision chains and wraparound.

## 3. Persistent chunks.idx

Persistent index is derived and deterministic. It MUST have:

- magic/version;
- entry count;
- sorted records by full CID bytes;
- integrity checksum;
- enough metadata to rebuild the in-memory table without reading every object payload;
- no pointer values or native struct padding.

If `chunks.idx` is missing/corrupt, `index rebuild` scans canonical object headers and rewrites it atomically.

## 4. Index consistency verification

Verify MUST detect:

- index record whose object file is missing;
- object file absent from index;
- duplicate CID record;
- header metadata disagreement;
- corrupt index checksum/order.

`verify --repair` MAY rebuild the index. Index rebuild is considered safe derived-state repair and must be reported.

## 5. Verification levels

### quick

- repository layout and FORMAT;
- refs parse and target existence;
- object frame header CRC;
- index checksum/consistency sample or full header-level check;
- journal state.

### full

Everything in quick plus:

- decode every reachable object;
- payload CRC;
- recompute every CID;
- validate FILE chunk lengths/full-file digest as far as chunk availability permits;
- verify TREE ordering and child references;
- verify SNAPSHOT totals and validate parent linkage when the parent remains published; a missing unreferenced soft parent is informational history loss, not data corruption;
- verify Merkle reachability from every snapshot ref.

### scrub

Everything in full plus:

- verify every parity stripe and parity byte sequence;
- scan unreachable canonical objects too;
- classify recoverability of every corrupt/missing protected chunk.

## 6. XOR parity stripe design

Format v1 uses fixed `8+1` parity protection over CHUNK semantic bytes.

A stripe contains 1..8 data chunk members plus one PARITY object.

Member selection:

- only newly introduced CHUNK CIDs not already protected by an existing healthy stripe need assignment;
- sort candidate CIDs bytewise;
- group consecutive CIDs in groups of 8;
- final group may contain fewer than 8;
- each chunk belongs to at most one stripe named by the active `parity/CATALOG` in format v1; stale/unreachable PARITY objects may exist transiently and are ignored unless selected during explicit catalog repair.

## 7. PARITY payload

Fields:

```text
parity_format_version u16 = 1
member_count u8 (1..8)
reserved u8 = 0
max_length u64
repeat member_count, sorted by CID:
    chunk_cid[32]
    chunk_length u64
parity_bytes[max_length]
```

Parity byte `j` is XOR of byte `j` from every member whose length is greater than `j`; shorter members contribute zero.

PARITY CID is computed normally over the full canonical parity payload.

## 8. Recovery capability

If exactly one protected member chunk is missing or corrupt and the PARITY object plus all other members are healthy:

- reconstruct the missing semantic chunk bytes by XOR;
- truncate to its recorded original length;
- recompute expected CHUNK CID;
- recovery succeeds only if CID matches.

If two or more members in one stripe are unavailable/corrupt, that stripe is unrecoverable by v1 parity and MUST be reported as such.

If the parity object itself is corrupt/missing but all chunks are healthy, data is healthy but redundancy is degraded; `verify --repair` may regenerate parity.

## 9. Repair behavior

`verify --repair` may:

- rebuild a missing/corrupt derived index;
- reconstruct exactly one bad/missing chunk per recoverable stripe;
- regenerate parity when all data members are healthy;
- remove stale temporary files after journal analysis;
- finish or discard incomplete derived-state writes that are provably not published.

Repair MUST NOT:

- rewrite history to omit unrecoverable files;
- change snapshot roots;
- invent bytes;
- discard refs to make verification green;
- modify a healthy historical object to a different CID.

## 10. Transaction journal

Every mutating operation that can publish authoritative state MUST have a journal transaction directory or record containing:

- transaction ID;
- operation type;
- start timestamp;
- intended ref/HEAD changes;
- stage/state;
- list or recoverable derivation of temporary files;
- completion marker.

At minimum snapshot create, snapshot delete, and GC require journal coverage.

## 11. Snapshot publication crash states

On startup or before a write command, recovery MUST inspect incomplete transactions.

Cases:

1. Crash before any final object rename: discard safe temps.
2. Crash after some immutable objects were published but before snapshot ref: leave them as unreachable objects; discard temps; no visible partial snapshot.
3. Crash after snapshot ref publication but before index update: snapshot remains valid; rebuild/update derived index.
4. Crash during HEAD replacement: atomic rename rules must leave old or new valid HEAD; inspect ref consistency.
5. Crash after completion but before journal cleanup: recognize published state and safely remove stale journal.

## 12. Single-writer lock

Mutating commands MUST acquire a repository writer lock. A concurrent writer must fail cleanly with exit code 9 rather than corrupt state.

Read-only commands may run concurrently where safe. A stale lock strategy MUST avoid blindly deleting a lock held by a live process.

## 13. Garbage collection reachability

GC roots are:

- every valid snapshot ref under `refs/snapshots/`;
- HEAD target;
- any explicitly documented retained transaction root needed for an in-progress operation.

Traverse each published SNAPSHOT into its root TREE, then child TREE/FILE and CHUNK objects. `parent_snapshot_cid` is a soft lineage link and is NOT traversed for GC reachability unless that parent independently has a published snapshot ref.

Active PARITY objects are retained only when they protect at least one live CHUNK and their stripe remains valid under the chosen GC policy.

## 14. GC dry run

`gc --dry-run` MUST perform reachability analysis and report proposed removals/repacking without deleting or rewriting anything.

A before/after recursive byte comparison in acceptance tests must show no repository mutation.

## 15. GC execution

Normal GC MUST:

1. lock repository;
2. journal plan;
3. compute live canonical object set;
4. identify unreachable objects;
5. build replacement parity layout for live chunks if `repack_parity=true` and any stripe has dead members;
6. durably write new parity objects before deleting old parity;
7. atomically update derived parity mapping/index state;
8. delete unreachable objects only after authoritative snapshot safety is established;
9. rebuild/update index;
10. mark journal complete.

A crash mid-GC MUST not destroy a chunk still reachable by a published snapshot.

## 16. GC and parity

Because dead chunks can belong to stripes with live chunks, GC MUST NOT simply delete a dead member and keep old parity as if healthy.

With `repack_parity=true`, live chunks from affected stripes are reassigned deterministically into new sorted CID groups and new parity is generated before obsolete stripe metadata is removed.

## 17. Repository verification result states

- `HEALTHY`: all required objects/refs/index/parity are valid.
- `DEGRADED-REPAIRABLE`: user data is intact or reconstructable, but at least one required object/index/parity condition is damaged and repair not yet applied.
- `REPAIRED`: repair ran and final full verification is healthy.
- `UNRECOVERABLE`: at least one reachable semantic object cannot be reconstructed/validated.

Exit 0 is only valid for HEALTHY or REPAIRED.

## 18. Corruption injection acceptance

Tests MUST directly alter repository bytes to prove detection, including:

- flip byte in raw CHUNK payload;
- flip byte in compressed bitstream;
- truncate object;
- alter header length;
- remove CHUNK object;
- remove two chunks in one parity stripe;
- corrupt PARITY only;
- corrupt TREE;
- corrupt SNAPSHOT;
- corrupt persistent index;
- leave stale temporary files;
- leave incomplete journal stages.
