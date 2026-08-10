# Human Acceptance Checklist

Use the final files under `evidence/screenshots/`; do not accept mockups or an image whose digest differs from `evidence/manifest.json`. All images should be 1440×900 captures of the actual X11 application.

## Visual Checkpoints

- [ ] `UI-EMPTY-LIGHT`: Light start surface, required actions, recents area, coherent spacing.
- [ ] `UI-EMPTY-DARK`: same surface in Dark theme without clipped/low-contrast text.
- [ ] `UI-WORKSPACE-MULTITAB`: nested file tree, several stable-size tabs, active/dirty/overflow states.
- [ ] `UI-SOURCE`: line numbers, UTF-8 source, editor caret/selection styling, custom chrome.
- [ ] `UI-SPLIT`: source and preview both usable with a visible draggable divider.
- [ ] `UI-PREVIEW`: read-only rendered document without editable table controls.
- [ ] `UI-RENDERED-EDIT`: source-backed rendered editing state and toolbar.
- [ ] `UI-MARKDOWN-ALL`: headings, paragraph inline styles, lists/tasks, quotes, code, table, raw text, malformed tail.
- [ ] `UI-IMAGE-SELECTED`: actual decoded image, selection outline and resize handle.
- [ ] `UI-IMAGE-RESIZE`: larger aspect-preserving gesture state, not a fabricated image.
- [ ] `UI-TABLE-EDIT`: actual GFM table grid/cell focus and table affordance.
- [ ] `UI-OUTLINE`: hierarchy/indentation and duplicate heading identity.
- [ ] `UI-COMMAND-PALETTE`: focused query and filtered enabled/disabled commands.
- [ ] `UI-STATISTICS`: complete statistics fields in blocking custom modal.
- [ ] `UI-VERSION-HISTORY`: real version records and actions.
- [ ] `UI-DIFF-SIDE-BY-SIDE`: aligned additions/deletions and change navigation.
- [ ] `UI-DIFF-INLINE`: inline/token refinement distinct from side-by-side.
- [ ] `UI-MODAL-BLUR`: content behind the modal is visibly CPU-blurred and dimmed.
- [ ] `UI-FROSTED-SCROLLED`: scrolled navigation treatment differs from top state.
- [ ] `UI-EXTERNAL-CONFLICT`: clear choices, both-version safety wording, ordinary save not implied safe.
- [ ] `UI-RECOVERY-CENTER`: trustworthy record identity and open/compare/discard/cancel actions.
- [ ] `UI-ERROR-SAVE`: operation/file, data-retained statement, Retry/Save As/Cancel recovery.

## Animation Sequence

- [ ] `UI-FROSTED-TOP` and `UI-FROSTED-SCROLLED` show two states of the same feature.
- [ ] `UI-BUTTON-HOVER`, `UI-BUTTON-PRESS-RIPPLE`, `UI-BUTTON-RELEASE` form a coherent pointer-origin state sequence.
- [ ] `UI-MODAL-OPEN-START`, `UI-MODAL-BLUR`, `UI-MODAL-CLOSE`, `UI-MODAL-END` form a coherent open/close sequence with no stale overlay.

## Functional Spot Check

- [ ] Switch Source→Split→Preview→Rendered Edit→Source; source bytes do not change.
- [ ] In Rendered Edit select H2, choose Set Heading Level 4, Undo once; source returns exactly.
- [ ] Table Tab/Shift+Tab moves cell focus; row/column/alignment actions Undo once.
- [ ] Select an image, resize continuously, release, Undo once; original aspect/size returns.
- [ ] Copy Traditional Chinese/English to another X11 client and paste the reverse direction.
- [ ] Change a file externally and verify Reload/Keep/Compare/Overwrite safety branches.
- [ ] Kill after a completed autosave interval and recover exact source bytes from Recovery Center.

Record reviewer name/date and any mismatch separately. `evidencecheck PASS` is not a substitute for these observations.
