# CVC Implementation Notes

This document records the repository serialization format, the Win32 platform
integration, and the design choices made where the specification permits
choice points. It is normative evidence for the required "implementation
notes describing repository serialization format" and "implementation notes
describing any permitted choice points" deliverables.

---

## 1. Object Model (fixed v1 canonical serialization)

CVC uses **content-addressed loose objects**. Every object's identifier is the
SHA-256 of its complete canonical envelope. Objects are immutable: the path in
`.cvc/objects/<aa>/<bb…>` is the first 2 hex characters of the id as a fan-out
directory, then the remaining 62 hex characters as the filename.

### 1.1 Loose-object envelope

```
<type> <length>\0<payload>
```

- `type` is an ASCII name: `blob`, `tree`, `commit`, or `symlink`.
- `length` is the decimal byte length of `payload`.
- `\0` is a single NUL separator.
- `payload` is the type-specific body.

The object id is `SHA-256(<full envelope bytes>)`.

### 1.2 Blob payload

The raw file bytes. Blob objects are deduplicated by content: two paths with
identical bytes share the same blob id (spec edge cases §30).

### 1.3 Tree payload (canonical, sorted)

```
count : u32 (big-endian)
then count entries, each:
  entry_type : u8
  name_length: u32 (big-endian)
  name       : name_length bytes (canonical UTF-8, no NUL, no '/', '\',
               control bytes, or Win32-forbidden characters)
  oid        : 32 bytes
```

`entry_type`:

| value | meaning                                   |
|-------|-------------------------------------------|
| 0x01  | regular file (blob)                       |
| 0x02  | file symbolic link                        |
| 0x03  | subtree (tree object)                     |
| 0x04  | directory symbolic link                   |

Entries MUST be sorted by unsigned byte order of the UTF-8 name
(`strcmp` order). `tree_sort_validate` sorts and rejects exact duplicates and
ordinal case-collisions. `verify` rejects an unsorted or non-canonical tree.

### 1.4 Commit payload

```
root_tree      : 32 bytes (id of root tree)
parent_count   : u8
parents        : parent_count × 32 bytes (ids of parent commits)
timestamp      : i64 big-endian (nanoseconds since the Unix epoch; from
                 wall-clock time or the CVC_TEST_TIMESTAMP hook)
message_length : u64 big-endian
message        : message_length bytes (canonical UTF-8, non-empty for a
                 stored commit)
```

First-parent order is preserved for `log` traversal.

### 1.5 Symlink payload

The symbolic-link **print name** (target string). The object type
`symlink` plus the tree `entry_type` (0x02 file vs 0x04 directory) records
both the target and whether the link is to a file or a directory. CVC stores
the link without dereferencing the target.

### 1.6 Known vectors

| object             | SHA-256                                                          |
|--------------------|------------------------------------------------------------------|
| empty blob `blob 0\0` | `473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813` |
| `blob 3\0abc`        | `c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6` |
| empty tree `tree 0\0` | `37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f` |
| symlink `symlink 6\0target` | `99141839c37ea810ef652b9e77d1770a93d34debd0a6e418dce83306659e6e60` |

These are asserted byte-for-byte by the D-category tests.

---

## 2. References, HEAD, and Merge State

### 2.1 Branch refs and HEAD

- A branch ref file (`.cvc/refs/heads/<branch>`) is exactly 65 bytes: 64
  lowercase hex characters plus a trailing `\n`.
- `HEAD` is a text file containing `ref: refs/heads/main` (symbolic).
- An unborn branch has no ref file yet; `save` creates the first commit and
  the ref atomically.

### 2.2 Merge state file

`.cvc/state` (only present during an in-progress or finalizing merge) begins
with the magic `CVCMS1\n` and then stores, length-prefixed:

```
magic            "CVCMS1\n"
orig_branch      (string)  branch at merge start
orig_commit      (32)      commit at merge start
target_branch    (string)  branch being merged in
target_commit    (32)      tip of the merged branch
message          (string)  merge message
provisional      (snapshot) provisional merge tree
conflict count   u32
per conflict: path(string) resolved(u8) has_resolution(u8) resolution(snapshot)
phase            u32       0 = MERGE_PHASE_CONFLICT, 1 = MERGE_PHASE_FINALIZING
finalizing_has_id u32
finalizing_commit (32)     intended merge commit id during finalizing
```

Strings are `u32 length + bytes`. Snapshots are `u32 leaf_count` followed by
per-leaf `(path string, type u8, oid 32)`.

### 2.3 Finalizing retry semantics

During the `finalizing` phase, a retry of `merge --continue` MUST reuse the
exact recorded intended commit id (no new timestamp), MUST reject a
replacement `-m`, and MUST verify the frozen merge-controlled working-tree
projection still matches before moving the ref. If the ref already equals the
intended commit (stale completed state), `--continue` reports
already-completed with exit 0 and performs the permitted cleanup. See
`docs/TEST_EVIDENCE.md` and the L23 test.

