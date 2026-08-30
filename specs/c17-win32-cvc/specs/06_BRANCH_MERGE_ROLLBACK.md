# 06 — Branch, Merge, and Rollback Semantics

## 1. Commit Graph

The commit graph is a directed acyclic graph where edges point from a commit to its parent commit(s).

Ordinary commits have one parent except the root commit.

Non-fast-forward merge commits have exactly two parents:

1. first parent = pre-merge current branch tip (`ours`);
2. second parent = merged branch tip (`theirs`).

## 2. Branch Creation and Switching

Creating a branch copies the current commit reference state, including unborn state.

Switching branches does not create a commit.

Before modifying the working tree, CVC MUST determine whether tracked local changes or untracked collisions would be lost.

No partial switch is permitted.

## 3. Merge Preconditions

If any non-completed merge state is active, every new `cvc merge <branch>` form fails and the user must continue/abort that merge first. Otherwise, self-merge is detected before dirty-tree checks and is a no-op that does not modify the working tree, even if the working tree is dirty. For a distinct target branch with no active merge state, `cvc merge <branch>` requires:

- valid current repository;
- target branch exists;
- no existing merge state, whether its conflicts are unresolved or already marked resolved;
- no tracked working-tree changes relative to `HEAD`;
- no filtered-out/ineligible collision with any path that the fast-forward target or computed merge/provisional result would need to materialize.

Any selected added/modified/deleted/type-changed path relative to current `HEAD` counts as dirty for this precondition. While merge state exists, branch create/delete/switch, save, rollback, and a new merge are forbidden.

Failure of a precondition leaves repository history and working tree unchanged.

### 3.1 Unborn branch cases

- the self-merge no-op above also applies when current branch is unborn;
- if the target branch is unborn and current branch is born, report no commits to merge and perform no history/worktree change;
- if both distinct branches are unborn, report no commits to merge and perform no change;
- if current branch is unborn and target branch is born, treat the operation as a fast-forward from the implicit empty history, subject to ordinary collision safety. The current branch becomes born at the target tip.

## 4. Already-Up-To-Date Case

If target tip is equal to current tip, or target tip is an ancestor of current tip, merge reports already up to date and creates no commit.

## 5. Fast-Forward Case

If current tip is an ancestor of target tip:

- no merge commit is created;
- current branch ref advances to target tip;
- working tree is safely materialized to target tree;
- second branch remains unchanged.

## 6. Merge Base

For divergent history, CVC MUST find a common ancestor to use as merge base.

The implementation MUST correctly handle the graph created by this product, including prior merge commits.

If both branch tips are born but have **no common ancestor**, v1 treats them as unrelated histories. `merge` MUST fail nonzero before creating merge state, materializing any path, or moving any ref. CVC does not synthesize an implicit empty merge base and has no `--allow-unrelated-histories` option.

A **best common ancestor** is a common ancestor that is not itself an ancestor of another common ancestor candidate. If exactly one exists, use it. If multiple best common ancestors exist, v1 selects the candidate with the lexicographically smallest 64-character lowercase hexadecimal commit ID. Acceptance tests avoid cases that would require recursive virtual merge-base synthesis, but this tie rule is still mandatory and traversal order MUST NOT affect it.

A breadth/depth graph search with visited sets is acceptable.

## 7. Three-Way Tree Merge

Tree merge is recursive by child name. It MUST NOT treat two directory object IDs as an immediate add/add conflict merely because their descendant trees differ.

At each child name, let `B`, `O`, and `T` be the base, ours, and theirs entry, where an entry may be absent.

The first identity rules are:

- `O == T` => use that entry (including both absent);
- `O == B` => use `T`;
- `T == B` => use `O`.

After those rules, neither side is simply unchanged from base.

### 7.1 Both sides are directories

If both `O` and `T` are subtrees, recursively merge their children. If `B` is absent, recurse using an empty base tree. If `B` is also a subtree, use it as the base. If `B` is a non-directory entry, this is an incompatible type-change conflict at the current path.

This rule means two branches may independently add different files under the same newly created directory and merge cleanly.

### 7.2 Regular files and Windows symbolic links

