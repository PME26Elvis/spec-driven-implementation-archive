# Acceptance Test Matrix

## 1. Purpose

The submission MUST include automated tests covering the behaviors below. The evaluator may also execute independent black-box tests.

Tests must exercise the real production implementation. Mock repositories, hard-coded command outputs, fake hashes, or bypass-only test code do not satisfy a case.

## 2. Test Categories

### A. Build and Smoke

- A01: clean build from submitted sources.
- A02: `cvc --help` exits 0 and lists required commands.
- A03: unknown command fails nonzero.
- A04: missing repository error is handled.
- A05: help lists `resolve` plus merge continue/abort forms.
- A06: `log --max-count` accepts canonical positive decimal and rejects zero/sign/leading-zero/nonnumeric/overflow forms.
- A07: invoking repository commands from a nested ordinary subdirectory still scans/operates on the entire discovered repository root.
- A08: duplicate singleton CLI options are rejected rather than silently applying first/last-wins behavior.
- A09: branch/path list output uses the required deterministic unsigned-byte ordering.
- A10: Unicode command-line arguments (Chinese/emoji message or path) enter through the wide Windows boundary and round-trip to canonical UTF-8 without ANSI-code-page dependence.
- A11: normal product execution does not require Cygwin/MSYS/WSL or invoke a shell to implement required behavior.
- A12: redirected/piped stdout and stderr containing Chinese/emoji repository-visible text are UTF-8 bytes independent of the active Windows console code page.

### B. Initialization

- B01: init creates repository metadata.
- B02: second init refuses overwrite.
- B03: default branch is `main`.
- B04: empty repository status/log do not crash.
- B05: save on an empty unborn repository creates no commit.
- B06: init creates exact v1 HEAD/unborn-main ref layout and persistent zero-length lock file.
- B07: detected partial init failure does not leave a false-valid repository.

### C. JSON Parser

- C01: valid minimal config.
- C02: valid full config.
- C03: nested arrays/objects.
- C04: escaped string characters.
- C05: BMP Unicode escape.
- C06: surrogate pair to UTF-8.
- C07: unpaired surrogate rejection.
- C08: invalid UTF-8 rejection.
- C09: duplicate key rejection, including escape-equivalent spellings such as `"a"` and `"\u0061"`.
- C10: trailing comma rejection.
- C11: comments rejection.
- C12: trailing garbage rejection.
- C13: numeric overflow rejection.
- C14: unknown schema key rejection at top level and nested section level.
- C15: wrong schema type rejection.
- C16: UTF-8 BOM rejection.
- C17: `\u0000` is parsed length-safely and rejected semantically in a pattern/key context rather than truncated.
- C18: `format_version: 1.0` and `1e0` are rejected semantically.
- C19: `cvc config show` and `cvc config validate` operate on the real handwritten parser/schema; malformed config fails validation and existing-repository commands, while `init` itself creates the initial config without requiring one beforehand.
- C20: malformed JSON-number spellings are rejected lexically, not partially consumed.

### D. SHA-256 and Object Store

- D01: standard SHA-256 empty-string vector.
- D02: standard `abc` vector.
- D03: multi-block vector.
- D04: incremental chunk-boundary equivalence.
- D05: identical file bytes deduplicate.
- D06: existing valid object reused.
- D07: corrupt existing object at expected ID causes failure.
- D08: tree order deterministic under different creation orders.
- D09: commit serialization deterministic with `CVC_TEST_TIMESTAMP`.
- D10: fixed canonical blob envelope produces the specified known object IDs.
- D11: canonical tree binary encoding/order is independently decoded/checked by tests rather than only round-tripped through production code.
- D12: malformed/noncanonical tree ordering or envelope encoding is rejected by verify.
- D13: canonical empty-tree and root-commit vectors produce the fixed IDs specified by the repository model.
- D14: malformed `CVC_TEST_TIMESTAMP` forms are rejected on operations that would create a commit, without ref movement.
- D15: canonical Windows `symlink 6\0target` object produces the specified fixed ID.
- D16: independently encoded one-entry file-link (`0x02`) and directory-link (`0x04`) trees produce the two specified fixed IDs and differ only by the required entry-type byte.

### E. Eligibility and Scanning

