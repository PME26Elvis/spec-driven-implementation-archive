# 02 — Engineering Constraints and Prohibited Substitutions

## 1. Language

All production application logic, physics engine code, custom rendering/UI code, test executables, benchmark executables, validation executables, and required developer utilities must be implemented in **ISO C17**.

Allowed non-C files are limited to:

- Build metadata such as Makefiles.
- JSON scene/config/fixture files.
- CSV/JSON generated reports.
- Markdown documentation.
- Plain-text ignore/config files.
- Static raster assets if needed for application icons or glyph data.
- Windows resource/manifest metadata used only for executable icon/version/DPI-awareness/native resource declarations and containing no product logic.

No required feature may be implemented in Python, C++, Rust, JavaScript, TypeScript, Java, Go, shell script, Lua, or another programming language.

## 2. Target platform

The application targets a native 64-bit Windows desktop using classic Win32 desktop APIs. Windows 10 22H2-class and Windows 11-class behavior are the reference compatibility family.

The product must be an actual native Windows desktop application and not a terminal UI, browser application, remote webpage, WebView shell, or pre-rendered animation.

## 3. Allowed system/library surface

Allowed runtime/development library categories:

- ISO C17 standard library.
- Standard mathematical library supplied by the C implementation.
- **User32** only for low-level top-level window, native message/input, cursor, focus/capture, clipboard-if-used, client geometry, DPI/window notifications, and ordinary OS window lifecycle.
- **GDI32** only for DIB/software-framebuffer presentation and minimal font/glyph bitmap/metric acquisition if needed.
- **Kernel32** for high-resolution monotonic timing, Unicode-capable low-level file operations/replacement, and basic OS facilities where ISO C17 has no suitable desktop-quality primitive.
- **Imm32** for system IME composition integration with the application's custom text editors.
- Minimal monitor/window queries in the same low-level Win32 desktop API class when needed for correct per-monitor DPI/coordinate behavior.

Including `windows.h` does not grant permission to use every Windows subsystem exposed by it. The semantic whitelist above remains controlling.

GDI text/glyph primitives may only provide glyph bitmaps/metrics if needed. They must not be used as a substitute for custom text-field behavior, UI component/layout/state, clipping, selection/caret, animation, blur, graphs, or scene rendering.

The operating system's standard non-client frame/title bar and native IME candidate window are allowed and do not count as required application widgets.

## 4. Forbidden GUI/rendering substitutions

The following are prohibited:

- GTK.
- Qt.
- SDL.
- SFML.
- GLFW.
- Cairo.
- Skia.
- NanoVG.
- Dear ImGui.
- Nuklear.
- raylib.
- OpenGL.
- Vulkan.
- Direct2D.
- DirectWrite as the required text-layout/rendering engine.
- Direct3D.
- DirectComposition.
- WinUI / Windows App SDK UI controls.
- WPF or Windows Forms.
- MFC or ATL controls/wrappers.
- Windows Common Controls as substitutes for mandatory custom widgets.
- Common Dialog / `IFileDialog` as the required Open/Save As UI.
- `MessageBox` as the required modal/error/confirmation UI.
- DWM/Mica/Acrylic/system backdrop effects as the required blur/frosted-glass implementation.
- Native menu/dropdown APIs as the required custom menu/dropdown implementation.
- Direct rendering through a browser/WebView.
- Embedding HTML/CSS/JavaScript as the primary UI.
- Any retained-mode or immediate-mode GUI toolkit that supplies ready-made buttons, panels, layouts, modal dialogs, animation, blur, or widgets.

The application must implement its own UI widget/state/layout/rendering layer.

## 5. Required custom software renderer responsibilities

The implementation must itself provide at least:

- Pixel/framebuffer ownership.
- Rectangle fills.
- Rounded rectangle fills.
- Circle/ellipse fills or sufficiently accurate rasterization.
- Convex polygon fills.
- Line and polyline drawing.
- Borders/strokes.
- Alpha blending.
- Clipping/scissoring.
- Box-shadow approximation.
- Glow rendering.
- Off-screen surface support required by blur and transitions.
- Separable blur or equivalent custom blur algorithm.
- Animation interpolation.
- Z-order/compositing for panels, popovers, and modals.

