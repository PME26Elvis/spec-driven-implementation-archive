# 02 — Product and Document Model

## 1. Application Purpose

The application is a desktop Markdown authoring environment intended for ordinary text documents containing Markdown structure, links, tables, code blocks, task lists, and local images.

The primary document format is UTF-8 Markdown text.

## 2. Primary Application Modes

The application MUST provide all four modes below.

### 2.1 Source Mode

A plain Markdown source editor.

The user sees Markdown punctuation and edits the source directly.

### 2.2 Split Mode

The application displays:

- Editable Markdown source on one side.
- Live rendered preview on the other side.

Changes in the source MUST update the preview without requiring an explicit refresh action.

### 2.3 Preview Mode

The application displays a rendered Markdown document without source markup as the primary presentation.

Preview Mode is not required to be editable.

### 2.4 Rendered Editing Mode

The application displays the rendered document while allowing direct editing of its content and structure.

This is a mandatory editing mode, not a read-only preview with occasional dialogs.

Edits performed in Rendered Editing Mode MUST update the underlying Markdown source.

Switching from Rendered Editing Mode to Source Mode MUST expose Markdown source representing the same document state.

Switching repeatedly between modes MUST NOT progressively alter semantically unchanged Markdown merely because it was rendered.

## 3. Document Identity

A document has at minimum:

- Current UTF-8 source buffer.
- Current file path, if saved/opened from disk.
- Dirty/modified state.
- Undo/redo history.
- Render model derived from source.
- Version-history association.
- Current editor mode.
- Current cursor/selection state relevant to the active editing surface.
- Current zoom level.

## 4. New Document

Creating a new document MUST create an empty unsaved document.

The document MUST be editable before choosing a file path.

The application MUST visibly distinguish unsaved documents from saved clean documents.

## 5. Open

Open MUST allow the user to choose an existing Markdown/text file through an application-visible file operation flow.

The selected file MUST be loaded as UTF-8 when valid.

Invalid UTF-8 handling MUST produce an explicit error rather than silently replacing arbitrary bytes and then saving the changed content over the original.

If the active document has unsaved changes, opening another document MUST first invoke the unsaved-change decision flow.

## 6. Save

Save MUST:

- Write the current Markdown source to the current file path.
- Preserve valid UTF-8.
- Clear dirty state only after a successful write.
- Report write failure.

A failed save MUST NOT clear dirty state.

## 7. Save As

Save As MUST allow choosing a new destination path.

After successful Save As:

- The new path becomes the current path.
- The document remains loaded.
- Dirty state clears if the saved bytes represent the current document state.

If the target already exists, overwrite MUST require an explicit confirmation flow.

## 8. Close and Exit

Closing a modified document or exiting with a modified document MUST offer:

- Save.
- Discard.
- Cancel.

Cancel MUST return to the editor with no content loss.

Save MUST proceed only if the save succeeds or the user successfully completes Save As for an untitled document.

If saving fails, the close/exit operation MUST stop.

## 9. External File Changes

External modification, deletion, conflict, compare, reload, explicit overwrite, and Save As behavior is mandatory and is fully defined in `11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md`. The implementation MUST NOT silently overwrite an externally changed file through ordinary Save.

## 10. Dirty-State Semantics

An operation that changes the persisted Markdown representation MUST set the document dirty.

The following are examples of document-changing operations:

- Typing or deleting text.
- Applying Markdown formatting.
- Editing rendered content.
- Inserting or removing an image reference.
- Resizing an image when size is persisted in the document representation.
- Moving selected text.
- Table structural edits.
- Task-list state changes.
- Restoring a historical version.

Purely visual operations MUST NOT set dirty state, including:

- Scrolling.
- Moving the mouse.
- Opening a statistics dialog.
- Changing editor zoom if zoom is an application preference rather than document content.
- Opening version history without restoring a version.

## 11. Undo/Redo Model

Undo and redo MUST operate on user-meaningful editing operations.

