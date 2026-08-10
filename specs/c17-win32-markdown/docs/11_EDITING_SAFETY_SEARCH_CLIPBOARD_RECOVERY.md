# 11 — Editing Transactions, Search, Clipboard, External Changes, Recovery, and Unicode

## 1. Scope

This document freezes the v1.0 behavior for undo/redo, find/replace, clipboard and drag/drop, external file modifications, autosave/crash recovery, and Unicode/IME editing.

These behaviors are mandatory in Source Mode and, where meaningful, in Rendered Editing Mode.

## 2. Undo/Redo Model

The application MUST maintain an undo/redo history per open document.

Undo/redo MUST operate on document-edit transactions rather than on arbitrary renderer state.

The history MUST survive switching between Source, Split, Preview, and Rendered Editing modes while the document remains open.

Saving a document MUST NOT clear undo history.

Closing the document MAY discard its in-memory undo history after all save/close decisions are complete.

### 2.1 Mandatory Transaction Boundaries

Each of the following MUST be reversible as one logical undo transaction:

- One paste operation.
- One cut operation.
- One selection drag-move operation.
- One selection drag-copy operation.
- One toolbar or shortcut formatting command.
- One heading-level change.
- One list indent/outdent command.
- One task-list checkbox toggle.
- One table row insertion.
- One table row deletion.
- One table column insertion.
- One table column deletion.
- One table alignment change.
- One image insertion.
- One image removal.
- One completed image resize gesture from pointer-down to pointer-up.
- One image storage-mode conversion.
- One Replace command.
- One Replace All command.
- One version-restore command.

A single mandatory structural operation MUST NOT require multiple Undo commands to restore the exact pre-operation source.

### 2.2 Typing Coalescing

Ordinary adjacent text insertion MAY be coalesced into larger undo transactions.

If typing is coalesced, a transaction MUST end when any of the following occurs:

- Caret is moved by pointer or navigation key beyond the immediately adjacent insertion position.
- Selection changes.
- Paste/cut/formatting/structural command occurs.
- Active document changes.
- Editing mode changes.
- Focus leaves the editable document surface.
- More than 1500 ms elapses between committed text insertions.

Backspace/Delete runs MAY be coalesced using the same boundary rules.

Exact character-by-character coalescing inside those limits is implementation-defined and is not a release-gate comparison metric.

### 2.3 Redo Invalidation

After Undo, any new edit MUST invalidate redo entries that are no longer descendants of the current state.

A non-editing action such as scrolling, opening statistics, switching theme, or moving focus MUST NOT invalidate redo.

### 2.4 Mode Consistency

An edit made in Rendered Editing Mode and then undone in Source Mode MUST undo the same underlying document operation.

Undo MUST NOT depend on recreating a hidden rich-text document that differs from the Markdown source.

## 3. Find UI

The application MUST provide an in-document Find surface.

Opening Find MUST be available through `Ctrl+F` and through an application command/menu.

The Find surface MUST include:

- Search text field.
- Next match command.
- Previous match command.
- Match count or current-match position when matches exist.
- Case-sensitive toggle.
- Whole-word toggle.
- Close action.

The Find surface MUST NOT permanently obscure the document caret or prevent normal scrolling after it closes.

### 3.1 Matching Semantics

Search MUST operate on the current Markdown source text, not only on visible rendered text.

Search MUST support UTF-8 content and exact matching for Traditional Chinese.

Case-sensitive mode MUST compare Unicode scalar values exactly.

Case-insensitive mode MUST at minimum fold ASCII `A-Z` to `a-z`. Non-ASCII case folding MAY be exact rather than locale-aware; the behavior MUST be documented and deterministic.

Whole-word matching MUST use the following v1.0 boundary rule:

- ASCII letters, ASCII digits, and underscore are word characters.
- Each non-ASCII Unicode code point is treated as a word character for adjacency purposes.
- A whole-word match requires a boundary or document edge on both sides.

