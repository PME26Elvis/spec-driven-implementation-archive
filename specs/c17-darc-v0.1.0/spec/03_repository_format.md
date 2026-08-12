# 03 — Repository Layout and Deterministic Format

## 1. Repository directory

A DARC repository initialized at `REPO` MUST contain at least:

```text
REPO/
  FORMAT
  HEAD
  refs/
    snapshots/
  objects/
    sha256/
      aa/
        <62-hex>
  index/
    chunks.idx
  parity/
    CATALOG
  state/
    scan-cache
  journal/
  tmp/
  locks/
```

Additional internal files are allowed if documented, but canonical object content and references must remain compatible with this specification.

## 2. FORMAT file

`FORMAT` is UTF-8 text and MUST identify:

```text
DARC
format=1
hash=sha256
chunking=buzhash64
compression=lzh1
parity=xor8+1
```

Unknown mandatory format identifiers MUST cause exit code 10.

## 3. Object addressing

Every canonical immutable object is addressed by SHA-256 of its canonical uncompressed semantic payload, domain-separated by object type:

```text
CID = SHA256("DARC\0" || type_tag || version_u16_le || payload)
```

`type_tag` is one byte. Format-v1 tags:

| Tag | Object |
|---:|---|
| 1 | CHUNK |
| 2 | FILE |
| 3 | TREE |
| 4 | SNAPSHOT |
| 5 | PARITY |

The canonical on-disk file path is lower-case hex:

```text
objects/sha256/<first-2-hex>/<remaining-62-hex>
```

The same CID MUST never map to different semantic payload bytes.

## 4. Integer and string encoding

Canonical binary payloads use:

- unsigned integers: little-endian fixed width where the field width is specified;
- variable-length byte strings: `u64_le length` followed by exactly `length` bytes;
- arrays: `u64_le count` followed by entries;
- booleans: one byte `0` or `1`;
- optional values: one-byte presence flag then value if present;
- paths and symlink targets: raw byte strings, not locale-dependent text;
- human labels such as snapshot names: UTF-8 byte strings, validated as UTF-8.

No C struct memory image may be written directly because padding/alignment is not canonical.

## 5. Framed object file

Each object file stored under `objects/` has:

```text
magic[8]            = "DARCOBJ1"
object_type u8
codec u8             0=raw, 1=lzh1
reserved u16         = 0
uncompressed_len u64_le
stored_len u64_le
header_crc32c u32_le
stored_payload[stored_len]
payload_crc32c u32_le
```

`header_crc32c` covers bytes from `object_type` through `stored_len` inclusive. `payload_crc32c` covers `stored_payload` bytes.

After decoding, CID verification MUST recompute SHA-256 over the domain-separated canonical payload.

CRC-32C is an early damage check; SHA-256 is the authoritative content identity check.

## 6. CHUNK payload

CHUNK payload is exactly the original uncompressed chunk bytes. No metadata is included in the semantic payload.

This guarantees identical chunk bytes share a CID even when originating from different files, paths, snapshots, or repositories using format v1.

## 7. FILE payload

FILE payload fields, in order:

```text
file_format_version u16 = 1
logical_size u64
chunk_count u64
repeat chunk_count:
    chunk_cid[32]
    chunk_length u64
file_content_sha256[32]
```

`file_content_sha256` is SHA-256 of the full logical file byte stream and is used as an additional whole-file verification/diff signal. It MUST be computed by streaming.

The sum of chunk lengths MUST equal `logical_size`.

## 8. TREE payload

A TREE represents one directory. Entries are sorted by raw name bytes using unsigned bytewise lexicographic order. The payload begins with:

```text
tree_format_version u16 = 1
entry_count u64
```

It is followed by exactly `entry_count` child records. For each child:

```text
name: byte_string
entry_type u8: 1=file, 2=directory, 3=symlink, 4=hardlink
mode u32             = st_mode & 07777
mtime_ns i64
```

Type-specific fields:

- file: FILE CID[32], hardlink_group u64 or 0;
- directory: TREE CID[32];
- symlink: raw target byte_string;
- hardlink: canonical target path byte_string, hardlink_group u64.

UID/GID are not stored in format v1.

Directory metadata itself is stored in the parent entry. Real source-root directory metadata is therefore stored on the corresponding child of the synthetic virtual root; SNAPSHOT stores only the virtual root fixed mode/mtime fields.

## 9. Hard-link canonicalization

Within each snapshot scan root:

- regular files sharing `(st_dev, st_ino)` and having link count >1 are treated as one hard-link group;
- paths are sorted canonically before assigning groups;
- the lexicographically first path becomes the group primary and is encoded as a normal file entry with nonzero group ID;
- later paths are encoded as hardlink entries referring to the primary path;
- hardlink group IDs are assigned `1..N` in primary-path sort order.

This makes hard-link topology deterministic for identical filesystem metadata/path structure.

## 10. SNAPSHOT payload

Fields:

```text
snapshot_format_version u16 = 1
created_ns i64
name optional utf8_string
parent_snapshot_cid optional [32]
root_tree_cid [32]
root_mode u32
root_mtime_ns i64
source_root_count u64
repeat:
    source_root_label utf8_string
profile_hash [32]
logical_bytes u64
entry_counts: files u64, dirs u64, symlinks u64, hardlinks u64
unique_chunks_referenced u64
new_chunks_introduced u64
new_stored_bytes u64
```

Snapshot CID is the normal object CID of this payload. Fixed `created_ns` is used for bit-reproducible acceptance tests.

`root_tree_cid` always names a synthetic virtual-root TREE. Each requested SOURCE becomes exactly one child under this virtual root using its stable source-root label. For a directory SOURCE the child is a directory entry carrying that source directory's real mode/mtime and TREE CID; for a regular-file SOURCE it is a file entry. The virtual root itself has fixed `root_mode=0755` and `root_mtime_ns=0` and is excluded from entry counts. This makes one-root and multi-root snapshots use the same deterministic layout.

`new_stored_bytes` is the sum of stored CHUNK payload bytes newly introduced by this snapshot after raw/LZH1 selection, excluding framed-object headers and parity overhead.

## 11. Snapshot refs

`refs/snapshots/<64-hex-snapshot-cid>` files contain exactly the same full lower-case 64-hex snapshot CID plus newline. Snapshot names live only inside SNAPSHOT objects and are resolved by scanning/indexing published refs, so duplicate names remain representable and correctly ambiguous.

`HEAD` contains the current published snapshot CID plus newline, or is empty in a new repository.

A ref file is not published until all objects reachable from the snapshot are durable.

## 12. Merkle semantics

The repository Merkle root for a snapshot is the `root_tree_cid`.

Because every TREE contains child CIDs and metadata in canonical order, and every FILE contains ordered CHUNK CIDs and full-file digest, changing content, path name, type, mode, mtime, symlink target, hard-link topology, or child membership changes the appropriate ancestor CIDs up to the root.

SNAPSHOT then binds the root tree to snapshot metadata and parent linkage.

## 13. Index is derived, not authoritative

`index/chunks.idx` accelerates CID lookup but MUST be rebuildable from immutable objects.

A missing or corrupt index MUST NOT make canonical snapshot data unrecoverable when objects themselves remain healthy.

## 14. Temporary files

Incomplete files MUST be written under `tmp/` or with a clearly temporary suffix and MUST NOT appear at final object paths until their complete bytes have been written, flushed, checked, and atomically renamed.

Stale temporary files are recoverable garbage and may be removed after journal inspection.

## 15. Canonical path safety

Canonical repository paths are relative and use `/` as separator. Snapshot source entries MUST NOT contain:

- absolute archive paths;
- `.` path segments after normalization;
- `..` path segments;
- NUL bytes.

Raw filename bytes other than `/` and NUL are permitted. Text output escapes invalid UTF-8 bytes as `\xNN`; JSON output represents path bytes with an explicit escaped display string plus optional hex field when the path is not valid UTF-8.

## 16. Atomic write primitive

For every authoritative small metadata file/ref update:

1. write new bytes to a file in the same filesystem;
2. flush file data with `fsync`;
3. close;
4. atomically rename over/in place;
5. `fsync` the containing directory.

A failed step MUST leave either the previous authoritative file or a recoverable transaction state.


## 17. Soft snapshot parent linkage

`parent_snapshot_cid` records lineage and incremental provenance, but it is a soft historical link. A child snapshot remains valid if the parent's published ref is later deleted and GC reclaims the parent's SNAPSHOT/TREE objects.

Therefore verification reports a missing parent as `history_parent_unavailable`, not as semantic data corruption, unless the parent still has a published ref. GC does not traverse parent links for reachability.

## 18. Derived incremental scan cache

`state/scan-cache` is non-canonical, rebuildable/dispensable optimization state keyed by published snapshot CID and canonical path. It may record the previous scan's `(device,inode,size,mtime_ns,ctime_ns,mode)` identity tuple and associated FILE CID.

- Device/inode/ctime MUST NOT be inserted into canonical TREE/SNAPSHOT payloads.
- Loss/corruption of scan-cache only disables the no-read incremental fast path; correctness falls back to rereading files.
- Cache update occurs only after snapshot publication and is atomic.
- `verify --repair` may discard/rebuild safe cache state.

## 19. Parity catalog

`parity/CATALOG` is the current protection mapping from CHUNK CIDs to PARITY CIDs. It is atomically updated and journaled but is repairable metadata rather than user snapshot semantics.

If it is missing/corrupt, scrub verification can scan valid PARITY objects, choose non-overlapping valid stripes deterministically in ascending PARITY CID order, then generate new parity for uncovered live chunks when `--repair` is explicit. Loss of the catalog must not by itself make healthy chunk bytes unreadable.

## 20. Forward compatibility

Readers MUST reject unknown object format versions and unknown mandatory codec/type values. They MUST NOT guess at layout.
