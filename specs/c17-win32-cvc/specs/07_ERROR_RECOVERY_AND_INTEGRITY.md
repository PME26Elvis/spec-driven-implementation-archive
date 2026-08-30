# 07 — Error Handling, Atomicity, Recovery, and Integrity

## 1. General Safety Rule

No command may knowingly leave a branch ref pointing to a missing, partially written, or hash-invalid commit object.

Repository metadata updates MUST be ordered so that newly referenced objects are durable before refs are moved to them.

## 2. Repository Locking

`.cvc/lock` MUST be an actual zero-length ordinary non-reparse file, opened without replacing or truncating it, and locked with nonblocking Win32 `LockFileEx` byte-range locking. The file remains zero length because the required lock range may extend beyond EOF.

The exact lock range is byte offset `0`, length `1`. Read-only commands use `LOCKFILE_FAIL_IMMEDIATELY` without `LOCKFILE_EXCLUSIVE_LOCK`; mutating commands use `LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK`. Competing processes MUST be able to open the persistent lock file concurrently so that serialization is governed by the byte-range lock rather than by an accidental exclusive file-open share mode.

Mutating repository commands acquire the exclusive lock before reading state that will govern mutation and hold it through the complete operation. At minimum:

- `save`;
- branch create/delete;
- `switch`;
- `merge`;
- `merge --continue`;
- `merge --abort`;
- `resolve`;
- `rollback`;
- `restore`.

Read-only repository commands (`status`, `diff`, `log`, branch list, `verify`, `config show`, `config validate`) acquire a nonblocking shared lock for their repository read. Multiple readers may coexist. An active writer causes reader acquisition to fail; active readers cause a writer acquisition to fail. The command then exits nonzero with a concise repository-busy diagnostic rather than waiting indefinitely.

`UnlockFileEx` is used on normal release. Closing the lock handle or process termination releases the kernel lock, so v1 has no stale-lock timeout, PID-file stealing, age-based lock recovery, or lockfile deletion protocol. Merely seeing the persistent `.cvc/lock` file is never evidence that a lock is held.

Required product behavior MUST NOT use a different hidden lock file as the actual serialization mechanism while leaving `.cvc/lock` decorative.

## 3. Atomic File Replacement

Mutable metadata such as refs, HEAD, merge state, and config rewrites performed by CVC MUST use same-volume write-to-temporary + `FlushFileBuffers` + Win32 rename/replacement publication semantics rather than destructive in-place truncation where a crash could produce ambiguous partial content. Required native ordering is further defined by `09_WINDOWS_PLATFORM_CONTRACT.md`.

## 4. Object Write Protocol

A new loose object MUST be written such that an interrupted write cannot be mistaken for a valid completed object.

Acceptable protocol:

1. serialize complete canonical bytes;
2. calculate ID;
3. if valid object already exists, reuse it;
4. otherwise write a uniquely named temporary file under repository storage;
5. successfully flush the complete temporary file with `FlushFileBuffers`;
6. install it at the final object path with a same-volume Win32 rename/publication operation that never exposes partial object bytes;
7. handle a racing existing identical object safely.

Temporary files must not be traversed as valid objects.

## 5. Save Failure Semantics

If save fails before branch-ref update:

- previous branch tip remains unchanged;
- newly written unreachable objects may remain;
- such unreachable objects do not constitute repository corruption.

If branch-ref update succeeds, all objects reachable from the new commit must already be complete and hash-valid.

## 6. Working-Tree Update Safety

Commands that materialize snapshots (`switch`, `rollback`, merge fast-forward, clean/conflicted merge application, merge abort, and recursive `restore`) MUST preflight foreseeable collisions and permissions before destructive changes.

For **detected runtime failures** after a multi-path materialization has begun, a command that returns failure MUST restore the pre-command tracked working-tree state for the affected operation and leave the controlling HEAD/ref/history state unchanged, except that already-created unreachable immutable objects may remain. A rollback journal, backup-renames, or equivalent strategy is therefore required where simple preflight cannot guarantee this property.

History/ref ordering MUST satisfy:

- `switch`: materialize safely, then atomically update `HEAD`; if the HEAD update fails, restore the old tracked tree before returning failure;
- fast-forward merge: materialize safely, then update the current branch ref; if ref update fails, restore the old tracked tree;
- clean divergent merge/rollback: all new objects may be installed first while unreachable, then materialize the intended tree, then atomically move the branch ref; if ref movement fails, restore the old tracked tree;
- conflict-producing merge: persistence of merge state and provisional/conflict working-tree materialization must behave as one logical transition on normal detected errors. If the transition cannot complete, restore the pre-merge tracked tree, remove incomplete merge state, and keep the branch ref unchanged.

