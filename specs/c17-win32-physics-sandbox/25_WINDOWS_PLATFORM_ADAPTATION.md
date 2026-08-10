# 25 — Windows Platform Adaptation and Equivalence Contract

Version: **1.0 Windows sibling**
Status: **Normative final / scope frozen**

## 1. Purpose

This document defines the Windows-native platform boundary for the v1.0 2D Physics Sandbox while preserving the product, physics, UI-engine, validation, evidence, and anti-placeholder difficulty of the Linux/X11 sibling.

This is not a feature-reduced port. Requirements that are platform-independent remain semantically identical unless this document explicitly states a Windows-specific equivalent.

## 2. Reference Windows target

The product is a native **64-bit Windows desktop application** using the classic Win32 desktop API surface.

Reference behavior must be valid on Windows 10 22H2-class and Windows 11-class desktop environments. A submission may support additional Windows versions, but that does not weaken the required behavior here.

The application must be a native desktop window, not a console-only UI, browser/WebView application, packaged webpage, pre-rendered animation, or wrapper around another GUI application.

## 3. Allowed operating-system API boundary

The mandatory production implementation may use these low-level Windows system libraries as the OS boundary:

- **User32** for top-level window creation, message dispatch, mouse/keyboard input, cursor state, focus, pointer/mouse capture, clipboard primitives if implemented, client geometry, and DPI/window notifications.
- **GDI32** only for DIB/framebuffer presentation, native font/glyph discovery/rasterization/metrics where needed, and the minimum device-context operations required to transfer the application-owned pixel buffer.
- **Kernel32** for high-resolution monotonic timing, file I/O/replacement, process/basic system facilities, and memory primitives when ISO C17 has no suitable equivalent.
- **Imm32** for Windows IME composition integration required by editable Unicode text fields.

System header umbrella inclusion such as `windows.h` does not expand the allowed semantic surface. Calling a higher-level subsystem through declarations reachable from that header remains prohibited unless explicitly allowed above.

OS APIs needed solely to query ordinary monitor/window information may be used when they are part of the same low-level Win32 desktop boundary and do not supply prohibited widgets, rendering, layout, animation, blur, physics, parsing, plotting, or file-selection UI.

## 4. Representative permitted Win32 responsibilities

The exact function names are not mandated, but an equivalent implementation may use functionality in the following classes:

- window class registration and top-level window creation;
- `GetMessage`/`PeekMessage`-class message retrieval and dispatch;
- `WM_PAINT`, `WM_SIZE`, `WM_MOVE`, `WM_DPICHANGED`, focus, keyboard, mouse, wheel, capture, close, and IME messages;
- `SetCapture`/`ReleaseCapture`-class pointer capture;
- per-monitor DPI queries and DPI-awareness configuration;
- `QueryPerformanceCounter`/`QueryPerformanceFrequency`-class timing;
- `CreateDIBSection`, `SetDIBitsToDevice`, `StretchDIBits`, or equivalent GDI transfer of a completed software framebuffer;
- wide-character Win32 file operations needed for correct Unicode paths;
- `ImmGetCompositionStringW`-class IME composition retrieval and candidate/composition-window positioning.

This list describes the intended low-level boundary and is not permission to use Win32 controls as the application UI.

## 5. Explicitly prohibited Windows substitutions

The following may not implement or replace any mandatory application-rendered UI, rendering effect, graph, modal, editor control, or scene drawing:

- WinUI / Windows App SDK UI controls;
- WPF;
- Windows Forms;
- .NET GUI wrappers;
- MFC;
- ATL GUI/control wrappers;
- Qt, GTK, SDL, SFML, GLFW, Cairo, Skia, NanoVG, Dear ImGui, Nuklear, raylib;
- Direct2D;
- DirectWrite as a layout/rendering engine;
- DirectComposition;
- OpenGL, Vulkan, Direct3D, or GPU-accelerated scene/UI rendering;
- WebView / WebView2 / browser-rendered HTML/CSS/JavaScript;
- Common Controls (`BUTTON`, `EDIT`, `LISTBOX`, `COMBOBOX`, `TREEVIEW`, `LISTVIEW`, `TRACKBAR`, `PROGRESS`, native tab/toolbar/status controls, and equivalent ready-made widgets) for mandatory product controls;
- Common Dialog APIs or `IFileDialog` as the required Open/Save As UI;
- `MessageBox` as the required application modal/confirmation/error UI;
- native menu/dropdown APIs such as system popup menus as a replacement for required application-rendered menus/dropdowns;
- DWM/Mica/Acrylic/system backdrop APIs as a replacement for the required custom progressive dim/blur/frosted-glass effects.

The standard non-client window frame/title bar supplied by the OS is allowed. Native IME candidate UI supplied by the user's input method is also allowed. Neither exception may be used to replace the application-rendered client-area controls.

## 6. Required framebuffer architecture

The application owns the final client-area software framebuffer.

Required model:

1. physics and UI state are evaluated by application code;
2. world, widgets, text glyphs, graphs, shadows, glow, blur, modal backdrop, and animations are composited into application-owned pixel memory;
3. the completed client framebuffer is transferred to the native window using the permitted GDI presentation boundary;
4. GDI is not used as the primary retained UI or world renderer.

