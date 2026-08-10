# Task Package Changelog — Windows Variant

## 1.0.0

First frozen Windows platform release, derived from the frozen Linux/X11 v1.0.0 assignment while preserving product/physics/editor/gameplay scope and acceptance workload.

### Platform binding conversion

- target changed to native Windows 10 22H2 / Windows 11 x64 desktop using C17 and low-level Win32 services;
- X11/XCB responsibilities mapped to narrowly permitted User32/GDI32/Kernel32/Imm32 services;
- formalized one real top-level HWND and application-owned software framebuffer;
- prohibited Win32 Common Controls/native required widgets, Common Dialog/IFileDialog replacement, MessageBox modal substitution, GDI application rendering, Direct2D/DirectWrite/GDI+, DWM Acrylic/Mica/backdrop substitution, WinUI/WPF/WinForms/MFC, and other high-level substitutes;
- retained custom file picker, custom text fields, custom modals/popups, custom blur, animation, layout, hit testing, focus model, and rendering;
- mapped monotonic timing to the Windows high-resolution performance-counter class of service;
- defined Unicode Win32 boundary, UTF-16 surrogate handling, Unicode system clipboard, and committed IME result handling;
- defined Per-Monitor DPI Awareness v2, WM_DPICHANGED behavior, and OS-DPI × user-scale composition;
- defined Windows atomic-save/replacement, sharing/access failure, recovery location, file identity, and long-path safety semantics;
- mapped locscan symlink behavior to Windows symbolic-link/junction/reparse-point traversal;
- added Windows USER/GDI/HANDLE leak gates and platform-binding acceptance tests;
- defined headless execution as requiring zero HWND/interactive-desktop dependency.

### Cross-platform parity retained

The following remain intentionally unchanged from the Linux v1.0.0 benchmark assignment:

- 163 stable requirement IDs;
- >=420 meaningful automated-test floor and domain floors;
- V01–V38 static and A01–A25 transition/interaction evidence counts;
- scene format `PINBALL_TABLE 2` and mandatory v1 migration;
- replay format and deterministic semantics;
- all canonical `.pbt` fixtures and malformed corpus bytes;
- golden physics checkpoints;
- object types, editor behavior, gameplay, scoring, Nudge/Tilt, diagnostics, utilities, performance workloads, and canonical J01–J24 journey.
