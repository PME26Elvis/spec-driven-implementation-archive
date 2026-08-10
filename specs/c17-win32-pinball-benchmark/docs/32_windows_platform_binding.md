# 32 — Windows Platform Binding and Native API Boundary

This document is normative for the Windows v1.0.0 variant. It replaces Linux/X11 platform assumptions while intentionally preserving every platform-independent product, physics, scene, replay, editor, gameplay, test, and evidence requirement.

## 1. Platform objective

Pinball Sandbox is a native Win32 desktop program written in C17.

The Windows port SHALL feel like the same benchmark assignment, not a separate easier implementation. Windows supplies only the operating-system boundary that X11/POSIX supplied in the sibling variant. The application still implements its own UI engine, software renderer, editor model, physics, persistence formats, replay, diagnostics, test runner, and engineering utilities.

## 2. Required desktop target

Mandatory baseline:

- Windows 10 22H2 x64; and
- Windows 11 x64.

A release may be evaluated on either. The implementation SHALL not intentionally depend on a Windows 11-only UI framework or visual material.

ARM64 is optional and does not replace the x64 requirement.

## 3. C language contract

Production application and required engineering utilities are C17.

C++ source, C++ UI frameworks, C++/WinRT, Rust, C#, managed CLR code, JavaScript front ends, or generated bridge programs do not satisfy the implementation requirement.

Windows SDK headers/import libraries and the compiler C runtime are platform/build dependencies, not prohibited third-party product frameworks.

## 4. Toolchain neutrality

This package does not mandate MSVC, clang-cl, MinGW-w64, CMake, Ninja, Make, Visual Studio, or another development workflow.

A conforming build may use any toolchain that produces the required native C17 Win32 behavior.

The delivered release SHALL document how it is built, but the benchmark specification does not prescribe installation commands, package managers, IDEs, agents, or build hosts.

## 5. Runtime dependency rule

The program SHALL NOT require a separately installed high-level application framework.

Windows system DLLs and compiler runtime components are allowed. If the chosen compiler produces a runtime dependency that is not guaranteed on the supported clean Windows target, the delivery SHALL include or statically link the legally redistributable runtime needed for the executable to start.

Fetching a runtime after launch is prohibited.

## 6. Permitted Win32 API categories

The implementation MAY use low-level Windows SDK services in the following categories, limited to their stated purpose.

### 6.1 User32

Permitted for:

- window-class registration;
- creating/destroying the real top-level window;
- receiving and dispatching window messages;
- client sizing, minimize/maximize/restore, title update, and ordinary non-client integration;
- keyboard, mouse, wheel, focus, activation, capture, and cursor handling;
- Unicode system clipboard integration for text fields;
- monitor/work-area/DPI discovery;
- invalidation/paint scheduling and normal presentation integration;
- detecting close/system commands and application activation state.

User32 does not authorize native required controls or dialogs.

### 6.2 GDI32

Permitted for:

- creating/reusing a DIB section or equivalent device-independent presentation surface;
- transferring the final application-owned pixel buffer to the top-level window;
- selecting/querying a Windows font for low-level glyph acquisition;
- acquiring glyph bitmap/outline data and metrics needed by the application's own text renderer;
- resource queries/cleanup required by those narrow operations.

GDI32 SHALL NOT be the application UI/canvas renderer.

### 6.3 Kernel32 and base system services

Permitted for:

- high-resolution monotonic timing;
- sleeping/yielding or basic scheduling support;
- file/directory enumeration and Unicode filesystem access;
- temp/replacement/flush/durability operations;
- file metadata/identity and error information;
- process/exit status;
- environment/application-data path discovery at a basic OS-service level;
- UTF-8/UTF-16 conversion at the Windows boundary;
- ordinary handles and synchronization if needed by the implementation.

Concurrency is optional; correctness SHALL NOT depend on a background-thread architecture.

### 6.4 Imm32

Permitted only for low-level desktop IME integration needed to receive/query committed composition result text and related input context state.

It SHALL NOT be used to host a hidden native text editor that replaces the custom text field.

### 6.5 Other Windows system APIs

A Windows SDK API not named above is allowed only when it provides a comparably low-level OS service that is necessary for a mandatory behavior and does not replace a required hand-built subsystem.

The implementation's dependency report SHALL identify such use and its purpose.

## 7. Explicitly prohibited Windows substitutes

The following are prohibited as implementations of mandatory UI/rendering behavior:

- Win32 Common Controls;
- native BUTTON/EDIT/LISTBOX/LISTVIEW/TREEVIEW/COMBOBOX/tab/toolbar/status/trackbar required controls;
- GetOpenFileName/GetSaveFileName/Common Item Dialog/IFileDialog for the mandatory custom picker;
- MessageBox for the mandatory modal system;
- TrackPopupMenu/native application menus as the mandatory command UI;
- GDI Rectangle/RoundRect/Ellipse/Polygon/GradientFill/TextOut/DrawText-style direct window drawing as the application renderer;
- Direct2D;
- DirectWrite used as the required text layout/rendering engine;
- GDI+;
- DirectComposition;
- DWM blur-behind, Acrylic, Mica, backdrop material, or equivalent compositor effect as the required blur/frosted-glass implementation;
- MFC;
- WTL/ATL UI wrappers used as a widget/application framework;
- WinForms;
- WPF;
- WinUI/XAML;
- UWP UI;
- WebView/Electron/browser UI;
- SDL/GLFW/Qt/GTK/Cairo or equivalent cross-platform UI/rendering frameworks.

These rules do not prohibit the operating-system-provided non-client title bar/frame.

## 8. Native window topology

The normal GUI application SHALL create one real top-level application HWND.

Required client UI is rendered inside that client area.

Native child windows SHALL NOT implement required controls, panels, popups, modals, tooltips, file lists, property fields, command palette, or canvas.

A hidden helper HWND is acceptable only for a narrowly justified optional OS-integration feature, MUST be documented, and MUST NOT be required by headless execution.

## 9. Top-level window behavior

The top-level window SHALL support:

- normal move;
- resize subject to minimum client usability;
- minimize;
- maximize;
- restore;
- OS close/Alt+F4;
- taskbar activation;
- focus/activation change;
- per-monitor DPI transition.

The window title SHALL expose application/table/dirty/mode information according to the product spec.

## 10. Non-client chrome

Using the standard Windows title bar, border, minimize/maximize/close buttons, resize frame, and system menu is permitted.

A custom title bar is not required and yields no acceptance advantage.

The hand-built UI requirement starts at the application client area.

## 11. Application-owned framebuffer

All required client pixels originate in application-owned software-rendered pixel storage.

A valid design may use:

- a persistent 32-bit DIB section whose bits are directly written by the renderer; or
- a separately allocated software framebuffer copied into a DIB for presentation.

Equivalent documented software-buffer designs are permitted.

## 12. Framebuffer pixel format

The implementation SHALL define its internal channel order and alpha representation.

Required properties:

- at least 8 bits per R/G/B channel;
- alpha/compositing precision adequate for the UI effects;
- no uninitialized padding/channel bytes exposed visually;
- deterministic clipping and overflow-safe stride/size arithmetic;
- resize allocation checked before multiplication/commit.

Presentation conversion must not alter physics or authored state.

## 13. GDI presentation boundary

GDI may copy/present the final buffer during WM_PAINT or another coherent presentation path.

The following pattern is acceptable in principle:

1. application computes layout and UI state;
2. application software renderer draws every required client element into its framebuffer;
3. GDI transfers the completed pixels to the client DC;
4. paint validates the dirty region.

The exact function names are implementation choices.

## 14. No direct GDI UI drawing

It is a mandatory failure if the required visual system is substantially implemented by issuing GDI shape/text calls directly for each visible control or table primitive.

In particular, required blur, rounded corners, shadows, ripple, glow, capsule transitions, flipper/table geometry, selection handles, debug vectors, and charts/indicators shall be software-rendered by project code.

## 15. Paint/expose model

The program SHALL remain correct when Windows asks for repaint after:

- uncover;
- minimize/restore;
- resize;
- maximize/restore;
- DPI change;
- popup/modal close;
- animation;
- device-context recreation.

It SHALL not assume the screen preserves previous pixels.

## 16. Flicker and background erase

The custom renderer SHALL avoid visible white/default-background flashes during ordinary resize/animation.

The implementation may suppress/reconcile background erase if it reliably repaints the complete required area. Suppression must not expose stale pixels.

## 17. Glyph acquisition boundary

Low-level GDI font selection, metrics, and glyph bitmap/outline acquisition are permitted.

Examples of the allowed class of service include retrieving glyph metrics or antialiased glyph masks/outlines from a selected system font.

The application remains responsible for:

- UTF-8 text model;
- code-point decoding;
- line/single-line placement required by the product;
- glyph positioning/cache ownership;
- clipping;
- selection/caret rendering;
- color/alpha composition;
- fallback behavior;
- final framebuffer composition.