The renderer must not delegate world-object/UI rendering to GDI geometry/text calls one primitive at a time as its primary renderer. The intended architecture is a software-composited application framebuffer presented to the Win32 client area through a minimal DIB/GDI transfer path.

## 6. UI engine responsibilities

The custom UI layer must own:

- Widget bounds.
- Layout.
- Hover state.
- Pressed state.
- Focus state.
- Disabled state.
- Pointer capture.
- Keyboard focus routing.
- Hit testing.
- Scroll containers.
- Popover/modal stacking.
- Animation state.
- Dirty/invalidated state as applicable.

Ready-made native child controls (including BUTTON/EDIT/LISTBOX/COMBOBOX/SCROLLBAR/Common Controls), native menus, MessageBox, Common Dialog/IFileDialog, or equivalent OS widgets may not replace these requirements.

## 7. Physics library restrictions

No external physics/collision/math engine may be used, including but not limited to:

- Box2D.
- Chipmunk2D.
- Bullet.
- ReactPhysics3D.
- dyn4j-derived ports.
- SAT libraries.
- Geometry collision libraries.
- External vector/matrix packages.

The required algorithms must exist in the submitted C source.

## 8. Data/parser restrictions

No external JSON, YAML, CSV, serialization, database, or schema-validation library may implement required data handling.

If JSON is used, its parser and serializer must be implemented in C as part of the submission.

CSV output must perform correct quoting/escaping without an external library.

## 9. Test restrictions

The required unit, integration, physics-validation, benchmark, and developer-tool test logic must be part of the submission.

A passing result produced solely by a hard-coded `PASS`, a skipped test, a test that never calls the implementation under test, or a manually edited result file is invalid.

Generated reports must identify each executed test/validation case and pass/fail result.

## 10. No placeholder policy

Prohibited completion substitutes include:

- Buttons that animate but do nothing.
- Inspector fields that do not modify engine state.
- Debug overlays drawn from fake/sample coordinates.
- Scene save that merely writes a screenshot.
- Load that always opens a built-in scene regardless of file contents.
- Contact crosshairs placed at body centers or body-center midpoints.
- “Collision” implemented only by reversing velocity at arbitrary thresholds.
- Polygon collision implemented by AABB overlap only.
- A “joint” drawn as a line while bodies remain unconstrained.
- A “dynamic tree” data structure that is not used by candidate-pair generation.
- Sleeping represented only by a color flag without removing bodies from ordinary integration/solver work.
- Performance reports containing static sample values.

## 11. Determinism expectation

Given:

- identical scene data,
- identical fixed time step,
- identical user/scripted input sequence,
- identical executable/build,

the simulation must produce repeatable body states within floating-point reproducibility expected on the same machine/runtime.

Randomized validation tools must accept and report an explicit seed.

## 12. Numerical representation

The engine may use `float` or `double`, but one scalar type must be selected consistently for physics state.

Tests must use tolerances appropriate to the chosen type.

NaN and infinity must never be intentionally accepted as valid editable scene properties.

## 13. Source organization expectation

Exact file names are not mandated, but the codebase must have recognizable boundaries equivalent to:

- application shell
- software renderer
- UI engine
- physics math
- rigid body/shape
- broad phase
- narrow phase/manifold
- solver
- joints
- scene/parser/serialization
- diagnostics
- tests
- verification
- dev tools

A single monolithic source file containing the entire project does not satisfy maintainability expectations.

## 14. Windows platform contract

The detailed Windows API boundary, framebuffer/presentation model, DPI behavior, UTF-8/UTF-16 boundary, IME integration, focus/capture semantics, and prohibited native substitutions are normative in `25_WINDOWS_PLATFORM_ADAPTATION.md`.

All `WIN-*` and `E2E-WIN-*` cases in `26_WINDOWS_PLATFORM_VALIDATION.md` are release blocking.
