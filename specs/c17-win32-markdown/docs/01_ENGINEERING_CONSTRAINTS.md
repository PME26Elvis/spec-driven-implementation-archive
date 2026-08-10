# 01 — Engineering Constraints

## 1. Target

The product MUST be a native Windows desktop application.

The implementation language MUST be ISO C17-compatible C.

The implementation MUST compile as C; compiling the application sources as C++ is not an acceptable substitute.

The normative acceptance platform is 64-bit Windows 11 on an ordinary desktop session. The implementation MAY additionally support Windows 10 or other Windows editions, but broader compatibility MUST NOT be used to weaken any v1.0 requirement.

The application MUST be a native Win32 desktop process. UWP, packaged web shells, browser applications, and .NET desktop applications are not substitutes.

## 2. Allowed Windows Platform Surface

The implementation MAY use ISO C17 library facilities and low-level/native Windows SDK facilities needed to integrate with the operating system.

The allowed system boundary includes, when required:

- `Kernel32`-class facilities for files, directories, memory, clocks, synchronization, process state, and durable writes.
- `User32`-class facilities for top-level windows, the message loop, pointer/keyboard input, clipboard integration, DPI notifications, and application-window integration.
- `Gdi32`-class facilities for presenting an application-controlled bitmap/framebuffer and narrowly scoped glyph drawing.
- `Shell32` and ordinary Windows shell integration for external link activation, file drops, and platform file/folder selection where specified.
- COM/OLE initialization only where needed for permitted Windows system components such as the Common Item Dialog or Windows Imaging Component.
- Windows Imaging Component (WIC) solely for required image codec decode/encode.
- `Imm32`/standard Win32 IME integration for composition, committed text, and candidate/composition placement.
- DirectWrite or GDI text APIs solely as a system glyph shaping/rasterization boundary under Section 6.1.
- Windows cryptographic hashing/random facilities where a requirement explicitly permits an operating-system cryptographic primitive.
- Other Windows SDK calls that provide equivalent low-level operating-system integration without implementing an application feature that this task explicitly requires the submission to author.

Use of a Windows API is permitted only where it supplies operating-system integration or a narrowly scoped system primitive. It MUST NOT be used as a shortcut to delegate a mandatory editor, Markdown, widget, layout, animation, diff, versioning, Base64, workspace, statistics, or verification feature.

The exact API calls used inside this allowed boundary are not prescribed unless a later requirement defines observable behavior that depends on a Windows semantic.

## 3. Prohibited UI/Rendering Substitutions

The application MUST NOT use third-party or high-level GUI/application frameworks, including but not limited to:

- Qt.
- GTK.
- SDL.
- GLFW.
- Dear ImGui.
- Electron.
- Chromium Embedded Framework.
- WebView2 or other WebView-based application shells.
- Tk.
- wxWidgets.
- FLTK.
- WinUI/XAML as the application UI implementation.
- WPF.
- Windows Forms.
- MFC.
- ATL UI wrappers.
- Avalonia or other managed cross-platform UI frameworks.
- Any browser-based UI rendered as the primary application interface.

The application MUST NOT embed a web browser to obtain Markdown rendering, layout, animation, blur, editing, or diff behavior.

The mandatory editor surface MUST NOT be implemented using the Win32 `EDIT` control, Rich Edit control, or another native rich-text control.

The mandatory tab strip, workspace tree, buttons, menus, status bar, modal surfaces, context menus, scrollbars, and Markdown rendered surface MUST NOT be implemented by composing standard Win32 common controls such as native Button, Tab, TreeView, ListView, Toolbar, StatusBar, or comparable controls and then styling them superficially.

Native operating-system Open/Save/Folder picker dialogs are an explicit exception: they MAY be used for filesystem selection because they are operating-system integration, not the authored in-application modal/widget system. The normal Windows title bar, minimize/maximize/close buttons, resize border, and system window menu MAY also remain OS-provided.

## 4. Required Custom UI Work

The following MUST be implemented by the submission rather than delegated to a prohibited toolkit/control:

- Widget state model.
- Button rendering and states.
- Menus and menu items inside the application client area.
- Toolbars.
- Navigation controls.
- Status bar.
- Tabs.
- Workspace/file tree.
- Outline tree.
- Scrollbars used by application content surfaces.
- Text editing surface.
- Selection rendering.
- Caret rendering.
- Application modal/dialog surfaces other than the explicit native filesystem-picker exception.
- Context menus.
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
- Table rendered-editing interaction.
- Diff visualization.

## 5. Software Rendering Requirement

The application MUST maintain an application-controlled rendering path sufficient to draw the required custom UI.

The submission MUST NOT satisfy the modern UI requirements by composing ordinary Win32 controls with default platform appearance.

A typical compliant design is an application-owned 32-bit pixel surface that is presented to the Win32 window through GDI/DIB facilities, but the exact internal architecture is not prescribed.

The implementation MAY use `CreateDIBSection`, `StretchDIBits`, `BitBlt`, or equivalent low-level Windows presentation primitives.

Direct2D, DirectComposition, Direct3D effect graphs, or other higher-level accelerated scene/effect systems MUST NOT be used to obtain mandatory widget rendering, blur, shadows, animation, or Markdown layout. Hardware acceleration is not required for v1.0.

The implementation MUST author the required raster/compositing behavior for rounded surfaces, opacity composition, custom shadows, glow, ripple, application-content blur, clipping, and animation state rather than delegating those visual requirements to a retained-mode UI/effect framework.

The exact software rasterizer organization is not prescribed.

## 6. Text and Unicode

The document model MUST use UTF-8 content.

At minimum, editing, searching, cursor movement, selection, statistics, save/reopen, version history, diff, clipboard round trips, and rendered editing MUST behave correctly for mixed Traditional Chinese and ASCII/Latin text.

The application MUST NOT treat UTF-8 continuation bytes as independently editable characters.

User-visible cursor movement and deletion MUST avoid producing invalid UTF-8.

Where grapheme-cluster behavior is specified elsewhere, the implementation MUST follow that behavior rather than byte-oriented movement.

Windows filesystem paths and native text-bearing Win32 interfaces MUST be handled through Unicode-capable Windows APIs. The implementation MUST NOT depend on the process ANSI code page for Traditional Chinese filenames, workspace paths, clipboard text, IME text, or native file dialogs.

The application MAY convert between its internal UTF-8 representation and Windows UTF-16 at the operating-system boundary.

### 6.1 Glyph Drawing Boundary

The assignment tests text editing, Unicode correctness, layout, and custom UI behavior; it does not require implementing a modern TrueType/OpenType font rasterizer or complex-script shaper from first principles.

The implementation MAY use DirectWrite or GDI/system font facilities solely to resolve fonts, shape glyph runs when necessary, obtain glyph metrics, and rasterize/draw glyphs.

Using those facilities MUST NOT delegate editor paragraph layout, Markdown block layout, selection semantics, caret placement, widget layout, scrolling, rich-text editing, or rendered-editing behavior to a ready-made control.

Traditional Chinese and the normative emoji/combining fixtures MUST be visibly renderable on the acceptance Windows environment through the chosen allowed font path. Missing-glyph boxes for ordinary Traditional Chinese fixture text are a failure.

## 7. Win32 Message/Event Boundary

The application MUST own its main event/message loop and application state transitions.

Required input and paint behavior MUST remain responsive under ordinary Win32 message delivery.

Long parsing, diff, history, recovery, or I/O operations MUST NOT cause the UI to ignore paint/input messages for the prolonged durations prohibited by the performance specification.

The application MAY use worker threads for expensive non-UI work, but UI state mutation and rendering synchronization MUST be correct and deterministic.

## 8. DPI Awareness

The Windows application MUST be DPI-aware.

On the normative acceptance platform it MUST correctly handle at least 100%, 150%, and 200% scale factors without clipped mandatory controls, incorrect hit testing, displaced caret/selection geometry, or unusably small/large custom UI.

The application MUST respond correctly when a window is moved between monitors with different DPI and receives the corresponding DPI-change behavior.

A Per-Monitor-V2-aware implementation is the expected baseline. The exact manifest/API mechanism is not prescribed, but silently relying on DPI virtualization and then accepting blurred/misaligned custom rendering is not compliant.