A sudden process kill or power loss during multi-file working-tree materialization is not required to provide a fully transactional filesystem snapshot in v1. However, refs MUST still obey object-before-ref durability rules, and a later command must never interpret temporary/journal files as committed content.

Acceptance tests may inject ordinary failures such as a Windows sharing/permission failure on a target path or a simulated ref-update failure and will check both that CVC reports failure and that working-tree/history state satisfies these rules.

### 6.1 Directory-container and collision rules

Because directories are structural rather than independently versioned, an existing untracked directory is **not** a collision merely because a target snapshot also needs a directory container at that path. CVC may reuse the directory and MUST preserve unrelated untracked descendants; collisions are checked at actual target entries/structural type transitions.

In particular:

- target needs directory container + existing untracked directory => allowed, then check descendants individually;
- target needs regular file/supported symbolic link + existing untracked directory => collision unless that directory/structure belongs to current tracked state and the command is explicitly allowed to replace tracked state;
- target needs directory container + existing untracked regular file/supported symbolic link/unsupported reparse entry => collision;
- removing tracked descendants MUST NOT recursively delete unrelated untracked descendants merely to remove a now-unneeded directory;
- a directory that becomes empty after removal of tracked descendants MAY be removed, but empty-directory preservation is not required.

These rules apply consistently to switch, rollback, merge materialization/abort, and recursive restore.

### 6.2 Regular-file replacement and hard-link safety

When CVC replaces the bytes of a working-tree regular file during materialization, it MUST NOT implement replacement by opening the existing pathname and truncating/writing the same NTFS file record. The existing pathname may be a hard link to an unrelated path, including one outside the repository.

CVC MUST instead create a new temporary ordinary file on the same volume, write and flush it, then replace the destination pathname using the operation's safe Win32 rename/replacement and rollback strategy. Deleting a tracked regular file removes only that pathname.

This rule intentionally breaks any prior hard-link relationship at the replaced repository path while preserving the bytes visible through the unrelated hard-link name.

## 7. Integrity Verification

`cvc verify` MUST validate at least:

### 7.1 Repository metadata

- required metadata directories/files exist with the required ordinary non-reparse directory/regular-file types and no reparse-point redirection;
- fixed repository metadata names have the exact canonical on-disk spelling from the repository layout; Windows case-only aliases are rejected;
- object fan-out directories/files use exact lowercase hexadecimal spelling, not a case-insensitive alias;
- persistent lock file exists, is an ordinary non-reparse regular file and has zero length (v1 lock state is the Win32 byte-range lock, never file bytes);
- format version recognized;
- HEAD syntactically valid and uses the exact v1 symbolic-ref format;
- current branch exists;
- every branch ref syntactically valid;
- the complete branch-ref namespace obeys the same v1 prefix file/directory and Windows ordinal case-insensitive collision rules required at branch creation; externally introduced `Feature`/`feature`-style duplicate refs or file/directory prefix conflicts are repository integrity errors rather than two independently valid branches;
- every non-unborn branch ref names an existing valid commit.

### 7.2 Object hashing

For each verified object:

- read full canonical bytes;
- recompute SHA-256;
- confirm computed hash equals path/object ID.

### 7.3 Object structure and canonicality

- envelope exactly matches the v1 canonical grammar;
- declared payload length matches actual payload;
- type is known;
- tree/commit integer fields and lengths are in bounds;
- tree entries are in strict unsigned-byte canonical name order with no exact duplicates and no Windows ordinal case-insensitive sibling collision;
- non-root committed empty subtrees are rejected;
- commit message is nonempty valid UTF-8 and parent count is valid; a two-parent commit has two distinct parent IDs;
- every verified `blob` object, reachable or unreachable, satisfies the v1 size/NUL-prefix eligibility rule;
- every verified `symlink` object, reachable or unreachable, is nonempty valid UTF-8 and contains no NUL byte;
- referenced object types match expected entry types, including both Windows file-link (`0x02`) and directory-link (`0x04`) entries referencing `symlink` objects;
- reserializing the parsed object under v1 rules would produce exactly the stored canonical bytes.

### 7.4 Graph and active-state integrity

