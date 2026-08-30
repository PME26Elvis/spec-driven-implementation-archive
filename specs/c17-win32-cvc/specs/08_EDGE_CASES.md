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

Eligible regular files may contain Chinese, emoji, combining characters, and other UTF-8 data as long as no NUL occurs in the binary-test prefix and size is within limit. Eligibility does not require the complete file to be valid UTF-8: malformed UTF-8 bytes and NUL bytes after the 8192-byte probe remain ordinary blob bytes.

Diff is line-based and byte-exact; it is not required to calculate grapheme-level changes. Human diff output uses the byte-safe renderer from `05_DIFF_AND_FILTERING.md` so redirected output remains valid UTF-8 without corrupting arbitrary blob bytes.

## 6. UTF-8 Filenames and Native UTF-16

Windows directory enumeration yields UTF-16 names. Any valid UTF-16 filename that converts to a canonical Windows-safe UTF-8 component, including Chinese and emoji, MUST be versionable when otherwise eligible.

CVC MUST preserve exact canonical UTF-8 spelling in repository objects/output and MUST convert back to the same Unicode scalar sequence for native Win32 calls. Unicode normalization is not performed.

## 7. Invalid or Unsupported Filename Encoding

A native filename containing an unpaired UTF-16 surrogate is not representable in CVC's canonical UTF-8 path model. The scanner MUST skip that entry/subtree without opening it as tracked content, emit a warning, and include it in the ignored summary.

Likewise, a native component violating the Windows-safe repository-component rules (for example reserved DOS device spelling where constructible through lower-level means, trailing dot/space ambiguity, or a forbidden/control character) is not versionable. CVC MUST never truncate, normalize, or rename such an entry into a different tracked path.

If a previously tracked valid path disappears and an unsupported native spelling appears, ordinary selected-snapshot semantics treat the old tracked path as absent.

## 8. Hidden Files and Windows Attributes

A dot-prefixed filename such as `.env` is an ordinary path except for the reserved `.cvc` metadata name. Windows `FILE_ATTRIBUTE_HIDDEN` by itself does not exclude an otherwise ordinary file; hidden/system/archive attributes are not versioned.

## 9. ACLs, Read-Only State, and Executable Metadata

Windows ACL/security-descriptor metadata, DOS read-only/hidden/system/archive attributes, and any Unix-like executable-bit concept are out of scope. Snapshots track content/type/path only.

If native ACL, sharing, or read-only state prevents a required write/delete/rename, the command follows the detected-failure rollback rules and returns nonzero. It MUST NOT report a partially applied materialization as success.

## 10. Timestamps and Filesystem Metadata

Creation/access/write timestamps, NTFS file IDs, owner/security descriptor, attributes, compression/encryption/sparse metadata, short names, alternate data streams, and hard-link identity are not versioned.

CVC MUST compare actual content/object identity rather than trusting timestamp metadata alone as proof that a tracked file is unchanged. Metadata caching MAY be used only as an optimization with a correctness-preserving fallback.

## 11. Hard Links

NTFS hard-linked regular files are treated as independent repository paths. If their unnamed-stream bytes are identical they naturally share one content-addressed blob object.

Hard-link relationship itself is not restored. Replacing one tracked hard-linked path during restore/switch/rollback/merge MUST not mutate bytes visible through another hard-link pathname; the replaced repository pathname receives an independently installed file entry as needed.

## 12. Windows Symbolic-Link and Reparse-Point Cases

Required supported-symbolic-link cases:

- relative file symbolic link;
- absolute file symbolic link;
- dangling file symbolic link;
- relative directory symbolic link;
- dangling directory symbolic link, proving stored link kind is sufficient without target access;
- symbolic link to directory is not traversed during scan;
- symbolic-link loop does not recurse;
- changed PrintName target;
- file-symbolic-link <-> directory-symbolic-link kind change;
- symbolic link replaced by regular file and reverse type change.

No target dereference is allowed.

A junction or other non-symlink reparse point is ignored and MUST NOT be traversed as a directory. Repository metadata redirected through any reparse point is an integrity error rather than an ignored working-tree entry.

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

CVC MUST dynamically handle paths beyond small fixed buffers and MUST NOT impose the legacy Win32 `MAX_PATH` 260-character limit on otherwise valid paths. Acceptance may construct a repository entry whose native absolute spelling exceeds 260 UTF-16 code units while remaining within ordinary extended-length NTFS limits.