This rule is intentionally deterministic and language-neutral rather than a full linguistic tokenizer.

### 3.2 Navigation

Next from the final match MUST wrap to the first match.

Previous from the first match MUST wrap to the final match.

The active match MUST be visibly distinguished from non-active matches.

Invoking Find Next MUST scroll the active editing surface so the match is visible.

## 4. Replace

The application MUST provide Replace and Replace All.

`Ctrl+H` MUST open a replace-capable search surface.

Replace MUST replace only the active match and then select/navigate to the next applicable match.

Replace All MUST apply to all current non-overlapping matches in the document using the search options active when the command is invoked.

Replace All MUST be one undo transaction.

If the replacement text itself contains the search string, Replace All MUST NOT recursively replace newly inserted content during the same command.

Regex matching is NOT required in v1.0.

## 5. Clipboard

The application MUST support Unicode text Copy, Cut, and Paste through the Windows clipboard using `CF_UNICODETEXT`-compatible interoperability. The document remains UTF-8 internally; Windows UTF-16 conversion occurs only at the OS boundary.

Required shortcuts:

- `Ctrl+C` Copy.
- `Ctrl+X` Cut.
- `Ctrl+V` Paste.
- `Ctrl+A` Select All.

The application MUST interoperate with ordinary external Windows applications for Unicode plain text.

Windows clipboard interoperability requirements, newline normalization, and failure behavior are additionally frozen by `16_WINDOWS_PLATFORM_CONTRACT.md`. Raw bitmap/DIB clipboard import is optional; Unicode plain-text clipboard behavior is mandatory.

### 5.1 Copy

Copy with a non-empty selection MUST place the exact selected textual content on the clipboard.

Copy with no selection MUST NOT alter the document.

### 5.2 Cut

Cut MUST first make the selected text available through clipboard ownership and then remove it from the document as one undo transaction.

If clipboard ownership cannot be established, the application MUST NOT silently delete the selection.

### 5.3 Paste

Plain-text paste MUST convert valid Windows Unicode clipboard text into valid internal UTF-8 and insert it at the caret, replacing the current selection if one exists.

Malformed/unrepresentable external Unicode input MUST be rejected with an explicit non-destructive error rather than producing invalid UTF-8.

Windows CRLF or other line endings received from the clipboard MUST be normalized to the editor's internal newline model.

## 6. Internal Text Drag and Drop

The existing selected-text drag behavior is mandatory.

Dragging a selection to a new valid insertion point MUST move it as one undo transaction.

Holding `Ctrl` while completing the drag MUST copy rather than move the selected text.

Dragging into the selected range itself MUST produce no destructive change.

When a drag approaches the top or bottom edge of a scrollable editor, auto-scroll MUST occur while the pointer remains in the activation region.

The insertion caret/marker MUST remain visible during the drag.

## 7. File Drag and Drop

The application MUST accept file drops from Windows Explorer or another Windows shell/file-manager source through an Explorer-compatible native file-drop mechanism. The implementation may use `WM_DROPFILES`, OLE drag/drop, or an equivalent native mechanism; the acceptance behavior, not one API, is normative.

At minimum:

- Dropping one supported image file over an editable document MUST invoke the image-insertion behavior.
- Dropping one Markdown/text file over the workspace/tab area MUST offer/open it as a document rather than inserting its raw path into the text.
- Dropping multiple supported image files MUST insert them in deterministic drop-list order.

Unsupported file types MUST produce a non-destructive message rather than crash or insert corrupt source.

Clipboard bitmap ingestion is not required; file-based image drag/drop is required.

## 8. External File Modification Detection

The application MUST detect when an open saved file has materially changed on disk since the editor last loaded or successfully saved that file.

The detection mechanism is not prescribed.

A timestamp alone MUST NOT be treated as proof of content change if the implementation can cheaply detect that content is unchanged.

### 8.1 Clean Document Changed Externally

