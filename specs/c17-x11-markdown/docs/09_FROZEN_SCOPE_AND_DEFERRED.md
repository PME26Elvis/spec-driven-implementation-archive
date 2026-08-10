# 09 — v1.0 Frozen Scope and Explicitly Deferred Features

## 1. Status

This document replaces the draft open-decisions list.

The mandatory v1.0 scope is frozen by the normative documents in this package.

There are no `TBD` requirements whose absence permits a mandatory v1.0 feature to be omitted.

Items explicitly listed as deferred/optional below are not required for v1.0 and MUST NOT be used as substitutes for mandatory behavior.

## 2. Frozen Platform

v1.0 targets one native Linux desktop application written in C17.

X11/Xlib and/or XCB are the permitted native window-system surface.

A second operating system, Wayland-native backend, or cross-platform abstraction is not required.

Multiple top-level application windows are optional; one top-level application window with multi-document tabs is sufficient.

## 3. Frozen GUI Boundary

Qt, GTK, SDL, GLFW, Dear ImGui, Electron, browser/WebView shells, and comparable high-level GUI frameworks remain prohibited.

Custom widgets, layout, hit testing, editor surface, modal system, animations, blur, Markdown layout, and diff visualization remain application-authored.

X11 font/text facilities may be used for glyph drawing as defined by the engineering constraints; authoring a complete font rasterizer is not required.

## 4. Frozen Markdown Scope

Required Markdown is the CommonMark-oriented/GFM-style set explicitly enumerated by `03_MARKDOWN_AND_EDITING.md`.

Generic arbitrary HTML execution is not required and active HTML/web content MUST NOT be executed inside the editor.

The supported inline `<img>` subset used for persistent resized-image metadata is mandatory.

Mermaid, mathematical LaTeX rendering, diagram engines, syntax-extension plugin APIs, and embedded web content are deferred.

## 5. Frozen Editing Modes

All four modes are mandatory:

- Source.
- Split.
- Preview.
- Rendered Editing.

Rendered Editing is real source-backed editing and includes the structural behaviors frozen in `12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`.

## 6. Frozen Workspace Scope

Multi-document tabs, one active workspace root, file tree, Outline, workspace state, recent workspaces/files, and session restoration are mandatory.

Opening multiple workspace roots in one window is not required.

Opening multiple top-level windows is optional.

## 7. Frozen Image Scope

Mandatory readable image formats:

- PNG.
- JPEG.
- BMP.

PNG/JPEG may use the narrowly scoped system codec exception defined by the engineering constraints.

The assignment does not require an authored JPEG or PNG decoder from first principles.

The editor MUST still author its own:

- Base64 codec.
- Relative-path/asset behavior.
- Data-URI handling.
- Image selection and resize interaction.
- Image properties/context actions.
- Markdown/HTML image serialization.
- Portable image conversion/export logic.

GIF animation, WebP, AVIF, SVG rasterization, TIFF, HEIF, and camera RAW are optional.

## 8. Frozen Image Size Persistence

Unresized images may remain ordinary Markdown image syntax.

Persistently resized images use the constrained inline `<img>` representation defined in `05_IMAGES_AND_MEDIA.md`.

No opaque sidecar-only size metadata may be used as the sole persistence mechanism.

## 9. Frozen Image Storage Choices

Both are mandatory:

- Relative managed/local assets.
- Base64 data-URI embedded images.

Insertion-time choice, per-image conversion, Save As relocation protection, and both portable export forms are mandatory.

Raw bitmap clipboard image ingestion remains optional; file image drag/drop is mandatory.

## 10. Frozen Search Scope

Find, Find Next/Previous, Replace, and Replace All are mandatory.

Case-sensitive and deterministic case-insensitive modes plus whole-word option are mandatory.

Regular expressions are deferred.

Multi-file/workspace-wide search and replace is optional; v1.0 requires in-document search/replace.

## 11. Frozen Undo/Redo Scope

All structural transaction boundaries in `11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md` are mandatory.

Persistent undo history after closing/reopening the application is not required.

Document version history is separate and persistent.

## 12. Frozen Autosave/Recovery Scope

Recovery autosave is mandatory and separate from authored-file Save and version-history creation.

Default maximum dirty interval is 30 seconds, configurable within the Preferences bounds.

Recovery Center startup behavior and crash-recovery tests are mandatory.

Cloud sync and remote backup are not required.

## 13. Frozen Version Creation Policy

A history version is created after a successful explicit Save when source differs from the latest stored version.

An explicit Create Version command is mandatory.

Autosave recovery does not create user-visible versions.

Unchanged repeated Save does not create duplicates.

## 14. Frozen Version Storage/Retention

