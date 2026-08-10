# 12 — Rendered Editing, Table GUI, Outline Navigation, and Document Lifecycle

## 1. Purpose

Rendered Editing Mode is a first-class authoring surface. It MUST NOT be implemented as a read-only preview with a generic text box overlay that only handles plain paragraphs.

The underlying source of truth remains Markdown text.

The rendered editor MUST maintain source mappings sufficient to apply structural edits without creating an unrelated opaque rich-text document format.

## 2. General Rendered-Editing Rules

Clicking editable rendered content MUST place a caret or structural selection at the corresponding logical source location.

Typing MUST update the Markdown source and the rendered view.

Selections spanning multiple inline styles or multiple text nodes MUST map to a deterministic contiguous source range when the selected content is contiguous in source.

Rendered editing MUST preserve unsupported or source-only syntax rather than silently deleting it.

When a rendered operation cannot be safely represented without changing an unsupported construct, the application MUST fall back to a source-oriented editing affordance or refuse the operation with a clear explanation; it MUST NOT silently flatten the content.

## 3. Paragraphs

Clicking paragraph text MUST place the caret at the clicked logical text position.

Enter in a paragraph MUST split the paragraph at the caret into two Markdown paragraphs unless the caret is inside another structure whose rules override Enter.

Backspace at the start of a plain paragraph immediately following another plain paragraph SHOULD merge the paragraphs in a predictable way.

## 4. Headings

Rendered headings MUST be directly editable.

A heading control or context action MUST allow changing levels H1 through H6.

Changing heading level MUST update source syntax while preserving heading text and inline formatting.

Converting a heading to Paragraph MUST be supported.

Setext headings MAY be normalized to ATX syntax after a structural heading edit; simple text edits MUST NOT gratuitously normalize them solely because the document was opened.

## 5. Bold, Italic, Strikethrough, and Inline Code

Rendered selections MUST be formattable using the same commands/shortcuts as Source Mode where the target construct is semantically valid.

Toggling Bold MUST add/remove strong emphasis around the selected logical text.

Toggling Italic MUST add/remove emphasis.

Toggling Strikethrough MUST add/remove the required GFM delimiter representation.

Toggling Inline Code MUST use backtick delimiters with sufficient delimiter length when selected text contains backticks.

The implementation MUST avoid malformed delimiter nesting in the normative fixtures.

## 6. Links

A rendered link MUST expose separate edit and activation behaviors.

In Rendered Editing Mode:

- Ordinary click selects/places caret in the link text rather than unexpectedly launching a browser.
- `Ctrl+Click` activates the link.
- A context action MUST provide `Edit Link`.

`Edit Link` MUST allow editing:

- Display text.
- Destination.
- Optional title if present/supported by the source form.

Relative links to Markdown files inside the active workspace SHOULD open in an application tab.

HTTP/HTTPS links MUST be opened through the user's external system handler rather than embedded web content.

Unsupported or dangerous URI schemes MUST not be executed automatically.

## 7. Lists

Rendered ordered, unordered, and task lists MUST be directly editable.

Enter at the end of a non-empty list item MUST create a sibling list item of the same kind.

Enter on an empty list item MUST terminate that list level or reduce nesting according to ordinary editor behavior.

Tab at the start/within a list item MUST indent the item one level when structurally valid.

Shift+Tab MUST outdent one level.

Indent/outdent MUST preserve child items with their parent unless the selected operation explicitly targets multiple items.

Ordered-list source numbering MAY be normalized after structural list editing, but the rendered semantic order MUST remain correct.

## 8. Task Lists

Rendered task-list checkboxes MUST be interactive.

Clicking a checkbox MUST toggle `[ ]` and `[x]`/equivalent checked syntax in source as one undo transaction.

Toggling MUST NOT move the caret unexpectedly into another list item.

The checked state MUST survive Save/Reopen and mode switching.

## 9. Block Quotes

Rendered blockquote text MUST be editable.

A command/context action MUST allow converting a selected paragraph to/from blockquote.

Nested blockquotes in the fixture corpus MUST preserve nesting on ordinary text edits.

## 10. Fenced Code Blocks

