# 03 — Markdown and Editing Behavior

## 1. Markdown Compatibility Target

The application MUST implement the ordinary authoring constructs of CommonMark plus the GFM-style extensions explicitly listed here.

The specification intentionally names required constructs rather than accepting a vague claim of “Markdown support.”

## 2. Required Block Constructs

The parser, source editor, preview renderer, split preview, and rendered editing model MUST support:

- Paragraphs.
- ATX headings levels 1 through 6.
- Setext headings.
- Thematic breaks.
- Block quotes.
- Unordered lists.
- Ordered lists.
- Nested lists.
- Task-list items.
- Indented code blocks.
- Fenced code blocks using backticks.
- Fenced code blocks using tildes.
- Tables using GFM-style pipe syntax.
- Blank-line separation behavior.
- Raw HTML blocks/inline HTML as source-preserved content. Generic raw HTML MUST NOT execute scripts or embed active web content. The renderer MAY display unsupported raw HTML as literal/source-styled content. The narrowly defined `<img>` subset used for persisted resized images MUST render as an image when its attributes satisfy `05_IMAGES_AND_MEDIA.md`.

## 3. Required Inline Constructs

The application MUST support:

- Plain text.
- Emphasis.
- Strong emphasis.
- Combined strong + emphasis.
- Strikethrough.
- Inline code.
- Links.
- Autolinks.
- Images.
- Backslash escapes.
- Entity-like source text according to the chosen CommonMark-compatible behavior.
- Hard line breaks.
- Soft line breaks.

## 4. Links

A rendered link MUST be visually identifiable as interactive.

Editing its label in Rendered Editing Mode MUST update the Markdown link label without silently deleting the destination.

Editing the destination MUST be possible through an explicit interaction.

The user MUST be able to inspect the destination before activating it.

Malformed link syntax MUST remain editable source text and MUST NOT crash the parser.

In Preview Mode, activating an HTTP/HTTPS link MAY occur by ordinary click. In Rendered Editing Mode, ordinary click edits/selects the link and `Ctrl+Click` activates it. HTTP/HTTPS destinations MUST use the system external handler; the application MUST NOT embed a browser. Relative Markdown links inside the workspace SHOULD open in an editor tab. Unsupported URI schemes MUST not execute automatically.

## 5. Inline Formatting Commands

The application MUST expose commands for at least:

- Bold / strong.
- Italic / emphasis.
- Strikethrough.
- Inline code.
- Link insertion.
- Heading level selection.
- Block quote.
- Bulleted list.
- Numbered list.
- Task list.
- Fenced code block.
- Table insertion.
- Image insertion.

Formatting commands MUST work from at least Source Mode and Rendered Editing Mode where semantically applicable.

## 6. Baseline Shortcuts

The following shortcuts MUST be available:

- Ctrl+N — New.
- Ctrl+O — Open.
- Ctrl+S — Save.
- Ctrl+Shift+S — Save As.
- Ctrl+Z — Undo.
- Ctrl+Y — Redo.
- Ctrl+Shift+Z — Redo as an additional mandatory alias.
- Ctrl+X — Cut.
- Ctrl+C — Copy.
- Ctrl+V — Paste.
- Ctrl+A — Select All.
- Ctrl+F — Find.
- Ctrl+H — Replace.
- Ctrl+Shift+P — Command Palette.
- Ctrl+W — Close active tab.
- Ctrl+Tab — Next tab.
- Ctrl+Shift+Tab — Previous tab.
- Ctrl+B — Strong/Bold.
- Ctrl+I — Emphasis/Italic.
- Ctrl+K — Insert/Edit Link.
- Ctrl+mouse-wheel — Zoom.

Additional shortcuts MAY be provided but MUST NOT conflict with mandatory shortcuts.

## 7. Applying Inline Formatting to a Selection

When a non-empty text selection is formatted as strong/emphasis/strikethrough/inline code:

- The command MUST apply to the selected content.
- Undo MUST restore the exact prior source content in one logical step.
- The selection MUST not unexpectedly disappear before the command is applied.
- UTF-8 boundaries MUST remain valid.

If the selection is already surrounded by the corresponding unambiguous Markdown markers, invoking the same command SHOULD remove/toggle the formatting.

Ambiguous nested-marker toggle behavior MUST follow the deterministic normative fixture expectations: the command applies/removes the smallest unambiguous delimiter pair directly enclosing the selection; if no such pair exists, it wraps the selection without rewriting unrelated outer delimiters.

## 8. Applying Inline Formatting Without a Selection

If no text is selected, a formatting command MUST either:

- Insert paired syntax and place the caret between it, or
- Enter a documented pending-format state that produces equivalent Markdown as the user types.

The application MUST use one consistent behavior for each formatting command.

## 9. Headings

Rendered headings MUST have visually distinct scale/weight by level.

Rendered Editing Mode MUST allow:

- Editing heading text directly.
- Changing heading level through UI command.
- Converting a heading to a paragraph.

The resulting Markdown MUST remain valid and inspectable in Source Mode.

## 10. Lists

The editor MUST support editing list content without requiring the user to switch to Source Mode for every operation.

Pressing Enter at the end of a non-empty list item SHOULD create a new peer item.

Pressing Enter on an empty list item SHOULD exit the list level or otherwise follow a documented, consistent list-exit behavior.

Nested lists MUST render with visible nesting.

Undo MUST restore list structure after structural edits.

## 11. Task Lists

Rendered task-list checkboxes MUST be interactive in Rendered Editing Mode.

Toggling a checkbox MUST update the corresponding `[ ]` / `[x]` source representation.