Unsafe fixed-size path concatenation is prohibited. If the host filesystem rejects a genuinely over-limit path, CVC fails safely rather than truncating it.

## 21. Unsupported Reparse Points and Native Special Cases

A working-tree entry with `FILE_ATTRIBUTE_REPARSE_POINT` and a tag other than `IO_REPARSE_TAG_SYMLINK` is ignored and never opened/traversed as ordinary tracked content. This includes junctions and mount points.

NTFS alternate data streams are not enumerated/versioned; only the unnamed data stream of an ordinary regular file participates in eligibility, hashing, diff, and restoration.

## 22. Nested `.cvc`

A nested real non-reparse directory that aliases the reserved `.cvc` name is an opaque repository boundary. The parent repository does not track that nested metadata directory or ordinary files inside the nested repository root.

A reparse point named `.cvc` is not a valid nested repository boundary to follow; it is a safety-sensitive unsupported entry and MUST NOT be traversed.

## 23. Windows Case Semantics

Canonical tree names preserve exact UTF-8 bytes and unsigned-byte sort order, but sibling names that collide under the Windows ordinal case-insensitive rule are forbidden.

Required behaviors include:

- `Readme.txt` and `README.TXT` cannot coexist in one committed tree;
- a case-only rename is represented as old-path deletion plus new-path addition;
- switch/restore/materialization across a case-only spelling change succeeds without losing bytes or unrelated entries;
- glob matching remains case-sensitive on canonical UTF-8 bytes;
- branch creation rejects wrong-case namespace collisions while command lookup requires exact stored branch spelling.
- an NTFS 8.3 alternate name, when present, is not tracked as a second canonical path and MUST NOT be allowed to alias an intended materialization/ref target to some different native entry.

Acceptance uses ordinary Windows/NTFS semantics and may also deliberately construct a case-sensitive directory containing colliding spellings to ensure CVC rejects the selected snapshot deterministically rather than choosing an enumeration winner.

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
- ref namespace prefix collision (`topic` versus `topic/ui`) is rejected when the conflicting branch already exists;
- Windows-reserved DOS device component (for example `CON` or `topic/COM1.txt`) is rejected;
- a component ending in dot/space or containing a forbidden Win32 character/backslash is rejected;
- case-insensitive ref namespace collision (for example existing `Feature` then create `feature`, or existing `topic/UI` then create `TOPIC/ui`) is rejected while exact stored spelling is preserved;
- wrong-case branch lookup does not succeed merely because native NTFS lookup is case-insensitive.

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
- regular-file/symbolic-link type conflict, including incompatible file-link/directory-link kinds;
- abort;
- resolve + continue;
- editing a conflict path after `resolve` makes continue fail until it is resolved again;
- out-of-band movement/corruption of the recorded **current** HEAD/ref while merge state exists is detected and not overwritten; movement/deletion of the target branch ref does not retarget the merge because the target commit is pinned in state;
- unborn-target and unborn-current merge cases;
- two independently committed born branches with no common ancestor are unrelated histories and merge is rejected before mutation;
- automatic text merge whose composed bytes become ineligible is converted to a conflict rather than committed;
- a retryable finalizing merge whose frozen merge-controlled working-tree projection was manually edited/collided refuses to move the ref until that projection again matches the frozen intended result or the merge is aborted; unrelated noncolliding untracked/ineligible paths remain irrelevant;
- prior merge commit in history;
- merge of branches that independently introduce case-colliding sibling spellings fails before mutation rather than creating an unmaterializable tree.

## 28. Directory Collision Semantics

A directory outside the selected snapshot (for example because its contents are tracking-excluded) may coexist with a target structural directory when none of its descendants collide. For example, with `dir/keep.txt` excluded by tracking policy, switching to a branch that tracks `dir/a.txt` MUST NOT fail merely because directory `dir/` already exists; `keep.txt` is preserved. Conversely, such an untracked/filtered directory at a path where the target needs a regular file/supported symbolic link is a collision and MUST NOT be recursively deleted.

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

## 32. Interrupted Temporary Files and Windows Sharing

Orphan temporary files left by an interrupted write MUST NOT be interpreted as objects or refs.

A later normal command may ignore or clean them according to documented policy. A destination held open by another process with incompatible Windows sharing is a detected failure and MUST trigger the same rollback/no-ref-movement safety semantics rather than an in-place truncation workaround.
