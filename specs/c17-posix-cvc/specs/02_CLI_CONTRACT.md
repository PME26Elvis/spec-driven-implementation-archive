# 02 — CLI Contract

## 1. General Rules

Invocation form:

```text
cvc <command> [options] [arguments]
```

Unknown commands, unknown options, missing required arguments, and malformed option values MUST fail nonzero and print a useful diagnostic to stderr.

Successful ordinary commands return exit status `0`.

Expected user-correctable failures return a nonzero exit status. v1 does not mandate distinct numeric error codes; acceptance relies on zero versus nonzero plus the required state/output semantics.

Output intended for normal users goes to stdout. Errors and warnings go to stderr.

Every required `-m <message>` option uses the same rule: the argument must be nonempty and valid UTF-8; no whitespace trimming is performed. A whitespace-only but nonempty message is therefore allowed.

Required path operands (`restore` and `resolve`) are nonempty canonical repository-root-relative UTF-8 paths using `/`. Absolute paths, empty paths, control bytes, and operands containing empty, `.` or `..` segments MUST be rejected. Their meaning does not change when CVC is invoked from a subdirectory of the repository. In these grammars the required positional path slot remains a path even when its first byte is `-`; a tracked file such as `-notes.txt` therefore remains directly restorable/resolvable.

Unless a command-specific rule says otherwise, duplicate occurrences of a singleton option such as `-m`, `--include`, `--exclude`, `--max-count`, or `--no-diffstat` MUST be rejected rather than resolved by an undocumented first/last-wins rule.

Whenever a command emits a list of repository paths or branch names, the default ordering MUST be ascending lexicographic order of their unsigned UTF-8 bytes unless that command explicitly defines a history order instead. Conflict-path reporting follows the same path order. This requirement does not prescribe prose wording around the list.

For CLI blocking rules, **active merge state** means a merge whose current branch ref is still the recorded original pre-merge commit. A stale `finalizing` state whose ref already equals its recorded intended merge commit is logically completed: read-only commands may report cleanup pending without modifying it, and the next mutating command removes that stale state under the exclusive lock before dispatching the requested operation.

## 2. `cvc init`

Syntax:

```text
cvc init
```

Behavior:

- creates `.cvc/` in the current directory plus actual directories `refs/heads/`, `objects/`, and `state/`;
- creates persistent zero-length regular file `.cvc/lock`;
- creates `HEAD` with exact bytes `ref: refs/heads/main\n`;
- creates zero-length `refs/heads/main`, representing unborn default branch `main`;
- does not create an initial commit;
- creates `.cvc/config.json` containing at least `{"format_version":1}` in valid JSON form;
- MUST fail rather than overwrite any preexisting `.cvc` filesystem entry.

Expected success output includes the repository root path or a concise confirmation. If a detected initialization failure occurs after CVC created `.cvc`, it MUST remove only the metadata entries it created for that attempted initialization and leave no directory that could be mistaken for a valid initialized repository; it must never delete a preexisting `.cvc` entry.

## 3. `cvc status`

Syntax:

```text
cvc status [--include=<patterns>] [--exclude=<patterns>]
```

Shows the difference between the current branch tip and the working-tree snapshot selected by the current repository `tracking` configuration plus built-in eligibility rules. The optional CLI include/exclude arguments filter **displayed ordinary status paths only** and never change what a later save stores. When absent, status display filtering defaults to include `["**"]` and exclude `[]`; it does not inherit the `diffstat` section. During an active merge, conflict paths and the merge-in-progress warning are safety-critical and MUST remain visible even if a display filter would otherwise hide those paths.

Minimum categories:

- added;
- modified;
- deleted;
- type-changed;
- conflicted, when merge conflicts are active;
- ignored summary.

A clean tree MUST be explicitly recognizable as clean.

Status MUST NOT modify repository or working-tree content.

## 4. `cvc save`

Syntax:

```text
cvc save -m <message> [--include=<patterns>] [--exclude=<patterns>] [--no-diffstat]
```

Required behavior:

1. acquire repository writer lock;
2. reject save if any merge operation is active, whether conflicts are unresolved or already marked resolved;
3. compute the complete working-tree snapshot selected by repository tracking configuration;
4. compare it with current commit snapshot, or an empty snapshot when no commit exists;
5. if no tracked change exists, do not create a commit and report `nothing to save` or equivalent;
6. otherwise write necessary loose objects;
7. create a commit object;
8. atomically move the current branch ref to the new commit;
9. release lock.

`-m` message MUST be nonempty after argument parsing. It is stored as UTF-8 bytes without implicit trimming other than removal of command-line shell quoting performed by the shell.

### 4.1 Diffstat on save

Diffstat is enabled by default unless disabled by config or `--no-diffstat`.

