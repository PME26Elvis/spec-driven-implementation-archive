# 01 — Scope and Engineering Constraints

## 1. Product identity

The product is named **Analog Clock Workbench**.

It is a single-window desktop application for manipulating and simulating one clock. It is not a wall-clock utility, alarm clock, calendar, world-clock, timer, stopwatch, or timezone application.

The application operates only on an internal simulated time. It must not initialize, reset, or synchronize canonical simulated time from the operating system's current wall-clock/calendar time. Startup time comes from configuration/runtime state as specified.

## 2. Mandatory implementation language

The production implementation must be written in **ISO C17**.

C++ compilation, C++ standard library facilities, Objective-C, Rust, Python, JavaScript, Lua, embedded scripting engines, or generated source code that replaces required implementation work are prohibited.

The code may use implementation-defined integer widths only when guarded by explicit compile-time checks. Prefer `<stdint.h>` fixed-width types for persisted formats.

Windows-specific entry points such as `wWinMain`/`wmain` and Win32 declarations do not change the requirement that submitted production source is C17 rather than C++.

## 3. Target platform and allowed platform interface

Project B targets the ordinary **Win32 desktop API on 64-bit Windows 10 22H2 and Windows 11**. “Win32” in this task pack names the desktop API and does not imply a 32-bit executable.

The implementation must use Unicode (`...W`) Win32 APIs whenever a platform API has separate ANSI and Unicode forms. Persisted/configuration text remains UTF-8 as defined elsewhere; platform filesystem/window strings may be converted to UTF-16 at the platform boundary.

Allowed Win32 responsibilities are limited to:

- registering a window class and creating/destroying the single top-level application window;
- receiving keyboard, pointer, wheel, mouse-leave, focus, paint, DPI, resize, minimize/restore, capture-loss/cancel-mode, and close messages;
- pointer capture/release and mouse-leave tracking needed for drag/hover interactions (for example `SetCapture`/`ReleaseCapture` and `TrackMouseEvent`);
- querying modifier-key state;
- running/waiting in the native message loop;
- obtaining the client rectangle, monitor/work-area dimensions, DPI information, and client/non-client size conversions needed for bounds/scaling (for example `AdjustWindowRectExForDpi`);
- positioning/resizing the ordinary top-level window in response to validated application sizing and DPI messages;
- transferring the application's completed software-rendered pixels to the window using a 32-bit DIB presentation path;
- creating only the GDI resources strictly necessary for that pixel transfer;
- high-resolution monotonic timing and wait primitives needed by the event/animation loop;
- narrow filesystem/process primitives expressly allowed below.

The intended architecture is that the application owns the interface and Win32 supplies only the desktop/windowing substrate.

Native child controls are not part of the allowed UI implementation. Settings, navigation, text fields, buttons, sliders, modals, menus, and other mandatory controls must be implemented by the application renderer and event system.

## 4. Standard C library

The ISO C17 standard library is allowed in full.

Examples include:

- memory allocation;
- strings and byte operations;
- mathematics;
- file I/O where its semantics are sufficient;
- integer and floating-point conversion;
- assertions and diagnostics.

The simulated clock's value must not be derived continuously from the system wall clock.

## 5. Narrow Windows system facilities

Where ISO C cannot provide a required primitive, the implementation may use a minimal Windows system facility for that primitive only.

Permitted examples are:

- `QueryPerformanceCounter`/`QueryPerformanceFrequency` or an equivalent monotonic high-resolution counter for animation frame timing;
- `CreateFileW`, `WriteFile`, `FlushFileBuffers`, `ReplaceFileW`/`MoveFileExW`, and related narrow filesystem operations needed for robust/atomic configuration and runtime-state save/replace;
- `FindFirstFileW`/`FindNextFileW`, `GetFileAttributesExW`, and related metadata operations for Project A traversal;
- UTF-8 ↔ UTF-16 conversion at the platform boundary with `MultiByteToWideChar`/`WideCharToMultiByte` using UTF-8 code pages and strict invalid-sequence handling where available;
- process exit/status and basic waits such as `Sleep` or `MsgWaitForMultipleObjectsEx` when needed by the event loop.

These facilities must not provide a replacement for required algorithms, parsing, matching, layout, text rendering, image processing, animation, state management, or UI systems.

For Project A filesystem work, directory enumeration may use the native Win32 APIs, but wildcard/pattern APIs must not implement the required `locscan` matcher.

## 6. Forbidden libraries, frameworks, and higher-level Windows graphics stacks

