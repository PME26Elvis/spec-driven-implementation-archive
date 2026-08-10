# 10 — Workspace, Tabs, Session State, and Portable Assets

## 1. Scope

The application MUST support both standalone Markdown-file editing and folder-oriented workspace editing.

A workspace is a user-selected directory that provides:

- A root boundary for the file tree.
- A stable base for relative document assets.
- A location for workspace-specific editor state and version-history data.
- A context in which multiple Markdown documents may remain open as tabs.

Opening a workspace MUST NOT change the textual contents of Markdown files merely because the folder was opened.

## 2. Workspace Selection

The application MUST provide an explicit **Open Folder / Open Workspace** command.

The user MUST be able to choose an existing directory as the active workspace root.

Only one workspace root is required per application window.

Opening a new workspace while another workspace is active MUST run the workspace-transition flow defined below rather than silently discarding tabs or unsaved edits.

## 3. Standalone File Mode

The application MUST remain usable without an active workspace.

A Markdown file opened directly from disk outside a workspace MUST open as a normal document tab.

Standalone documents MUST support all editor modes, statistics, images, history, diff, save, and Save As behaviors that do not inherently require a workspace root.

Features that specifically require a workspace-relative location MUST present an explicit alternative or disabled explanation rather than failing silently.

## 4. Workspace Transition

When the user opens or switches to a different workspace, the application MUST inspect all currently open dirty documents.

The transition MUST NOT proceed until each dirty document is resolved through Save, Discard, or Cancel semantics.

Cancel MUST keep the current workspace and all tabs unchanged.

After a successful transition, the previous workspace session state MUST be persisted before the new workspace state is loaded.

## 5. Workspace Metadata Directory

Workspace-specific editor metadata MUST be stored beneath a single hidden directory at the workspace root:

`.mdeditor/`

This directory is application metadata and MUST NOT be treated as user-authored Markdown content.

At minimum it MUST contain or logically own:

- Workspace/session state.
- Persistent version-history records associated with files in the workspace.
- Integrity/version metadata required to decode those records.

The implementation MAY split these into multiple files/subdirectories.

The application MUST NOT store image assets inside `.mdeditor/` unless those files are purely editor metadata and are never referenced by user Markdown.

## 6. Metadata Isolation

Deleting `.mdeditor/` MUST NOT delete or corrupt Markdown documents or image assets.

If `.mdeditor/` is missing, unreadable, or intentionally removed, the application MUST still be able to open the workspace documents.

Loss of `.mdeditor/` MAY remove workspace UI state and built-in history, but MUST NOT make the authored files unusable.

Corruption of workspace metadata MUST be reported as a recoverable editor-state/history problem, not as corruption of the Markdown source files themselves.

## 7. Workspace Session State

The application MUST persist enough workspace state to restore a useful editing session after normal application restart.

At minimum persist:

- Ordered list of open document tabs that belong to the workspace.
- Active tab.
- Editor mode per open document.
- Cursor position per open document.
- Selection range when practical; if not restored, this limitation MUST be documented and tested.
- Vertical scroll position per editor surface.
- Split-view divider position.
- Per-document zoom.
- Left sidebar width.
- Expanded/collapsed directory nodes in the workspace tree.
- Sidebar collapsed/expanded state.

Window screen position is not mandatory.

## 8. Unsaved Documents and Session State

An untitled unsaved document MAY exist while a workspace is open.

A normal session-state write MUST NOT pretend that unsaved document contents are safely persisted unless the autosave/crash-recovery specification later explicitly requires this.

On clean application exit, dirty untitled documents MUST still pass through the normal Save / Discard / Cancel flow.

## 9. Session-State Write Safety

Workspace state updates MUST use a failure-safe write strategy.

A partially written state file MUST NOT replace the last valid state.

At minimum, state persistence MUST use a write-to-temporary-file then replace/rename commit pattern or an equivalently robust authored mechanism.

If the newest state is invalid on startup, the application SHOULD attempt the last known valid state if such a backup exists; otherwise it MUST start the workspace with a clean session and show a non-destructive warning.

## 10. Workspace File Tree

When a workspace is active, the primary window MUST provide a left-side workspace/file-tree region.

The tree MUST display directories and files under the workspace root.

`.mdeditor/` MUST be hidden from the ordinary workspace tree by default.

The tree MUST support expanding and collapsing directories.

Directory expansion state MUST be visually clear.

## 11. Tree Ordering

Within each directory, the default ordering MUST be deterministic:

1. Directories first.
2. Files second.
3. Case-insensitive lexical order for Latin filenames, with a deterministic byte/code-point tie break.

The implementation MUST NOT depend on filesystem enumeration order.

## 12. File Visibility

Markdown files MUST be clearly recognizable in the tree.