Using `Rectangle`, `RoundRect`, `Ellipse`, `Polygon`, `LineTo`, `GradientFill`, `AlphaBlend`, `TextOut`, `DrawText`, or equivalent GDI drawing calls directly against the window as the primary implementation of required world/UI rendering is prohibited.

A submission may use GDI to obtain glyph bitmaps/metrics, but application code must own text placement, wrapping/field layout where required, selection, caret, focus visuals, clipping, compositing, and animation.

## 7. Native window/message-loop contract

The top-level client window must remain responsive while simulation and rendering are active.

The implementation must:

- process normal paint, input, sizing, move, DPI, focus, close, and IME messages;
- avoid running the entire simulation in a blocking window procedure;
- cap fixed-step catch-up as required by the core timing specification;
- preserve deterministic physics independently of how frequently Windows produces paint/input messages;
- avoid using `WM_TIMER` cadence as the authoritative physics time step;
- treat paint invalidation/presentation cadence as separate from the fixed physics timeline.

`WM_PAINT` may trigger presentation/redraw work, but must not become an alternate variable-step physics driver.

## 8. High-resolution timing

Simulation wall-clock accumulation and performance measurement must use a monotonic high-resolution Windows timing primitive suitable for desktop simulation.

`QueryPerformanceCounter` with its matching frequency is the normative capability class.

The timing layer must:

- convert ticks without integer overflow for expected run durations;
- never derive fixed `dt` from wall-clock frame duration;
- handle long pauses/minimize/drag-resize without unbounded catch-up;
- expose enough timing state for existing fixed-step validation.

## 9. DPI-awareness and coordinate spaces

The application must behave as **per-monitor-DPI-aware** software rather than relying on bitmap virtualization.

Required behavior:

- UI measurements are defined in application logical units and converted deterministically to device pixels;
- the software framebuffer matches the actual client pixel dimensions used for presentation;
- pointer coordinates are converted into the same client/device coordinate model before custom hit testing;
- world-space camera transforms remain independent of monitor DPI;
- moving the window between monitors with different scale factors does not change physics state, force/impulse world values, Shape Cast geometry, or replay digests;
- a DPI change recomputes UI layout, glyph cache entries where required, framebuffer dimensions, hit regions, and visual scale without corrupting state;
- the suggested rectangle from a DPI-change notification or an equivalent correct resize policy is honored so the window remains usable.

DPI scale must not be double-applied to input coordinates or framebuffer size.

## 10. Resize, minimize, restore, and paint recovery

During interactive sizing, ordinary resize, minimize, restore, occlusion, and repaint:

- framebuffer allocation/resizing must remain memory-safe;
- a zero-sized/minimized client area must not cause division by zero or invalid allocation;
- scene state and editor selection must survive;
- no stretched stale pixels may be presented as a completed frame after resize;
- restore must not cause an unbounded simulation catch-up burst;
- a paint request after the window has been obscured/restored must reconstruct/present valid current content;
- render suppression while minimized must not mutate physics behavior except through the already-defined simulation pause/lag policy.

## 11. Mouse capture and focus semantics

Custom widget/body drag operations that start inside the client area must use Windows capture semantics or an equivalent reliable low-level mechanism so dragging continues when the cursor leaves the window while the button remains held.

The UI/input layer must correctly handle:

- capture acquisition;
- normal button-up release;
- unexpected capture loss/change;
- application deactivation;
- window focus loss;
- destruction/deletion of the actively dragged body/widget;
- modal opening while another pointer interaction is active.

Capture loss must cancel or safely commit the current gesture according to the relevant tool contract. It must never leave a permanent pressed/dragging state.

Keyboard state must similarly clear or reconcile held-key state on focus loss so Alt-Tab or another activation change cannot leave a simulated shortcut/key permanently held.

## 12. Keyboard and text-input boundary

The custom text/numeric editors own editing semantics. Windows only supplies low-level key/text/IME events.

Required separation:

- physical shortcut/navigation keys are handled as key events;
- committed textual characters are handled through Unicode text input;
- IME composition is not treated as committed text until the IME commits it;
- a text field must not receive both translated text and a second duplicate insertion from the same key path.

The implementation must support ordinary Unicode scene/body names and file paths, including Chinese text input through a system IME.

## 13. UTF-8 / UTF-16 boundary

Application-owned persistent strings and JSON text use UTF-8.

Win32 APIs that require wide strings are called through validated UTF-16 conversions at the platform boundary.

Conversions must correctly handle:

- Basic Multilingual Plane characters;
- valid UTF-16 surrogate pairs;
- invalid/unpaired surrogate input without memory corruption;
- empty strings;
- long but valid paths/labels up to documented application limits;
- round-trip conversion where the Windows API returns the same Unicode path/text.

Lossy ANSI-code-page conversion is not acceptable for user scene names or scene file paths.

## 14. IME composition

Editable text fields must interoperate with Windows IME composition through Imm32-class APIs/messages.

At minimum:

- composition start establishes a composition state in the focused custom editor;
- intermediate composition text is visibly distinguished from committed text;
- composition updates do not dirty/commit the underlying property until a real commit occurs;
- result/commit text is inserted exactly once;
- cancellation removes composition without inserting stale text;
- focus transfer/cancel does not leave orphaned composition state;
- candidate/composition placement is associated with the active custom field rather than a stale screen position;
- numeric fields may reject non-numeric committed text according to the normal validation rules without crashing the IME flow.

The operating system's IME candidate window is not considered a prohibited ready-made application widget.

## 15. Unicode-aware file paths

Open, Save, Save As, export, replay, checkpoint, and evidence-related application file paths must be able to represent ordinary Unicode Windows paths.

The required app UI for Open/Save As remains custom-rendered. A path-entry modal or a custom browser may be used. Native Common Dialog / `IFileDialog` is prohibited for satisfying the required UI workflow.

File behavior must handle:

- drive-letter and rooted paths;
- backslash separators;
- filenames containing non-ASCII Unicode;
- existing read-only/permission-denied targets;
- directory targets where a file is required;
- replace/rename failure;
- temporary safe-save file cleanup;
- Windows sharing/locking failure without claiming success.

Scene JSON content and deterministic serialization remain platform-independent.

## 16. Close protocol and application modals

Top-level close requests, including the window close button and system close command, must route through the same unsaved-change state machine as the product's New/Open workflows.

The required Save / Discard / Cancel UI must be application-rendered inside the client area.

Using `MessageBox` or a native dialog as the mandatory close confirmation is invalid.

## 17. Native/non-client behavior boundary

The implementation may rely on the OS for:

- window frame/title bar;
- minimize/maximize/restore buttons;
- move/resize behavior initiated through the non-client frame;
- standard system cursor shapes;
- IME candidate windows;
- ordinary desktop composition outside the application's client rendering.

The implementation may not count any of those as satisfying the custom button, modal, menu, animation, blur, focus, graph, inspector, or scene-rendering requirements.

## 18. Linux/X11-to-Windows equivalence matrix

| Linux/X11 sibling responsibility | Windows sibling equivalent | Equivalence requirement |
| --- | --- | --- |
| X11 top-level window | User32 top-level Win32 window | Same application client functionality |
| X11 event loop | Win32 message loop | Input/render cadence cannot drive variable-step physics |
| X11 mouse/keyboard events | Win32 mouse/keyboard messages | Same custom hit-testing and shortcut behavior |
| X11 pointer grab/capture | Win32 mouse capture | Drag remains correct outside client area |
| X11 text input/XIM-class composition | Win32 Unicode + Imm32 IME | Unicode/Chinese input remains functional |
| X11 visual/pixel presentation | GDI DIB/framebuffer transfer | App owns complete software framebuffer |
| X11 core glyph source if used | GDI glyph bitmap/metric source if used | Glyph source only; custom text/UI layout remains app-owned |
| Linux monotonic timing | QueryPerformanceCounter-class timing | Fixed-step semantics remain identical |
| Linux file operations | Kernel32 wide-character file operations | Safe save and Unicode paths preserved |
| X11 window expose | WM_PAINT invalidation/repaint | Reconstruct/present valid current frame |
| X11 configure/resize | WM_SIZE + DPI-aware layout | No physics change; framebuffer safely resized |
| X11 display-scale assumptions | Per-monitor DPI handling | Device scale changes visual size only, not world physics |

## 19. Cross-platform invariant set

The following must remain semantically equivalent between the Linux and Windows v1.0 task packages:

- shape and body scope;
- fixed-step integration;
- broad phase, narrow phase, manifolds, solver, joints;
- CCD/TOI and Shape Cast;
- sensors, queries, replay/checkpoints;
- collision filtering;
- force/impulse and recorder behavior;
- Solver Inspector production observability;
- scene JSON semantics and deterministic ordering;
- Golden fixture definitions and numeric envelopes;
- mandatory non-platform test IDs and expected counts;
- performance workload semantics;
- PASS/BLOCKED release philosophy;
- anti-placeholder and prohibited-external-library rules.

A Windows port may not loosen a numeric physics tolerance merely because the windowing system differs.

## 20. Windows-only surface that must not affect physics

The following state is platform/UI state and must not alter the canonical physics digest unless an explicit user command changes the world:

- DPI scale;
- top-level window position;
- monitor identity;
- minimize/restore state;
- repaint/expose frequency;
- custom UI hover/focus state;
- active IME composition that has not committed a scene property;
- framebuffer pixel dimensions;
- OS cursor shape.

## 21. Required implementation evidence

The Windows implementation must make it possible to audit that:

- permitted DLL/API categories are used only for the allowed boundary responsibilities;
- required custom controls are application-rendered rather than child Win32 controls;
- the final framebuffer is software-composited by submitted C17 code;
- DWM/Direct2D/DirectWrite/Direct3D/Common Controls/Common Dialog/MessageBox are not used to satisfy mandatory UI/rendering behavior;
- Unicode and IME paths execute through real custom editor state;
- DPI, resize, focus, and capture handling do not mutate physics.

The mechanism used to collect this evidence is not prescribed.