- E01: ordinary text tracked.
- E02: empty file tracked.
- E03: NUL at first byte ignored.
- E04: NUL at inspected final byte ignored.
- E05: NUL immediately after inspection window does not trigger binary rule.
- E06: size-eligibility boundary logic is tested without requiring a committed large-file fixture; a sparse/truncated temporary file or direct filesystem-size boundary test is acceptable.
- E07: a non-symlink reparse point such as an NTFS junction is ignored and not traversed.
- E08: nested repository boundary not traversed.
- E09: hidden ordinary file tracked.
- E10: Chinese filename tracked.
- E11: deep directory tree.
- E12: several thousand small files.
- E13: empty directories alone are not versioned.
- E14: an eligible file containing NUL just after the 8192-byte probe round-trips without truncation.
- E15: an unpaired-UTF-16 native filename (where constructible by a low-level test helper) or another unsupported Windows component is skipped with warning/ignored accounting and is never truncated/normalized into a tracked path.
- E16: a path whose native absolute spelling exceeds legacy `MAX_PATH` still saves/statuses/restores correctly through the extended-length path logic.
- E17: Windows hidden attribute alone does not exclude an otherwise eligible ordinary file; alternate data streams are not versioned.

### F. Windows Symbolic Links and Reparse Points

- F01: relative file symbolic link save/restore preserves PrintName.
- F02: absolute file symbolic link save/restore preserves PrintName.
- F03: dangling file symbolic link is versioned/restored without target access.
- F04: directory symbolic link is not traversed during scan.
- F05: symbolic-link loop does not recurse.
- F06: PrintName target modification is detected.
- F07: regular-file <-> file-symbolic-link type change.
- F08: reparse/symbolic-link collision safety prevents path escape during materialization.
- F09: dangling directory symbolic link round-trips using the committed directory-link kind without opening the target.
- F10: file-symbolic-link <-> directory-symbolic-link kind change is a type change and is represented by tree entry `0x02` versus `0x04`.
- F11: a junction/mount-point/other reparse tag is ignored and never substituted for a supported symbolic link.
- F12: if native symbolic-link creation is denied, the materializing command fails/rolls back and never writes a regular-file/junction substitute.

### G. Save/Status

- G01: first save root commit.
- G02: no-op save creates no commit.
- G03: add/modify/delete status categories.
- G04: type-changed category.
- G05: ignored summary.
- G06: save stores complete selected snapshot.
- G07: tracked-to-binary transition removes tracked entry but leaves physical file.
- G08: filter removal removes path from snapshot but leaves physical file.
- G09: long/UTF-8 commit message.
- G10: `save.show_diffstat=false` suppresses save diffstat while preserving the exact saved snapshot.
- G11: empty commit message is rejected where `-m` is required.

### H. Glob/Filtering

- H01: `*` does not cross `/`.
- H02: `?` matches one non-separator byte.
- H03: `**` crosses directories.
- H04: `**/*.md` includes root-level Markdown.
- H05: include then exclude precedence.
- H06: save command-line list replaces diffstat config list without changing saved tree.
- H07: malformed comma list rejected.
- H08: empty repository tracking include array selects no tracked paths; empty diffstat include array suppresses stats only; `--include=` is rejected.
- H09: diffstat filter does not affect saved tree.
- H10: Chinese exact-path pattern.
- H11: a pattern containing three-or-more consecutive stars is rejected.
- H12: `save --include/--exclude` never changes snapshot membership; only repository `tracking` config can do so.
- H13: `status`/`diff` command-local display filters do not inherit `diffstat` config when omitted and never alter tracking membership.
- H14: CLI comma-list empty elements are rejected and whitespace remains literal pattern data.
- H15: glob matching remains case-sensitive on canonical UTF-8 bytes even on a case-insensitive Windows namespace.

### I. Diff/Myers

- I01: identical files => zero edits.
- I02: add line at beginning.
- I03: add line at end.
- I04: delete middle lines.
- I05: replacement.
- I06: repeated-line ambiguity still yields valid shortest script.
- I07: no-final-newline distinction.
- I08: CRLF byte distinction.
- I09: Chinese/emoji line content.
- I10: edit script reconstructs new bytes.
- I11: save diffstat totals match file edits.
- I12: newly added/deleted file line counts.
- I13: eligible content containing NUL after the 8192-byte probe is diffed length-safely without C-string truncation.
- I14: diff of eligible content containing malformed UTF-8/control/NUL-after-probe bytes uses the required byte-safe renderer; redirected output is valid UTF-8 and represents the original differing bytes unambiguously.

### J. Branch and Switch