## 18. DirectWrite restriction

DirectWrite may not be used as the required text layout/rasterization engine in this benchmark variant.

This intentionally keeps the hand-built text/UI burden comparable to the Linux low-level font path.

## 19. Unicode internal representation

Platform-independent authored/user strings remain valid UTF-8 as specified elsewhere.

At the Win32 boundary, APIs naturally exposing UTF-16 SHALL be converted to/from valid UTF-8 deliberately.

The application SHALL not use active ANSI code-page strings as the canonical representation of user content or file paths.

## 20. UTF-16 conversion correctness

Required conversion behavior:

- valid surrogate pair -> one Unicode scalar -> valid UTF-8;
- BMP non-surrogate -> corresponding scalar;
- isolated high/low surrogate -> reject input unit or replace visibly/safely according to documented input-error policy;
- no creation of overlong UTF-8;
- no UTF-8 encoding of surrogate scalar values;
- NUL handling obeys each field/path contract.

Automated unit tests SHALL cover these boundaries.

## 21. Windows file paths

Filesystem APIs at the platform boundary SHALL be Unicode-capable.

Required path behaviors include:

- spaces;
- Chinese Unicode components;
- drive-root navigation;
- relative path resolution only where explicitly used by a CLI/config operation;
- canonical path tracking for open documents;
- no shell concatenation/execution;
- no fixed `MAX_PATH` storage assumption.

## 22. Long path safety

Path buffers SHALL be dynamically sized.

The release SHOULD declare long-path awareness in its manifest.

A >260-character path shall either work when the OS/filesystem permits or fail with a real transactional path/I/O error. Silent truncation, buffer overwrite, or conversion to an unrelated path is prohibited.

## 23. Custom file picker

The required Open/Save As interface remains application-drawn.

It shall enumerate and navigate Windows filesystem roots/directories itself through permitted low-level filesystem services.

Native shell dialogs may not be substituted even though they are available on Windows.

## 24. Directory reparse points

The file picker SHALL avoid infinite traversal when encountering symbolic-link/junction/reparse-point cycles.

For ordinary browsing, entering a user-selected reparse directory is allowed if the target is accessible and cycle-safe.

`locscan` follows the stricter policy in document 11: do not traverse redirecting directory reparse points by default.

## 25. Monotonic timing

UI animation and real-time scheduling require a monotonic high-resolution clock.

The Windows implementation SHALL use the high-resolution performance-counter class of service or another documented Windows monotonic source with equivalent precision/stability.

Wall-clock/local time SHALL NOT drive physics steps, animation elapsed intervals, replay timestamps, cooldowns, or launcher charge.

## 26. Fixed-step scheduler isolation

Win32 message arrival frequency is not the simulation timestep.

WM_TIMER, WM_PAINT, mouse-move message density, monitor refresh rate, or message-loop iteration count SHALL NOT directly determine physical `dt`.

The fixed-step accumulator/catch-up policy in document 20 remains authoritative.

## 27. Message pump

The implementation may use blocking or nonblocking Win32 message-pump patterns.

Acceptance observes behavior, not the chosen GetMessage/PeekMessage arrangement.

The pump must remain responsive while Play runs, without creating a second independent physics solver/clock.

## 28. Frame pacing

A target visual update cadence is an implementation detail subject to performance requirements.

No fixed display refresh rate is assumed. A 60/120/144-Hz monitor must not change deterministic physics or replay fingerprints.

## 29. Pointer input

Mouse position and buttons SHALL be converted from Win32 client input into device-independent UI coordinates before hit testing.

Wheel input follows document 16 routing.

Double-click timing may use Windows-provided semantics where double-click behavior is optional/used.

## 30. Pointer capture

Active drag/slider/scroll manipulation SHALL retain pointer ownership outside the client area through a coherent Win32 capture mechanism.

The implementation SHALL handle capture loss explicitly. It may cancel and restore the pre-drag transaction or finish according to the already-observed release state; it may not leave a permanently pressed or half-committed control.

## 31. Keyboard input

Physical Win32 key messages SHALL be translated to the logical editor/gameplay actions defined by the product.

Replay records logical actions, not virtual-key codes, scan codes, repeat flags, or message timestamps.

Key-repeat behavior must not create duplicate logical DOWN transitions where the replay contract expects a state edge.

## 32. Text input vs command keys

Character/text input and command-key handling are separate concerns.

A focused text field receives committed text without causing editor/gameplay shortcuts from those characters.

