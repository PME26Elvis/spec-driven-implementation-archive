# 07 — Testing, Acceptance, Definition of Done, and Release Gates

## 1. Principle

The assignment is complete only when mandatory functionality is implemented and demonstrated through executable tests plus human-reviewable evidence.

A visually convincing prototype with incomplete behavior is not complete.

## 2. Test Ownership

The submission MUST include its own tests and validation utilities/scripts needed to verify the product.

The task pack defines what must be validated but intentionally does not prescribe which test runner, shell orchestration, screenshot command, or implementation workflow must be used.

## 3. Required Test Layers

The final submission MUST contain at least:

- Unit tests for pure logic.
- Integration tests for subsystem boundaries.
- End-to-end application tests for critical user flows.
- Deterministic fixtures/corpus files.
- Negative/error-path tests.
- Regression tests for fixed defects discovered during implementation.

## 4. Minimum Unit-Test Areas

Unit tests MUST cover at least:

- UTF-8 decoding/encoding utilities used by editing logic.
- Cursor/text-position mapping.
- Selection range operations.
- Undo/redo core.
- Markdown lexer/parser structures.
- Required Markdown constructs.
- Render model generation.
- Statistics calculations.
- Mixed Chinese/Latin word-count rules.
- Search matching.
- Diff algorithm.
- Diff refinement/tokenization.
- Version delta encode/decode.
- Version reconstruction.
- Integrity/checksum logic used by version storage.
- Configuration parsing for Workstream A.
- Ignore/path matching for Workstream A.

## 5. Markdown Fixture Corpus

The test corpus MUST include documents containing:

- Empty file.
- One-line plain text.
- Traditional Chinese only.
- English only.
- Mixed Chinese/English/numbers.
- All heading levels.
- Nested emphasis/strong cases.
- Escaped punctuation.
- Nested lists.
- Ordered lists with multi-digit indices.
- Task lists checked and unchecked.
- Block quotes including nesting.
- Inline code with Markdown-looking punctuation.
- Fenced code with Markdown-looking punctuation.
- Links.
- Autolinks.
- Images.
- GFM table.
- Long line.
- Large multi-paragraph document.
- Incomplete/malformed Markdown states.

## 6. Rendered Editing Tests

Tests MUST verify that rendered editing changes Markdown source rather than an unrelated hidden rich-text model.

At minimum:

- Edit paragraph text in rendered mode; inspect source.
- Edit heading text; inspect source.
- Toggle task list; inspect source.
- Edit link label without destroying destination.
- Apply bold to rendered selection; inspect source.
- Undo each operation; compare exact expected source.
- Switch modes without editing; verify source stability.

## 7. Selection Drag-Move Tests

At minimum test:

- Move selected text forward within same line.
- Move selected text backward within same line.
- Move selection to another line.
- Move Chinese text.
- Move mixed styled text where supported.
- Drop into original selection; no corruption.
- Cancel drag; no content change.
- Undo move in one step.
- Redo move in one step.

## 8. Statistics Tests

Use fixed input documents with hand-computed expected counts.

Tests MUST cover:

- Raw source characters.
- Rendered/plain-text characters excluding Markdown syntax.
- Word count.
- Total lines.
- Non-empty lines.
- Paragraphs.
- Headings.
- Images.
- Links.
- Fenced code blocks.
- Selection count replacing whole-document count in status UI.

## 9. Search Tests

At minimum:

- ASCII match.
- Traditional Chinese match.
- Multiple matches.
- No match.
- Match adjacent to Markdown syntax.
- Search after document edit.
- Search after undo.

## 10. File Operation Tests

At minimum:

- New → edit → Save As.
- Open existing file.
- Modify → Save.
- Modify → close → Cancel.
- Modify → close → Discard.
- Modify → close → Save.
- Untitled modified → close → Save → choose path.
- Simulated save failure leaves dirty state.
- Overwrite prompt for Save As existing path.
- Invalid UTF-8 open error.

## 11. Image Tests

Final tests MUST include mandatory formats once format scope is resolved.

At minimum behavior tests include:

- Insert valid local image.
- Render image.
- Select image.
- Resize larger.
- Resize smaller.
- Preserve aspect ratio.
- Undo resize.
- Save/reopen preserves displayed size.
- Missing source file shows placeholder.
- Right-click context menu.
- Save Image As success.
- Save Image As overwrite confirmation.
- Save Image As failure does not alter document.
- Remove image does not delete original file.

## 12. Version History Tests