Supported local image files MUST be visible because they are relevant document assets.

Other ordinary files MAY also be shown.

The application MUST NOT attempt to render arbitrary binary files as Markdown when activated.

Unsupported file activation MUST result in a clear non-destructive message or an explicitly supported external/open behavior.

## 13. Tree Activation

Activating a Markdown file from the tree MUST:

- Focus the already-open tab if that path is already open.
- Otherwise open that file in a new tab.

The same physical file MUST NOT accidentally be represented by multiple independent editable tabs merely because it was activated repeatedly from the tree.

Path identity MUST account for normalized path semantics sufficiently to avoid obvious duplicates such as `./notes.md` and `notes.md` within the same workspace.

## 14. Tree Context Operations

The workspace tree MUST provide working operations for:

- New Markdown file.
- New folder.
- Rename.
- Delete.
- Refresh/rescan.

Delete MUST require confirmation for non-empty folders and for files not already recoverable through a platform trash API.

The specification does not require use of a desktop trash/recycle-bin service.

A failed filesystem operation MUST show an error and leave the tree/document state consistent with disk.

## 15. Rename and Open Tabs

Renaming an open file through the workspace tree MUST update the corresponding tab path and label after the filesystem rename succeeds.

If the rename fails, the tab and tree MUST retain the old path.

Renaming a directory containing open files MUST update all affected open-document paths after success.

Relative image resolution MUST be recomputed for moved/renamed Markdown documents.

The application MUST NOT silently rewrite image references solely because a parent directory was renamed unless the asset relocation policy explicitly requires it.

## 16. Delete and Open Tabs

Deleting a file that is open in a tab MUST require an explicit confirmation that makes the open-document consequence clear.

A dirty open file MUST NOT be deleted without separately resolving its unsaved state.

After confirmed deletion, the application MAY keep the buffer as an untitled/recovered document or close it; the chosen behavior MUST be consistent and covered by tests.

For this task pack, the preferred mandatory behavior is to keep a dirty or open deleted buffer as an **orphaned unsaved tab** so user content is not lost.

## 17. Multiple Document Tabs

The editor MUST provide a tab strip for open documents.

Each tab MUST show at minimum:

- A useful document name.
- Active/inactive visual state.
- Dirty/modified state.
- Close affordance.

Untitled tabs MUST use deterministic names such as `Untitled 1`, `Untitled 2`, and so on for the current session.

## 18. Tab Activation

Clicking a tab MUST activate it without modifying document content.

Switching tabs MUST restore that document's editor mode, cursor, selection policy, scroll position, and zoom state.

Tab switching MUST NOT create a version-history snapshot by itself.

## 19. Tab Closing

Closing a clean tab MUST close it immediately.

Closing a dirty tab MUST present Save / Discard / Cancel.

Cancel MUST leave the tab open and active.

A failed save MUST prevent closure.

## 20. Tab Reordering

Tabs MUST be reorderable by pointer drag.

The insertion location MUST be visible during drag.

Reordering tabs MUST affect session state only and MUST NOT set document dirty state.

The new order MUST survive a normal workspace restart.

## 21. Tab Overflow

The application MUST remain usable when more tabs are open than fit horizontally.

The tab strip MUST provide a deterministic overflow strategy such as horizontal scrolling plus an overflow/list button.

Tabs MUST NOT shrink until labels and close controls become unusable.

The active tab MUST be brought into view automatically.

## 22. Mandatory Tab Shortcuts

At minimum:

- `Ctrl+W` — close active tab.
- `Ctrl+Tab` — activate next tab.
- `Ctrl+Shift+Tab` — activate previous tab.
- `Ctrl+Shift+T` — reopen the most recently closed file-backed tab when still addressable.

Reopening a dirty discarded tab is not required unless crash recovery later defines a safe source for that content.

## 23. Sidebar Resize and Collapse

The workspace sidebar MUST be resizable by dragging its divider.

A documented minimum and maximum width MUST prevent it from consuming the entire editor or becoming impossible to restore.

The sidebar MUST support animated collapse/expand behavior consistent with the custom motion specification.

Its last width and collapsed state MUST be stored in workspace session state.

## 24. Image Persistence Models

The application MUST support two first-class image persistence models inside Markdown documents:

1. **Relative asset reference**.
2. **Embedded Base64 data URI**.

Both models MUST render, resize, survive save/reopen, participate in version history, and support `Save Image As…`.

Neither model may be implemented as a placeholder alias for the other.

## 25. Relative Asset References

For relative-asset mode, the Markdown image destination MUST resolve relative to the Markdown document's directory using ordinary relative path semantics.

The application MUST be able to insert an image by copying the selected source image into a workspace/document asset directory and referencing the copied file relatively.

