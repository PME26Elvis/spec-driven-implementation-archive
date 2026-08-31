# CVC v1 Repository Serialization

This document describes the bytes written by this implementation. Committed object bytes follow the fixed CVC v1 canonical format; implementation-private recovery data is isolated under `.cvc/state/`.

## Repository layout

```text
.cvc/
  HEAD
  config.json
  refs/
    heads/
  objects/
    <2 lowercase hex>/
      <62 lowercase hex>
  state/
  lock
```

`.cvc/lock` is a persistent zero-length regular file. Its on-disk contents do not encode lock state; active locks are POSIX `fcntl` whole-file record locks.

## HEAD and refs

`HEAD` contains exactly:

```text
ref: refs/heads/<branch-name>\n
```

A logical branch `<name>` is the regular file `.cvc/refs/heads/<name>`.

- unborn branch: zero bytes;
- born branch: exactly 64 lowercase hexadecimal commit-ID bytes followed by `\n`.

HEAD/ref changes are installed through fsynced temporary files followed by atomic `rename`, then parent-directory `fsync`.

## Loose object pathname and identity

A raw 32-byte SHA-256 digest is displayed as 64 lowercase hexadecimal bytes. An object whose ID is `abcdef...` is stored at:

```text
.cvc/objects/ab/cdef...
```

Objects are uncompressed and immutable. New object bytes are written and fsynced to a temporary regular file in the destination fan-out directory, then installed without overwriting an existing valid object. If the final pathname already exists, its hash, envelope, type, and payload are validated instead of being rewritten.

## Canonical envelope

Every object is exactly:

```text
<ASCII type> 0x20 <minimal decimal payload length> 0x00 <payload bytes>
```

The type is exactly one of `blob`, `symlink`, `tree`, or `commit`. The decimal length has no leading zero unless it is the one byte `0`. No bytes may follow the declared payload. SHA-256 covers the complete envelope, not just the payload.

## Blob

Blob payload is exactly the eligible regular-file bytes. No line-ending, whitespace, Unicode, or final-newline normalization occurs.

Canonical vectors used by the unit tests:

- `blob 0\0` → `473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813`
- `blob 3\0abc` → `c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6`

Here `\0` denotes one literal NUL envelope separator.

## Symlink

Symlink payload is exactly the link-target byte sequence returned by `readlink`. The target is opaque data and is not canonicalized or interpreted as a repository path. Restoration calls `symlink` with those stored bytes and does not dereference the target.

## Tree payload

All integers are big-endian. A tree payload is:

```text
u32 entry_count
repeat entry_count times:
    u8  entry_type
    u32 name_length
    u8  name[name_length]
    u8  object_id[32]
```

Entry types:

```text
0x01 regular-file blob
0x02 symlink
0x03 subtree
```

`object_id` is raw binary SHA-256, not hex. `name` is one UTF-8 child component. Entries are sorted by unsigned name bytes and duplicates are rejected. Non-root committed empty subtrees are rejected as noncanonical; an empty root tree is valid.

Canonical empty-tree payload is four zero bytes. Its complete-object ID is:

```text
37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f
```

The implementation computes operational snapshots as sorted repository-relative leaf paths, then deterministically emits recursive tree objects. The flat in-memory representation is not an alternate persisted format.

## Commit payload

All IDs are raw 32-byte digests and all integer fields are big-endian:

```text
u8  root_tree_id[32]
u8  parent_count
u8  parent_id[parent_count][32]
i64 timestamp_unix_seconds
u64 message_length
u8  message[message_length]
```

`parent_count` is 0 for a root commit, 1 for ordinary save/rollback, and 2 for a divergent merge. Parent order is preserved. Timestamp is signed 64-bit two's-complement Unix seconds. Message bytes must be nonempty valid UTF-8 and are preserved exactly.

Canonical root-commit vector:

- root = canonical empty-tree ID;
- parent count = 0;
- timestamp = 0;
- message = one byte `x`;
- payload length = 50;
- complete-object ID = `b76903cf9661046c99f6f4d4e9ceda05cef2607b47bd9b2f9396ea67ad1e72ab`.

`CVC_TEST_TIMESTAMP` is the sole test hook affecting commit serialization. It changes only the timestamp field and is rejected unless it is the required canonical signed decimal form fitting `int64_t`.

## Configuration serialization

`.cvc/config.json` is ordinary UTF-8 JSON. `init` writes:

```json
{"format_version":1}
```

JSON is not itself content-addressed. Every existing-repository command parses and validates it before using repository state. The parser is length-aware and handwritten; decoded duplicate keys are rejected.

## Private merge state

Merge/recovery state is not a committed object and therefore cannot affect canonical history interpretation. It is stored at `.cvc/state/merge` using a private binary format beginning with the ASCII magic `CVCMERGE1` and one phase byte:

- phase 1: resolution-active;
- phase 2: finalizing/retryable.

The record then stores length-prefixed branch names, original/target/base commit IDs, length-prefixed message bytes, provisional root tree ID, a sorted list of conflict records (base/ours/theirs kinds and IDs, textual/resolved flags, accepted resolution identity), and the intended merge-commit ID for phase 2. On load, structural fields and every referenced object are validated before the state is trusted.

Private state is written with the same durable metadata-update helper as refs. It never introduces a fifth committed object type.

## Working-tree transaction data

Multi-path materialization uses temporary backup paths under `.cvc/state/`. Existing affected tracked entries are moved by rename to transaction backup storage before replacement. Installed entries are recorded as soon as the installing rename succeeds, including the case where a following directory `fsync` fails. A detected failure removes newly installed entries and renames backups into place before the command returns failure.

Transaction backup names are implementation-private and are not refs, loose objects, or history.