The production application and Project A utilities must not link to or embed:

- GTK;
- Qt;
- SDL;
- GLFW;
- SFML;
- Cairo;
- Skia;
- GDI+;
- Direct2D;
- DirectWrite;
- Direct3D;
- DirectComposition;
- Windows Imaging Component as a rendering shortcut;
- OpenGL;
- Vulkan;
- DWM blur/acrylic/mica/compositor effects used to replace the required software blur/effects;
- FreeType;
- HarfBuzz;
- Pango;
- image-processing/rendering libraries used as a substitute for required drawing work;
- ncurses/PDCurses for the GUI;
- JSON libraries;
- YAML libraries;
- general parser generators used to generate the JSON/YAML parsers;
- GUI immediate-mode libraries;
- animation libraries;
- data-binding/state-management libraries.

If a library or Windows component effectively implements one of the specifically required hand-built subsystems, it is prohibited even if not listed above.

## 7. GDI / native drawing restriction

The application must maintain an application-owned software pixel buffer and implement its own mandatory UI drawing into that buffer.

GDI may be used only for lifecycle/paint bookkeeping and transfer of the completed pixel buffer to the top-level window. `StretchDIBits` or `SetDIBitsToDevice` are acceptable presentation operations. A `CreateDIBSection` presentation resource is also allowed if application code still performs all required rasterization itself.

GDI drawing/text/effect primitives such as `TextOut`, `DrawText`, `Rectangle`, `RoundRect`, `Ellipse`, line/path drawing, `GradientFill`, `AlphaBlend`, or equivalent APIs must not implement required UI graphics. GDI+, Direct2D, DirectWrite, DirectComposition, DWM blur, layered-window opacity, or native theme rendering must not replace the software renderer.

This restriction exists so anti-aliasing, alpha composition, clipping, blur, shadows, rounded geometry, glyph rendering, and transitions are implemented consistently in application C code.

## 8. Single-window application

Project B uses one top-level Win32 application window (`HWND`). The ordinary OS-provided non-client frame/title bar is allowed; the custom-rendering requirement applies to the application client area and all mandatory application controls.

Settings, help, confirmation, and recoverable error surfaces must be application-rendered panels or modals inside that top-level window rather than additional native child windows or dialog controls.

A native file picker is not required and must not be necessary for normal acceptance testing.

`MessageBoxW` is not a substitute for any normal application modal. It may be used only as a last-resort fatal diagnostic before a usable application window exists, provided the same error is also available as a concise diagnostic when launched from a terminal/test harness.

## 9. No external assets required for correctness

The application must remain fully functional if run with only its executable and configuration files.

Required icons, glyphs, and UI shapes must not depend on downloading web assets or on installed fonts.

If the implementation includes optional image assets, missing optional images must not break any mandatory functionality.

## 10. Character and path support

All textual application data and configuration strings are UTF-8.

The application must correctly preserve UTF-8 byte sequences in configuration values and user-visible labels.

Mandatory user-entered content is intentionally limited so that a full international text-shaping engine is not required. However, the configuration parser must not corrupt Chinese UTF-8 text.

Windows filesystem paths used by Project A and persistence/config loading must be handled through Unicode-capable Win32 paths internally so that spaces and non-ASCII path components are not corrupted by an ANSI code-page round trip. Every command-line argument that can contain a path (`ROOT`, `--config FILE`, `cfgcheck FILE`, `stateprobe FILE`) must originate from the Unicode Windows command line—for example `wmain`/`wWinMain` arguments or an equivalent `GetCommandLineW` path—and only then be converted explicitly to validated UTF-8 where the project-owned core uses UTF-8. A legacy ANSI `main(char **)`/ACP round-trip that corrupts a valid non-ASCII path is non-conforming. This does not require extended-length-path support beyond ordinary Windows path limits unless the implementation chooses to provide it.

## 11. Application size and complexity

No source-line-count target is a release criterion.

The task is intentionally structured so that substantial implementation is required in:

- software rendering;
- geometry and hit testing;
- animation;
- state synchronization;
- input editing;
- undo/redo;
- configuration parsing;
- validation;
- tests;
- Project A tooling.

The implementation must not remove required functionality merely to reduce code size.

## 12. Project boundaries

Project A and Project B are both mandatory parts of one assignment.

Project A exists as a separate engineering deliverable, not as a runtime dependency that prevents Project B from launching.

Project B must be able to build and run even if Project A binaries have not been built, but Project A is required for final delivery and validation.

## 13. Win32 DIB presentation and DPI baseline