Logical layout metrics MAY be expressed in application logical units, but conversion to device pixels MUST be consistent for rendering and hit testing.

## 9. Markdown Engine

The Markdown parser and renderer MUST be implemented as part of the submission.

The implementation MUST NOT call an external Markdown renderer, command-line converter, browser engine, COM HTML component, or preinstalled Markdown library to generate the required rendered surface.

## 10. Versioning and Diff

The document version-history subsystem MUST be application-owned.

The product MUST NOT rely on Git, another VCS executable, PowerShell, a shell command, or a Windows shell extension as the implementation of document history or graphical diff.

The diff algorithm required by `06_VERSION_HISTORY_AND_DIFF.md` MUST be implemented by the submission.

## 11. Images

Image handling MUST NOT be implemented by opening images in an external editor for resizing.

The visible resize interaction MUST occur within the editor.

The application MUST decode and display PNG, JPEG, and BMP images.

The Windows Imaging Component is a narrowly scoped system-codec exception and MAY be used solely for image decode/encode and pixel-format conversion required to make those formats available to the application renderer.

WIC MUST NOT provide UI, Markdown parsing, Base64 handling, asset management, image-selection/resize interaction, application layout, or editor-rendering behavior.

BMP MAY instead be decoded/encoded directly by the submission.

Shelling out to image conversion programs is not an acceptable implementation of mandatory image support.

## 12. Clipboard / Shell / File Picker Exceptions

The Windows clipboard may be accessed through the standard Win32 clipboard APIs. Plain text interoperability MUST use the Unicode clipboard format defined by the Windows platform contract.

Explorer-compatible file drag/drop MAY be implemented through `WM_DROPFILES`, OLE drag/drop, or another native Windows shell mechanism that meets the observable acceptance behavior. The task pack does not prescribe which one.

Opening external HTTP/HTTPS links MAY delegate only the final launch to the user's registered Windows handler. The application MUST NOT embed a browser.

Native Common Item Dialog / file-picker UI MAY be used for Open, Save As, image selection, workspace folder selection, and equivalent filesystem-picking operations. Custom application confirmation/error/version/statistics/preferences/diff modals remain authored UI.

## 13. File/Directory Platform Integration

Mandatory file operations MUST use Unicode-capable Windows filesystem APIs or an equivalent C runtime path that demonstrably supports the normative Unicode/long-path fixtures.

The editor MUST not keep ordinary document files exclusively locked merely because they are open in a tab. External modification, rename, and deletion workflows defined elsewhere must remain testable when the filesystem and sharing semantics permit them.

External-change detection MAY use `ReadDirectoryChangesW`, metadata/content polling, or another robust mechanism. The mechanism is not prescribed; the observable behavior is.

Safe replacement writes MUST satisfy the Windows-specific durability/atomicity contract in `14_PERFORMANCE_AND_FAILURE_HANDLING.md`.

## 14. Build and Repository Expectations

The repository MUST include a deterministic documented Windows build entry point.

The application and Workstream A utilities MUST build from authored C source without requiring C++ compilation.

The submission MAY choose one documented native Windows C toolchain; supporting every Windows compiler is not required.

Generated build products MUST be separable from authored source and documentation.

The repository MUST not require checked-in executable binaries as the only way to run core functionality.

The build MUST fail clearly if a mandatory source file is absent.

The final application executable MUST be a native Windows desktop executable, not a launcher for a browser/runtime-hosted app.

## 15. No Fake Implementations

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
- Hosting a hidden Rich Edit/WebView/native common control and drawing a cosmetic custom layer over it while delegating the mandatory behavior to the hidden control.

## 16. Robustness Baseline

The application MUST handle malformed Markdown as editable text and MUST NOT require Markdown validity before saving.

A failure in preview parsing MUST NOT silently discard source text.

A failure to load an image MUST NOT corrupt surrounding document content.

A failed save MUST leave the current in-memory document available for retry or Save As.

A failed version-history operation MUST not silently overwrite the live document.

A Win32 API failure MUST be converted into an application-level error path that preserves user data according to the corresponding requirement; raw `GetLastError()` values alone are not sufficient user-facing error handling.