The change MUST mark the document dirty.

The toggle MUST be undoable.

## 12. Code Blocks

Fenced code blocks MUST:

- Preserve content whitespace.
- Render using a monospaced presentation.
- Visually distinguish block boundaries.
- Preserve optional info-string/language text in source.

The application MUST NOT execute code from a code block.

Rendered Editing Mode MUST permit editing the textual contents of the block.

The implementation MUST prevent Markdown syntax inside a fenced block from being incorrectly rendered as ordinary surrounding Markdown.

## 13. Inline Code

Inline code MUST render distinctly from ordinary text.

Direct editing MUST preserve the inline-code semantic until the user explicitly removes or structurally breaks it.

## 14. Block Quotes

Block quotes MUST have visible indentation and/or a quote rule.

Nested block quotes MUST preserve visible nesting.

Rendered Editing Mode MUST allow editing the quoted text.

## 15. Tables

Tables are mandatory.

At minimum, a table MUST support:

- Header row.
- Separator row in Markdown representation.
- One or more body rows.
- Multiple columns.
- Empty cells.
- Inline Markdown inside cells where compatible with the parser.

Rendered table requirements:

- Clear row/column boundaries or spacing.
- Distinguishable header row.
- Cell text remains selectable/editable in Rendered Editing Mode.

The user MUST be able to insert a basic table through a UI command without manually typing the separator syntax.

Rendered table editing MUST provide insert row above/below, delete row, insert column before/after, delete column, and per-column default/left/center/right alignment controls. Keyboard cell navigation and serialization rules are normative in `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.

## 16. Thematic Breaks

Thematic breaks MUST render as a visual divider.

They MUST remain distinguishable from accidental text punctuation in the parser according to the compatibility target.

## 17. Rendered Editing Model

Rendered Editing Mode is a semantic editor over the Markdown document, not an unrelated rich-text document.

The underlying Markdown source remains authoritative for persistence.

Every rendered-editing operation MUST map back to a Markdown source transformation.

The editor MAY internally use an AST or equivalent structured representation.

If an operation cannot be represented without loss in the supported Markdown subset, the UI MUST either:

- Reject the operation with clear feedback, or
- Apply a documented Markdown transformation.

It MUST NOT silently serialize arbitrary proprietary rich-text data outside the Markdown file and then claim the Markdown file alone represents the document.

## 18. Source/Rendered Round-Trip Integrity

For a source document containing supported Markdown constructs:

1. Open source.
2. Enter Rendered Editing Mode.
3. Make no document edit.
4. Return to Source Mode.

The source MUST remain byte-identical when no edit is made. Mode switching alone MUST NOT normalize delimiters, list numbering, table spacing, line endings, or image syntax.

## 19. Direct Text Editing in Rendered Mode

The user MUST be able to:

- Place the caret in ordinary rendered text.
- Insert text.
- Delete backward/forward.
- Select text by pointer drag.
- Extend selection by keyboard.
- Copy/cut/paste selected text.
- Apply inline formatting.
- Move a selected text range by drag.

The caret MUST visibly identify insertion position.

Selection MUST remain visible over mixed inline styles.

## 20. Selected-Text Drag Move

A non-empty textual selection MUST be draggable within the editable document.

Required behavior:

- Pressing inside the established selection and beginning a drag initiates text move rather than clearing the selection immediately.
- During drag, the editor MUST show the prospective insertion position.
- Dropping at a valid location removes the text from the original range and inserts it at the target.
- The operation MUST preserve the logical text content.
- The operation MUST be one undo step.
- Dropping back into the original selection MUST not duplicate or destroy text.
- Invalid drop targets MUST cancel without modifying the document.
- Dragging within 24 logical pixels of the top/bottom edge of a scrollable editing surface MUST auto-scroll. Scroll speed MUST increase monotonically as the pointer approaches the edge, with a practical cap that keeps the insertion marker visually trackable.

Holding `Ctrl` while completing a selected-text drag MUST copy rather than move the selected text. The completed copy is one undo transaction.

## 21. Cross-Construct Dragging

Moving selected text across paragraphs or simple inline-format boundaries MUST be supported where the resulting Markdown is representable.

Moving a partial selection through structurally complex constructs such as table boundaries, fenced code block boundaries, or image nodes MAY be restricted.

Any restriction MUST be visible and deterministic rather than silently corrupting source structure.

## 22. Selection Semantics

Selection endpoints MUST correspond to valid Unicode text positions.

The application MUST support selections spanning:

- Multiple words.
- Multiple lines.
- Multiple paragraphs.
- Mixed Chinese and Latin text.
- Inline formatted runs.

Selection statistics MUST update live.

## 23. Split-View Synchronization

In Split Mode, source edits MUST trigger rendered preview updates.

The application MUST make a reasonable effort to keep source and preview scroll positions semantically aligned.

The acceptance suite uses block-anchor cases rather than requiring pixel-identical scroll offsets. Source/render synchronization MUST satisfy the normative block-mapping behavior in `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.

Click-to-sync is mandatory at Markdown block granularity. Split Mode source/render mapping, draggable divider, and synchronized scrolling are defined in `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.

## 24. Parser Failure Isolation

Incomplete syntax is normal during editing.

The parser MUST tolerate transient states such as:

- Unclosed emphasis marker.
- Unclosed inline code marker.
- Unclosed fenced code block.
- Partially typed link.
- Incomplete table row.
- Broken image syntax.

The application MUST continue accepting input.

A parser error MUST NOT discard or rewrite the user's source.
