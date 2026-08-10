# 01 — Engineering Constraints

## 1. Target

The product MUST be a native Linux desktop application.

The implementation language MUST be ISO C17-compatible C.

The implementation MUST compile as C; C++ compilation is not an acceptable substitute.

## 2. Allowed Platform Surface

The implementation MAY use:

- ISO C17 standard library facilities.
- POSIX/Linux operating-system facilities where required for files, directories, clocks, process-visible state, and secure system services.
- X11/Xlib and/or XCB as the native window-system interface.
- Basic X11 facilities for window creation, event handling, pointer/keyboard input, clipboard/selection integration, and presenting application-rendered output.

Use of a platform API is permitted only where it supplies operating-system integration rather than a ready-made application feature that this assignment explicitly requires the implementer to build.

## 3. Prohibited UI/Rendering Substitutions

The application MUST NOT use third-party or high-level GUI/application frameworks, including but not limited to:

- Qt.
- GTK.
- SDL.
- GLFW.
- Dear ImGui.
- Electron.
- Chromium Embedded Framework.
- WebView-based application shells.
- Tk.
- wxWidgets.
- FLTK.
- Any browser-based UI rendered as the primary application interface.

The application MUST NOT embed a web browser to obtain Markdown rendering, layout, animation, blur, editing, or diff behavior.

## 4. Required Custom UI Work

The following MUST be implemented by the submission rather than delegated to a prohibited toolkit:

- Widget state model.
- Button rendering and states.
- Menus and menu items.
- Toolbars.
- Navigation controls.
- Status bar.
- Scrollbars used by the application content surfaces.
- Text editing surface.
- Selection rendering.
- Caret rendering.
- Dialogs and modal surfaces.
- Hit testing.
- Focus management.
- Keyboard navigation for required application controls.
- Layout calculations.
- Application-level animation timeline and easing.
- Ripple animation.
- Border glow effect.
- Drop shadows.
- Application-content blur used by modal or frosted-glass effects.
- Rendered Markdown layout.
- Diff visualization.

## 5. Software Rendering Requirement

The application MUST maintain an application-controlled rendering path sufficient to draw the required custom UI.

The submission MUST NOT satisfy the modern UI requirements by composing ordinary native X11 child controls with default platform appearance.

The rendering design MAY use one or more pixel buffers, pixmaps, or equivalent application-managed surfaces.

The exact internal rasterization architecture is not prescribed.

## 6. Text and Unicode

The application MUST support UTF-8 document content.

At minimum, editing, searching, cursor movement, selection, statistics, save/reopen, and diff operations MUST behave correctly for mixed Traditional Chinese and ASCII/Latin text.

The application MUST NOT treat UTF-8 continuation bytes as independently editable characters.

User-visible cursor movement and deletion MUST avoid producing invalid UTF-8.

Where grapheme-cluster behavior is specified elsewhere, the implementation MUST follow that behavior rather than byte-oriented movement.

### 6.1 Glyph Drawing Boundary

The assignment tests text editing, Unicode correctness, layout, and custom UI behavior; it does not require implementing a modern TrueType/OpenType rasterizer from scratch.

The implementation MAY use X11 text/font-set facilities available through the allowed X11 surface for glyph drawing.

Using those facilities MUST NOT delegate editor layout, selection, caret placement, Markdown layout, widget layout, or rich-text behavior to a high-level toolkit.

Traditional Chinese text required by the fixtures MUST be visibly renderable on the target Linux environment through the chosen allowed font path.

## 7. Markdown Engine

The Markdown parser and renderer MUST be implemented as part of the submission.

The implementation MUST NOT call an external Markdown renderer, command-line converter, browser engine, or preinstalled Markdown library to generate the required rendered surface.

## 8. Versioning and Diff

The document version-history subsystem MUST be application-owned.

The product MUST NOT rely on Git, another VCS executable, or a shell command as the implementation of document history or graphical diff.

The diff algorithm required by `06_VERSION_HISTORY_AND_DIFF.md` MUST be implemented by the submission.

## 9. Images

Image handling MUST NOT be implemented by opening images in an external editor for resizing.

The visible resize interaction MUST occur within the editor.

The application MUST decode and display PNG, JPEG, and BMP images. PNG and JPEG are a narrowly scoped exception to the no-third-party rule: the implementation MAY use the system `libpng` and `libjpeg` C codec APIs (or ABI-compatible system equivalents) solely for image encode/decode. BMP MAY be decoded directly by the submission. Image codec libraries MUST NOT provide UI, layout, Markdown parsing, Base64 handling, resizing interaction, asset management, or rendering-engine behavior. Shelling out to image conversion programs is not an acceptable implementation of mandatory image support.

## 10. Build and Repository Expectations

The repository MUST include a deterministic documented build entry point.

Generated build products MUST be separable from authored source and documentation.

The repository MUST not require checked-in binaries as the only way to run core functionality.

The build MUST fail clearly if a mandatory source file is absent.

## 11. No Fake Implementations

The following are prohibited:

- Placeholder controls that visually exist but do nothing.
- Hard-coded screenshots presented as UI.
- Hard-coded Markdown previews that do not derive from current document content.
- Hard-coded diff output.
- Hard-coded statistics.
- Test-only behavior that bypasses production logic.
- Mock persistence presented as version history.
- Disabled controls for mandatory features.
- “Coming soon” panels for mandatory features.
- Launching an external application to perform a mandatory in-app function unless explicitly permitted.

## 12. Robustness Baseline

The application MUST handle malformed Markdown as editable text and MUST NOT require Markdown validity before saving.

A failure in preview parsing MUST NOT silently discard source text.

A failure to load an image MUST NOT corrupt surrounding document content.

A failed save MUST leave the current in-memory document available for retry or Save As.

A failed version-history operation MUST not silently overwrite the live document.