At minimum:

- Create deterministic history sequence.
- Restart/reopen and history remains.
- Reconstruct every stored version.
- Compare adjacent versions.
- Compare non-adjacent versions.
- Restore older version.
- Undo/dirty-state behavior after restore: restore is one undoable document transaction and leaves the restored content dirty until explicitly saved.
- Detect intentionally corrupted history record.
- Live document survives corrupt history error.

## 13. Diff Tests

At minimum:

- Identical documents.
- One inserted line.
- One deleted line.
- Replacement line.
- Change one word in a long line.
- Multiple separated changes.
- Chinese text modification.
- Empty → non-empty.
- Non-empty → empty.
- Large document with repeated lines.

Expected edit regions MUST be deterministic for the final algorithm/tie-break rules.

## 14. UI State Tests

Automated or semi-automated validation MUST cover functional state transitions for:

- Button default → hover → pressed → release.
- Mode capsule changes active mode.
- Modal blocks background input.
- Context menu opens and dismisses.
- Statistics dialog opens.
- History view opens.
- Diff mode switches.
- Zoom changes.
- Scroll changes frosted-navigation progress.

Tests may validate numeric/internal state in addition to screenshot evidence.

## 15. Screenshot Evidence

The final delivery MUST provide human-reviewable screenshots for the visual checkpoints named in `04_UI_UX_AND_INTERACTION.md` and the stable screenshot IDs required by this v1.0 package.

Screenshots MUST be produced from the actual application.

Mockups, design-tool exports, or manually composed replicas do not satisfy this requirement.

The task pack does not prescribe the screenshot command or capture mechanism.

## 16. Evidence Manifest

A machine-readable final evidence manifest is mandatory.

The file MUST be `evidence/manifest.json` and MUST satisfy `15_DEV_FIXTURES_AND_EVIDENCE.md`.

The final evidence run MUST also include a concise human-readable release report that states:

- Build identifier.
- Total unit/integration/E2E/performance/failure/regression tests.
- Passed/failed/skipped counts.
- Release Gate results.
- Paths to screenshot/evidence artifacts.
- Known non-mandatory limitations, if any.

`evidencecheck` passing does not replace human screenshot inspection.

## 17. No Selective Test Reporting

The final validation report MUST show the complete mandatory suite result.

A submission MUST NOT report only passing tests while omitting known failing mandatory tests.

Mandatory release tests MUST NOT be marked skipped.

If an environment prevents a mandatory test from running, the corresponding Release Gate remains failed/incomplete until the test can be executed and passed.

## 18. Required Test Layers

The final suite MUST include all of the following layers:

- Pure-logic unit tests.
- Parser/render-model tests.
- Storage/version/recovery unit tests.
- Workstream A utility tests.
- GUI subsystem integration tests.
- Filesystem/workspace integration tests.
- Windows clipboard/input/drag-drop integration tests where required.
- End-to-end product flows.
- Deterministic performance tests.
- Deterministic fault-injection/error tests.
- Regression tests for defects discovered during implementation.

A screenshot-only demo is not a test suite.

## 19. Expanded Mandatory Unit-Test Areas

In addition to earlier sections of this document, unit tests MUST cover:

- Undo transaction composition and redo invalidation.
- Find matching and wrap semantics.
- Replace All non-recursive semantics.
- Whole-word rules.
- UTF-8 editing-unit boundary helpers.
- Normative combining-mark/emoji fixture movement and deletion.
- Source↔render block mapping.
- Table row/column/alignment serialization.
- Outline extraction and duplicate-heading identity.
- Workspace recent-list de-duplication.
- Preference parsing/default recovery.
- Autosave/recovery record encode/decode/integrity.
- External-change baseline comparison.
- Safe-save/fault-injection state transitions.
- LZSS compression/decompression and malformed stream rejection.
- History retention/pruning with pinned versions.
- Fixture generator deterministic PRNG if present.
- SHA-256 known-answer vectors.
- Evidence-manifest path normalization and digest verification.

## 20. Mandatory Rendered Editing E2E Matrix

At minimum demonstrate:

1. Paragraph text edit → source update.
2. Heading text edit → source update.
3. H2→H4 structural change → one-step Undo.
4. Bold toggle on rendered selection.
5. Italic toggle on rendered selection.
6. Strikethrough toggle on rendered selection.
7. Inline-code toggle where selected text contains a backtick.
8. Link display-text edit.
9. Link destination edit.
10. Ctrl+Click external HTTP/HTTPS activation path.
11. Ordered/unordered list Enter behavior.
12. List Tab/Shift+Tab indent/outdent.
13. Task checkbox toggle.
14. Nested blockquote text edit.
15. Fenced code edit containing Markdown punctuation.
16. Code fence/info-string edit.
17. Image selection/delete/Undo.
18. Table direct cell edit.
19. Table row add/delete.
20. Table column add/delete.
21. Table four-state alignment.
22. Table Tab/Shift+Tab navigation.
23. Mode cycle Source→Split→Preview→Rendered Edit→Source without semantic data loss.

## 21. Mandatory Workspace and Tab E2E Matrix

At minimum demonstrate:

- Open/switch workspace.
- Nested tree expansion and deterministic ordering.
- Create/rename/delete filesystem entries.
- Duplicate filenames in different directories.
- Multiple tabs.
- Duplicate-path prevention.
- Dirty-tab close Save/Discard/Cancel.
- Tab pointer reorder.
- Overflow tab access.
- `Ctrl+Tab`, `Ctrl+Shift+Tab`, `Ctrl+W`.
- Session restore of order, active tab, caret, scroll, mode, zoom, split ratio, and sidebar state.
- Corrupt/missing workspace metadata without Markdown loss.
- Rename an open file and parent directory.
- Delete an open backing file and preserve in-memory content.
- Multi-dirty-document application exit with one untitled file and one injected save failure.
- Save All with partial failure while failed tabs remain dirty.

## 22. Mandatory Search, Clipboard, External-Change, and Recovery Matrix

At minimum demonstrate:

- Find Chinese text.
- Find wrap next/previous.
- Case-sensitive ASCII search.
- Case-insensitive ASCII search.
- Whole-word boundaries.
- Replace one.
- Replace All across >=100 matches and Undo once.
- Copy mixed Chinese/English to an external Windows clipboard consumer.
- Paste mixed Chinese/English from an external Windows clipboard producer.
- Cut with successful clipboard ownership.
- Selection drag-move and one-step Undo.
- Ctrl-drag copy.
- Image-file drop.
- Markdown-file drop to tab/workspace region.
- Clean external modification → Reload.
- Clean external modification → Keep Current → blocked unaware Save.
- Dirty external modification → Compare.
- Dirty external modification → explicit overwrite.
- External deletion → Save As recovery.
- Process kill after autosave → Recovery Center → recovered bytes match last completed recovery write.
- Corrupt one recovery record while another valid record remains recoverable.

## 23. Mandatory Unicode / IME Matrix

At minimum demonstrate:

- Traditional Chinese source edit.
- Traditional Chinese rendered edit.
- Windows IME composition and commit.
- IME cancellation leaves no phantom bytes.
- Undo committed composition.
- Combining-mark fixture moves/deletes as one required editing unit.
- Variation-selector emoji fixture moves/deletes as one required editing unit.
- Required ZWJ emoji fixtures move/delete without invalid UTF-8.
- Mixed Unicode Save/Reopen exact scalar-sequence preservation.
- Diff on Chinese text without byte-splitting.
- Search highlight boundaries do not split UTF-8.

## 23A. Mandatory Windows Platform Matrix

At minimum demonstrate:

- Native Windows executable launches without a browser/runtime-hosted UI substitute.
- Unicode workspace/file paths containing Traditional Chinese round-trip through Open, Save As, tree operations, recents, and Workstream A tools.
- One local absolute path longer than the legacy 260-character `MAX_PATH` limit opens, edits, saves, and verifies successfully.
- Case-equivalent paths on the normative case-insensitive NTFS volume do not create duplicate document buffers.
- New/Rename rejects representative Windows-invalid and reserved filenames without corrupting workspace state.
- Windows `CF_UNICODETEXT` copy/paste interoperates with an ordinary external Windows application.
- Windows Explorer image-file drop and Markdown-file drop perform the required actions.
- Real Windows Traditional Chinese IME composition, update, commit, cancellation, and Undo behavior pass in the custom editor.
- IME candidate/composition UI is positioned reasonably near the custom caret.
- 100%, 150%, and 200% DPI acceptance states retain correct layout, caret/selection alignment, and custom-control hit testing.
- A DPI change while the window remains open does not require restart and does not leave stale scaling geometry.
- External-process write/rename/delete scenarios remain detectable; the editor does not create an unnecessary exclusive lock that invalidates the specified conflict flows.
- Locked-target/sharing-violation Save fails non-destructively.
- Safe Save staging/flush/final replace failure paths preserve the prior target and dirty in-memory buffer.
- Directory reparse points are not recursively followed by default and cannot cause root escape/hang.
- Preferences/recents use per-user Roaming AppData and standalone recovery uses per-user Local AppData as specified.
- Evidence/fixture manifest path validation rejects drive-letter absolute, UNC, extended-namespace, rooted, traversal, and reparse escape forms.

