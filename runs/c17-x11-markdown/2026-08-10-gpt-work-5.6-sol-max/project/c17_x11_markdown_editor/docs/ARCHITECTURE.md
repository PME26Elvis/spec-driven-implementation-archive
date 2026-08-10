# Architecture

## System Boundary

Lattice Markdown is one native C17 process with one X11 top-level window. Xlib/Xft provide the display and text primitives; libpng/libjpeg are narrowly scoped codec boundaries. Layout, controls, menus, modals, focus, animation, blur, selection, document state, parsing, persistence, diff, Base64, and tooling are authored in this source tree.

The most important invariant is:

> `MdDocument.source` is the only authoritative document representation.

The render model contains byte ranges into source and is rebuilt after source transactions. It is never serialized as an independent rich-text document. A rendered edit, table action, task toggle, image resize, link edit, search replacement, or history restore calls the same source transaction API and therefore participates in exact Undo/Redo.

## Layers

| Layer | Ownership | Important invariants |
|---|---|---|
| `core` | Bounded buffers/bytes, UTF-8 decode, required grapheme-like editing units, Base64, SHA-256, CRC-32, xorshift64*, paths, wildcards, files | Checked size arithmetic; strict UTF-8 and Base64 rejection; no external hash/codec command |
| `json` | Authored recursive-descent JSON parser and escaped writer | Duplicate object keys and malformed syntax fail with position context |
| `document` | Source, render blocks/headings, source mapping, cursor/selection, transactions, Undo/Redo, Markdown/plain text, search, statistics, links, tasks, tables | Transactions validate UTF-8, rebuild the mapped model, clear Redo on divergent edit |
| `diff` | Deterministic Myers line diff, token refinement, delta records, LZSS | Stable tie rules; corrupt/malformed delta or compressed stream rejected |
| `image` | PNG/JPEG/BMP decode, PNG/JPEG write, data URI format mapping | BMP is authored; libpng/libjpeg are codec-only; dimensions/overflow checked |
| `storage` | Safe save, prefs/recents, workspace/session, history, recovery, assets, Save As relocation, exports | Failed write keeps dirty source; checksums guard records; authored Markdown is outside metadata replacement |
| `ui` | X11/Xft window, custom painting/widgets, XIM/XIC, clipboard, Xdnd, commands, all four modes, interactions | Modal blocks background input; Preview rejects edits; commands share document/storage APIs |
| `tools` | `locscan`, `fixturegen`, `evidencecheck`, evidence assembly | Deterministic CLI contracts and non-zero failure classes |

## Document Transaction Flow

1. X11 input, a custom control, command palette item, or direct table/image interaction identifies a source range.
2. `md_document_replace` validates range boundaries and replacement UTF-8.
3. One `MdUndoEntry` records removed/inserted bytes plus before/after selection.
4. The source buffer changes and the entire source-mapped render model is rebuilt.
5. Redo is invalidated for a new divergent edit; an explicit Undo/Redo performs the inverse transaction.
6. Dirty/conflict/orphaned states remain independent: editing never silently authorizes a disk overwrite.

Typing coalesces compatible adjacent input. Structural commands, table operations, Replace All, image conversion/resize completion, history restore, and drag-move are deliberately single transactions.

## Unicode Editing Units

Source offsets are byte offsets, always checked at UTF-8 boundaries. Movement and deletion use authored helpers that keep together the normative acceptance fixtures: combining marks, variation selectors, emoji skin modifiers, and chained ZWJ sequences. Storage preserves exact scalar byte sequences. XIM commits UTF-8 through `Xutf8LookupString`; reset/cancel paths do not insert preedit bytes.

## Markdown Model

The block model recognizes paragraphs, ATX/Setext headings, thematic breaks, nested quotes, unordered/ordered/task items, fenced/indented code, GFM tables, images, raw HTML, and blank blocks. Inline rendering recognizes emphasis/strong/strike, escapes, code spans with variable backtick runs, links/autolinks, and images. Malformed/incomplete Markdown is kept as editable text and parsed conservatively.

Outline entries store level, source offset, and block identity, so duplicate labels remain distinct. Statistics are derived from source plus rendered/plain text and cache against document generation.

## X11 UI and Input

The application allocates one top-level window and an application back pixmap. It draws all controls with Xlib/Xft, including tabs, tree/outline, editor surfaces, custom menus/modals, status, capsule, ripples, and CPU-produced modal blur. A poll-driven event loop processes expose/configure, pointer/key, focus, selection, Xdnd, timers, autosave, external checks, and animation without a GUI toolkit.

Clipboard ownership uses `CLIPBOARD`, `UTF8_STRING`, `TARGETS`, and `XA_STRING` fallback. Cut deletes only after ownership succeeds. Xdnd v5 negotiates `text/uri-list`: supported images insert through asset paths and Markdown documents open as tabs.

## Storage and Failure Safety

Safe save writes to a same-directory temporary file, detects short/write/close errors, applies the existing mode where possible, and renames only after complete success. The injected ENOSPC, EACCES, partial-write, close, and rename faults cross that production boundary. External disk SHA-256 is compared to the document baseline; ordinary Save is blocked after a conflict until Reload/Keep/Compare/explicit overwrite resolves it.

Workspace state lives under `.mdeditor/` and is structurally validated. History records use a binary header, CRC/SHA-backed payload checks, full-snapshot boundaries, delta/LZSS payloads, retention, and pins. Recovery records are independently checksummed; corrupt entries are skipped while valid siblings remain openable. Preferences corruption falls back to defined defaults.

Relative assets are copied into a deterministic managed-assets directory with collision handling. Embedded assets use real `data:image/...;base64,` bytes. Save As supports copy/rebase or deliberate keep-reference policy. Portable single-file export embeds supported local images; portable Markdown+assets externalizes embedded images transactionally.

## Evidence Trust Model

The release scripts run real binaries and X11 application instances. `evidencegen` calculates:

- the application build SHA-256;
- a source-tree identity over every authored file in the LOC inventory;
- test-log, screenshot, fixture-manifest, and artifact sizes/digests;
- screenshot PNG header dimensions;
- exact performance fixture bindings.

`evidencecheck` independently reparses and verifies the manifest, normalized paths, root containment after symlink resolution, digests, dimensions, fixture manifests, test counts/categories, performance schemas, and failure schemas. Mutation tests prove it rejects missing files, digest changes, screenshot omission, failed counts, and path escape. Semantic screenshot review remains human-owned.