History uses periodic full snapshots at least every 20 versions, intermediate deterministic deltas, integrity metadata, and the specified LZSS compression profile when beneficial.

Retention limits are 200 versions and 64 MiB encoded payload per document, with pinned-version behavior as specified in `06_VERSION_HISTORY_AND_DIFF.md`.

Git or another VCS is not the implementation of this feature.

## 15. Frozen Diff Scope

Application-authored Myers-style line diff plus word/token refinement is mandatory.

Both side-by-side and inline/unified graphical views are mandatory.

Image-thumbnail visual diff is optional; source-level image-reference diff is mandatory.

Semantic AST diff is optional.

## 16. Frozen Table Scope

Rendered table editing MUST support:

- Direct cell editing.
- Keyboard cell navigation.
- Insert/delete row.
- Insert/delete column.
- Default/left/center/right column alignment.

Arbitrary merged cells, spreadsheet formulas, rich cell blocks, column pixel-resize persistence, and CSV import/export are not required.

## 17. Frozen Split/Outline Scope

The Split divider is draggable and persisted.

Synchronized scrolling uses block/source mapping and is enabled by default.

The Outline is live and heading-derived.

Pixel-identical source/preview scroll positions are not required.

## 18. Frozen Theme and Keyboard Scope

Light and Dark themes are mandatory.

Keyboard-only completion of core file/edit/navigation flows is mandatory.

Full screen-reader accessibility, AT-SPI integration, user-rebindable shortcuts, and complete WCAG conformance are not required.

The product MUST nevertheless expose visible focus and not encode diff/error state using color alone.

## 19. Frozen Font Scope

The product must visibly support the normative Traditional Chinese/Latin/emoji fixtures on the target environment.

Implementing a full font discovery engine, font editor, font download manager, or OpenType shaper from scratch is not part of v1.0.

The chosen allowed X11/system glyph path must not delegate application/editor layout semantics to a high-level GUI toolkit.

## 20. Frozen Link Policy

HTTP/HTTPS links use the external system handler.

Rendered Editing requires Ctrl+Click for activation so ordinary click remains an editing operation.

Relative Markdown links inside a workspace should open as editor documents.

An embedded browser is prohibited.

## 21. Frozen Preferences Scope

Mandatory preferences are those listed in `13_COMMANDS_RECENTS_PREFERENCES_KEYBOARD.md`.

A graphical shortcut remapper, custom CSS/theme editor, plugin preferences, cloud account settings, and telemetry settings are not required.

## 22. Frozen Performance Scope

`medium`, `large`, and `stress-long-line` generated fixtures plus responsiveness/failure gates are mandatory.

The task does not require benchmark parity with a particular commercial editor.

The thresholds are acceptance floors for avoiding demo-only implementations.

## 23. Frozen Development Utilities

All three Workstream A utilities are mandatory:

- `locscan`.
- `fixturegen`.
- `evidencecheck`.

They must be real C17 tools with tests, not shell aliases to existing utilities.

## 24. Export Scope

Mandatory export is limited to the two portable Markdown image-storage forms already specified:

- Single-file Markdown with local supported images embedded.
- Markdown plus managed asset directory.

Generic HTML export is deferred.

PDF export is deferred.

Image/screenshot export of whole documents is deferred.

## 25. Spellcheck and Language Services

Spellcheck, grammar checking, dictionary management, translation, autocomplete, and AI writing assistance are deferred.

They are intentionally excluded so the benchmark remains focused on editor, parser, UI engine, persistence, versioning, and verification engineering.

## 26. Plugin System

A plugin/extension API is deferred.

Mandatory functionality may not be supplied only through optional plugins.

## 27. Networking and Collaboration

Network access is not a product requirement.

Cloud document sync, collaborative multi-user editing, comments, remote repositories, remote images fetching, and account/login systems are deferred.

The editor MUST remain fully functional for required workflows offline with local files.

## 28. Security/Product Encryption

Document encryption is not part of this Markdown-editor v1.0 scope.

The earlier exploratory encrypted-database idea is not carried into this product.

Integrity checks for history/recovery/evidence are required where explicitly specified, but the product does not claim to be an encrypted vault.

## 29. What the Task Pack Does Not Regulate

The package intentionally does not regulate:

- Which model or agent implements it.
- MCP/tool usage.
- IDE/editor choice.
- Whether the implementer searches the web or reads specifications.
- Package-install workflow.
- Debugging workflow.
- Screenshot-capture command.
- Build-machine orchestration.

Those are experiment/execution conditions external to the software assignment.

The package regulates the delivered product, authored implementation boundaries, tests, evidence, and acceptance results.