---

## 3. Win32 Platform Layer

- **Unicode filesystem interface**: all path operations use the wide Win32 API
  (`CreateFileW`, `MoveFileExW`, `FindFirstFileW`, …). UTF-8 ↔ UTF-16
  conversion is hand-written (`utf8.c`), never delegated to a code page or
  ICU.
- **Repository locking** (`win32.c`): `.cvc/lock` is a persistent zero-byte
  ordinary file opened without truncation with
  `FILE_SHARE_READ|WRITE|DELETE` so competing processes can open it
  concurrently. Commands then take a **nonblocking** `LockFileEx` byte-range
  lock over exactly one byte at offset 0:
  - read-only command: `LOCKFILE_FAIL_IMMEDIATELY` (shared lock);
  - mutating command: `LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK`
    (exclusive lock).
  
  Multiple readers coexist; any writer conflicts with readers and writers.
  Lock-acquisition failure returns repository-busy (exit 5) without waiting.
  `UnlockFileEx` releases on success; process termination also releases the
  lock. No PID stealing or stale-lock recovery; the lock file is never
  recreated.
- **Atomic object publication**: a new immutable object is written to a
  temporary file then installed at its final object pathname with a
  same-volume rename so partial bytes are never exposed. If the final object
  already exists, CVC validates and reuses it only if it is the identical
  valid canonical object.
- **Atomic ref publication**: mutable ref/HEAD replacement uses
  `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` (+ `MOVEFILE_WRITE_THROUGH`)
  so the pathname has old-or-new visibility, never in-place truncation. A ref
  MUST NOT move until every newly referenced object is fully written, flushed,
  installed, and hash-validated.
- **Durability**: file-handle flushing plus same-volume write-through
  replacement ordering. `REPLACEFILE_WRITE_THROUGH` is not used because it is
  unsupported.
- **Symlinks**: creation, reading, and materialization use
  `CreateSymbolicLinkW` / reparse-point reading, without dereferencing.
  Non-symlink reparse points are never traversed.

---

## 4. Hand-written Algorithms (no delegation)

| capability        | module    | notes                                                    |
|-------------------|-----------|----------------------------------------------------------|
| SHA-256           | `sha256.c`| passes FIPS test vectors                                  |
| JSON parser       | `json.c`  | full grammar; duplicate keys, trailing commas, comments,  |
|                   |           | BOM, unpaired surrogates, overflow all rejected;         |
|                   |           | schema validation with unknown-key rejection             |
| UTF-8 validate/conv| `utf8.c` | ordinal validation; surrogate-pair decoding               |
| glob matcher      | `glob.c`  | grammar per spec 05 §9; `**` crossing `/`, `*`, `?`,     |
|                   |           | `{}`, `[]`, `!`, escaping; include/exclude precedence    |
| Myers diff        | `diff.c`  | shortest-edit-script, byte-safe rendering                |
| three-way merge   | `merge.c` | merge-base, recursive tree merge, text merge, conflict,  |
|                   |           | resolve/re-resolve, continue, abort                      |
| repo verification | `verify.c`| detects mandatory corruption classes                     |
| repo traversal    | `scan.c`  | handwritten over low-level Win32 enumeration             |

---

## 5. Permitted Choice Points (as implemented)

- **Timestamp source**: commit timestamp is wall-clock time with nanosecond
  resolution, overridable via the `CVC_TEST_TIMESTAMP` environment variable
  (decimal integer; malformed values are rejected).
- **Ref-format encoding**: 64 lowercase hex characters + `\n` (text, not
  binary).
- **Merge-base selection**: first-parent based common-ancestor search through
  the commit graph.
- **Sorting** of tree entries and output uses unsigned-byte order on canonical
  UTF-8 names.
- **`--no-diffstat`** and command-local display filters do not alter stored
  tracking membership.
- **Object fan-out**: 2/62 (first two hex chars directory, remainder file).

---

## 6. Integrity & Failure-Safety Highlights

- `verify` detects: missing/invalid blobs, trees, and commits; wrong-type
  references; unsorted/non-canonical trees; bad branch refs and HEAD;
  non-canonical object paths (uppercase/mixed-case fan-out); over-long
  components; Win32-invalid/device names; unknown repository format; and
  hash-invalid unreachable objects.
- Merge preconditions (no merge state, clean tracked tree, no
  filtered-out/ineligible collision) block switch/save/rollback/merge before
  any mutation.
- A merge whose automatic result becomes ineligible (NUL in the inspected
  prefix, or >8 MiB) is turned into a structured conflict that materializes
  **ours** at the conflict root — never conflict markers, never a committed
  ineligible tree.
- Working-tree materialization failures (e.g. a read-only or incompatible-open
  destination file) fail cleanly without moving refs and preserve the
  pre-command tracked state.