Ctrl/Shift/Alt state used for shortcuts shall not corrupt IME/Unicode text delivery.

## 33. IME committed text

The custom text field SHALL accept committed text from Windows desktop input methods.

Using IMM to query `GCS_RESULTSTR` from a composition-result message is explicitly permitted.

A custom candidate list/composition underline/editor is not required.

A hidden native EDIT/RICHEDIT control used solely to obtain IME behavior is prohibited.

## 34. Clipboard text

System clipboard interoperability for text fields SHALL use Unicode text semantics.

The custom structured object clipboard remains independent/in-process.

The application SHALL handle clipboard contention/failure without deadlock or destructive clearing.

Clipboard memory ownership SHALL follow Win32 ownership rules and not leak per copy operation.

## 35. Focus and activation

Window deactivation/focus loss SHALL clear/synthesize release of held gameplay input as defined in document 23.

Focus change shall not implicitly mutate authored scene state.

When focus returns, no flipper/launcher/editor drag may remain logically held unless a new input transition occurs.

## 36. Close semantics

WM_CLOSE, Alt+F4, taskbar Close, and an in-app Quit command converge on the same dirty-document decision path.

Destroying the HWND before the Save/Discard/Cancel decision completes is prohibited.

## 37. Session shutdown

On Windows session shutdown/end-session notification, the application SHOULD make a best-effort recovery snapshot of dirty work if time/OS policy permits.

It SHALL NOT falsely mark a formal scene saved merely because the process is being ended.

The ordinary Recovery contract remains the safety mechanism; a blocking shutdown UI flow is not mandatory.

## 38. Per-Monitor DPI Awareness v2

The release SHALL be Per-Monitor DPI Aware v2.

The preferred declaration is the executable application manifest so DPI mode exists before any window/UI-dependent API use.

An equivalent process-default mechanism established before UI creation is acceptable if the resulting behavior is identical and verifiable.

DPI-unaware/system-DPI-aware bitmap virtualization is not accepted.

## 39. DPI baseline

96 DPI is the 1.0 Windows device scale.

Required acceptance points where available:

- 96 DPI = 1.00;
- 120 DPI = 1.25;
- 144 DPI = 1.50;
- 192 DPI = 2.00.

Other DPI values SHALL remain finite and usable even if they are not explicit screenshot checkpoints.

## 40. WM_DPICHANGED-equivalent transition

When the top-level window DPI changes:

- update device scale;
- apply/reconcile recommended top-level geometry;
- recompute client pixel geometry;
- resize framebuffer safely;
- rerasterize text/UI;
- maintain world transform semantics;
- preserve deterministic simulation state;
- preserve logical focus/modal/popup ownership where valid;
- do not add an Undo command or dirty authored data.

## 41. OS DPI and user scale

Windows monitor DPI and the application's 100/125/150/200% user UI setting are independent factors.

The implementation SHALL compose them as defined in document 23 rather than treating one as a replacement for the other.

Canvas world zoom remains independent of both.

## 42. Native DPI scaling prohibition

The Windows compositor's DPI bitmap scaling of a DPI-unaware framebuffer is not the required HiDPI implementation.

The application must re-layout/re-rasterize at effective target scale.

## 43. Atomic save services

The Windows file layer may use native handle-based file I/O or C stdio plus Win32 operations where appropriate.

The observable atomicity/fault behavior in document 24 is mandatory.

A valid solution commonly uses a same-directory temporary file, full flush/close, then an atomic/replace operation; exact API choices are not prescribed.

## 44. Sharing violations and file locks

Windows commonly denies replacement/open operations because another process holds an incompatible share mode.

Such denial is a normal recoverable external I/O failure:

- preserve previous destination bytes;
- remain dirty;
- report the path/operation context;
- permit Retry/Save As through normal product flow;
- do not spin/retry indefinitely.

## 45. Read-only attributes/access denial

A destination made read-only or inaccessible SHALL fail transactionally.

The app SHALL NOT silently clear filesystem attributes or elevate privileges merely to force a save.

## 46. Recovery storage

Recovery/state storage uses a per-user writable Windows application-data location. `%LOCALAPPDATA%\PinballSandbox\Recovery` is the normative logical path when available.

Formal user scene files remain wherever the user explicitly opens/saves them.

## 47. File identity

Windows file IDs/volume identity/timestamps may support external-change detection, but byte-content fingerprint remains authoritative under ambiguous metadata.

No POSIX inode assumption may leak into the Windows implementation.

## 48. Headless contract