The source image outside the workspace MUST NOT be deleted or modified by insertion.

## 26. Default Asset Directory

For documents inside a workspace, the default managed asset directory MUST be:

`assets/`

at the workspace root unless a document-specific conflict or configuration requires a nested path.

The application MAY allow changing the default asset directory in workspace settings, but `assets/` support is mandatory and acceptance tests will use it.

For a standalone saved Markdown document without a workspace, the default managed asset directory MUST be a sibling directory named:

`<document-basename>.assets/`

Example: `report.md` → `report.assets/`.

## 27. Asset Filename Collision Policy

Copying an image into a managed asset directory MUST NOT silently overwrite an unrelated existing file.

If a same-name file exists:

- If byte-for-byte identical, the implementation MAY reuse it.
- Otherwise, it MUST generate a deterministic non-colliding filename or request explicit overwrite/rename.

The final chosen path MUST be reflected in the Markdown source.

## 28. Embedded Base64 Data URI

Embedded image mode MUST serialize the image bytes directly into the Markdown image destination as a `data:` URI with Base64 payload.

For example, the structural form is:

`![alt](data:image/<type>;base64,<payload>)`

The actual MIME subtype MUST correspond to the accepted image format.

The Base64 encoder and decoder MUST be authored in C as part of the implementation.

Calling an external Base64 command, shelling out to another program, or using a third-party Base64 implementation does not satisfy this requirement.

## 29. Base64 Correctness

The authored Base64 implementation MUST correctly handle:

- Empty input.
- 1-byte remainder.
- 2-byte remainder.
- 3-byte aligned input.
- Padding with `=`.
- Payloads containing all possible byte values.
- Rejection of malformed characters/padding in embedded data.

Round-trip tests MUST compare decoded bytes exactly to the original image bytes.

## 30. Embedded-Image Size and Performance

Embedding an image increases Markdown source size substantially.

The editor MUST remain responsive with the embedded-image payloads generated by the mandatory `medium` and `large` profiles in `15_DEV_FIXTURES_AND_EVIDENCE.md`.

The source editor MAY visually abbreviate very long data-URI payloads only if the underlying source remains exactly editable/recoverable and there is an explicit way to reveal/edit raw source.

A fake token such as `[embedded image]` MUST NOT replace the actual Markdown bytes on disk.

## 31. Image Insert UX

When inserting a local image, the application MUST offer a clear persistence choice:

- **Copy to assets and use relative path**.
- **Embed in Markdown (Base64)**.

The UI MAY remember a workspace/document default so the user is not repeatedly interrupted.

The remembered default MUST remain visible/changeable from an image-insertion settings affordance or equivalent control.

## 32. No Save-Time Nagging

Ordinary `Save` MUST preserve the current image representation and MUST NOT ask the user to choose relative-vs-embedded mode on every save.

Changing plain text elsewhere in a document MUST NOT unexpectedly convert its images.

## 33. Per-Image Conversion

A rendered image context menu MUST allow conversion of the selected image between supported representations when conversion is possible:

- **Embed Image in Document**.
- **Externalize Image to Assets**.

Conversion MUST be a real document edit, set dirty state, and be undoable as one logical operation.

## 34. Embed Conversion

Converting a relative/local image to embedded mode MUST:

1. Read the referenced image bytes.
2. Determine/validate the supported image media type.
3. Base64-encode the exact bytes.
4. Replace the image destination with a data URI.
5. Preserve alt text and persisted display dimensions.

The original image file MUST NOT be deleted automatically.

If reading or encoding fails, the Markdown source MUST remain unchanged.

## 35. Externalize Conversion

Converting an embedded image to relative-asset mode MUST:

1. Parse and validate the data URI.
2. Base64-decode the payload.
3. Validate the decoded image according to mandatory image-format rules.
4. Choose the managed asset destination.
5. Write the asset atomically.
6. Replace the image destination with the correct relative path only after the asset write succeeds.
7. Preserve alt text and persisted display dimensions.

Failure at any step MUST leave the original embedded Markdown intact.

## 36. Save As and Relative Images

When `Save As` moves a Markdown document to another directory, existing relative paths may resolve differently.

Before commit, the application MUST detect relative image references whose resolved target would change or become missing at the new destination.

The Save As flow MUST provide explicit choices sufficient to avoid accidental breakage:

- Copy/rebase referenced local assets so the new file remains self-contained with working relative references.
- Preserve references exactly as written, with a warning when their resolved meaning changes.
- Cancel.

The application MUST NOT silently create broken references.

Embedded data-URI images are unaffected by document relocation.

## 37. Portable Markdown Export

The application MUST provide an explicit **Export Portable Markdown…** command separate from ordinary Save/Save As.

This command is intended to transform asset representation deliberately.