- J01: create/list branches.
- J02: create branch before first commit.
- J03: switch between divergent snapshots.
- J04: current branch marker.
- J05: duplicate branch creation fails.
- J06: delete current branch fails.
- J07: any selected dirty working-tree change, including an added eligible path, blocks an actual branch switch.
- J08: filtered-out path collision protection.
- J09: ineligible-file collision protection.
- J10: unrelated filtered-out/ineligible path is preserved, including an excluded descendant inside a directory container also needed by the target snapshot.
- J11: branch ref namespace prefix collision is rejected deterministically, while an empty stale intermediate ref directory does not permanently block valid later branch creation.
- J11b: a branch name beginning with `-` is rejected.
- J12: switching to the current branch is a no-op that preserves dirty local changes.
- J13: deleting a noncurrent branch with commits unreachable from every other branch succeeds only with the required warning; objects/history are not physically deleted.
- J14: invalid branch names (including malformed Unicode input where constructible, control bytes after UTF-8 conversion, `.`/`..`, `..` substring, bad slash forms, trailing dot/space, backslash, forbidden Win32 characters, reserved DOS device basenames including the Windows superscript-digit COM/LPT forms, reserved `HEAD`, and overlength names) are rejected.
- J15: branch namespace rejects ordinal case-insensitive collisions (for example `Feature` versus `feature`) while branch command lookup requires the exact stored spelling.
- J16: switching between commits that differ only by canonical filename case safely performs the case-only materialization without losing content.
- J17: an untracked native sibling whose spelling differs only by Windows ordinal case from a target tracked path is treated as a collision rather than silently overwritten; the explicit tracked case-only transition remains allowed.

### K. Revision Resolution and Restore

- K01: full commit ID.
- K02: branch revision.
- K03: unique >=8-char prefix.
- K04: ambiguous prefix rejection.
- K05: restore regular file.
- K06: restore Windows symbolic link with both target and file/directory link kind.
- K07: recursive directory restore.
- K08: absent revision path fails safely.
- K09: recursive directory restore removes current-HEAD tracked descendants absent from source while preserving unrelated untracked descendants.
- K10: restore collision/failure leaves the requested subtree unchanged.
- K11: a valid unreachable commit object can be resolved by full ID/unique prefix; a corrupt canonical-path candidate causes integrity failure instead of being ignored.
- K12: root-relative path operands remain root-relative from subdirectories and a tracked operand beginning with `-` is accepted in the required positional path slot.
- K13: native absolute/drive paths, backslashes, empty/control-containing operands, `.`/`..` segments, and Windows-invalid/reserved components are rejected.

### L. Merge

- L01: self merge no-op.
- L02: already-up-to-date ancestor case.
- L03: fast-forward.
- L04: divergent different-file clean merge.
- L05: non-overlapping same-file edits merge.
- L06: identical overlapping edit/same-boundary identical insertion merges once.
- L07: overlapping nonidentical edits and different same-boundary insertions produce conflict markers/state.
- L07b: boundary insertion versus adjacent replacement composes in the fixed before/after order.
- L08: modify/delete conflict.
- L09: add/add different-content conflict.
- L10: regular-file/symbolic-link conflict and incompatible file-link/directory-link kind conflict.
- L11: dirty-tree precondition.
- L12: `save` and branch/switch/rollback mutations remain blocked for the entire active merge state, including after all conflicts are marked resolved but before continue/abort.
- L13: `resolve` + `merge --continue` creates two-parent commit from provisional tree plus explicit resolutions.
- L14: unrelated edit to a nonconflicting provisional merge path blocks continue rather than leaking into commit.
- L15: `merge --abort` restores pre-merge tracked tree.
- L16: merge-base traversal through an earlier merge commit.
- L17: two branches add different files under the same new directory and merge recursively without a directory-level false conflict.
- L18: editing a resolved conflict path after `resolve` blocks continue until re-resolved.
- L19: out-of-band movement/corruption of the recorded current ref during merge state is detected without ref clobber.
- L20: unborn target/current branch merge semantics.
- L21: automatic merge result that becomes ineligible is a conflict and never enters a committed tree.
- L22: finalizing merge state is retryable when ref movement did not occur; resolve/restore/new-merge mutations are blocked in that phase. If the intended ref already moved, read-only commands recognize logical completion without mutating state and the next mutating command safely cleans stale state before proceeding.
- L23: finalizing retry reuses the exact recorded merge commit regardless of changed clock/test timestamp; retry `-m` is rejected, and retry after ref-already-moved returns successful completed-finalization without rewriting history.
- L24: when multiple best common ancestors exist, merge-base selection follows the fixed lexicographically-smallest commit-ID rule.
- L25: structural conflicts materialize only the specified ours representation/absence and do not invent auxiliary side files.
- L26: `resolve` accepts only an exact recorded conflict root; re-running it replaces the recorded resolution with the current root state.
- L27: safety-critical merge/conflict status remains visible even when status display filters would otherwise hide that path.
- L28: aborting a retryable finalizing merge restores pre-merge tracked state; abort after the intended ref already moved never moves successful history backward and only cleans completed state.
- L29: a merge whose otherwise-composed directory would contain two distinct canonical names colliding under Windows ordinal case-insensitive semantics fails before materialization/merge-state creation and leaves history/working tree unchanged.
- L30: two independently committed born branches with no common ancestor are rejected as unrelated histories before any working-tree/ref/state mutation.
- L31: after a ref-update failure leaves retryable `finalizing` state, manual modification/collision inside the frozen merge-controlled working-tree projection blocks `merge --continue` ref movement until that projection again matches the intended merge result or the merge is aborted; an unrelated noncolliding untracked/ineligible file remains irrelevant.
- L32: out-of-band movement/deletion of the target branch ref after conflict state begins does not retarget the merge; continue still uses the pinned target commit, while movement of the recorded current HEAD/ref remains an integrity failure.

