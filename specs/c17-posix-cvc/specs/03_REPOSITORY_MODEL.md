# 03 — Repository and Object Model

## 1. Repository Layout

Required v1 layout:

```text
.cvc/
  HEAD
  config.json
  refs/
    heads/
  objects/
    <2-hex-prefix>/
      <62-hex-suffix>
  state/
  lock
```

Temporary files and subdirectories MAY exist under `.cvc/state/`, but they MUST NOT be mistaken for refs or objects. `.cvc/lock` is a persistent zero-length regular file created by `init`; its existence does not mean the repository is currently locked. Active locking is defined by the POSIX advisory record lock in the error/recovery specification.

## 2. HEAD

`HEAD` MUST contain exactly the UTF-8 bytes:

```text
ref: refs/heads/<branch-name>\n
```

The referenced branch is the current branch. Detached HEAD operation is out of scope and MUST NOT be created by required commands. A malformed, non-UTF-8, absolute, escaping, or nonexistent HEAD ref is an integrity error.

## 3. Branch Refs

A branch `<name>` is stored logically at `refs/heads/<name>` beneath `.cvc`.

Its content is exactly one of:

- zero bytes for an unborn branch; or
- 64 lowercase hexadecimal commit-ID bytes followed by one `\n` byte for a born branch.

Branch ref updates MUST be atomic.

Branch names MUST satisfy all of the following:

- 1 to 128 UTF-8 bytes;
- valid UTF-8;
- must not begin or end with `/`;
- no empty `/`-separated segments;
- no `.` or `..` segments;
- no ASCII control bytes;
- no backslash;
- no substring `..`;
- no trailing `.`;
- must not begin with `-`, avoiding ambiguity with required merge/CLI option forms;
- name `HEAD` is reserved.

Because v1 maps `/` in branch names to ref subdirectories, two **existing branch refs** that have a path-prefix file/directory collision cannot coexist. For example, if branch `topic` exists, creation of `topic/ui` MUST fail, and vice versa. This collision is a namespace error, not permission to reject `fix/ui` when no prefix conflict exists. Empty intermediate ref directories left after deletion are not branches and MUST NOT permanently block later creation of the corresponding prefix branch; deletion may prune them immediately, or creation must safely remove an empty stale directory before installing the ref.

## 4. Loose Object Store

Every object is stored separately at the exact fan-out path:

```text
.cvc/objects/<first-2-lowercase-hex>/<remaining-62-lowercase-hex>
```

The object ID is SHA-256 of the complete canonical object bytes and is printed as exactly 64 lowercase hexadecimal characters.

No object compression is allowed.

## 5. Required Object Types

CVC MUST implement exactly the four v1 semantic object types used by snapshots:

- `blob`;
- `symlink`;
- `tree`;
- `commit`.

Extra private object types MUST NOT be required to interpret committed history. Merge/recovery metadata belongs under `.cvc/state/` rather than the committed object graph.

## 6. Canonical Object Envelope

All v1 objects use exactly:

```text
<ASCII-type> SP <minimal-decimal-payload-length> NUL <payload-bytes>
```

where:

- type is exactly `blob`, `symlink`, `tree`, or `commit`;
- `SP` is byte `0x20`;
- payload length is unsigned decimal ASCII with no leading zero except the single digit `0`;
- `NUL` is byte `0x00`;
- exactly the declared number of payload bytes follow, with no trailing bytes.

The object ID hashes this entire envelope. Alternative envelopes are nonconforming in v1.

All integer fields inside tree/commit payloads use big-endian byte order. Unsigned fields use ordinary unsigned binary representation. The signed commit timestamp uses signed 64-bit two's-complement representation encoded big-endian.

## 7. SHA-256

SHA-256 MUST be implemented in the submission's own C source.

Required characteristics:

- correct for empty input;
- correct across arbitrary incremental update chunk boundaries;
- handles messages larger than one SHA-256 block;
- no external hashing executable or library;
- test vectors MUST include standard known SHA-256 vectors;
- canonical-object vectors MUST include `blob 0\0`, whose ID is `473a0f4c3be8a93681a267e3b1e9a7dcda1185436fe141f7749120a303721813`, and `blob 3\0abc`, whose ID is `c1cf6e465077930e88dc5136641d402f72a229ddd996f627d60e9639eaba35a6`.

In the vector notation above, `\0` denotes one literal NUL byte and is not two text characters.

## 8. Blob Objects

Blob payload is exactly the byte content of an eligible regular file.

CVC MUST preserve bytes exactly. It MUST NOT normalize line endings, whitespace, Unicode normalization, final-newline presence, or bytes occurring after the 8192-byte eligibility probe.

Identical eligible file bytes MUST resolve to identical blob object IDs regardless of pathname.

## 9. Symlink Objects

Symlink payload is the exact byte sequence returned as the symbolic-link target by the operating system.

The payload is not interpreted as a repository path and may name an absolute path, relative path, nonexistent path, or a path containing `..`. POSIX symlink targets cannot contain NUL, but other bytes including newline are data and MUST be preserved.

