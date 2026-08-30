# 08 — Required Edge Cases and Behavioral Clarifications

## 1. Empty Repository

After `init` and before first save:

- `branch` shows current `main` as unborn;
- `log` reports no commits without crashing;
- `status` treats selected eligible files as added;
- first `save` creates a zero-parent root commit.

Saving an entirely empty selected working tree before any commit MUST report nothing to save and MUST NOT create an empty root commit. The first commit is created only when the selected snapshot differs from the implicit empty snapshot.

## 2. Empty Directories and Empty Files

Empty working-tree directories are not versioned and by themselves do not make status dirty or cause a save. Directories required to hold tracked descendants are recreated as needed during materialization. An empty root tree may exist in a non-root commit that records deletion of all previously tracked paths.

### 2.1 Empty Files

Empty regular files are eligible text files.

Adding/deleting an empty file changes tree state but contributes zero insertion/deletion lines.

## 3. Final Newline

These files are distinct:

```text
abc\n
```

and

```text
abc
```

Diff output must make the difference observable.

## 4. CRLF

CRLF bytes are preserved exactly.

Changing LF to CRLF is a content change.

## 5. UTF-8 Content

Eligible regular files may contain Chinese, emoji, combining characters, and other UTF-8 data as long as no NUL occurs in the binary-test prefix and size is within limit.

Diff is line-based and byte-exact; it is not required to calculate grapheme-level changes.

## 6. UTF-8 Filenames

Valid filesystem UTF-8 names including Chinese MUST be versionable.

CVC MUST preserve exact filename bytes and display them without intentional mangling.

## 7. Invalid Filename Encoding

Filesystems may permit non-UTF-8 filename bytes or control characters in names. In v1, a path component with invalid UTF-8 or an ASCII control byte (`0x01`-`0x1f`, `0x7f`) is not versionable: scanners MUST skip the entry without opening/dereferencing it as content, emit a warning, and include it in the ignored summary. If a previously tracked valid path disappears and an unsupported spelling appears, normal selected-snapshot semantics treat the old tracked path as absent.

All stored tree path components created by conforming v1 writers therefore satisfy these display-safe UTF-8 constraints in addition to structural path rules. Dotfiles such as `.env` remain valid.

## 8. Hidden Files

Dotfiles are ordinary paths except `.cvc` at repository root and nested repository boundaries.

A file such as `.env` is tracked by default if otherwise eligible.

## 9. Permissions and Executable Bit

File permission metadata and executable-bit tracking are out of scope.

Snapshots track content/type/path only.

Restored regular files may use a documented default/create-mode subject to umask.

## 10. Timestamps and Filesystem Metadata

mtime, ctime, inode number, owner, group, ACLs, xattrs, and hard-link identity are not versioned.

CVC MUST compare actual content/object identity rather than trusting mtime alone as proof that a tracked file is unchanged.

Metadata caching MAY be used only as an optimization with a correctness-preserving fallback.

## 11. Hard Links

Hard-linked regular files are treated as independent repository paths.

If bytes are identical they naturally share one content-addressed blob object.

Hard-link relationship itself is not restored. Replacing one tracked hard-linked path during restore/switch/rollback/merge MUST not mutate bytes visible through another hard-link pathname; the replaced repository path becomes a new inode as needed.

## 12. Symlink Cases

Required cases:

- relative symlink;
- absolute symlink;
- dangling symlink;
- symlink to directory;
- symlink loop;
- changed link target;
- symlink replaced by regular file;
- regular file replaced by symlink.

No target dereference is allowed.

## 13. Large Files

A regular file of exactly 8,388,608 bytes is not excluded by size alone.

A regular file of 8,388,609 bytes is ineligible.

Acceptance tests need not create very large payloads beyond boundary verification where practical; sparse-file techniques MAY be used by tests.

The task does not require performance tuning for large-file handling.

## 14. Binary Heuristic

NUL at byte 0 => ineligible.

NUL at byte 8191 => ineligible.

NUL at byte 8192 with no earlier NUL => does not trigger the NUL-prefix rule because only the first 8192 bytes are inspected. Such a file remains eligible, and CVC MUST preserve/hash/diff its later NUL byte without C-string truncation. Human diff output may escape non-printable bytes, but the stored/reconstructed bytes must remain exact.

No claim is made that this recognizes all binary formats.

## 15. Tracked-to-Ineligible Transition

If committed `a.txt` becomes >8 MiB or gains a NUL within its inspected prefix:

- status reports the tracked path as deleted from the versionable snapshot and also counts the ineligible working-tree file in the ignored summary;
- save creates a commit without that tracked path;
- the physical ineligible file remains in the working tree.

Later switch/rollback to a commit that tracks `a.txt` MUST treat the existing ineligible file as an untracked collision and refuse to overwrite it.