### M. Rollback

- M01: rollback ancestor tree.
- M02: rollback cross-branch commit tree.
- M03: rollback creates new commit.
- M04: rollback parent is pre-rollback HEAD, not target.
- M05: old history remains reachable through parents.
- M06: dirty working tree blocks rollback.
- M07: collision blocks rollback without ref movement.
- M08: explicit rollback still creates a new single-parent commit when the target tree equals the current tree.
- M09: rollback on an unborn current branch fails without creating history.

### N. Verification and Corruption

- N01: healthy repository verifies.
- N02: edited blob object detected by hash mismatch.
- N03: missing blob detected.
- N04: missing tree detected.
- N05: malformed tree entry detected.
- N06: malformed commit detected.
- N07: bad branch ref detected.
- N08: bad HEAD detected.
- N09: unknown repository format rejected.
- N10: unreachable valid object does not fail required reachable verification.
- N11: any reparse-point substitution (symlink/junction/etc.) of a required metadata/object/ref path is rejected rather than followed.
- N12: verify rejects a committed tree that references an ineligible blob, a malformed/non-UTF-8/NUL-containing symlink object, a bad link entry type, a Windows-invalid component, or an ordinal case-insensitive sibling collision.
- N13: a malformed/hash-invalid object at a canonical loose-object pathname fails `verify` even when unreachable; an unreachable valid object remains acceptable.
- N14: object references whose actual object type does not match the tree/commit expectation are rejected.
- N15: an otherwise-unreachable canonical commit/tree with a missing or wrong-type referenced object fails `verify`.
- N16: uppercase/noncanonical loose-object pathname spelling and reparse-point fan-out/object paths are rejected even though Windows lookup could otherwise alias/follow them.
- N17: a canonical tree containing a component whose UTF-16 length exceeds the repository volume's reported maximum component length is rejected as unmaterializable on the mandatory host profile.
- N18: `verify` rejects an externally introduced branch-ref namespace collision (ordinal case-insensitive duplicate or file/directory prefix conflict) even when each individual ref file's contents are otherwise syntactically valid.

### O. Locking and Failure Safety

- O01: held Win32 `LockFileEx` exclusive lock on byte range [0,1) of `.cvc/lock` blocks a second writer.
- O02: a read-only repository command fails cleanly as repository-busy while the exclusive lock is held; multiple nonexclusive `LockFileEx` readers may coexist.
- O03: failed object write leaves old branch ref unchanged.
- O04: failed ref update does not make partial object appear committed.
- O05: orphan temporary object is not treated as valid object.
- O06: Windows ACL/read-only/sharing/materialization failure restores pre-command tracked state and does not falsely report successful switch/rollback/recursive restore.
- O07: simulated failure of HEAD/ref update after successful materialization restores the old tracked working tree.
- O08: restore participates in writer serialization.
- O09: replacing a tracked regular file that is hard-linked to an outside/unrelated path does not mutate bytes at the other hard-link path.
- O10: reader/writer serialization uses the specified Win32 byte range [0,1), competing processes can open the shared lock file, and `.cvc/lock` remains zero bytes.
- O11: `FlushFileBuffers` + same-volume object/ref publication ordering is exercised; ref publication never precedes durable installation of referenced objects.
- O12: holding a destination file open with incompatible Windows sharing causes a clean rollback/no-ref-movement failure rather than in-place truncation.

## 3. Test Quality

Tests MUST be deterministic except where explicitly validating filesystem behavior.

Tests SHOULD create isolated temporary repositories and clean them afterward.

Tests MUST NOT rely on a developer's preexisting home-directory configuration.

## 4. Required Test Evidence

Submission MUST provide a command or script that runs the complete automated test suite and returns nonzero if any mandatory test fails.

The final test summary MUST include:

- number of tests run;
- passed;
- failed;
- skipped.

Mandatory acceptance behaviors MUST NOT be silently skipped.