Restoration recreates a symbolic link with the stored target bytes.

## 10. Tree Objects

A tree payload is exactly:

```text
u32 entry_count
repeat entry_count times:
    u8  entry_type
    u32 name_length
    u8  name[name_length]
    u8  object_id[32]
```

Entry type byte values are:

- `0x01` = regular-file blob;
- `0x02` = symlink;
- `0x03` = subtree.

`object_id` is the raw 32-byte SHA-256 value, not hexadecimal text.

Each `name` is one child filename component, not a repository-relative multi-component path. It MUST be nonempty, valid UTF-8, contain no ASCII control byte (`0x01`-`0x1f` or `0x7f`), and MUST NOT contain `/` or NUL or equal `.` or `..`.

Entries MUST be serialized in ascending lexicographic order of the unsigned name bytes. No locale collation or Unicode normalization is permitted. Duplicate names are invalid.

The canonical empty-tree object consists of payload bytes `00 00 00 00`; its complete-object SHA-256 ID MUST be `37b344f390f440a6a43040c9b0da9937d8f0d9d2b4db80cd1e2385054835c50f`.

Directories are structural only. Writers MUST NOT create subtree entries solely to preserve empty working-tree directories. An empty tree payload (`entry_count = 0`) is valid as a commit root for an empty snapshot; committed non-root empty subtrees are noncanonical and MUST be rejected by verification.

## 11. Commit Objects

A commit payload is exactly:

```text
u8  root_tree_id[32]
u8  parent_count
u8  parent_id[parent_count][32]
i64 timestamp_unix_seconds
u64 message_length
u8  message[message_length]
```

`root_tree_id` and each parent are raw 32-byte SHA-256 values. Parent order is semantically significant.

`parent_count` MUST be:

- `0` for a root commit;
- `1` for an ordinary save or rollback commit;
- `2` for a non-fast-forward merge commit.

Values greater than 2 are invalid.

The payload MUST end immediately after the declared message bytes.

Canonical commit vector: a root commit whose `root_tree_id` is the canonical empty-tree ID above, `parent_count = 0`, `timestamp_unix_seconds = 0`, and one-byte message `x` has payload length `50` and complete-object SHA-256 ID `b76903cf9661046c99f6f4d4e9ceda05cef2607b47bd9b2f9396ea67ad1e72ab`.

## 12. Timestamp

Commit timestamp is signed 64-bit Unix time in whole seconds since `1970-01-01T00:00:00Z`, encoded as defined above. Normal CLI operation obtains the current wall-clock time and converts it to this representation.

For deterministic tests, the environment variable `CVC_TEST_TIMESTAMP` is the one permitted time override. When present for a command that creates a commit, its value MUST match `0|-?[1-9][0-9]*` in ASCII and fit `int64_t`; leading `+`, leading zeroes, `-0`, surrounding whitespace, overflow, or any other malformed form MUST make the commit-creating command fail before ref movement. It affects timestamp only.

No other environment variable may bypass required repository behavior for acceptance.

## 13. Commit Message Encoding

Commit messages MUST be valid UTF-8 byte sequences. Invalid UTF-8 message arguments are rejected. Embedded NUL bytes are impossible through ordinary argv and need not be supported.

Messages may contain spaces, newlines supplied through argv, and non-ASCII characters. The exact bytes are preserved.

## 14. Snapshot Semantics

Each commit tree is a complete snapshot of all versioned regular-file and symlink paths selected for that save, with directories represented only as necessary structural trees. It is not a list of patches.

Unchanged files reuse their prior blob object IDs naturally through content addressing.

A new save MUST NOT duplicate a loose object whose object ID already exists and whose bytes verify correctly.

If an object file already exists at an expected ID but its bytes do not hash to that ID or are noncanonical for its type, the repository is corrupt and the write MUST fail rather than overwrite it silently.

## 15. Object Reachability

Objects are reachable when traversable from any born branch ref through commit parents and commit trees.

`verify` MUST validate all such reachable objects. If active merge/recovery state references additional commits/objects required to continue or abort safely, those references and objects MUST also be validated even when not branch-reachable.

`verify` MUST also validate every file located at a canonical loose-object pathname `.cvc/objects/<2-hex>/<62-hex>` even when unreachable. An unreachable object is not an error merely because it is unreachable, but a canonical-path loose object that is hash-invalid, malformed, or noncanonical **is** an integrity error. Temporary/non-object filenames outside the canonical pathname grammar are handled by the temporary-file rules and are not promoted to objects.

## 16. No Object Mutation

Once a valid loose object exists, CVC MUST treat it as immutable.

No command may rewrite a valid object's bytes in place.

## 17. Repository Format Version

The repository format version is the required `format_version` key in `.cvc/config.json`.

Version for this assignment is exactly integer `1`, and it selects the canonical layouts/encodings in this document.

Unknown future format versions MUST be rejected safely with a clear diagnostic rather than guessed.
