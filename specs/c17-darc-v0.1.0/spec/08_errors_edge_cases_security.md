# 08 — Errors, Edge Cases, and Safety

## 1. Stable error codes

Human errors MUST include stable symbolic codes. Required minimum set:

```text
E_USAGE
E_CONFIG_FORMAT
E_CONFIG_PARSE
E_CONFIG_SCHEMA
E_REPO_NOT_FOUND
E_REPO_FORMAT
E_REPO_LOCKED
E_SNAPSHOT_NOT_FOUND
E_SNAPSHOT_AMBIGUOUS
E_PATH_NOT_FOUND
E_IO
E_PERMISSION
E_SPECIAL_FILE
E_FILE_CHANGED_DURING_SCAN
E_OBJECT_CORRUPT
E_OBJECT_MISSING
E_INDEX_CORRUPT
E_MERKLE_MISMATCH
E_PARITY_DEGRADED
E_UNRECOVERABLE
E_RESTORE_CONFLICT
E_RESTORE_ESCAPE
E_UNSUPPORTED_VERSION
E_INTERNAL
```

## 2. Error message requirements

An actionable error includes:

- symbolic code;
- operation;
- relevant path or CID when safe;
- OS error context for I/O failures;
- whether repository mutation occurred;
- recovery suggestion when one is defined.

Passwords/secrets are not part of v1, but generic safety rule: never dump raw arbitrary file content into errors.

## 3. Path bytes and display

Linux names may be invalid UTF-8. Repository semantics use raw bytes.

Text display:

- valid UTF-8 may be printed normally;
- control bytes and invalid sequences are escaped as `\xNN`;
- newline/tab in names must not break one-entry-per-line reports.

JSON:

- include a safe display string;
- for non-UTF-8 path bytes also include `path_hex` containing lower-case hex of the raw canonical path.

## 4. Long paths and names

No fixed 256-byte/1024-byte path buffer is acceptable. Use dynamic allocation and checked arithmetic.

The implementation MUST handle paths substantially longer than traditional `PATH_MAX` when OS operations can be performed component-wise, or fail cleanly with OS-derived error without overflow.

## 5. Integer overflow

Every length/count calculation that can originate from file sizes or repository bytes MUST be checked for overflow before allocation, addition, multiplication, seek, or buffer indexing.

Malformed repository length fields MUST never drive unchecked allocation.

## 6. Permissions

Default scan behavior is `error`. If config selects skip:

- skipped path is reported;
- snapshot metadata includes a skipped count;
- success output cannot imply complete coverage without noting skips.

Restore permission failures cause command failure and must not leave a corrupt final file.

## 7. Special files

Sockets, FIFOs, block devices, and character devices are not archived in v1. Default is error; configurable skip is allowed.

Never read indefinitely from FIFO/device content as if a regular file.

## 8. Repository inside source tree

DARC MUST detect its own repository directory when scanning a containing source and exclude it automatically, even if include rules would match it. A warning should state the exclusion.

## 9. Recursive repository references

A restore destination located inside the active repository MUST be rejected unless it is clearly outside all internal repository control directories and the implementation documents a safe mode. Mandatory acceptance expects rejection.

## 10. Symlink attacks during restore

Restore safety checks must not rely only on a preflight string check. Destination parent components must be opened/validated so a preexisting symlink cannot redirect writes outside `--to`.

Race-resistant `openat`/`mkdirat`-style techniques are strongly expected.

## 11. Snapshot selector ambiguity

If two CIDs share a supplied prefix, the command MUST fail rather than choose one. Same for duplicate snapshot names.

## 12. Corrupt refs

A ref with malformed hex, wrong length, extra garbage, or missing target is corruption. Read commands must report it; GC MUST NOT silently ignore a malformed ref and then delete potentially reachable data.

## 13. Object collision defense

When storing a CID that already exists:

- validate the existing object;
- if healthy and semantic payload has expected CID, reuse;
- if unhealthy, stop with corruption;
- never overwrite an existing canonical object simply because its path matches the newly computed CID.

## 14. Truncated writes

Every object decoder must distinguish clean EOF at exact end from truncation. Truncation is corruption.

## 15. Decompression bombs

Object frame declares expected uncompressed length. Decoder must enforce it and MUST NOT emit beyond that size.

For CHUNK objects, format/config max chunk size gives an additional bound.

## 16. Malformed Huffman tables

Reject over-subscribed, impossible, or otherwise invalid canonical code-length sets. Do not continue with guessed codes.

## 17. Invalid Merkle references

TREE/FILE references and a SNAPSHOT root-tree reference to missing or wrong-type CIDs are corruption. `parent_snapshot_cid` is the sole soft-reference exception: if its parent no longer has a published ref, the parent object may legitimately have been reclaimed and is reported only as unavailable history.

## 18. Parent-history traversal bounds

Valid content-addressed snapshot objects do not normally permit a practical parent cycle without also violating CID integrity. Commands that follow available soft-parent history MUST nevertheless use explicit visited/depth protection so malformed/corrupt data cannot cause unbounded recursion or looping. CID integrity failures take precedence over attempting to interpret fabricated history.

## 19. Concurrent mutation

Read commands encountering a repository mutation must either operate on a consistent published view or fail/retry safely. They must not parse half-written authoritative files; atomic publication provides the baseline.

## 20. Disk-full handling

Simulated short writes/ENOSPC must be tested through fault injection wrappers.

A failed snapshot due to disk full MUST leave prior snapshots valid and the new snapshot unpublished.

GC must not delete live old data before replacement parity/metadata is durable.

## 21. Allocation failure handling

The project SHOULD centralize allocation wrappers so tests can inject deterministic failures. Mandatory critical paths must handle NULL without undefined behavior.

## 22. Timestamp range

mtime and snapshot timestamps are signed 64-bit nanoseconds. Conversion/formatting must avoid overflow and report out-of-range display values safely.

## 23. Modes and umask

Restore applies final mode explicitly. Temporary creation may be affected by umask but final requested permission bits must match archived mode where permitted by OS.

## 24. TOCTOU scan changes

The before/after stat retry rule is mandatory. A file that repeatedly changes causes snapshot failure, not a potentially inconsistent successful snapshot.

## 25. Zero and tiny data

Must correctly support:

- empty repository with no snapshots;
- snapshot of empty directory;
- zero-byte files;
- files shorter than rolling window;
- files shorter than min chunk size;
- exactly min/avg/max lengths;
- one-byte files.

## 26. Huge directory fanout

Directory child ordering must be deterministic for thousands of entries. Sorting cannot depend on locale collation.

## 27. Duplicate file names under different directories

They remain distinct canonical paths and must not collide in manifests.

## 28. Cleanup rules

A command failure may leave only:

- journal evidence needed for recovery;
- immutable unreachable objects already safely published;
- temporary files identifiable as stale.

It MUST NOT leave a ref to an incomplete snapshot.