It MUST support at least:

- **Single-file Markdown**: convert all decodable local/relative supported images to embedded Base64 data URIs in the exported copy.
- **Markdown + assets folder**: externalize all embedded supported images and copy/rebase local image dependencies into an assets directory accompanying the exported Markdown.

Export MUST NOT silently alter the currently open source document unless the user explicitly chooses to replace/adopt the exported representation.

## 38. Export Transaction Safety

Portable export MUST stage output so a partial failure does not leave a falsely successful package.

If any mandatory image cannot be read/decoded/externalized, the export MUST report the affected image and MUST NOT claim full success.

The original document and original assets MUST remain unchanged by a failed export.

## 39. Relative Path Normalization

Newly generated relative image paths MUST use `/` as the Markdown path separator.

The path written to Markdown MUST be relative to the Markdown file, not merely relative to the workspace root, unless the two are equivalent for that file.

The implementation MUST correctly handle Markdown documents in nested workspace directories.

## 40. Workspace Move Portability

If a workspace directory containing both Markdown files and their referenced `assets/` tree is moved as a whole, relative image references MUST continue to work without source rewriting.

Acceptance tests MUST cover this portability property.

## 41. Missing Relative Asset Repair

When a relative image cannot be resolved, the missing-image placeholder context menu SHOULD provide **Locate/Relink Image…**.

If implemented, relinking MUST update only the selected image destination and be undoable.

Relinking is mandatory for v1 of this task pack.

## 42. Data URI Error Handling

Malformed or unsupported data URIs MUST NOT crash the renderer.

The editor MUST preserve the literal source text and show a visible invalid-image placeholder.

The placeholder MUST distinguish at least:

- Malformed Base64/data-URI syntax.
- Unsupported media type.
- Decoded bytes that fail mandatory image validation.

## 43. `Save Image As…` Across Representations

For a relative image, `Save Image As…` MUST copy the referenced image bytes.

For an embedded image, it MUST decode and write the exact embedded bytes.

The operation MUST NOT use a screenshot of the rendered image when source bytes are available.

## 44. Version History Interaction

Version history MUST capture Markdown source representation exactly enough to distinguish relative-path and Base64-embedded forms.

Converting image storage mode MUST appear in history/diff as a document change.

The graphical diff SHOULD avoid dumping thousands of Base64 characters as the only human-visible explanation; it MAY collapse unchanged/opaque data payloads and present an image-aware summary while retaining access to raw source diff.

## 45. Workspace and History Identity

History association for workspace files MUST remain stable across normal application restarts.

Renaming a file through the application's workspace tree MUST migrate/retain its history association rather than silently starting a fresh unrelated history.

Copying a file outside the application is not required to carry its hidden history metadata.

## 46. Mandatory Workspace Acceptance Cases

At minimum, tests MUST cover:

- Open a workspace.
- Expand/collapse nested folders.
- Create a Markdown file from the tree.
- Open several files into tabs.
- Re-activate an already-open file without duplicate tab creation.
- Reorder tabs and restart; order restores.
- Restore active tab, cursor, scroll, mode, zoom, and sidebar state.
- Rename an open file.
- Rename a directory containing open files.
- Delete an open file and exercise the orphaned-tab policy.
- Switch workspace with one or more dirty tabs.
- Corrupt workspace state and recover without damaging source files.

## 47. Mandatory Image-Representation Acceptance Cases

At minimum, tests MUST cover:

- Insert image as relative asset.
- Verify copied asset bytes equal source bytes.
- Insert image as Base64 data URI.
- Decode embedded payload and verify exact byte equality.
- Save/reopen both representations.
- Resize and preserve dimensions for both representations.
- Convert relative → embedded and Undo/Redo.
- Convert embedded → relative and Undo/Redo.
- Handle filename collision in assets directory.
- Save As a document containing relative images to another directory.
- Move a complete workspace and verify relative images still resolve.
- Export single-file Markdown with all supported images embedded.
- Export Markdown + assets with embedded images externalized.
- Right-click Save Image As for both forms.
- Broken relative reference repair.
- Malformed Base64 placeholder/error.

## 48. Prohibited Substitutes

The following do not satisfy this subsystem:

- Storing absolute paths while labeling them relative.
- Keeping image bytes only in editor metadata while writing a fake Markdown placeholder.
- Treating Base64 as optional text that the renderer does not decode.
- Calling `base64`, Python, Node, browser APIs, or another executable to perform Base64 conversion.
- Saving a rendered screenshot instead of original embedded/referenced bytes.
- Using duplicate tabs with independent unsynchronized buffers for the same file.
- Storing unsaved Markdown content only in workspace session metadata and claiming it is saved.
- Hiding a broken Save As relocation problem instead of resolving or warning about it.
