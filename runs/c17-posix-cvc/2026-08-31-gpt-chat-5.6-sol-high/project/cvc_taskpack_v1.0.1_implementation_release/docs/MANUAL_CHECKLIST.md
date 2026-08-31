# Manual Acceptance Checklist Review

The task-pack manual checklist was reviewed after the automated acceptance run. Items below are marked complete only where the implementation was exercised by the automated suite, direct unit tests, supplemental filesystem tests, or release audit. The automated cases intentionally use real temporary repositories and the production `cvc` executable.

## A. Basic Repository

- [x] `cvc init` creates `.cvc` and current branch `main`.
- [x] Re-running init refuses to overwrite repository.
- [x] A text file can be saved with a message.
- [x] `cvc log` shows that commit.
- [x] Editing the file appears as modified in `status`.
- [x] A no-op save does not create history.
- [x] Running status/save from an ordinary nested subdirectory addresses the full repository working tree.
- [x] Empty directories alone are not versioned.

## B. Diff and Stats

- [x] Adding/removing lines produces stable `+/-` diff output.
- [x] Save prints insertion/deletion totals by default.
- [x] `--no-diffstat` and `save.show_diffstat=false` suppress stats without changing the saved tree.
- [x] `diffstat` config filters change statistics only, not stored content.

## C. Filters

- [x] `save --include=src/**,docs/**` limits displayed diffstat paths without changing saved content.
- [x] `save --exclude=**/*.log` suppresses matching diffstat paths only.
- [x] Repository `tracking.include` / `tracking.exclude` control snapshot membership.
- [x] Exclude wins over include within each filter pair.
- [x] `.cvc` is never tracked.

## D. Binary/Large Ignore

- [x] Small regular file with NUL in the first 8192 bytes is not tracked.
- [x] The strictly-greater-than-8 MiB exclusion boundary is tested without a permanently bundled large fixture.
- [x] No binary diff/merge feature is substituted for the required text eligibility rules.

## E. Symlink

- [x] A symlink is saved as link-target bytes rather than target contents.
- [x] Dangling symlink is versionable.
- [x] Switching/restoring does not follow a hostile symlink outside the repository; supplemental structural-symlink restore test also verifies the outside directory stays unchanged.

## F. Branches

- [x] Branch creation/listing works.
- [x] Branch switch changes tracked working-tree content correctly.
- [x] Any selected dirty change prevents an actual branch switch.
- [x] Filtered-out/ineligible collision prevents overwrite while unrelated excluded paths are preserved.

## G. Merge

- [x] Fast-forward merge works without a redundant merge commit.
- [x] Independent branch edits merge cleanly.
- [x] Non-overlapping same-file edits merge cleanly.
- [x] Conflicting same-file edits produce visible conflict markers.
- [x] `status` reports unresolved conflicts.
- [x] `save` is blocked during conflict.
- [x] Resolve + continue creates a two-parent merge commit without including unrelated edits.
- [x] Editing a previously resolved conflict makes continue refuse until re-resolved.
- [x] Different additions under the same new directory merge recursively rather than falsely conflicting on the directory.
- [x] Abort restores pre-merge tracked state.

## H. Rollback

- [x] Rollback reproduces the target snapshot.
- [x] Rollback creates a new commit.
- [x] Previous commits remain in history.

## I. JSON Config

- [x] Valid JSON config is accepted.
- [x] Duplicate decoded keys are rejected.
- [x] Comments/trailing commas are rejected.
- [x] Unicode escape and surrogate-pair handling works.
- [x] Unknown config keys fail instead of being silently ignored.

## J. Integrity

- [x] `cvc verify` succeeds on a healthy repository.
- [x] Corrupting a referenced object makes verify fail.
- [x] Removing a referenced object makes verify fail.
- [x] A malformed object at a canonical loose-object pathname makes verify fail even when unreachable.
- [x] Corruption is not silently repaired or ignored.

## K. Implementation Authenticity

- [x] Production code is C17.
- [x] JSON parser is handwritten in project source.
- [x] SHA-256 is handwritten in project source.
- [x] Myers diff is handwritten in project source.
- [x] Merge algorithm/state machine is implemented in project source.
- [x] Production source contains no Git/diff/rsync/jq/etc. subprocess implementation.
- [x] Production executable has no third-party runtime implementation dependency and does not use POSIX `glob`/`fnmatch`/high-level walkers for core behavior.
- [x] Loose objects follow the fixed v1 canonical envelope/tree/commit serialization and fixed vectors.

## L. Delivery

- [x] Clean-source build instructions are in `README.md`.
- [x] Complete automated test command (`make test`) is documented.
- [x] Test suite invokes the real `cvc` executable.
- [x] No mandatory test is skipped or expected-fail.
- [x] Listed limitations are only v1 out-of-scope features and do not contradict mandatory requirements.
