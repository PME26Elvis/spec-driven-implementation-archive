# Manual Acceptance Checklist

This checklist is intentionally simple enough for human comparison across independent implementations.

An evaluator may use it after automated tests.

## A. Basic Repository

- [ ] `cvc init` creates `.cvc` and current branch `main`.
- [ ] Re-running init refuses to overwrite repository.
- [ ] A text file can be saved with a message.
- [ ] `cvc log` shows that commit.
- [ ] Editing the file appears as modified in `status`.
- [ ] A no-op save does not create history.
- [ ] Running status/save from an ordinary nested subdirectory still addresses the full repository working tree.
- [ ] Empty directories alone are not versioned.

## B. Diff and Stats

- [ ] Adding/removing lines produces plausible `+/-` diff output.
- [ ] Save prints insertion/deletion totals by default.
- [ ] `--no-diffstat` and `save.show_diffstat=false` suppress those stats without changing the saved tree.
- [ ] `diffstat` config filters change statistics only, not stored content.

## C. Filters

- [ ] `save --include=src/**,docs/**` limits displayed diffstat paths without changing saved content.
- [ ] `save --exclude=**/*.log` suppresses matching paths from diffstat only.
- [ ] Repository `tracking.include` / `tracking.exclude` control snapshot membership.
- [ ] Exclude wins over include within each filter pair.
- [ ] `.cvc` is never tracked.

## D. Binary/Large Ignore

- [ ] Small file with NUL in first 8192 bytes is not tracked.
- [ ] The >8 MiB exclusion rule is implemented; no dedicated large binary fixture is required in the submission.
- [ ] No binary diff/merge facility is required or presented as implemented.

## E. Windows Symbolic Links / Reparse Points

- [ ] A Windows symbolic link is saved as a link, not target contents.
- [ ] File-link and directory-link kinds are preserved; a dangling directory link can be restored without opening its target.
- [ ] Switching/restoring does not follow a hostile symbolic link or junction outside repository.
- [ ] A junction/unsupported reparse point is ignored and not traversed.
- [ ] If Windows denies link creation, CVC fails safely rather than substituting a copied target, junction, or regular file.

## F. Branches

- [ ] Branch creation/listing works.
- [ ] Branch switch changes tracked working-tree content correctly.
- [ ] Any selected dirty change prevents an actual branch switch.
- [ ] Filtered-out/ineligible collision prevents overwrite, while unrelated excluded paths are preserved.

## G. Merge

- [ ] Fast-forward merge works without redundant merge commit.
- [ ] Independent branch edits merge cleanly.
- [ ] Non-overlapping same-file edits merge cleanly.
- [ ] Conflicting same-file edits produce visible conflict markers.
- [ ] `status` reports unresolved conflicts.
- [ ] `save` is blocked during conflict.
- [ ] Resolve + continue creates a two-parent merge commit without accidentally including unrelated edits.
- [ ] Editing a previously resolved conflict makes continue refuse until the path is resolved again.
- [ ] Two branches adding different files under the same new directory merge recursively rather than falsely conflicting at the directory.
- [ ] Abort restores pre-merge tracked state.
- [ ] A merge that would create `Readme.txt` + `README.TXT` as siblings fails before working-tree/history mutation.

## H. Rollback

- [ ] Rollback reproduces the target snapshot.
- [ ] Rollback creates a new commit.
- [ ] Previous commits remain in history.

## I. JSON Config

- [ ] Valid JSON config is accepted.
- [ ] Duplicate keys are rejected.
- [ ] Comments/trailing commas are rejected.
- [ ] Unicode escape handling works.
- [ ] Unknown config keys fail instead of being silently ignored.

## J. Windows Path / Unicode Behavior

- [ ] Chinese/emoji command-line/path input round-trips through the wide Windows boundary without ANSI-code-page corruption.
- [ ] Redirected/piped output containing repository-visible Unicode is UTF-8 and does not depend on the active console code page.
- [ ] A valid path beyond legacy 260-character `MAX_PATH` works.
- [ ] Case-only filename rename is observable and can be materialized safely.
- [ ] Case-insensitive sibling/branch namespace collisions are rejected deterministically.
- [ ] Win32-reserved path components/device names, including superscript-digit COM/LPT aliases, are rejected rather than normalized into another path.

## K. Integrity

- [ ] `cvc verify` succeeds on healthy repository.
- [ ] Manually corrupting a referenced object makes verify fail.
- [ ] Removing a referenced object makes verify fail.
- [ ] A malformed object deliberately placed at a canonical loose-object pathname makes verify fail even if unreachable.
- [ ] Corruption is not silently repaired or ignored.
- [ ] `verify` rejects branch-ref namespace states that CVC branch creation itself forbids (case-insensitive duplicate or ref-prefix file/directory collision).

## L. Implementation Authenticity

- [ ] Production code is C17.
- [ ] JSON parser is handwritten in project source.
- [ ] SHA-256 is handwritten in project source.
- [ ] Diff algorithm is handwritten in project source.
- [ ] Merge algorithm is implemented in project source.
- [ ] No Git/diff/rsync/jq/etc. subprocess is used to fake required functionality.
- [ ] No third-party runtime library, POSIX compatibility runtime, Win32 wildcard helper, or equivalent library substitute implements the core requirements.
- [ ] Native filesystem access uses wide-character Win32 semantics; required UTF-8/UTF-16 conversion is handwritten C rather than ANSI/OEM code-page conversion.
- [ ] Repository serialization uses file-link `0x02` and directory-link `0x04` tree entries exactly as specified.
- [ ] Loose objects follow the fixed v1 canonical envelope/tree/commit serialization.

## M. Delivery

- [ ] Clean-source build instructions are present.
- [ ] Complete automated test command is present.
- [ ] Test suite runs against real `cvc` implementation.
- [ ] No mandatory test is skipped.
- [ ] Known limitations do not contradict mandatory requirements.

- [ ] Windows pathname safety does not treat an exposed 8.3 alternate short name as a second tracked path or silently overwrite through such an alias.