- path added on only one side => use the added entry;
- same path added identically on both sides => use it;
- same path added differently as non-directory entries on both sides => conflict;
- deleted on one side and unchanged on the other => deletion wins via the identity rules above;
- deleted on both => deleted;
- deleted on one side and modified on the other => conflict;
- supported Windows symbolic link changed differently on both sides (target object or file-link/directory-link kind) => conflict;
- when base/ours/theirs are all regular-file blobs and both sides changed, invoke the required textual three-way merge.

### 7.3 Type changes

After the identity rules, incompatible types at the same path are a structural conflict. Examples include regular file vs symbolic link, regular file vs directory, symbolic link vs structural directory, and file-symbolic-link vs directory-symbolic-link when both sides require incompatible kinds.

A structural conflict is recorded at the minimal path whose entry types/absence cannot be reconciled. Descendants beneath that conflict root are not separately reported as independent conflicts unless they belong to a different non-overlapping conflict root.

### 7.4 Windows case-collision result

After recursively composing a directory's children, the tentative child set MUST be checked for the Windows ordinal case-insensitive sibling-collision rule. Two distinct canonical names such as `Readme.txt` and `README.TXT` cannot both enter one valid Windows tree.

If a merge would create such a collision from otherwise individually valid branch trees, the merge MUST fail **before any merge-state or working-tree materialization begins**, leave the current ref/tree unchanged, and report a platform namespace collision naming the conflicting canonical paths. This is not an ordinary resolvable merge conflict in v1 because a root-level collision may have no nonempty single conflict path that `cvc resolve <path>` can represent. The user must rename/remove the colliding path on a branch and retry the merge.

## 8. Text Three-Way Merge

When base, ours, and theirs are all eligible regular text files and both sides modified the path, CVC MUST attempt a handwritten three-way textual merge. The implementation MAY derive each side's edits from its Myers edit script, but the merge decision rules below are fixed.

Treat each side as a set/sequence of edits against **base-line coordinates**. A non-insertion edit consumes a half-open base interval `[start,end)` and supplies replacement lines (possibly empty for deletion). An insertion consumes no base line and is located at one base boundary coordinate. Implementations may internally represent edits differently, but observable conflict/clean behavior must be equivalent to these rules.

Required composition rules:

1. An edit present identically on both sides — same base interval/boundary and byte-identical replacement lines — is applied exactly once.
2. Two nonzero-width edits whose consumed base intervals overlap and are not identical are a conflict. Adjacent intervals where one ends exactly where the other starts do not overlap and are applied in base order.
3. Two insertions at the same base boundary merge once if their inserted line bytes are identical; different insertions at that same boundary are a conflict.
4. An insertion strictly inside the base interval consumed by the other side's nonzero-width edit is a conflict.
5. An insertion exactly at the start boundary of the other side's nonzero-width edit is composed **before** that edit. An insertion exactly at the end boundary is composed **after** that edit.
6. All other edits operating on disjoint base regions compose automatically in increasing base-coordinate order.

Thus independently edited non-overlapping regions merge automatically, identical overlapping edits merge once, and incompatible overlap is never silently ordered by branch traversal or hash value. Changes at the beginning/end of the file and zero-line/empty-file boundaries obey the same coordinate rules.

If repeated identical base lines permit multiple shortest Myers scripts, the implementation MUST use a deterministic internal tie policy. Acceptance merge cases that assert clean-versus-conflict behavior will use sufficiently anchored content so that an alternative valid shortest alignment does not change the intended classification.

Before an automatically merged regular-file result is accepted into the provisional/final tree, the result bytes MUST satisfy the same v1 eligibility rule as a saved working-tree file (size at most 8 MiB and no NUL in the first 8192 bytes). If automatic composition would make the result ineligible, CVC records a conflict at that path instead of dropping it or committing an ineligible blob; the working tree retains/materializes ours for that conflict and the user may choose/delete/edit to an eligible resolution.

## 9. Conflict Markers

For a **textual-content conflict** where both sides provide regular-file content (including different add/add files and overlapping-edit conflicts), the working tree MUST contain a conflict-marked file using recognizable markers containing ours and theirs, for example:

```text
<<<<<<< ours
...
=======
...
>>>>>>> theirs
```

Exact marker labels may include branch names or commit IDs.

Conflict markers themselves are working-tree data and MUST NOT be automatically committed.