These are mandatory Windows parity requirements; they are not optional platform polish.

## 24. Mandatory Image / Asset Matrix

Tests MUST separately validate relative assets and Base64 embedded images.

Base64 tests MUST include:

- Standard known vectors.
- Empty payload.
- All 256 byte values.
- Input lengths with remainders 0, 1, and 2 modulo 3.
- Correct `=` padding.
- Invalid alphabet rejection.
- Invalid padding rejection.
- Exact binary round trip.

Image workflows MUST include:

- PNG insertion/render.
- JPEG insertion/render.
- BMP insertion/render.
- Corrupt PNG/JPEG/BMP non-crashing placeholder.
- Relative insertion copies bytes correctly and writes a working relative path.
- Embedded insertion writes a real data URI.
- Save/reopen both forms.
- Resize persistence for both forms.
- One continuous resize gesture → one undo transaction.
- Relative→embedded conversion with Undo/Redo.
- Embedded→relative conversion with Undo/Redo.
- Asset filename collision.
- Nested Markdown path calculation.
- Whole-workspace relocation while relative images remain valid.
- Missing image placeholder and relink workflow.
- Save Image As reproduces source image bytes rather than a screenshot.

## 25. Mandatory Save As / Portable Export Matrix

Tests MUST cover:

- Save As same directory with relative images.
- Save As different directory → copy/rebase.
- Save As different directory → deliberate keep references after warning.
- Cancel relocation flow.
- Portable single-file Markdown where all supported local images become embedded.
- Reopen exported single-file Markdown after original assets are made unavailable.
- Portable Markdown+assets where embedded images are externalized.
- Remove access to original sources and verify package remains renderable.
- Inject asset read/write failure and verify export fails transactionally without changing original document.

## 26. Mandatory History / Diff Matrix

Tests MUST cover:

- Version created on changed successful Save.
- No duplicate version on unchanged Save.
- Explicit Create Version.
- Autosave does not create visible history version.
- Exact restore.
- Restore while current document dirty requires explicit resolution.
- Full snapshot boundary at <=20 version intervals.
- Delta encode/decode.
- LZSS known fixture compress/decompress.
- Uncompressed fallback when compression is larger.
- Corrupt history payload detection.
- Restart persistence.
- Retention pruning beyond 200 versions.
- 64 MiB policy logic through synthetic/smaller-threshold test injection if producing 64 MiB in every unit test is impractical.
- Pinned version survives automatic pruning.
- Identical diff.
- Insert/delete/replace line diff.
- Repeated-line deterministic Myers cases.
- Chinese text modification.
- Word/token refinement.
- Side-by-side next/previous change navigation.
- Inline diff next/previous change navigation.
- Missing historical image does not block textual diff.

## 27. Mandatory Command / Preferences / Keyboard Matrix

Tests MUST cover:

- `Ctrl+Shift+P` opens palette and focuses query.
- Palette search/execute.
- Disabled command cannot execute.
- Recent files de-duplicate and reorder.
- Recent workspaces de-duplicate and reorder.
- Missing recent entry removal.
- Clear recent.
- Light/Dark theme switch without restart.
- Theme persists.
- Font size persists.
- Line spacing persists.
- Default image mode persists.
- Autosave interval persists.
- Default editor mode persists.
- Start surface exposes mandatory actions.
- Keyboard-only New→Edit→Save As→Close flow.
- Keyboard file-tree navigation.
- Keyboard Outline navigation.
- Keyboard menu operation.
- Context menu through Shift+F10/Menu key.
- Modal focus trap and restoration.
- Shortcut Reference agrees with mandatory bindings.

## 28. Mandatory Performance Matrix

Use `fixturegen` manifests to bind measured tests to exact generated fixture content.

Required evidence:

- `medium` open-to-interactive result.
- `large` open-to-interactive result.
- `large` full preview completion result.
- `medium` typing latency sample.
- `large` typing latency sample.
- `large` Find result.
- `large` Replace All result with >=1000 matches.
- `large` Outline construction/update result.
- `stress-long-line` open/edit/save result.
- 20 repeated `medium` open/close memory-observation result.