- parent commit IDs exist and are commits;
- commit parent count is 0..2;
- tree references exist;
- tree recursion contains no malformed repository-path entry, including a component that exceeds the current repository volume's reported maximum component length after canonical UTF-8-to-UTF-16 conversion;
- reachable graph traversal terminates using visited-object tracking;
- every verified tree/commit object reference resolves to an existing object of the required type, including references originating only from otherwise-unreachable canonical loose objects;
- active merge state, if present, is structurally valid and all commit/object references needed for continue/abort/finalization are valid; ordinary conflict/resolution state requires the current ref to equal the recorded original commit, while `finalizing` state is valid only when the ref equals either that original commit (retryable) or the recorded intended merge commit (logically complete, cleanup pending). A retryable finalizing state also requires `merge --continue` to re-check the frozen merge-controlled working-tree projection before ref movement as specified by the merge document; unrelated noncolliding untracked/ineligible paths remain outside that projection.

Because content-addressed commit cycles are not practically constructible without invalid hashes, any detected traversal anomaly is corruption.

## 8. Corruption Response

When normal commands encounter a required object whose hash or structure is invalid, they MUST fail rather than silently continue with partial history.

No automatic repair is required.

Diagnostics SHOULD identify the corrupt object ID/path.

## 9. Config Corruption

Malformed or schema-invalid `config.json` causes repository commands that depend on config to fail before mutating repository state.

`cvc config validate` must expose the error.

## 10. Missing Objects

A missing referenced object is a hard repository integrity error.

Status/diff/log/switch/merge/rollback MUST NOT fabricate missing content.

## 11. Merge-State Integrity

Merge state MUST contain enough information to support continue/abort/finalization deterministically, including:

- original current branch name and commit;
- target branch name and target commit;
- intended merge message;
- provisional nonconflicting merge result;
- conflict path set;
- resolved/unresolved state and each accepted resolved entry;
- phase (`conflicted`/resolution-active versus `finalizing`) and intended merge-commit ID when finalizing.

Malformed merge state causes mutating merge commands to fail safely. The stale-finalizing recovery rule in the merge specification is mandatory: completed finalizing state is cleaned only while holding the exclusive lock for a later mutating command; read-only commands recognize it without rewriting state.

## 12. Repository-Metadata Filesystem Safety

The repository metadata hierarchy MUST NOT be redirected through a Windows reparse point or unexpected file type. Before relying on it, normal commands/verification MUST require:

- `.cvc`, `.cvc/refs`, `.cvc/refs/heads`, `.cvc/objects`, and `.cvc/state` to be actual ordinary directories with `FILE_ATTRIBUTE_REPARSE_POINT` clear;
- `.cvc/HEAD`, `.cvc/config.json`, `.cvc/lock`, born/unborn ref files, and loose object files to be ordinary non-reparse regular files when present;
- intermediate branch-ref directories created for names such as `fix/ui` and object fan-out directories such as `objects/ab/` to be ordinary non-reparse directories;
- any merge/recovery state file consumed as authoritative metadata to be an ordinary non-reparse regular file.

CVC MUST validate path components sufficiently to prevent an intermediate junction/symlink from redirecting a later final-path open. A violation is a repository-integrity/safety error. CVC MUST NOT follow such a metadata reparse point and MUST NOT silently replace it as if it were valid metadata. Temporary-file creation and publication targets must remain inside verified real repository directories.

## 13. Path Traversal Defense

Repository object data MUST never be allowed to write outside repository root during restore/switch/rollback/merge.

Tree entry names containing `/`, `\`, `.`/`..` path components, NUL, Windows-forbidden/reserved component forms, case-insensitive sibling collisions, or other invalid canonical path forms MUST be rejected by verification/materialization.

## 14. Reparse-Point Traversal Defense

When materializing a tracked path, CVC MUST NOT follow an existing working-tree symbolic link, junction, mount point, or other reparse point in a way that writes or deletes a descendant outside repository root.

Example: if an untracked `dir` is a directory symbolic link or junction targeting a location outside the repository, restoring tracked `dir/file.txt` MUST detect the structural collision and fail rather than traverse the reparse point.

Supported symbolic links are materialized only as leaf entries. Unsupported reparse points are never traversed as structural directories.

## 15. Disk Full / I/O Failure

Every required read/write/flush/rename failure MUST be checked.

CVC MUST NOT report success after a failed mandatory filesystem operation.

Exact recovery from disk-full conditions is not required beyond preservation of previous valid refs and rejection of partial objects as valid.

## 16. Read-Only Commands

`status`, `diff`, `log`, `branch` list, `config show/validate`, and `verify` MUST NOT alter branch history.

They SHOULD avoid writing repository state entirely. Lock acquisition itself may open the persistent `.cvc/lock` file as required by Section 2; this is not a repository-state mutation.