At minimum the following MUST be undoable:

- Text insertion.
- Text deletion.
- Paste.
- Cut.
- Formatting command.
- Selected-text drag move.
- Selected-text drag copy where supported.
- Image insertion.
- Image removal.
- Image resize commit.
- Table row/column/alignment structural edits required by `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.
- Task-list toggle.
- Rendered-mode structural edit.

A single completed drag-move operation MUST undo as one logical operation.

An image resize gesture MUST not require dozens of Undo invocations for intermediate pointer-move frames; the committed resize is one logical undo step.

Redo history MUST be invalidated when a new document-changing edit occurs after an undo.

## 12. Clipboard Baseline

The application MUST support ordinary text:

- Copy.
- Cut.
- Paste.

Clipboard text MUST preserve UTF-8.

Selection copy MUST copy exactly the selected textual content represented by the active editing mode.

UTF-8 text clipboard interoperability is mandatory. File-based image drag/drop is mandatory. Raw bitmap/image clipboard ingestion is optional for v1.0; omitting it does not weaken file insertion, drag/drop, or Base64/relative-asset requirements.

## 13. Find/Search

The editor MUST provide in-document search.

Search MUST support UTF-8 Traditional Chinese text.

At minimum:

- Find dialog or find bar.
- Next match.
- Previous match.
- Visible match indication.
- No-match feedback.
- Search term remains editable.

Byte-substring behavior that splits a UTF-8 character is prohibited.

Replace and Replace All are mandatory. Literal matching, case-sensitive toggle, deterministic ASCII case-insensitive behavior, whole-word matching, wrap navigation, and one-transaction Replace All semantics are defined in `11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md`. Regex is not required.

## 14. Zoom

Ctrl + mouse-wheel MUST adjust editor text size/zoom within the active document view.

Zoom changes MUST be visually continuous enough not to appear as a mode reset.

Zoom MUST NOT modify Markdown source.

A practical min/max zoom bound MUST prevent zero-size text and pathological unbounded allocation.

The UI MUST provide visible feedback for the resulting zoom percentage or equivalent scale.

## 15. Status Bar

The lower-left status area MUST update live.

When there is no active selection, it MUST show at least:

- Current line number.
- Current column position.
- Total character count.

Line and column numbers are 1-based. Column counts user-visible editing units from the start of the logical line and MUST NOT expose a raw UTF-8 byte offset. For a selection, line/column correspond to the active caret end of the selection.

When there is an active text selection, the primary count MUST switch from whole-document character count to selected character count.

The selected-count state MUST be visually distinguishable from the whole-document state.

The status information MUST update after keyboard movement, pointer selection, edits, undo/redo, drag moves, and mode changes.

## 16. Document Statistics Command

A dedicated command/button MUST open a document statistics surface.

The statistics MUST include at least:

- Raw source Unicode character count.
- Rendered/plain-text Unicode character count with Markdown syntax excluded.
- Word/token count under the defined mixed-language rules.
- Total physical line count.
- Non-empty line count.
- Paragraph count.
- Heading count.
- Image count.
- Link count.
- Fenced code-block count.

Statistics MUST be computed from the current live document, including unsaved edits.

Statistics MUST update when the document changes; reopening the dialog after a change MUST not show stale cached numbers.

## 17. Character Counting

Counts MUST operate on decoded Unicode text, not bytes.

Newline representation MUST not cause CRLF to count as two visible text characters in the rendered/plain-text count.

The raw-source character count MUST count each LF newline separator as one character. Internally loaded CRLF MUST be normalized for counting so one visible line break counts as one character, not two. The rendered/plain-text character count MUST exclude Markdown syntax characters removed by the parser and MUST use the same Unicode editing-unit rules used by the normative fixture corpus.

## 18. Word Counting

The statistics view MUST separate character count from word count.

For mixed Chinese/Latin text, the required high-level behavior is:

- Each CJK ideographic character contributes one CJK text unit for the user-facing word/statistics calculation.
- Contiguous Latin letters/digits forming an ordinary token contribute one word.
- Whitespace separates Latin tokens.
- Markdown punctuation excluded from rendered/plain-text statistics MUST not create phantom words.

The v1.0 word/token rule is deterministic:

- Each Unicode scalar in CJK Unified Ideographs (`U+4E00–U+9FFF`), CJK Extension A (`U+3400–U+4DBF`), CJK Compatibility Ideographs (`U+F900–U+FAFF`), and supplementary CJK ideograph ranges used by the fixture corpus contributes one word unit.
- A contiguous run of ASCII letters or digits contributes one word. Internal underscore may remain in the same ASCII token.
- A contiguous run of non-ASCII, non-CJK characters that is not Unicode whitespace and is not an ASCII punctuation boundary contributes one word unit; this makes ordinary accented-Latin words one token without requiring a full linguistic dictionary.
- Unicode whitespace and ASCII punctuation separate non-CJK tokens. Apostrophe or hyphen surrounded on both sides by Latin letters/digits MAY remain inside one token.
- Markdown syntax removed from rendered/plain-text statistics contributes no word units.

The normative fixtures MUST include exact expected totals for Traditional Chinese, English, accented Latin, and mixed documents.

## 19. Multi-Document Application Model

The application MUST support multiple simultaneously open documents in a tabbed interface.

The complete tab lifecycle, ordering, overflow, duplicate-path prevention, workspace restore, and relevant shortcuts are normative in `10_WORKSPACE_TABS_AND_ASSETS.md`.

The active tab determines which document receives editing commands, statistics, search, version-history commands, and mode changes.

Commands MUST NOT accidentally mutate a background/inactive tab merely because it was previously focused.

## 20. Workspace Model

The application MUST support selecting a folder as an active workspace.

The workspace provides the root for the file tree, managed relative image assets, session state, and workspace-associated history metadata.

Opening a workspace MUST NOT require converting standalone files or rewriting their Markdown.

The application MUST also remain usable for standalone files when no workspace is open.

## 21. Workspace State vs Document State

Workspace state and Markdown document content are separate persistence domains.

Workspace state MAY remember UI/session information such as tabs, cursor positions, scroll positions, zoom, sidebar state, and expanded tree nodes.

Workspace-state persistence MUST NOT clear a document's dirty state or be presented to the user as if the Markdown document itself has been saved.

Document edits become durable document content only through the defined document save/recovery mechanisms.

## 22. Image Representation Choice

A Markdown document MAY contain a mixture of relative-path images and Base64 data-URI images.

The representation is per image, with a remembered insertion default permitted at document/workspace level.

Ordinary Save MUST preserve the representation currently present in source.

Representation conversion is an explicit user edit or an explicit portable-export operation, not an automatic side effect of unrelated saves.

## 23. Portable Export

The application MUST provide an explicit portable Markdown export workflow capable of producing either:

- One Markdown file with supported images embedded as Base64 data URIs.
- One Markdown file plus a managed assets directory with supported embedded/local images externalized/copied and referenced relatively.

The open document MUST remain unchanged unless the user explicitly adopts the exported representation.


## 23. v1.0 Cross-Cutting Product Requirements

The following are mandatory and normative through their dedicated specifications:

- Undo/redo transaction semantics, Find/Replace, clipboard, file drag/drop, external changes, autosave/recovery, and Unicode/IME: `11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md`.
- Detailed Rendered Editing semantics, table GUI editing, Outline, split synchronization, and multi-document close lifecycle: `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.
- Command Palette, recent files/workspaces, preferences, Light/Dark themes, and keyboard-only operation: `13_COMMANDS_RECENTS_PREFERENCES_KEYBOARD.md`.
- Large-document responsiveness and fault/corruption handling: `14_PERFORMANCE_AND_FAILURE_HANDLING.md`.