Project B must support an ordinary Windows 10 22H2 / Windows 11 desktop using a normal top-level overlapped window and 32-bit true-color desktop composition.

The renderer must maintain an application-owned RGB/RGBA software framebuffer. For presentation, the implementation must produce a conventional top-down 32-bit `BI_RGB` DIB-compatible buffer (commonly BGRX/BGRA byte layout, with alpha ignored by ordinary opaque DIB presentation) or explicitly convert/copy into one. The final visible UI remains opaque at the native window level; per-pixel application alpha is resolved by the software renderer before presentation.

`StretchDIBits`/`SetDIBitsToDevice` or an equivalent GDI raw-DIB transfer operation is allowed only as a **1:1 final pixel transfer** into the client area: source and destination pixel dimensions for the presented region must match. GDI interpolation/stretching, mapping transforms, or other presentation-time scaling must not implement `window.ui_scale`, DPI adaptation, animation scale, or any required geometry. Those operations belong to the project-owned software renderer. Hardware-accelerated compositing APIs, GDI/GDI+ primitive rendering, layered-window alpha, DWM blur/acrylic/mica, Direct2D, Direct3D, and OpenGL must not replace the required software rendering.

The process must declare **Per-Monitor V2 DPI awareness** before the top-level window is created, using an application manifest or `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)`. Legacy bitmap DPI virtualization is not acceptable. Native DPI changes (`WM_DPICHANGED`) must not silently alter the task pack's application-level `window.ui_scale`; OS DPI and `ui_scale` are separate concepts. The implementation may apply the suggested native window rectangle supplied by `WM_DPICHANGED`, subject to the task pack's supported client-size bounds, but it must then recompute actual client geometry and rebuild/reuse the 1:1 framebuffer accordingly. It must not multiply renderer scale by monitor DPI or otherwise treat the suggested rectangle as permission for a hidden presentation stretch. The application must remain sharp and hit testing must stay aligned with the presented pixels after a DPI change.

If the required ordinary 32-bit DIB presentation path cannot be established, startup must fail with a clear diagnostic rather than render corrupt colors. The required presentation tests must verify that no hidden GDI stretch is needed for the supported 1.0/1.25/1.5 application UI scales.

## 14. Non-goals

The following are explicitly out of scope:

- real-world timezone support;
- daylight saving time rules;
- network time synchronization;
- alarms;
- timers or stopwatches;
- calendar/date UI;
- multiple clock faces at once;
- plugin systems;
- cloud synchronization;
- account systems;
- audio playback;
- system tray integration;
- mobile support;
- UWP/WinUI/WPF implementation;
- cross-platform/multi-backend window-system abstraction as a mandatory deliverable;
- accessibility APIs beyond the keyboard/focus behavior explicitly required by this task pack.

## 15. Build expectations

A normal build must fail on compile warnings that indicate type misuse, missing declarations, incompatible pointer conversions, or format-string mismatches.

The source tree must contain a root `build.bat`. The following commands are mandatory:

```text
build.bat all
build.bat test
build.bat clean
```

`build.bat all` must build the four required executables into `build\`:

```text
build\locscan.exe
build\cfgcheck.exe
build\stateprobe.exe
build\analog-clock-workbench.exe
```

The required build path is a Microsoft-compatible C compiler invocation: `build.bat` must default to `cl.exe` and may also support `clang-cl` through a documented environment override. Production C must compile in C17 mode; for MSVC/clang-cl style compilers the intended strict build is equivalent to `/std:c17 /W4 /WX /utf-8` plus any project-specific warning suppressions that are narrowly justified in the submission documentation. Generated source files required to build must be present in the submission.

Project B may directly link only the ordinary C runtime and the Windows system libraries necessary for the explicitly allowed substrate operations (normally `kernel32`, `user32`, `gdi32`, and `shell32` only when command-line/path helpers genuinely require it). Project A may use only the ordinary C runtime plus narrow Windows filesystem/console substrate APIs. Linking a permitted system DLL does not make its higher-level UI/rendering functions permitted.

An additional Makefile/CMake/project file may be supplied for developer convenience, but it does not replace the mandatory `build.bat` interface and must not introduce forbidden runtime libraries.

The normal GUI executable must not require a separate console window for ordinary interactive use. Test/diagnostic builds may use a console, and fatal startup diagnostics must remain observable as defined above.

## 16. Runtime determinism option

The application must provide a deterministic validation mode described in the testing specification.

This mode is solely for repeatable tests and must not replace normal interactive behavior.