Clicking inside a rendered fenced code block MUST enter an editable code-block state without interpreting Markdown delimiters inside the code as formatting.

The code block editor MUST support multiline text, selection, clipboard, undo/redo, and scrolling when needed.

A language/info-string field MAY be shown inline or in a small contextual control; the user MUST be able to edit the info string.

If code content contains the current fence delimiter run, the editor MUST choose a safe fence length or alternate required fence type when serializing structural edits.

## 11. Inline Code

Inline code MUST remain editable as literal text.

Markdown syntax characters typed inside inline code MUST not become nested emphasis/link syntax merely because the rendered view is active.

## 12. Thematic Breaks

Rendered thematic breaks MUST be selectable as structural objects.

Delete/Backspace on a selected thematic break MUST remove its source construct as one undo transaction.

## 13. Images in Rendered Editing

Images MUST be selectable as structural objects.

Selection MUST expose resize handles and the image-specific context menu defined in the image specification.

Arrow-key movement SHOULD move focus between image and adjacent text positions; pointer operation is mandatory.

Delete/Backspace while an image object is selected MUST remove the image construct as one undo transaction.

Alt text and destination/storage representation MUST remain editable through an image properties action.

## 14. Tables — Rendering and Selection

A Markdown table MUST render as a grid with visible cell boundaries or an equivalently clear table treatment.

Clicking a cell MUST place the caret inside that cell's textual content.

The active cell MUST be visually identifiable.

A table-specific contextual toolbar or menu MUST be available when caret/selection is inside the table.

## 15. Table Keyboard Navigation

Tab inside a table cell MUST move to the next cell in row-major order.

Shift+Tab MUST move to the previous cell.

Tab from the final cell MUST append a new body row and move to its first cell.

Shift+Tab from the first cell MUST remain in the first cell or move to a deterministic pre-table position; it MUST NOT corrupt source.

Enter inside a cell MUST insert text according to the application's single-line Markdown-cell policy and MUST NOT create a syntactically ambiguous raw newline inside a cell.

For v1.0, Enter SHOULD move to the next cell rather than insert a physical newline.

## 16. Table Row Operations

The GUI MUST provide:

- Insert row above.
- Insert row below.
- Delete current row.

Deleting the final body row MUST leave a syntactically valid header/separator table or convert/remove the table through a confirmed deterministic rule.

The header row MUST NOT be deleted as though it were an ordinary body row without a structural conversion rule.

## 17. Table Column Operations

The GUI MUST provide:

- Insert column before.
- Insert column after.
- Delete current column.

New cells MUST be initialized empty.

Deleting the last remaining column MUST require confirmation to remove the entire table or convert its textual content; it MUST NOT emit an invalid zero-column table.

## 18. Table Alignment

For each column the GUI MUST support:

- Left alignment.
- Center alignment.
- Right alignment.
- Default/no explicit alignment.

The separator row syntax MUST be updated accordingly.

Changing alignment MUST be one undo transaction.

## 19. Pipe Escaping

Literal `|` characters in table-cell content MUST be represented so the Markdown source remains a valid table.

Inline code and escaped pipes in the normative table fixtures MUST round-trip without accidental column splitting.

## 20. Table Source Stability

Text-only edits inside a cell SHOULD minimize unrelated source churn.

Structural row/column/alignment operations MAY normalize table spacing for the affected table.

Normalization MUST NOT alter cell text, links, inline code, emphasis, or image destinations.

## 21. Outline Panel

The application MUST provide a document Outline derived from headings in the active document.

The Outline MUST coexist with the workspace file tree through an explicit sidebar tab/pill/segmented navigation mechanism.

The required sidebar views are at least:

- Files.
- Outline.

## 22. Outline Structure

Each heading MUST appear with:

- Heading text with Markdown formatting removed for display.
- Hierarchical indentation reflecting heading level transitions.
- A visual indication for the heading corresponding to the current caret/viewport when reasonably determined.

Empty headings MUST remain navigable with an explicit placeholder label.

Duplicate heading text MUST remain distinct entries based on source position.

## 23. Outline Live Update

Adding, deleting, renaming, or changing the level of a heading MUST update the Outline without requiring file reopen.