Performance results MUST be recorded in `evidence/manifest.json` with exact fixture-manifest digest.

## 29. Mandatory Failure Matrix

The final suite MUST demonstrate the complete failure matrix from `14_PERFORMANCE_AND_FAILURE_HANDLING.md`, including:

- Read-only save failure + Save As recovery.
- Injected Windows disk-full failure.
- Injected partial write.
- Injected flush/finalization failure.
- Injected final Replace/Move failure.
- Locked-target/sharing-violation Save failure.
- Invalid UTF-8 rejection without mutation.
- Truncated workspace state.
- Corrupt history record.
- Corrupt recovery record.
- Missing/corrupt images.
- Directory reparse-point traversal boundary / cycle termination.
- >260-character Unicode local path.
- Windows-invalid/reserved filename rejection.
- Path names with spaces, Traditional Chinese, and Windows-legal punctuation.

For destructive-risk tests, original-file digest before/after MUST be recorded when applicable.

## 30. Mandatory Visual Evidence

Screenshots MUST come from the real application and MUST include at least the stable IDs in `15_DEV_FIXTURES_AND_EVIDENCE.md`:

- `UI-EMPTY-LIGHT`
- `UI-EMPTY-DARK`
- `UI-WORKSPACE-MULTITAB`
- `UI-SOURCE`
- `UI-SPLIT`
- `UI-PREVIEW`
- `UI-RENDERED-EDIT`
- `UI-MARKDOWN-ALL`
- `UI-IMAGE-SELECTED`
- `UI-IMAGE-RESIZE`
- `UI-TABLE-EDIT`
- `UI-OUTLINE`
- `UI-COMMAND-PALETTE`
- `UI-STATISTICS`
- `UI-VERSION-HISTORY`
- `UI-DIFF-SIDE-BY-SIDE`
- `UI-DIFF-INLINE`
- `UI-MODAL-BLUR`
- `UI-FROSTED-SCROLLED`
- `UI-EXTERNAL-CONFLICT`
- `UI-RECOVERY-CENTER`
- `UI-ERROR-SAVE`
- `UI-DPI-SCALED`

At least one visual evidence sequence MUST demonstrate hover→press/ripple→release for a primary button.

At least one visual evidence sequence MUST demonstrate modal scale/opacity plus progressive background dim/blur at open, fully-open, and close/end states.

At least two frosted-navigation screenshots MUST show top-of-document and scrolled states using the same document/theme/window size.

`UI-DPI-SCALED` MUST be captured at 150% or 200% Windows scaling and show enough editor/custom-control content for a reviewer to inspect scaling, text alignment, and hit-target geometry.

The capture mechanism is not prescribed.

## 31. Human Acceptance Checklist

A human reviewer MUST be able to check the release without reading implementation internals first.

The final human checklist MUST make it straightforward to answer yes/no for at least:

- App launches and is visibly custom-drawn.
- No mandatory control is visibly dead/placeholder.
- All four editor modes work on the same document.
- Markdown-all fixture renders plausibly and remains editable.
- Rendered Editing visibly changes Source Mode Markdown.
- Tables can be edited structurally.
- Images can be inserted/resized/converted/saved.
- Tabs/workspace/file tree/Outline behave coherently.
- Search/replace/clipboard work with Chinese text.
- Recovery Center restores a killed dirty session.
- External conflict is not silently overwritten.
- Version history survives restart.
- Both graphical diff modes show precise changes.
- Statistics change appropriately for selections.
- Light/Dark themes are polished and readable.
- Required hover/ripple/glow/capsule/modal/blur/frosted effects are visibly present.
- Keyboard-only core flow works.
- Large fixture remains usable.
- Save failures preserve data.
- Evidence manifest and tools validate.

## 32. Stable Release Gates

Every gate below is mandatory for v1.0.

