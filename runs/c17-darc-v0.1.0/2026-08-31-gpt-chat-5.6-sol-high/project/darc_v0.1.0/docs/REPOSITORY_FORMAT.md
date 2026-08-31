# Repository Format v1

## Layout

```text
REPO/
  FORMAT
  HEAD
  refs/snapshots/<64-hex-snapshot-cid>
  objects/sha256/<first-2-hex>/<remaining-62-hex>
  index/chunks.idx
  parity/CATALOG
  state/defaults.json
  state/scan-cache
  journal/
  tmp/
  locks/writer.lock
```

`FORMAT` identifies v1 and the fixed algorithms (`sha256`, `buzhash64`, `lzh1`, `xor8+1`). `HEAD` is empty in an empty repository or contains one full snapshot CID plus newline. Published snapshot refs contain their own full lower-case CID plus newline.

## Object identity

Canonical object types are:

1. CHUNK
2. FILE
3. TREE
4. SNAPSHOT
5. PARITY

CID calculation is over the canonical *semantic* payload with the object-type domain separation implemented by `darc_object_cid`; compression never changes a semantic CID. Objects are immutable: if a path for a CID already exists, DARC loads and verifies it before treating it as a duplicate.

## On-disk object frame

Every file under `objects/sha256/` is:

```text
8 bytes   "DARCOBJ1"
1 byte    object type
1 byte    codec (0 raw, 1 LZH1)
2 bytes   reserved = 0
8 bytes   semantic/uncompressed payload length, little-endian
8 bytes   stored payload length, little-endian
4 bytes   CRC-32C of bytes [type..stored_len]
N bytes   stored payload
4 bytes   CRC-32C of stored payload
```

CHUNK may use raw or LZH1 storage. Other semantic objects are encoded canonically and validated for format version. Loading verifies frame structure/CRC, decompresses when required, then can recompute the semantic CID.

## Canonical FILE

FILE payload stores format version, logical byte length, ordered chunk-reference count, each `(chunk CID, chunk length)` pair, and a SHA-256 digest of the complete logical file. The sum of chunk lengths must equal logical length. Restore verifies the full digest before final publication.

## Canonical TREE

TREE entries are sorted by raw byte name, independent of filesystem enumeration/creation order. Each entry stores raw name bytes, type, required mode bits, nanosecond mtime, and type-specific data:

- regular file: FILE CID + hardlink group;
- directory: TREE CID;
- symlink: target bytes;
- hardlink entry: canonical primary path bytes + group identifier.

Names containing NUL or `/` are invalid semantic entries. TREE CIDs form the Merkle directory graph.

## Canonical SNAPSHOT

SNAPSHOT stores format version, creation timestamp, optional UTF-8 name, optional soft parent CID, root TREE CID/metadata, deterministic source-root labels, semantic profile hash, and snapshot statistics (logical bytes, file/dir/symlink/hardlink counts, unique/new chunks and newly stored bytes).

The parent link is historical metadata, not a hard reachability edge for GC: deleting an older snapshot ref does not invalidate a child snapshot.

## Derived metadata

`index/chunks.idx` is deterministic, CID-sorted, checksummed derived metadata and is fully rebuildable by scanning immutable objects. `parity/CATALOG` maps protected CHUNK members to PARITY objects and is repairable/repackable metadata. Neither replaces canonical snapshot semantics.

`state/scan-cache` records local stat identity for the incremental fast path. Device/inode identity is not part of canonical FILE/TREE/SNAPSHOT CIDs.

## Atomic publication

New immutable objects are written under `tmp/`, fsynced, atomically renamed into the CID path, and the containing directory is fsynced. Snapshot ref and HEAD updates use atomic-write/rename publication. The live single-writer lock uses `flock`; journal/tmp recovery removes only transient state, never canonical live objects.
