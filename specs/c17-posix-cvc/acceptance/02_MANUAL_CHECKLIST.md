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

## E. Symlink

- [ ] A symlink is saved as a symlink, not target contents.
- [ ] Dangling symlink works.
- [ ] Switching/restoring does not follow a hostile symlink outside repository.

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

## J. Integrity

- [ ] `cvc verify` succeeds on healthy repository.
- [ ] Manually corrupting a referenced object makes verify fail.
- [ ] Removing a referenced object makes verify fail.
- [ ] A malformed object deliberately placed at a canonical loose-object pathname makes verify fail even if unreachable.
- [ ] Corruption is not silently repaired or ignored.

## K. Implementation Authenticity

- [ ] Production code is C17.
- [ ] JSON parser is handwritten in project source.
- [ ] SHA-256 is handwritten in project source.
- [ ] Diff algorithm is handwritten in project source.
- [ ] Merge algorithm is implemented in project source.
- [ ] No Git/diff/rsync/jq/etc. subprocess is used to fake required functionality.
- [ ] No third-party runtime library or POSIX `glob`/`fnmatch` substitute implements the core requirements.
- [ ] Loose objects follow the fixed v1 canonical envelope/tree/commit serialization.

## L. Delivery

- [ ] Clean-source build instructions are present.
- [ ] Complete automated test command is present.
- [ ] Test suite runs against real `cvc` implementation.
- [ ] No mandatory test is skipped.
- [ ] Known limitations do not contradict mandatory requirements.
