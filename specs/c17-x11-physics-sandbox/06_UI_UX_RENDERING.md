# 06 — Custom UI/UX and Software Rendering Specification

## 1. Visual objective

The application must present as a deliberate modern desktop tool rather than a raw X11 demo.

Visual quality is a release requirement, not decoration.

The UI must be internally consistent in spacing, typography, elevation, corner radius, focus behavior, hover behavior, and animation timing.

## 2. Design system

The implementation must define reusable design tokens or equivalent constants for:

- background surfaces.
- elevated surfaces.
- text primary/secondary/muted.
- accent.
- danger/error.
- warning.
- success.
- borders.
- focus ring.
- corner radii.
- spacing scale.
- animation durations.
- shadow/elevation levels.

Hard-coded unrelated colors and spacing repeated independently across widgets should be avoided.

## 3. Required widget families

The custom UI engine must implement reusable forms of:

- button.
- icon button.
- segmented/capsule tab control.
- label.
- numeric input.
- text input sufficient for scene/body naming and file-path display/editing where used.
- checkbox/toggle.
- slider.
- dropdown/menu.
- scroll container.
- list/grid card.
- tooltip.
- popover.
- modal dialog.
- toast/banner notification.
- separator.
- collapsible section.
- status badge.

Widgets must be used consistently rather than each screen drawing bespoke one-off controls.

## 4. Interaction states

Interactive controls must distinguish at least:

- normal.
- hover.
- pressed.
- focused.
- disabled.

A disabled control must not activate through mouse or keyboard.

## 5. Hover elevation

Buttons and clickable cards must visibly lift on hover.

Required behavior:

- transition begins within one rendered frame of entering hover.
- apparent vertical lift target: 1–3 device pixels.
- shadow/elevation increases concurrently.
- duration: 100–180 ms.
- leaving hover reverses smoothly.

The effect must not alter surrounding layout positions.

## 6. Click ripple

Primary interactive buttons and scene cards must show a click/tap ripple.

Requirements:

- ripple originates at pointer-down coordinate within the widget.
- ripple expands radially.
- ripple alpha fades during expansion.
- ripple is clipped to the widget's rounded boundary.
- duration: 250–450 ms.
- multiple rapid clicks must not corrupt widget state; multiple concurrent ripples may coexist or replace deterministically.

A static flash across the whole button is not sufficient.

## 7. Border glow

Focused or actively selected controls must show an animated or smoothly transitioned border glow.

Requirements:

- glow does not obscure label/content.
- focus and validation-error glow appearances are distinguishable.
- glow is composited by the custom renderer.

## 8. Capsule sliding indicator

Top navigation and any segmented control must use a capsule-shaped active indicator.

When selection changes:

- indicator position interpolates from old bounds to new bounds.
- indicator width may interpolate if labels have different widths.
- label color transitions concurrently.
- duration: 180–320 ms.
- easing: smooth cubic Bézier or equivalent non-linear easing.

Instantly removing one capsule and drawing another is invalid.

## 9. Dynamic collapse

At least the right inspector and one additional suitable panel/section must support animated collapse/expand.

Required behavior:

- width or height transitions smoothly.
- child content opacity may fade during transition.
- hit testing follows visible state.
- collapsed content cannot receive hidden clicks/focus.
- layout reflows continuously or at a clearly defined transition boundary without flicker.

## 10. Modal transition

Opening and closing application-rendered modal dialogs must combine **scale and opacity**.

Open transition:

- starting scale between 0.94 and 0.98.
- starting opacity near 0.
- ending scale 1.0.
- ending opacity 1.0.

Close transition reverses appropriately.

Duration: 180–320 ms.

Easing must use an elastic-feeling but non-overshooting or lightly overshooting cubic Bézier-style curve. Exact control points must be defined in source constants.

The modal must not become interactive before it is visually present enough to be understood, and it must stop accepting input once close has committed.

## 11. Progressive backdrop dim + blur

When a modal opens:

- underlying application content darkens progressively.
- underlying application content blurs progressively.
- dim and blur animate with modal transition.
- blur is applied to application-rendered content, not to the entire desktop.

Required blur behavior:

- custom separable Gaussian approximation, box-blur passes, stack blur, or equivalent software algorithm.
- final blur radius visually equivalent to at least ~8 device pixels at 1× scale.
- blur may operate on downsampled off-screen surface for performance if result remains visually smooth.

A plain translucent black overlay without blur is insufficient.

## 12. Dynamic Frosted Glass Navigation

The top navigation bar must implement scroll-responsive frosted glass in the **Scenes**, **Diagnostics**, and **About** views where content can scroll beneath it.

Let `s` be vertical scroll offset.

Between 0 and a configured threshold `S` (default 96 px):

- backdrop blur intensity increases smoothly from minimal/zero to full nav blur.
- nav surface opacity may increase smoothly.
- bottom shadow strength increases from zero to full elevation.

Beyond `S`, values remain clamped to full strength.

Scrolling back upward must reverse continuously.

No abrupt threshold jump is acceptable.

## 13. Scroll containers

Custom scroll containers must support:

- wheel scrolling.
- clipped content.
- scrollbar or another clear position indicator for long content.
- correct hit testing in scrolled coordinates.
- nested interaction without sending clicks to off-screen elements.