When enabled, successful save output MUST include total insertions and deletions plus per-file statistics for changed eligible regular text files. For `save`, `--include` and `--exclude` apply **only to this diffstat presentation**; they MUST NOT alter the saved tree. They replace the configured diffstat include/exclude lists for that invocation.

Symlink creation/deletion/change is reported as a path change but does not contribute line insertion/deletion counts.

## 5. `cvc log`

Syntax:

```text
cvc log [--max-count=<N>]
```

Displays first-parent history starting at current `HEAD`.

For each commit, minimum fields:

- full or unambiguously abbreviated commit ID;
- commit message;
- timestamp;
- parent commit ID(s), or an explicit root indication.

Merge commits MUST expose both parent IDs.

`--max-count=<N>` requires ASCII `[1-9][0-9]*` fitting `uint64_t`; zero, a sign, leading zero, whitespace, nonnumeric data, or overflow MUST be rejected. A value larger than available history simply prints all available first-parent commits.

## 6. `cvc diff`

Syntax:

```text
cvc diff [<revision>] [--include=<patterns>] [--exclude=<patterns>]
```

Without `<revision>`, compare current commit snapshot to the working-tree snapshot selected by current repository tracking configuration and built-in eligibility rules.

With `<revision>`, compare the named commit snapshot to that same currently selected working-tree snapshot.

For `diff`, optional `--include` and `--exclude` filter displayed paths only. They MUST NOT affect repository tracking membership. When absent, diff display filtering defaults to include `["**"]` and exclude `[]`; the `diffstat` configuration is not consulted.

Output MUST identify changed paths and show line-oriented edits for eligible regular text files using a stable human-readable format.

The exact decoration may differ from unified diff, but context and `+`/`-` meaning MUST be clear.

Symlink changes MUST display old and new link-target bytes without dereferencing. The representation MUST be unambiguous and length-safe; implementations may escape control or invalid-UTF-8 target bytes rather than emitting them raw.

## 7. `cvc branch`

Branch listing remains read-only during an active merge. Branch creation and deletion MUST fail while active merge state exists; only the allowed resolution/finalization operations may change merge-related state until it is completed or aborted.

### 7.1 List

```text
cvc branch
```

Lists local branches and visibly marks the current branch.

### 7.2 Create

```text
cvc branch create <name>
```

Creates `<name>` at current commit.

Creation before the first commit is allowed; such a branch has an unborn ref state.

The new branch is not automatically switched to.

### 7.3 Delete

```text
cvc branch delete <name>
```

Rules:

- current branch MUST NOT be deleted;
- missing branch MUST fail;
- branch deletion does not delete objects;
- deletion of a branch containing commits not reachable from another branch is allowed but MUST print a warning before/with successful deletion;
- v1 has no `--force` branch-delete option. Supplying one is an unknown-option error.

## 8. `cvc switch`

Syntax:

```text
cvc switch <branch>
```

Switches to the target branch and updates tracked working-tree paths to exactly represent its snapshot, subject to collision safety. Switching to the already-current branch is a no-op: it reports that the branch is already current and MUST NOT discard or rewrite local working-tree changes.

Rules for an actual branch change:

- any active merge state forbids switching, even if every conflict has already been marked resolved;
- the current working-tree snapshot selected by repository tracking policy MUST equal current `HEAD`; any selected added/modified/deleted/type-changed path blocks switching;
- filtered-out/ineligible paths that are outside that selected snapshot but collide with a target tracked path forbid switching;
- unrelated filtered-out/ineligible paths MUST remain untouched;
- a successful switch atomically updates HEAD after working-tree safety checks and materialization succeed.

This deliberately conservative v1 rule does not carry ordinary selected local changes across branches. The same-branch no-op above is the only dirty-tree exception.

If the branch is unborn, the target tracked snapshot is empty.

For any detected failure after materialization begins, CVC MUST restore the pre-switch tracked working-tree state and leave `HEAD` naming the original branch before returning failure. Crash/power-loss recovery is governed separately by the error/recovery specification.

## 9. `cvc restore`

Syntax:

```text
cvc restore <path> --from <revision>
```

Restores one tracked path from `<revision>` into the working tree without creating a commit.

Behavior:

- regular file restores file bytes;
- symlink restores the link itself and its stored target text;
- if the source path is a directory, the requested tracked subtree is restored recursively to the source revision: paths tracked by current `HEAD` beneath that operand but absent from the source subtree are removed, while unrelated untracked/ineligible paths that do not collide are preserved;
- a path absent from the revision MUST fail and leave the requested working-tree path/subtree unchanged;
- if a source entry would overwrite a path that is not tracked by current `HEAD` (including an ineligible file, special file, or unrelated untracked directory), restoration MUST fail rather than destroy it;
- if current `HEAD` tracks the destination path (including a structural directory required by tracked descendants), `restore` is explicitly allowed to discard/replace its locally modified or type-changed working-tree representation, because restoration is the requested operation;
- tracking include/exclude configuration is not re-applied to the source revision.