Headless simulation/tests/utilities SHALL NOT create:

- top-level HWND;
- hidden helper HWND needed to run simulation;
- presentation HDC/DIB solely to satisfy physics;
- clipboard state;
- IME state;
- DPI/render initialization needed only by the GUI.

The core must run with no interactive desktop requirement.

## 49. GUI/headless module sharing

The Win32 adapter is outside the deterministic physics/editor core boundary.

GUI and headless paths share the same parser, validator, authored model, physics, gameplay/events, replay, trace, and deterministic math modules.

A separate simplified Windows-console solver is prohibited.

## 50. GUI subsystem/console behavior

Normal desktop launch SHALL not leave a distracting console window.

Headless/utility output still needs normal stdout/stderr/exit-status behavior.

Implementations may solve this with separate GUI/CLI executables or a single executable that attaches/uses a console appropriately. The packaging choice must not duplicate production logic.

## 51. Resource ownership

Every acquired Windows resource SHALL have a clear lifetime.

Examples requiring correct ownership include:

- HWND;
- HDC acquired from a window;
- memory DC if used;
- HBITMAP/DIB section;
- selected font/GDI objects;
- file/search HANDLEs;
- HGLOBAL clipboard allocations after ownership transfer rules;
- IME context acquisition/release;
- dynamically loaded handles if any.

Repeated UI operations shall not leak one object/handle per cycle.

## 52. GDI object selection rule

If a GDI object is selected into a DC for glyph acquisition, deletion/replacement SHALL respect GDI ownership/selection rules.

The release resource test is expected to expose repeated leaking/recreation mistakes.

## 53. Clipboard ownership

After successful SetClipboardData ownership transfer, the application SHALL follow the Windows clipboard memory ownership contract rather than double-freeing the object.

Clipboard-open failure is recoverable.

## 54. Error mapping

Win32 GetLastError/system error detail may be included in diagnostics but SHALL be mapped into the stable product diagnostic categories/codes where a stable code exists.

Raw Windows error number alone is not a substitute for product context.

## 55. Application manifest

The released executable SHOULD contain an application manifest declaring at least the appropriate modern DPI awareness. Long-path awareness SHOULD also be declared.

The manifest must not request elevated administrator execution for ordinary operation.

## 56. Privilege model

Normal application operation SHALL run as a standard user.

Administrator rights are not required for:

- launch;
- editing;
- simulation;
- saving to user-writable paths;
- recovery;
- tests/utilities in user-writable project/output directories.

A UAC elevation prompt on ordinary startup is a release failure.

## 57. Installation scope

An installer, MSI/MSIX package, registry integration, file association, Start-menu entry, and code signing are not required.

The delivered executable(s) and documented runtime files must be sufficient for direct evaluation.

## 58. File association

`.pbt`/`.pbr` Windows shell file associations are optional.

If implemented, they do not change Open/Save semantics or custom picker requirements.

## 59. System theme

Following Windows light/dark theme is optional unless another document requires a specific product theme.

Native theme services SHALL NOT replace custom palette, blur, shadows, controls, or animations.

## 60. Accessibility boundary

This Windows platform fork does not add a new mandatory UI Automation/MSAA layer beyond the keyboard/focus/reduced-motion requirements already in v1.0.

Implementing accessibility APIs is allowed if it does not substitute native controls for the required custom UI.

## 61. Monitor/work-area behavior

The window SHALL remain usable after maximize/restore and DPI-driven moves between monitors.

A modal is centered/clamped within the current client area, not placed using stale coordinates from a previous monitor.

## 62. Fullscreen

Exclusive fullscreen and borderless fullscreen are not required.

Normal maximized window support is sufficient.

## 63. Presenting during resize

Interactive resize may temporarily reduce visual frame rate, but:

- input remains responsive;
- client content is repainted coherently;
- fixed-step simulation uses the stall/catch-up policy rather than a giant dt;
- no framebuffer size overflow/use-after-free occurs.

## 64. Window minimum size

The client-area product minimum remains derived from the existing 1024×700 device-independent layout requirement.

Windows non-client frame pixels are not counted as usable client content.

DPI conversion shall preserve that logical minimum rather than hard-coding physical 1024×700 pixels on every monitor.

## 65. Window title text

Window title/path-derived display text supplied to Win32 SHALL use the Unicode API path.

No mojibake from ANSI title APIs is accepted for Chinese table names.

## 66. Cursor shapes

System cursors may be used for ordinary arrow/text/resize/hand affordances.