For structural/type/delete-modify conflicts where conflict markers are not the defined representation, the working tree MUST retain/materialize the **ours** representation at that conflict root (or absence if ours is absent), while merge state stores base/ours/theirs identities needed for safe resolution. The same ours-materialization rule applies to the special conflict raised because an otherwise clean automatic text-merge result would become ineligible under the 8 MiB/NUL-prefix rule. CVC MUST NOT create auxiliary `ours`/`theirs` side files for required conflict handling. This makes `cvc resolve <path>` without further edits deterministically choose ours for these conflicts and keeps the observable working tree consistent across implementations.

## 10. Merge Conflict State

When any conflict occurs:

- no merge commit is created;
- current branch ref remains at pre-merge `HEAD`;
- merge state records the original current branch name/commit, target branch name/commit, intended merge message, conflict paths, and a deterministic **provisional merged tree** for every nonconflicting path;
- successfully auto-merged nonconflicting paths are written to the working tree as the provisional merge result;
- `status` MUST report that a merge is in progress, identify the recorded target branch/commit, distinguish unresolved conflict paths, and when any are resolved show resolved versus unresolved counts;
- ordinary `save`, branch create/delete/switch, rollback, and another merge MUST refuse while the merge is active and not logically completed; completed stale-finalizing behavior follows Section 11.

The state also records the current branch name. `resolve`, normal conflict-phase `merge --continue`, and `merge --abort` MUST verify that `HEAD` still names that branch and its ref still equals the recorded original pre-merge commit. Manual/ref corruption or out-of-band movement of the **current** HEAD/ref causes a safe failure rather than clobbering the changed ref.

The target commit is pinned by object ID in merge state. Later out-of-band movement or deletion of the target branch ref MUST NOT silently retarget an already-started merge; continue/abort still use the recorded target commit as long as that commit/object graph remains valid. The recorded target branch name is descriptive state, not a live ref that is re-resolved during continue.

## 11. Conflict Resolution

The user resolves conflicts by editing/removing/replacing conflicted working-tree paths.

Required completion command:

```text
cvc merge --continue [-m <message>]
```

This command MUST be supported even though the basic merge syntax uses a branch name.

Before continuing from normal conflict/resolution state, CVC MUST require every conflict path to be explicitly marked resolved. `resolve` and ordinary `restore` are not valid once merge state has entered the `finalizing` phase; while a retryable finalizing state still points at the original ref, only `merge --continue`, `merge --abort`, and read-only commands are permitted.

Required resolution command:

```text
cvc resolve <path>
```

`cvc resolve` records that the user accepts the current working-tree state of that exact conflict root. It MUST serialize or otherwise record the chosen resolved entry at resolve time so the final merge tree is not inferred from unrelated later working-tree changes.

A resolution may be:

- absence, meaning delete the conflict root;
- one eligible regular file;
- one supported Windows symbolic link, stored without dereference and with its file-link/directory-link kind;
- a directory, in which case `resolve` snapshots the current eligible regular-file/supported-symbolic-link descendants beneath that conflict root recursively as the explicit chosen resolution. Built-in safety exclusions still apply, but repository tracking include/exclude filters are not re-applied inside this explicitly resolved subtree. Empty directories remain unversioned.

A resolution to an ineligible regular file at any captured path MUST be rejected because it cannot enter a repository tree. Special files and nested repositories beneath a directory resolution are excluded by built-in rules rather than followed/opened.

Before `merge --continue`, CVC MUST verify both:

1. every nonconflicting tracked path still matches the stored provisional merge result; and
2. every resolved conflict root still matches the exact resolution recorded by its most recent `cvc resolve`.

If a resolved path was edited after `resolve`, continue fails and instructs the user to run `cvc resolve <path>` again (or abort). This ensures a successful merge commit and working tree agree exactly. Unexpected edits to nonconflicting provisional paths likewise block continue rather than leaking into the commit. Untracked paths that do not collide are irrelevant.

`merge --continue` then creates a two-parent merge commit using:

- first parent = original pre-merge `HEAD`;
- second parent = recorded target commit;
- tree = stored provisional nonconflicting result plus each explicitly resolved conflict entry.

The current repository tracking filters are not re-applied to remove paths from this merge result.

If the original `merge <branch>` supplied `-m`, that message is stored in merge state. Otherwise the deterministic default message is stored. `merge --continue -m <message>` overrides the stored message; without `-m`, the stored message is used.

