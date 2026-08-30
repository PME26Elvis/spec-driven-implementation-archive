# 01 — Scope and Engineering Constraints

## 1. Product identity

The product is named **Analog Clock Workbench**.

It is a single-window desktop application for manipulating and simulating one clock. It is not a wall-clock utility, alarm clock, calendar, world-clock, timer, stopwatch, or timezone application.

The application operates only on an internal simulated time. It must not initialize, reset, or synchronize canonical simulated time from the operating system's current wall-clock/calendar time. Startup time comes from configuration/runtime state as specified.

## 2. Mandatory implementation language

The production implementation must be written in **ISO C17**.

C++ compilation, C++ standard library facilities, Objective-C, Rust, Python, JavaScript, Lua, embedded scripting engines, or generated source code that replaces required implementation work are prohibited.

The code may use implementation-defined integer widths only when guarded by explicit compile-time checks. Prefer `<stdint.h>` fixed-width types for persisted formats.

## 3. Allowed platform interface

The application must use **X11/Xlib** as the native desktop/window-system interface for Project B. XCB must not be called directly by submitted code; transitive Xlib implementation dependencies are irrelevant to compliance.

Allowed X11 responsibilities are limited to:

- creating and destroying application windows;
- receiving keyboard, pointer, focus, expose, resize, and window-close events;
- pointer capture/grab needed for drag interactions;
- querying modifier-key state;
- managing the native event loop;
- transferring the application's rendered pixels to the visible window;
- creating resources strictly necessary for that pixel transfer;
- obtaining basic monitor/window dimensions when needed for layout bounds.

The intended architecture is that the application owns the interface and X11 provides the windowing substrate.

## 4. Standard C library

The ISO C17 standard library is allowed in full.

Examples include:

- memory allocation;
- strings and byte operations;
- mathematics;
- file I/O;
- integer and floating-point conversion;
- time intervals needed for internal simulation where portable facilities are sufficient;
- assertions and diagnostics.

The simulated clock's value must not be derived continuously from the system wall clock.

## 5. Narrow system facilities

Where ISO C and X11 cannot provide a required primitive, the implementation may use a minimal POSIX/Linux facility for that primitive only.

Permitted examples are:

- a monotonic high-resolution clock for animation frame timing;
- filesystem primitives needed for atomic configuration save/replace;
- directory traversal for Project A;
- process exit/status and standard file metadata;
- basic sleep/poll/select integration if required by the event loop.

These facilities must not provide a replacement for required algorithms or UI systems.

## 6. Forbidden libraries and frameworks

The production application and Project A utilities must not link to or embed:

- GTK;
- Qt;
- SDL;
- GLFW;
- SFML;
- Cairo;
- Skia;
- OpenGL helper frameworks;
- Vulkan helper frameworks;
- Xft;
- Pango;
- FreeType;
- HarfBuzz;
- libpng or other image rendering libraries used as a substitute for UI drawing;
- ncurses for the GUI;
- JSON libraries;
- YAML libraries;
- general parser generators used to generate the JSON/YAML parsers;
- GUI immediate-mode libraries;
- animation libraries;
- data-binding/state-management libraries.

If a library effectively implements one of the specifically required hand-built subsystems, it is prohibited even if not listed above.

## 7. X11 drawing restriction

X11 primitive drawing calls must not be used as the primary UI renderer.

The application must maintain an application-owned software pixel buffer and implement its own UI drawing into that buffer.

X11 may transfer/present the completed buffer.

This restriction exists so that anti-aliasing, alpha composition, clipping, blur, shadows, rounded geometry, and transitions are implemented consistently in the application.

## 8. Single-window application

Project B uses one top-level X11 application window.

Settings, help, confirmation, and error surfaces must be application-rendered panels or modals inside the top-level window rather than additional native child windows or dialog widgets.

A native file-picker is not required and must not be necessary for normal acceptance testing.

## 9. No external assets required for correctness

The application must remain fully functional if run with only its executable and configuration files.

Required icons, glyphs, and UI shapes must not depend on downloading web assets.

If the implementation includes optional image assets, missing optional images must not break any mandatory functionality.

## 10. Character support

All textual application data and configuration strings are UTF-8.

The application must correctly preserve UTF-8 byte sequences in configuration values and user-visible labels.

Mandatory user-entered content is intentionally limited so that a full international text-shaping engine is not required. However, the configuration parser must not corrupt Chinese UTF-8 text.

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

## 13. X11 visual/presentation baseline

Project B must support the ordinary default X11 **TrueColor** visual at 24-bit depth, including common 32-bit storage layouts used by `XImage`.

The application must keep an internal application-owned RGBA/RGB software framebuffer and explicitly convert/copy its opaque final RGB result into an `XImage`-compatible presentation buffer when necessary. `XPutImage` (or an equivalent Xlib image-transfer call that does not perform UI drawing) is allowed.

Support for pseudocolor visuals, remote low-depth displays, XRender compositing, GLX/OpenGL, and MIT-SHM acceleration is not required. If the default visual is unsupported, startup must fail with a clear diagnostic rather than render corrupt colors.

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
- Wayland-native implementation;
- accessibility APIs beyond the keyboard/focus behavior explicitly required by this task pack.

## 15. Build expectations

A normal build must fail on compile warnings that indicate type misuse, missing declarations, incompatible pointer conversions, or format-string mismatches.

The source tree must contain a root `Makefile`. The following targets are mandatory:

```text
make all
make test
make clean
```

`make all` must build the four required executables into `build/`:

```text
build/locscan
build/cfgcheck
build/stateprobe
build/analog-clock-workbench
```

The build must compile production C in C17 mode and must not require manually generated source files that are absent from the submission. `CC` may be overridden by the caller. Project B may directly link only the ordinary C/POSIX runtime, the mathematics library where required by the platform, and Xlib (`libX11`). No submitted source may directly call XCB, XRender, Xft, or another X extension/library to bypass the renderer restrictions.

## 16. Runtime determinism option

The application must provide a deterministic validation mode described in the testing specification.

This mode is solely for repeatable tests and must not replace normal interactive behavior.