Using a system cursor does not count as a prohibited native UI control.

Custom cursor artwork is optional.

## 67. OS drag-and-drop

Shell/OLE file drag-and-drop into the app is not mandatory.

If implemented, it is additional behavior and must not bypass transactional Open/dirty protection.

## 68. Native notifications

Taskbar progress, tray icons, toast notifications, and Windows notification center integration are not required.

Required in-app errors/toasts remain custom-rendered.

## 69. Windows fonts

The implementation may choose an installed Windows font/fallback strategy within the permitted glyph boundary.

It SHALL not bundle/share copyrighted font files merely to satisfy the task.

Missing glyph behavior follows documents 16/23.

## 70. Font fallback

At minimum, the implementation SHALL make a reasonable platform fallback attempt for Chinese text before showing a missing-glyph marker when common Windows CJK fonts are available.

Exact shaping for complex scripts is not required beyond the v1.0 text baseline.

## 71. Text shaping non-goal

Full OpenType complex-script shaping, bidirectional paragraph layout, emoji color-font rendering, and typographic ligature engines are not mandatory.

This does not relax UTF-8 scalar safety, Chinese preservation/search, clipboard, or committed IME requirements.

## 72. Windows line endings

Scene/replay readers retain the platform-independent LF/CRLF acceptance rules.

Canonical scene writer SHOULD continue emitting LF so files remain cross-platform byte-stable where all semantic content is equal.

Human reports may use Windows line endings, but machine-readable canonical outputs used for deterministic comparison SHOULD specify/retain stable UTF-8/LF output where existing specs do so.

## 73. Cross-platform fixture invariance

Every acceptance fixture, malformed corpus file, golden checkpoint, scene-format grammar, replay semantic, canonical table, and canonical journey input supplied by this Windows package is intentionally identical to the Linux v1.0.0 source package unless `acceptance/platform_parity_manifest.json` explicitly states otherwise.

Production code SHALL NOT special-case the platform name when computing deterministic physics/reference results.

## 74. Cross-platform deterministic expectation

For identical scene bytes, replay logical actions, seed, and step count, the Windows implementation must satisfy the same canonical expected values/tolerances as the Linux assignment.

Exact floating-point bit identity across unrelated compiler/platform implementations is not required unless another test explicitly uses an exact integer/state field. The normative tolerances/checkpoint rules remain unchanged.

## 75. Import/dependency audit

Release evidence SHALL include an import/dependency summary sufficient to identify linked Windows DLLs/runtime files.

Presence of a Windows system DLL is not failure. Presence of a prohibited framework/rendering DLL or shipped library is failure unless it is clearly unrelated optional tooling and not part of the product path—which itself must be documented.

## 76. Source audit targets

Reviewers must be able to distinguish:

- Win32 adapter code;
- software renderer;
- custom UI controls/layout;
- text/glyph cache;
- file layer;
- deterministic core.

A monolithic source file is not automatically prohibited, but hiding high-level framework delegation behind a thin C wrapper is prohibited.

## 77. Windows platform evidence

The release evidence shall record:

- tested Windows version/build;
- architecture;
- DPI awareness result;
- OS DPI values exercised;
- user UI scales exercised;
- Unicode path test;
- clipboard/IME committed text test;
- headless no-HWND result;
- USER/GDI/HANDLE resource-cycle result;
- dependency/import audit;
- normal GUI no-unwanted-console result.

The method used to capture screenshots or query these facts is intentionally not prescribed.

## 78. Release failure examples

Mandatory failure includes any of the following:

- required UI built from native Win32 controls;
- custom picker replaced by IFileDialog;
- app blur provided by DWM backdrop;
- Direct2D/DirectWrite/GDI+ used as required renderer;
- 100%-DPI bitmap scaled by Windows virtualization for HiDPI acceptance;
- Chinese file path corrupted through ANSI conversion;
- headless mode creates a hidden HWND;
- save sharing violation truncates previous destination;
- one HBITMAP/HDC/HFONT/HANDLE leaked per modal/resize cycle;
- monitor DPI change mutates world/simulation coordinates;
- Alt+F4 bypasses dirty protection;
- replay stores virtual-key/scancode instead of logical actions.

## 79. Definition of Windows platform completion

The Windows platform port is complete only when:

1. every platform-independent v1.0 requirement remains satisfied;
2. this document is satisfied;
3. all Windows platform cases are represented in automated/manual evidence;
4. the Windows Platform Binding Gate passes;
5. no prohibited platform shortcut weakened the original hand-built engineering burden.