If the in-memory document is clean and disk content changes, the application MUST present a visible external-change notification with at least:

- Reload.
- Keep Current.
- Compare.

Reload MUST replace the in-memory content with current disk content and leave the document clean.

Keep Current MUST keep the editor buffer and mark a conflict state that prevents an unaware normal Save from silently overwriting the external content.

Compare MUST open a diff between editor content and disk content.

### 8.2 Dirty Document Changed Externally

If both editor and disk have diverged, the application MUST NOT automatically overwrite either version.

The user MUST be able to:

- Compare editor vs disk.
- Reload/discard editor changes after confirmation.
- Keep editor version and explicitly overwrite disk.
- Save editor version to a different path.
- Cancel and continue editing in conflict state.

### 8.3 External Deletion

If an open file is deleted externally, the tab MUST remain available in memory.

The application MUST visibly indicate that the backing file is missing.

The user MUST be able to Save As, recreate at the original path, or close/discard.

### 8.4 External Rename

The application is not required to infer arbitrary external renames as identity-preserving moves.

If the original path disappears, the missing-file flow applies unless the implementation can reliably identify the rename.

## 9. Save Conflict Gate

A document with unresolved external-change conflict MUST NOT be silently saved over the external version through ordinary `Ctrl+S`.

The Save flow MUST require an explicit overwrite decision, Save As, or reload/merge decision.

## 10. Autosave

The recovery-autosave capability is mandatory and MUST be enabled by default.

Autosave MUST write recovery state, not silently modify the authored Markdown file.

While periodic autosave is enabled, a dirty document MUST have recovery data written no later than the configured interval after its first unsaved edit. The default interval is 30 seconds and the Preferences UI may configure 10–300 seconds. Further dirty edits MUST trigger another recovery write no later than the next configured interval.

If the user explicitly disables periodic autosave in Preferences, killed-process recovery is not guaranteed for edits made after the last recovery record; the UI must show the warning defined by the Preferences specification. The implementation MAY additionally autosave on focus loss or application-idle events while autosave is enabled.

### 10.1 Atomicity

Recovery state MUST be written using a Windows-safe replacement pattern: stage to a temporary file, complete/flush/close that file, then replace or move it into place so a partial write cannot replace the last valid recovery record. The Windows platform contract governs the acceptable semantics.

A partially written recovery record MUST NOT replace the last known valid recovery record.

## 11. Recovery Location

For workspace documents, recovery data MUST live beneath:

`.mdeditor/recovery/`

For standalone documents and unsaved documents without a workspace, recovery data MUST live beneath the per-user Local AppData application-state/recovery location defined by `16_WINDOWS_PLATFORM_CONTRACT.md`.

Recovery data MUST NOT be stored beside a standalone Markdown file unless that location is itself the active workspace.

## 12. Recovery Record

Each recovery record MUST contain enough information to reconstruct:

- Document source bytes.
- Original/backing path if one existed.
- Whether the document was unsaved/new.
- Last successful on-disk baseline identity or content hash if available.
- Recovery timestamp.
- Document identity used by the session state.

The exact binary/container format is implementation-defined but MUST be versioned and integrity-checked.

## 13. Recovery Cleanup

After a successful explicit Save that makes the document clean, stale recovery data for that exact document state MUST be deleted or marked obsolete.

Closing a dirty document with explicit Discard MUST remove recovery state for discarded changes.

Closing the program normally after all documents are clean MUST leave no false-positive recovery entries.

## 14. Startup Recovery Center

If valid recovery records newer than their corresponding saved baselines exist at startup, the application MUST present a Recovery Center before silently replacing user data.

For each recoverable document the UI MUST show enough identity information to distinguish records, including path or `Untitled` identity and recovery time.

The user MUST be able to:

- Open recovered content.
- Compare recovered content with current disk content when both exist.
- Discard the recovery record.
- Defer a decision and continue without deleting it.