Update MAY be debounced briefly, but MUST appear within 250 ms after the editor becomes idle from the triggering edit under ordinary-size fixtures.

## 24. Outline Navigation

Clicking an Outline entry MUST:

- Activate the associated document if required.
- Scroll the active editor mode to that heading.
- Place caret/focus at or near the heading when the current mode is editable.

Navigation MUST work in Source, Split, Preview, and Rendered Editing modes.

## 25. Split Mode Divider

The Source/Preview divider in Split Mode MUST be draggable.

Each pane MUST retain at least 240 logical pixels of usable width while both panes are visible, unless the whole application window is narrower than the combined minimum.

Divider ratio MUST be persisted in workspace/session state.

A reset-to-equal command or double-click behavior SHOULD be provided.

## 26. Source ↔ Rendered Mapping

The parser/render model MUST retain mappings at least at block-construct granularity.

In Split Mode:

- Moving the source caret into a block MUST make the corresponding rendered block discoverable/visible when it is currently far outside the preview viewport.
- Clicking a rendered paragraph/heading/list/code/table block MUST allow the source pane to reveal the corresponding source region.

Exact pixel-locked scroll synchronization is not required.

## 27. Split Scroll Synchronization

The application MUST provide an enabled-by-default synchronized-scroll mode.

The required algorithmic behavior is anchor based:

1. Determine the topmost substantially visible Markdown block in the scrolled pane.
2. Map that block to its counterpart in the other pane.
3. Preserve an approximate fractional offset within the block where practical.
4. Scroll the other pane without causing feedback oscillation.

A toolbar/menu toggle MUST allow disabling synchronized scrolling.

## 28. Multi-Document Lifecycle

All open documents exist as independent tabs with independent dirty state, undo history, caret, selection, scroll state, mode, and zoom unless a preference explicitly defines a shared value.

Switching tabs MUST NOT save or discard changes automatically.

## 29. Close Tab

Closing a clean tab MUST close immediately.

Closing a dirty tab MUST offer:

- Save.
- Discard.
- Cancel.

Save failure MUST keep the tab open and preserve dirty content.

## 30. Close Multiple / Close Application

When multiple dirty tabs exist, application exit or workspace switch MUST present a multi-document resolution flow.

The flow MUST identify every dirty document and MUST support resolving each as Save or Discard, plus global Cancel.

A `Save All` action MUST attempt all dirty saved-path documents and then surface any failures rather than pretending the whole operation succeeded.

Unsaved untitled documents that require a path MUST each receive an explicit Save As decision.

If any document resolution is cancelled, application/workspace close MUST stop and remaining in-memory documents MUST stay intact.

## 31. Save All

The application MUST expose Save All through menu/command palette and shortcut `Ctrl+Shift+S` is reserved for Save As, so Save All MUST use a distinct shortcut such as `Ctrl+Alt+S` or have no default shortcut.

The chosen shortcut MUST be documented in the in-app shortcut reference.

Save All MUST NOT clear dirty indicators for documents whose writes failed.

## 32. Reopen Session

On normal restart of a workspace, restorable clean tabs MUST reopen in persisted order.

Dirty recovery-capable tabs MUST be reconciled with recovery records rather than silently restoring stale session text.

Missing files MUST appear as recoverable missing-path tabs or explicit session notices; they MUST NOT cause startup failure.

## 33. Mandatory Acceptance Cases

The final suite MUST include at least:

1. Directly edit heading text in Rendered Editing Mode and verify source.
2. Change H2 to H4 and undo once.
3. Toggle a task checkbox and verify `[x]`/`[ ]` source.
4. Edit a fenced code block containing Markdown punctuation without formatting leakage.
5. Add/delete table rows and columns and verify valid Markdown.
6. Cycle table cells using Tab/Shift+Tab.
7. Change all four table alignment states.
8. Rename a heading and observe immediate Outline update.
9. Click duplicate-named Outline entries and reach distinct source positions.
10. Drag Split divider and persist its ratio across reopen.
11. Demonstrate source-to-preview block synchronization and rendered-to-source reveal.
12. Close one dirty tab using Cancel and prove no state loss.
13. Exit with at least three dirty documents, including one untitled document and one simulated save failure.
