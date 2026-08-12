# 05 — Scanning, Metadata, Snapshots, and Incremental Backup

## 1. Scan roots

`snapshot create SOURCE...` accepts one or more files/directories. The canonical v1 acceptance path focuses on directory roots.

Each source root receives a stable logical label derived from its final non-empty path component unless explicitly disambiguated. Duplicate labels MUST be rejected unless the CLI offers an explicit label extension documented by the implementation.

Every source is represented as one child of the snapshot's synthetic virtual root. Restoring the full snapshot therefore creates these source labels beneath the restore destination. The virtual root is not counted as an archived directory; each directory source itself is counted.

The repository directory itself MUST be excluded if it resides below a source path.

## 2. Recursive traversal

Traversal MUST:

- use filesystem APIs directly;
- recursively enumerate directories;
- never depend on `find` or shell glob expansion for internal recursion;
- sort children by raw name bytes before canonical manifest construction;
- be robust to directory enumeration order differences;
- detect recursion loops if symlink following is enabled;
- respect cross-filesystem policy.

## 3. Supported entry types

Required:

- directory;
- regular file;
- symbolic link;
- hard link relationship between regular-file paths.

Special files:

- FIFO;
- socket;
- block device;
- character device.

They MUST either cause a clear error or be skipped according to `scan.on_special_file`. Skipping MUST be reported and must not be silent.

## 4. Symlink behavior

Default: do not follow symlinks. Store the symlink target bytes exactly as returned by the OS.

If `scan.follow_symlinks=true`:

- the implementation MUST prevent cycles;
- traversal outside the selected source root MUST be rejected unless explicitly documented as a safe supported extension;
- a link followed as content MUST have deterministic semantics documented in the produced implementation.

The mandatory acceptance path uses the default non-following behavior.

## 5. Cross-filesystem behavior

Default `scan.cross_filesystems=false` prevents descending into a directory whose device ID differs from the scan root device. Such entries are reported as skipped mount boundaries.

## 6. Include/exclude glob language

Patterns match canonical `/`-separated relative paths.

Required syntax:

- `*` matches zero or more bytes except `/`;
- `?` matches exactly one byte except `/`;
- `**` matches across directory boundaries;
- `[abc]` and `[a-z]` byte-class ranges;
- leading `!` negates an exclusion rule only when used in the exclude list;
- backslash escapes a metacharacter.

Rules are evaluated in order; later matching rules override earlier ones within the same list. Include filtering occurs before exclude filtering. Directory pruning must not incorrectly skip a descendant that could be re-included by a later pattern.

Pattern matching is byte-oriented and case-sensitive.

## 7. Metadata captured

For every entry as applicable:

- type;
- canonical relative name/path;
- POSIX mode permission/type-relevant bits;
- mtime in nanoseconds;
- logical file size;
- symlink target;
- hardlink group topology.

UID/GID and xattrs are not required in format v1.

## 8. Race handling during scan

Files may change while a snapshot is being created.

Required policy:

- capture stat information before reading;
- stream and hash/chunk the file;
- capture stat information after reading;
- if size, mtime, ctime, device, or inode identity changed during the read, retry the file once;
- if it changes again, fail that snapshot with `E_FILE_CHANGED_DURING_SCAN` rather than silently snapshot an indeterminate mix.

Snapshot publication MUST not occur after an unrecovered scan inconsistency.

## 9. Snapshot construction

A successful snapshot creation MUST:

1. resolve and validate config;
2. acquire single-writer repository lock;
3. create transaction journal record;
4. scan canonical paths;
5. build/reuse FILE and CHUNK objects;
6. build TREE objects bottom-up;
7. build parity stripes for newly introduced unprotected chunks as required;
8. build SNAPSHOT object;
9. durably publish all immutable objects;
10. atomically publish snapshot ref and HEAD;
11. update/rebuild derived index as needed;
12. mark transaction complete;
13. release lock.

If publication fails before step 10, the snapshot MUST remain invisible. Orphan immutable objects may remain and are later collectible.

## 10. Snapshot parent

By default, a new snapshot uses current HEAD as parent if HEAD exists. `--parent` overrides it. A parent must be a valid snapshot in the same repository.

A snapshot with identical root content/metadata but different timestamp or parent may have a different SNAPSHOT CID while reusing the same TREE/FILE/CHUNK objects.

## 11. Incremental backup semantics

Incremental behavior has two independent layers:

### 11.1 Storage incrementality

Already-present healthy CHUNK, FILE, and TREE objects MUST be reused by CID. Unchanged data MUST NOT be stored a second time.

### 11.2 Scan fast-path incrementality

When a parent snapshot is supplied and `snapshot.trust_unchanged_identity=true`, a regular file MAY reuse the parent's FILE object without rereading payload only when a healthy derived `state/scan-cache` record exists for that exact parent/path and all of the following current stat fields match its recorded values:

- device ID;
- inode;
- size;
- mtime_ns;
- ctime_ns;
- mode bits relevant to the snapshot entry.

Device/inode/ctime are optimization state only and MUST NOT enter canonical Merkle objects. If the cache is absent, stale, corrupt, or mismatched, content MUST be reread/chunked. Metadata-only changes may still reuse the FILE content object after content identity is established.

The implementation MUST expose counters showing fast-path reused files vs reread files.

## 12. Duplicate files

Two independent files with identical byte content but different inodes:

- are not hard links;
- remain separate file path entries;
- SHOULD resolve to the same FILE object if chunk sequence and file digest are identical;
- MUST reuse identical CHUNK objects.

Restore must recreate two independent files unless hard-link topology says otherwise.

## 13. Hard links

Hard-link detection is based on `(st_dev, st_ino)` during a consistent scan.

On restore with `create_hardlinks=true`, later members MUST be created as hard links to the primary after the primary is fully restored.

If hard-link creation fails due to destination filesystem limitations, restore MUST fail unless a future explicit fallback option is added. It MUST NOT silently duplicate bytes in the mandatory v1 behavior.

## 14. Empty files and directories

- Empty files have valid FILE objects with zero chunks and SHA-256 of empty byte stream.
- Empty directories have TREE objects with zero child entries.
- Empty directory metadata changes affect TREE ancestry.

## 15. Large files

Large files MUST be chunked and processed streaming. No file-size-based special case may bypass hashing, dedup, or integrity.

## 16. Snapshot naming

Optional snapshot name:

- UTF-8;
- 1..128 Unicode scalar values;
- no NUL;
- leading/trailing whitespace is preserved but discouraged;
- names need not be unique;
- IDs are authoritative when ambiguity exists.

## 17. Snapshot reference resolution

A snapshot selector may be:

- full 64-hex CID;
- unique hex prefix of at least 8 characters;
- `HEAD`;
- exact snapshot name only if it resolves uniquely.

Ambiguous prefixes/names MUST fail with candidate information.

## 18. Snapshot statistics

Creation output and stored snapshot metadata MUST expose at least:

- scanned entries;
- skipped entries;
- files reread;
- files fast-path reused;
- logical bytes;
- total chunk references;
- unique chunk references;
- reused existing chunks;
- newly stored chunks;
- raw vs compressed new chunk bytes;
- dedup/reuse percentage;
- parity stripe/member counts;
- root Merkle ID.