`restore` changes only the working tree and never creates a commit. It is allowed during ordinary conflict/resolution merge state as a way to edit the working tree before `resolve`, but it is forbidden during retryable `finalizing` state because that phase must preserve the already-verified resolution tree. A multi-path directory restore that reports failure MUST restore the pre-command state of the requested tracked subtree.

## 10. `cvc rollback`

Syntax:

```text
cvc rollback <revision> -m <message>
```

Creates a **new commit** whose tree equals the target revision's tree.

It MUST NOT:

- move the current branch pointer backward without a new commit;
- delete historical commits;
- rewrite parent links.

The rollback commit has the pre-rollback `HEAD` as its single parent. The current branch therefore MUST already have a commit; rollback from an unborn current branch fails.

Working-tree safety rules equivalent to `switch` apply before materializing the rollback tree. Repository tracking filters are not re-applied to the target revision: the new commit tree is exactly the target commit tree.

After success, every versioned path represented by the new `HEAD` matches that snapshot exactly; unrelated filtered-out/ineligible paths may remain as allowed by collision rules. Explicit rollback creates a commit even when the target tree happens to equal the current tree.

## 11. `cvc merge`

Required forms:

```text
cvc merge <branch> [-m <message>]
cvc merge --continue [-m <message>]
cvc merge --abort
```

Full semantics are defined in `06_BRANCH_MERGE_ROLLBACK.md`.

At CLI level:

- merging a nonexistent branch fails;
- merging the current branch into itself reports no-op, including when it is unborn;
- a dirty selected working snapshot relative to `HEAD` forbids merge;
- existing merge state forbids a new `merge <branch>`;
- fast-forward and already-up-to-date cases do not create unnecessary merge commits;
- divergent clean merge creates a merge commit;
- conflicts persist merge state and return nonzero;
- `--continue` is valid with resolution-active state only after all conflicts are explicitly resolved, and is also the retry/cleanup command for `finalizing`; `-m` may override the stored message only before the transition into finalizing and MUST be rejected on a finalizing retry;
- `--abort` is valid with an active not-yet-completed merge and takes no branch or `-m` argument; a stale `finalizing` state whose ref already equals the intended merge commit is cleaned as completed before command dispatch, so abort then reports that no merge remains to abort.

If `-m` is omitted for an actual merge commit, CVC MUST generate the deterministic default message `Merge branch '<name>'` using the exact target branch name. A syntactically valid `-m` supplied to a self/no-commit, target-unborn, already-up-to-date, or fast-forward merge is accepted but has no effect because no merge commit is created. The timestamp hook likewise matters only when a commit is actually created.

## 12. `cvc resolve`

Syntax:

```text
cvc resolve <path>
```

`resolve` is valid only while merge state is in the normal conflict/resolution phase (not `finalizing`) and `<path>` is exactly one currently recorded conflict root. It records the current working-tree resolution for that conflict as defined in `06_BRANCH_MERGE_ROLLBACK.md`. A non-conflict path, ancestor/descendant spelling of a different conflict root, invocation outside a merge, or invocation during `finalizing` MUST fail. Re-running `resolve` on the same conflict path is allowed and replaces the previously accepted resolution with the path's current state.

## 13. `cvc verify`

Syntax:

```text
cvc verify
```

Performs repository integrity verification defined in `07_ERROR_RECOVERY_AND_INTEGRITY.md`.

It MUST be read-only except for temporary files that are removed before exit.

Success output MUST clearly state verification success.

Any integrity failure MUST produce nonzero status.

## 14. `cvc config`

Required forms:

```text
cvc config show
cvc config validate
```

`show` prints the effective repository configuration in a deterministic human-readable representation.

`validate` parses and validates the configured JSON and exits zero only when syntactically and semantically valid.

Editing configuration through CLI is optional; direct file editing is sufficient.

## 15. Revision Resolution

Where `<revision>` is accepted, CVC MUST resolve in this order:

1. exact local branch name;
2. exact 64-hex commit ID;
3. unique hexadecimal commit-ID prefix of at least 8 hex digits.

An unborn branch name does not resolve to a commit and MUST fail with a clear diagnostic when a commit revision is required. Full IDs may name any valid commit object physically present in the object store, even if currently unreachable. Hex revision input accepts ASCII `0-9`, `a-f`, or `A-F` and is normalized for matching against lowercase stored IDs. Prefix resolution searches all physically present valid commit objects, including unreachable commits. Corrupt candidate objects MUST cause a repository-integrity failure rather than being silently ignored to manufacture uniqueness.

Ambiguous prefixes MUST fail and list or describe ambiguity without choosing arbitrarily.

## 16. Help

`cvc help` and `cvc --help` MUST provide concise usage for all required commands.

A command-specific help mechanism is recommended but not mandatory.
