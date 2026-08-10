# 05 — Images and Media

## 1. Scope

Images are first-class Markdown document elements.

The application MUST provide a workflow that is more usable than requiring the user to manually type image syntax and dimensions.

## 2. Image Insertion

The application MUST provide an explicit image insertion action.

The user MUST be able to select a local image file and insert an image reference at the current logical insertion point.

The resulting document MUST contain a persistent Markdown-compatible representation.

An image that has never been resized MAY remain ordinary Markdown image syntax. Once an explicit display width/height is stored, the source MUST serialize that image using the supported inline HTML `<img>` subset defined below so dimensions remain portable in the Markdown file.

## 3. Image Preview

A valid local image reference MUST render as an image in Preview, Split preview, and Rendered Editing Mode.

The renderer MUST reserve sensible layout space and must not draw over surrounding text.

## 4. Missing Image

If an image file cannot be loaded:

- The document MUST remain editable.
- The application MUST show a visible broken/missing image placeholder in rendered views.
- The placeholder SHOULD expose enough path/alt information to identify the failing reference.
- Surrounding Markdown MUST remain intact.

## 5. Image Selection

In Rendered Editing Mode, clicking an image MUST allow it to become the active selected object without requiring source editing.

The selected state MUST be visually obvious.

The selected image MUST expose resize affordances.

## 6. In-Editor Resize

The user MUST be able to resize a selected image by dragging resize handles or an equivalent direct manipulation affordance.

The resize must occur visually in the editor as the pointer moves.

The interaction MUST NOT launch an external image editor.

## 7. Aspect Ratio

By default, direct corner resizing MUST preserve the image aspect ratio.

If freeform resizing is later supported, it MUST require an explicit modifier or separate control rather than occurring accidentally.

## 8. Resize Bounds

The application MUST prevent an image from being resized to zero or negative dimensions.

A documented minimum rendered size MUST preserve visible/selectable handles.

A resize that exceeds the content viewport MAY temporarily extend beyond the view, but the application MUST remain recoverable through scrolling or resize-back interaction.

## 9. Resize Commit and Undo

Pointer movement during a resize gesture MAY produce many visual frames, but releasing/committing the resize MUST create one logical document edit.

One Undo MUST restore the dimensions from before the gesture.

One Redo MUST restore the committed resized dimensions.

## 10. Persistence of Resize

After resizing, saving, closing, and reopening the Markdown file, the intended displayed size MUST be restored from the persisted document representation.

The application MUST NOT rely solely on an opaque in-memory property that disappears on reopen.

The mandatory persisted resized-image form is an inline HTML element of the form `<img src="SOURCE" alt="ALT" width="N">`, optionally including `height="N"` only when the user explicitly disables aspect-ratio lock. Attribute order MAY vary. `SOURCE` may be a relative asset path or supported image `data:` URI. The parser MUST escape/encode attribute content safely and MUST round-trip this supported subset. Ordinary unresized `![alt](source)` syntax remains supported.

## 11. Alt Text

Inserted images MUST have editable alt text.

The user MUST be able to inspect and change alt text without manually reconstructing the entire image syntax.

Alt text must persist to the Markdown source.

## 12. Image Path Handling

The editor MUST support local file paths.

Relative paths SHOULD be preferred when the chosen image lies under a suitable document-relative location.

Absolute paths MAY be represented but MUST remain explicit.

Saving a document under a different directory MUST NOT silently rewrite image paths unless a defined relocation policy applies.

Relative image paths MUST be resolved from the directory containing the Markdown document. On insertion in relative-asset mode, the application MUST copy/import the asset into a user-visible asset location within the workspace/document context when needed and write a normalized relative path using `/` separators. Save As to a different directory MUST offer `Copy assets and rewrite relative paths`, `Keep current references`, or `Cancel`; the default action MUST be the safe copy-and-rewrite option when references would otherwise break. Workspace relocation as a whole MUST preserve references because paths remain relative.

## 13. Right-Click Image Menu

Right-clicking a rendered image MUST open a context menu containing at least:

- Save Image As…
- Edit Alt Text…
- Reset to Intrinsic Size.
- Remove Image.

Additional image actions MAY be included.

## 14. Save Image As

Save Image As MUST copy/export the underlying image data to a user-chosen destination.

It MUST NOT save a screenshot of the rendered image when the original decodable image data is available.

If the destination exists, overwrite MUST require confirmation.

If the copy/write fails, an error MUST be shown and the source image reference MUST remain unchanged.

## 15. Reset to Intrinsic Size

Reset to Intrinsic Size MUST restore the image to its decoded natural dimensions subject to viewport/layout bounds.

The operation MUST be undoable if it changes the persisted display size.

## 16. Remove Image

Remove Image MUST remove the corresponding image construct from the Markdown document.

It MUST NOT delete the original image file from disk.

The removal MUST be undoable.

## 17. Drag and Drop

Local-file drag/drop image insertion is mandatory and uses the same relative/Base64 storage policy as ordinary image insertion. Windows Explorer/file-drop interoperability is defined in `11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md` and `16_WINDOWS_PLATFORM_CONTRACT.md`.

## 18. Copy/Paste Images

Raw bitmap/image clipboard import is optional in v1.0. UTF-8 text clipboard and image-file drag/drop are mandatory. If bitmap clipboard import is implemented, it MUST create a real persistent image representation rather than an ephemeral preview-only object.

## 19. Decoder Scope

PNG, JPEG, and BMP are mandatory readable/displayable formats. The Windows edition MAY use Windows Imaging Component (WIC) solely for these image codec operations under `01_ENGINEERING_CONSTRAINTS.md`; BMP may alternatively be authored directly. Base64 encode/decode, asset conversion, path logic, resize interaction, Markdown/HTML image serialization, and editor rendering/layout behavior remain application-owned. No submission may claim support for a format that is only represented by a placeholder thumbnail.

## 20. Mandatory Dual Persistence

The final product MUST support both relative-path image references and Base64-embedded data-URI images as defined in `10_WORKSPACE_TABS_AND_ASSETS.md`.

A single document MAY contain both representations at once.

Ordinary Save MUST preserve the representation chosen for each image.

## 21. Authored Base64 Codec

Base64 encoding and decoding used by embedded images MUST be implemented in authored C code.

The implementation MUST NOT delegate Base64 transformation to a shell command, external executable, scripting language, browser engine, or prohibited third-party library.

The codec MUST have direct unit tests including malformed-input rejection and exact binary round trips.

## 22. Insert Storage Choice

Image insertion MUST expose at least these choices:

- Copy into managed assets and write a relative reference.
- Embed original supported image bytes as a Base64 data URI.

The UI MAY remember a default, but the choice MUST remain changeable.

## 23. Representation Conversion

The image context menu MUST include working actions to:

- Embed a referenced image into the Markdown document.
- Externalize an embedded image into the managed assets directory.

Each successful conversion is one logical undoable document operation.

A failed conversion MUST leave source unchanged.

## 24. Save As Relocation

Save As MUST detect when relocating the Markdown file would change the meaning of relative image paths.

The user MUST be able to preserve working assets through copy/rebase, deliberately preserve raw paths with a warning, or cancel.

The application MUST NOT silently save a newly broken document when it can identify the relocation problem.

## 25. Portable Markdown Export

The explicit portable-export feature MUST support:

- Single-file Markdown with supported local images embedded.
- Markdown plus managed assets with embedded images externalized and local dependencies copied/rebased.

Export is distinct from ordinary Save so normal editing does not repeatedly prompt for storage representation.