The merge commit object and any newly resolved objects MUST be durable before the current branch ref is moved. Before attempting that ref movement, merge state MUST durably record a `finalizing` phase plus the exact intended merge-commit ID.

Finalization rules are:

- if ref movement fails, the ref remains at the recorded original commit and merge state/resolution working tree are preserved so `merge --continue` can be retried; the already-durable intended merge-commit object remains recorded and unreachable;
- if the ref reaches the intended merge commit, merge is logically complete and the finalizing state is removed;
- if interruption or cleanup failure leaves finalizing state behind, every later repository command MUST first confirm `HEAD` still names the recorded current branch, then compare that branch ref: if it equals the original commit, preserve the merge as retryable; if it equals the intended merge commit, treat the merge as logically complete; any other HEAD/ref state is an integrity error and MUST NOT be overwritten. A later **mutating** command holding the exclusive repository lock MUST remove the stale completed finalizing state before performing its requested mutation. Read-only commands MUST NOT mutate the state merely to clean it; they continue using the intended commit as current history and MUST report that completed merge-state cleanup is pending.

A retry of `merge --continue` while `finalizing` and still at the original ref MUST reuse the exact already-recorded intended merge commit; it does not choose a new timestamp, rebuild with current environment time, or accept a replacement `-m`. Supplying `-m` in this retry phase is an error. If that intended commit object is missing or invalid, retry fails as repository corruption rather than silently generating a different commit.

Before retrying ref movement, CVC MUST also verify that the **frozen merge-controlled working-tree projection** still exactly matches the state captured immediately before entering `finalizing`: every leaf present in the intended merge tree has the same type/content, and every tracked/conflict path intentionally absent from that intended tree remains absent. This projection covers the pre-merge tracked namespace, provisional merge paths, conflict roots/resolved descendants, and every path materialized/deleted by the merge; it does **not** absorb unrelated untracked/ineligible paths that were never merge-controlled and do not collide with an intended path. Manual edits, removals, replacements, or new collisions inside the frozen merge-controlled projection after the first finalization attempt therefore block retry. CVC MUST leave the ref/state untouched and tell the user to restore that projection to the frozen resolution or abort. It MUST NOT overwrite such edits automatically merely to make retry succeed.

If `merge --continue` encounters stale completed finalizing state whose ref already equals the intended commit, it performs the permitted exclusive-lock cleanup and reports successful/already-completed finalization with exit status 0. `merge --abort` in that same completed state cleans stale state but MUST NOT move the ref backward and then reports that there is no active merge to abort with a **nonzero** exit status. An ordinary `merge --abort` invocation when no merge state exists likewise fails nonzero.

CVC MUST NOT move an already-successful merge ref backward merely because state cleanup failed.

## 12. Merge Abort

Required command:

```text
cvc merge --abort
```

It restores the exact tracked working-tree snapshot from pre-merge `HEAD`, removes merge state, and leaves branch refs unchanged.

Untracked/ineligible files not involved in collision handling MUST remain untouched.

Abort MUST refuse rather than overwrite a newly created untracked collision that prevents safe restoration.

## 13. Rollback

`cvc rollback <revision> -m <message>` is history-preserving.

The current branch MUST be born. Rollback on an unborn current branch fails because there is no pre-rollback `HEAD` commit to preserve as the new commit's parent.

Let:

- `H` = current HEAD commit;
- `T` = target revision.

On success create commit `R` such that:

- `R.parent[0] = H`;
- `R.tree = T.tree`;
- `R.message = provided message`.

`T` is not a parent merely because its tree was selected.

This distinguishes rollback from branch reset and preserves a linear audit trail on the current branch.

## 14. Rollback Safety

Before rollback materializes the target tree:

- the current working-tree snapshot selected by repository tracking policy MUST equal current `HEAD` (no selected added/modified/deleted/type-changed paths);
- merge state MUST be completely absent;
- untracked/ineligible collision checks MUST pass.

If any check fails, no commit/ref update occurs.

## 15. Root and Cross-Branch Rollback

Rollback may target:

- an ancestor;
- a commit reachable only through another branch;
- a root commit;
- a merge commit.

Only the target tree matters for resulting content. Repository tracking filters are not applied to remove paths from `T.tree`.

## 16. No History Rewriting Commands

The following concepts are outside v1 and MUST NOT be required:

- reset;
- rebase;
- amend;
- force branch movement to arbitrary commits.