- `RG-BUILD` — clean documented build and launch from authored source.
- `RG-CONSTRAINT` — C17/Win32 boundary; no prohibited GUI/browser/framework substitution.
- `RG-DEVTOOL-LOC` — `locscan` implementation/tests/report.
- `RG-DEVTOOL-FIXTURE` — `fixturegen` profiles/determinism/manifest/tests.
- `RG-DEVTOOL-EVIDENCE` — `evidencecheck` schema/path/digest/tests.
- `RG-EDITOR` — file operations, source editing, selection, zoom, basic shortcuts.
- `RG-UNDO` — mandatory undo/redo transaction contracts.
- `RG-SEARCH` — Find/Replace/Replace All.
- `RG-UNICODE` — UTF-8, Chinese, normative grapheme fixtures, IME.
- `RG-CLIPBOARD` — external Windows Unicode clipboard and required Explorer/internal drag/drop.
- `RG-RECOVERY` — autosave/recovery lifecycle and killed-process recovery.
- `RG-EXTERNAL` — external change/delete/conflict handling.
- `RG-MARKDOWN` — required parser/renderer constructs and malformed-input robustness.
- `RG-RENDEREDIT` — source-backed direct rendered editing.
- `RG-TABLE` — rendered table GUI/keyboard structural editing.
- `RG-OUTLINE` — live heading Outline and navigation.
- `RG-WORKSPACE` — workspace/file tree/metadata/session behavior.
- `RG-TABS` — multi-document tab lifecycle/reorder/overflow/shortcuts.
- `RG-IMAGE` — PNG/JPEG/BMP insertion/render/resize/context/error behavior.
- `RG-ASSET` — relative managed asset behavior and relocation semantics.
- `RG-BASE64` — authored Base64 codec and embedded-image handling.
- `RG-PORTABLE` — both portable Markdown export workflows.
- `RG-HISTORY` — persistent snapshot/delta/compression/retention history.
- `RG-DIFF` — deterministic algorithm and both graphical views.
- `RG-STATS` — status bar and full statistics correctness.
- `RG-COMMAND` — command model/palette consistency.
- `RG-PREFS` — recents/preferences/theme persistence.
- `RG-KEYBOARD` — keyboard-only mandatory navigation/actions/focus.
- `RG-UI` — custom visual design and mandatory motion/effects.
- `RG-WINPLATFORM` — Windows Unicode paths, IME, Explorer drop, file sharing, AppData, long-path, filename, reparse-point, and native-integration contract.
- `RG-DPI` — Windows DPI-aware layout/render/input behavior at mandatory scale factors.
- `RG-PERF` — medium/large/long-line responsiveness gates.
- `RG-ERROR` — mandatory I/O/corruption/fault-injection cases.
- `RG-EVIDENCE` — complete screenshot/test/performance/failure manifest integrity.

## 33. Definition of Done

The assignment is DONE only when all of the following are true:

1. Workstream A contains all three utilities and their mandatory tests.
2. Workstream B builds and launches from authored source.
3. Every stable Release Gate in Section 32 passes.
4. All mandatory Markdown constructs parse/render and required rendered-editing operations update source correctly.
5. All four editor modes operate on real shared document state.
6. Multi-document workspace/tab/session behavior passes.
7. Undo/redo, Find/Replace, Windows Unicode clipboard, Explorer/internal drag/drop, Unicode/IME, external conflicts, autosave, and recovery pass.
8. PNG/JPEG/BMP, relative assets, Base64 embedded images, resize, conversion, Save Image As, Save As relocation, and portable export pass.
9. Version history persists/reconstructs and obeys creation/compression/retention rules.
10. Side-by-side and inline diff pass deterministic fixtures.
11. Status bar and detailed statistics pass deterministic English/Chinese/mixed fixtures.
12. Command Palette, recents, preferences, Light/Dark themes, and keyboard-only flows pass.
13. Modern custom UI interaction/effect requirements are visibly implemented.
14. Performance gates pass on the final evidence run.
15. Windows platform parity gates, including DPI, long paths, Unicode paths, file sharing, AppData locations, reserved names, and reparse boundaries, pass.
16. Mandatory I/O/fault/corruption gates preserve user data as specified.
17. Required screenshots are present and come from the actual application.
18. `evidence/manifest.json` is complete and `evidencecheck` passes.
19. The final test summary contains zero failed mandatory tests and zero skipped mandatory release tests.
20. No mandatory feature is a placeholder, mock, hard-coded result, disabled stub, or prohibited substitute.
21. The delivered source package excludes unnecessary caches and disposable runtime artifacts.

## 34. Stop Condition

If any stable Release Gate remains failing, any mandatory test is skipped, or any mandatory feature is incomplete, the work MUST be reported as incomplete.

The implementer MAY describe remaining failures and partial progress, but MUST NOT label the assignment complete, finished, fully passing, production-ready, or v1.0-complete.

The correct implementation behavior is to continue debugging and validation until all mandatory gates pass or to report the unresolved failures explicitly.