Bulk `Open All Recoverable` and `Discard All` commands SHOULD be provided; they are not release gates.

## 15. Crash Recovery Validation

The test suite MUST include a process-termination scenario in which the editor is killed after recovery state exists but before normal shutdown.

On restart, the recovered source MUST match the last completed recovery write and MUST NOT be replaced with empty or stale placeholder content.

## 16. Unicode Storage Baseline

All Markdown document text MUST be stored internally in a representation that preserves valid UTF-8 round trips.

Saving and reopening a document containing Traditional Chinese, ASCII, accented Latin text, combining marks, and emoji MUST preserve the original Unicode scalar sequence unless the user explicitly edits it.

No editing command may leave the source buffer as invalid UTF-8.

## 17. Cursor and Deletion Units

Byte offsets MAY be used internally, but user-visible cursor movement MUST never stop inside a UTF-8 code unit sequence.

The v1.0 acceptance corpus additionally requires the editor to treat the following as one user-visible cursor/deletion unit in ordinary left/right movement and Backspace/Delete:

- A base character followed by one or more Unicode combining marks used in the supplied fixtures.
- Emoji plus variation selector used in the supplied fixtures.
- The exact emoji ZWJ sequences present in the supplied fixtures.

The task does not require shipping a complete Unicode database for every possible grapheme cluster ever standardized; it DOES require correct behavior for the normative Unicode fixture corpus.

## 18. Windows IME Composition

Traditional Chinese IME input is mandatory.

The editor MUST integrate with standard Windows IME facilities sufficiently to support visible composition, candidate/composition placement near the custom caret, committed text, and cancellation while maintaining an internal UTF-8 document. The detailed Windows composition contract is normative in `16_WINDOWS_PLATFORM_CONTRACT.md`.

During composition:

- Preedit text MUST be visibly distinguishable from committed document text, or the active system input method's equivalent preedit presentation MUST remain usable.
- Uncommitted composition MUST NOT be written into document history as permanent source text.
- Committing the composition MUST create normal editable document text.
- Cancelling composition MUST not leave partial UTF-8 bytes or phantom characters.

Switching tabs or closing a modal while an IME composition is active MUST either commit or cancel it according to a deterministic documented rule; it MUST NOT corrupt the document.

## 19. Unicode Selection and Statistics

Selection boundaries MUST align to valid user-visible editing units defined above.

The status-bar character count and statistics dialog MUST use the same Unicode counting rules across Source and Rendered Editing modes.

Search, diff highlighting, and source/rendered mapping MUST not split UTF-8 continuation bytes.

## 20. Mandatory Acceptance Cases

At minimum the final acceptance suite MUST demonstrate:

1. Paste Traditional Chinese text from an external Windows application through the Unicode clipboard.
2. Copy selected mixed Chinese/English text out of the editor and verify exact external clipboard content.
3. Use an installed Windows Traditional Chinese IME to compose and commit a phrase; programmatic insertion of already-final Chinese bytes does not satisfy this case.
4. Undo that committed phrase without corrupting surrounding text.
5. Replace All a Chinese phrase in multiple paragraphs and undo it in one step.
6. Drag selected text to another line and undo in one step.
7. Ctrl-drag selection and verify copy semantics.
8. Drop a PNG/JPEG/BMP file from a desktop-style file source and obtain a valid image construct.
9. Modify a clean file externally and exercise Reload.
10. Modify a dirty file externally and exercise Compare without data loss.
11. Delete a backing file externally and recover using Save As.
12. Kill the process after autosave and recover the latest valid recovery state.
13. Traverse fixture combining-mark and emoji sequences without generating invalid UTF-8.

## 21. Windows-Specific Cross-Reference

The Windows clipboard, Explorer drop, IME composition/candidate placement, external file sharing, safe-replace, path, and recovery-location semantics in `16_WINDOWS_PLATFORM_CONTRACT.md` are mandatory extensions of this document.