## 16. Tracking Configuration Changes

If a path tracked by the current commit becomes excluded by repository `tracking.exclude`, or ceases to match repository `tracking.include`, the next save removes that path from the new snapshot while leaving the working-tree file untouched.

This behavior applies only to repository tracking configuration. CLI `--include` / `--exclude` options never cause this removal because they are presentation filters.

## 17. Empty Tracking Include Set

A configured `tracking.include` empty array selects no ordinary working-tree paths.

On a nonempty current commit, saving under that repository configuration records deletion of previously tracked paths that are no longer selected.

CLI comma-list syntax does not represent an empty pattern list: `--include=` is invalid. An empty `diffstat.include` array in JSON suppresses save diffstat file entries while leaving snapshot membership unchanged.

## 18. Deep Trees

CVC MUST handle nested directory trees without fixed tiny depth limits.

Recursive implementation is allowed, but acceptance may include at least 64 nested directories. Stack exhaustion from an unnecessarily small static limit is nonconforming.

## 19. Many Files

Implementation MUST not encode a fixed small repository path limit such as 100 or 1000 entries.

Acceptance may use several thousand small files.

Exact performance targets are not mandated, but obviously quadratic full-repository behavior where avoidable SHOULD be avoided.

## 20. Path Length

CVC MUST dynamically handle paths beyond small fixed buffers such as 256 bytes, subject to operating-system filesystem limits.

Unsafe fixed-size path concatenation is prohibited.

## 21. Special Files

FIFO/socket/device entries are ignored and never opened as regular file content.

Scanning MUST NOT block waiting on FIFO data.

## 22. Nested `.cvc`

A nested repository is an opaque boundary.

The parent repository does not track the nested `.cvc` directory nor ordinary files inside that nested repository root.

## 23. Case Sensitivity

Repository path identity follows the host filesystem behavior for access, but internal object/tree names preserve exact bytes.

Acceptance runs on a case-sensitive filesystem.

## 24. Commit Message Edge Cases

Required:

- spaces;
- Chinese text;
- emoji;
- long message of at least 4 KiB.

A zero-length message is rejected by commands requiring `-m`.

## 25. Branch Name Edge Cases

Required tests include:

- `feature`;
- `fix/ui`;
- UTF-8 branch name;
- duplicate branch create;
- invalid `..`;
- invalid leading slash;
- delete current branch;
- ref namespace prefix collision (`topic` versus `topic/ui`) is rejected when the conflicting branch already exists.

## 26. Unique Revision Prefixes

At least 8 hex characters required for abbreviated IDs.

A prefix that resolves to multiple commits is rejected.

A hex-looking branch name is resolved as a branch name first when an exact branch match exists; otherwise commit prefix resolution applies.

## 27. Merge Edge Cases

Acceptance includes:

- merge self;
- target ancestor;
- fast-forward;
- independent files changed on two branches;
- both branches add different descendants under the same newly created directory and the directory merges recursively;
- non-overlapping edits in same file;
- identical same-region edit;
- conflicting same-region edit;
- modify/delete conflict;
- add/add conflict;
- file/symlink type conflict;
- abort;
- resolve + continue;
- editing a conflict path after `resolve` makes continue fail until it is resolved again;
- target/current ref out-of-band movement while merge state exists is detected and not overwritten;
- unborn-target and unborn-current merge cases;
- automatic text merge whose composed bytes become ineligible is converted to a conflict rather than committed;
- prior merge commit in history.

## 28. Directory Collision Semantics

A directory outside the selected snapshot (for example because its contents are tracking-excluded) may coexist with a target structural directory when none of its descendants collide. For example, with `dir/keep.txt` excluded by tracking policy, switching to a branch that tracks `dir/a.txt` MUST NOT fail merely because directory `dir/` already exists; `keep.txt` is preserved. Conversely, such an untracked/filtered directory at a path where the target needs a regular file/symlink is a collision and MUST NOT be recursively deleted.

## 29. Restore Subtree Edge Cases

Required behavior includes:

- recursive restore removes paths tracked by current `HEAD` beneath the operand when absent from the source revision;
- unrelated untracked descendants that do not collide are preserved;
- an untracked/ineligible collision with a source path aborts the requested restore without partial requested-subtree changes;
- source revision tracking is not filtered by current `tracking` config.

## 30. Object Deduplication

Two paths with identical bytes in one commit share the same blob ID.

The same file unchanged across 100 commits MUST not create 100 distinct blob objects.

## 31. No-Op Operations

No-op save, already-up-to-date merge, and self-merge MUST NOT create commits.

## 32. Interrupted Temporary Files

Orphan temporary files left by an interrupted write MUST NOT be interpreted as objects or refs.

A later normal command may ignore or clean them according to documented policy.