Kinetic/inertial scrolling is optional.

## 14. Software rendering quality

### 14.1 Alpha blending

Renderer must perform consistent source-over alpha composition.

### 14.2 Rounded rectangles

Rounded corners must not visibly break at common radii or scales.

### 14.3 Lines

World/debug lines must remain legible under zoom.

### 14.4 Convex fill

Convex polygon fill must not depend on X11 polygon fill as the primary rendering implementation.

### 14.5 Clipping

Every scroll container, ripple, modal surface, and panel needing clipped children must respect clipping bounds.

## 15. Text

Text must remain readable at all supported window sizes.

If X11 core glyph rasterization is used, it may only supply glyph pixels. The application still owns:

- placement.
- alignment.
- clipping.
- baseline/layout.
- field editing logic.
- focus/selection visuals.

The UI must not depend on a native X11 widget toolkit.

## 16. Numeric input

Physics property fields require numeric editing.

Numeric input must support:

- optional sign.
- decimal values.
- scientific notation is recommended but not mandatory.
- visible invalid-entry state.
- commit on Enter or focus loss.
- Escape reverts uncommitted edit.
- bounds validation.

Invalid temporary text must not immediately overwrite valid engine state.

## 17. Pointer capture

Dragging sliders, bodies, scrollbars, panel splitters, or creation gestures must continue correctly when pointer leaves the originating widget while the button remains held.

Release must terminate capture reliably.

## 18. Keyboard focus

At most one text/numeric editor owns keyboard text focus.

Tab must traverse principal inspector controls in a stable order.

Escape dismisses or cancels the highest-priority transient interaction.

## 19. Resize behavior

Window resize must:

- update framebuffer safely.
- preserve current scene state.
- recompute layout.
- avoid stretched previous-frame garbage.
- preserve camera center as reasonably as possible.
- keep modal centered or responsively positioned.

## 20. Rendering performance

At 1280×720 with the standard 100-body benchmark scene and default debug overlays off:

- UI/rendering should target 60 FPS.
- the application must remain responsive while physics runs.

Hard acceptance performance thresholds are in `09_TEST_VERIFICATION.md`.

## 21. Visual acceptance checkpoints

Required visual evidence includes:

- default Sandbox.
- hover-elevated button.
- click ripple mid-animation.
- active capsule indicator between two destinations or frame sequence proving motion.
- collapsed inspector transition frames.
- modal opening frame with scale/opacity transition.
- modal fully open with blurred/dimmed backdrop.
- Scenes view at scroll 0 and deep scroll showing different frosted-nav/shadow strength.
- collision contact crosshair/normal overlay.
- five-block stable tower.
- suspension bridge under load.

See `11_ACCEPTANCE_EVIDENCE.md` for packaging.

## 22. Force/impulse interaction visuals

The custom UI must provide a coherent tool state for Apply Force and Apply Impulse.

Required custom-rendered elements include:

- application-point marker;
- world-space vector arrow;
- magnitude label;
- editable numeric vector controls;
- active/preview distinction;
- transient committed-impulse indicator.

The vector arrow and numeric values must remain readable under camera zoom and must correspond to the same world-space value.

## 23. Motion Analysis graph rendering

The custom renderer must implement the required Motion Analysis time-series graph without external charting code.

The graph is a functional data-inspection widget, not decorative art. It must implement axis layout, numeric tick labels, series clipping, legend, cursor/sample readout, auto-ranging, and history navigation described in `14_FORCE_TRAJECTORY_TOOLS.md`.

Trajectory trails must be rendered in world space while graph series are rendered in UI/screen space. Camera transforms therefore affect trails but must not alter the stored sample values or graph data.

## 24. Solver Inspector UI

Solver Inspector is a custom-rendered diagnostics surface governed by `18_SOLVER_INSPECTOR.md`.

It must include:

- deterministic contact and joint lists;
- clear selected/pinned/inactive states;
- live scalar and vector fields;
- type-specific contact/joint sections;
- a scrollable per-iteration numeric table;
- a custom-rendered iteration graph;
- Capture Next Step, Freeze Capture, Clear Capture, and Export Trace controls.

Rows containing clamped impulses, active limits, non-finite values, or anomaly-sentinel conditions must be visually distinguishable without relying on color alone.

The panel must remain readable at the minimum supported window size and may use its own scroll container rather than truncating required fields.

Inspector graph rendering must consume the recorded trace data directly and must not use a decorative synthetic series.

## 25. Collision Matrix, Shape Cast, and Timeline UI

The custom UI engine must render the v1.0 additions without high-level UI/charting libraries.

Required surfaces:

- Collision Matrix editor with 16 named categories and symmetric pair controls;
- selected-body CCD mode control;
- selected-body category/mask/group controls;
- Shape Cast configuration/results panel and viewport sweep visualization;
- Replay Timeline with zoom/pan, cursor, command/event/anomaly/checkpoint/bookmark markers;
- replay seek/step/back/bookmark/fork controls;
- Golden Scenario status list/report entry points.

Timeline and graph rendering must remain functional at minimum supported window size through deliberate scrolling/zooming rather than truncating mandatory state.

Marker meaning may not rely on color alone. Keyboard focus and pointer hit testing must function through the same custom widget system as the rest of the application.
