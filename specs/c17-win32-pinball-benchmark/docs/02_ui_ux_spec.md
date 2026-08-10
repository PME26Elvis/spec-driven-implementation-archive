# 02 — UI / UX and Software Rendering Specification

## 1. Rendering architecture

The application MUST implement its own immediate-mode, retained-mode, or hybrid UI layer in C. The resulting system MUST provide reusable primitives for layout, hit testing, focus, animation, clipping, text placement, and drawing.

Win32 User32/GDI32 MAY open the top-level window, receive input, manage low-level platform state, and present the application-owned pixel buffer subject to document 32. Required visuals MUST NOT be replaced by native controls or system drawing primitives. Required visuals MUST NOT be replaced by native toolkit widgets.

## 2. Required software-rendered primitives

The renderer MUST support at least:

- solid rectangles;
- rounded rectangles;
- circles;
- thick lines;
- capsule shapes;
- alpha compositing;
- clipping rectangles;
- shadows;
- gradients or equivalent subtle tonal transitions;
- text and simple icons;
- offscreen surface/pixel-buffer operations sufficient for required blur.

Drawing MUST clip safely to window/panel boundaries. Alpha compositing MUST avoid obvious halos at normal scale.

## 3. Visual design direction

The UI SHALL present as a contemporary desktop creative/engineering tool rather than a raw debug program.

Required traits:

- clear hierarchy;
- rounded but restrained controls;
- consistent spacing system;
- legible typography;
- distinct canvas and panel surfaces;
- subtle shadows rather than heavy bevels;
- visible mode state;
- polished hover/pressed feedback;
- consistent motion timing.

The product need not imitate any branded application.

## 4. Required control states

Every applicable interactive control MUST render relevant states:

- normal;
- hover;
- pressed;
- focused;
- active/selected;
- disabled.

States may combine. State transitions MUST NOT cause geometry jitter.

## 5. Hover elevation

Buttons, tool cards, palette entries, and comparable targets SHALL visibly rise on hover.

Required behavior:

- begins within one rendered frame of hover detection;
- target upward visual offset: 1–3 px at 1× UI scale;
- shadow opacity/spread increases simultaneously;
- transition duration: 140–200 ms;
- leaving hover reverses smoothly from current animated state;
- disabled controls do not elevate.

A simple color swap does not satisfy this requirement.

## 6. Click ripple

Primary buttons, segmented controls, toolbar buttons, and palette items SHALL display a click ripple.

Required behavior:

- origin is pointer-down coordinate transformed into local control space;
- ripple expands radially;
- ripple is clipped to the control's rounded bounds;
- ripple fades while expanding;
- total duration: 280–450 ms;
- rapid clicks MAY produce overlapping ripples;
- decorative ripple MUST NOT delay the logical action.

A centered precomputed flash is not acceptable.

## 7. Border glow

Focused or selected high-value controls and active editor tools SHALL display a soft border glow.

- fade in/out: 120–220 ms;
- follows actual rounded outline;
- not rendered on disabled controls;
- focused and selected states remain distinguishable.

## 8. Sliding capsule indicator

Edit/Play mode and other mutually exclusive segmented controls SHALL use a sliding capsule indicator.

- indicator translates between segments rather than disappearing/reappearing;
- label/icon treatment updates coherently;
- duration: 180–280 ms;
- uses the standard application easing curve;
- rapid reversal starts from current interpolation progress.

## 9. Animated collapsible panel

At least one side panel, normally the left palette, MUST support animated compact/collapsed state.

- width animates smoothly;
- labels fade or clip gracefully;
- icon alignment interpolates without jumping;
- canvas receives/relinquishes freed space during transition;
- hit testing follows current visible geometry;
- invisible expanded controls MUST NOT remain clickable after collapse.

## 10. Modal opening/closing

Modal dialogs MUST combine scale and opacity.

Opening:

- initial scale: 0.94–0.97;
- final scale: 1.00;
- opacity 0 → 1;
- duration: 180–300 ms.

Closing reverses from the current state.

Normative application easing:

`cubic-bezier(0.22, 1.00, 0.36, 1.00)`

A mathematically equivalent sampled implementation is acceptable.

## 11. Modal backdrop dim and blur

Opening a modal MUST progressively dim and blur the application's own already-rendered background content.

- blur increases continuously during open;
- backdrop darkening increases continuously;
- close reverses both effects;
- modal remains unblurred;
- hit testing behind modal disables immediately when opening begins;
- effect applies only to the application-owned already-rendered content, not the desktop or other windows. DWM Acrylic/Mica/blur-behind is not a substitute.

Fully-open target:

- equivalent blur radius: 6–12 px at 1× scale;
- dark overlay alpha: 0.25–0.45.

A dark rectangle without blur fails.

## 12. Dynamic Frosted Glass top toolbar

The top toolbar SHALL use a frosted-glass treatment over application canvas content.

- canvas content geometrically behind toolbar is sampled from an offscreen application surface;
- toolbar applies background blur plus translucent tint;
- lower shadow intensity changes smoothly with interaction/runtime state;
- moving balls/objects under toolbar region do not reveal stale buffered frames;
- idle/editor state may reduce blur/shadow intensity but transitions smoothly.

A normalized internal or derivable `frost_level` 0–1 SHALL exist for verification. At 0 shadow/blur is minimal; at 1 full target treatment is applied.

## 13. Animation interruption semantics

All required animations MUST support interruption. If an animation reaches 40% and its target reverses, it reverses from approximately the current visual value, not by first jumping to an endpoint.

Applies to:

- hover;
- panel collapse/expand;
- modal open/close;
- segmented capsule;
- glow;
- toolbar frost intensity.

## 14. Animation timing independence

UI animation progress MUST use monotonic elapsed time. It MUST NOT assume a fixed display refresh rate. A dropped frame may skip a visual sample but MUST NOT permanently slow animation.

Physics timestep is separate from UI timing.

## 15. Canvas rendering

Canvas MUST render:

- table background;
- authored objects;
- Edit Mode selection states;
- active balls in Play/Preview;
- runtime score/HUD as appropriate;
- debug overlays when enabled.

Rendering order MUST be deterministic. Visual z-order MUST NOT change physics iteration order.

## 16. Editor visual states

Selected objects MUST have:

- visible selection outline;
- manipulation handles where applicable;
- readable object ID or tooltip on demand;
- distinction between primary selection and other multiselected objects.

Hovered unselected objects must be distinguishable without obscuring normal appearance.

## 17. Manipulation handles

Handles SHALL have at least a 10×10 px pointer target at normal scale. Rotate handles visually differ from resize handles. Dragging shows live feedback.

## 18. Properties Inspector

Inspector SHALL support:

- labels;
- numeric fields;
- sliders where appropriate;
- toggles;
- enum selectors;
- object references;
- action/event lists.

Numeric fields SHALL display precise values even when paired with sliders. Invalid typed values remain visibly invalid until corrected/committed; they MUST NOT silently become unrelated valid values.

## 19. Status bar

Status bar SHALL display context-dependent information including:

- current mode;
- zoom percentage;
- pointer world coordinate over canvas;
- selected-object count in Edit Mode;
- current simulation time in Play Mode;
- simulation speed;
- active ball count;
- score;
- validation/error indicator when relevant.

## 20. Toasts and notifications

Transient notifications SHALL animate in/out, avoid covering modal primary actions, remain long enough to read, not accumulate indefinitely, and support info/warning/error severity.

Critical destructive choices use modals.

## 21. Color/contrast

Exact colors are not fixed, but:

- text/background contrast remains readable;
- disabled controls remain legible while visibly inactive;
- warnings/errors use more than color alone;
- debug vectors are distinguishable from authored geometry.

## 22. UI scale baseline

Geometry MUST flow through a central UI scale factor rather than scattered unscalable constants. Windows Per-Monitor DPI behavior in document 23/32 is required; geometry still flows through centralized scale factors rather than scattered pixel constants.

## 23. Resize behavior

During continuous resize:

- no crash;
- no negative panel dimensions;
- no buffer overrun;
- no persistent stale content outside new bounds;
- canvas transform remains valid;
- active modal remains centered/clamped to available region.

## 24. Text clipping

Long table names, IDs, and validation messages MUST not draw outside assigned rectangles. Use clipping, ellipsis, scrolling, or wrapping depending on control type.

## 25. Visual acceptance principle

The specification defines observable states and transitions, not the utility used to capture screenshots or recordings. The implementation is responsible for producing required evidence by reasonable means available in its environment.

## 26. Focus, text, HiDPI, and Reduced Motion

The software UI engine SHALL implement the complete behavior in `23_text_input_focus_hidpi_and_reduced_motion.md`, including focus traversal/scopes, keyboard activation, UTF-8-safe text editing, Chinese preservation/search, 100/125/150/200% re-rasterized UI scales, Reduced Motion, popup input ownership, animation interruption, and repaint/damage correctness.

## 27. Advanced editor UI surfaces

The visual system SHALL provide usable surfaces for Layers/Groups/Lock state, alignment/distribution, exact Inspector transforms, Measurement, Scene Statistics, Event Trace, Collision Trace, and Command Palette. These MAY share the existing left/right/bottom panels but SHALL remain reachable at the minimum supported window size through scrolling/collapse where necessary.

## 28. New pinball mechanism states

Distinct rendered states are required for raised/dropped targets, active rollover indication, spinner orientation/motion, occupied Kickout, and Tilt. These states shall not be represented solely by changing text elsewhere in the UI.

## 29. High-DPI visual contract

At every required UI scale, shadows, blur, rounded corners, ripple clipping, focus ring, hit areas, and text shall be re-evaluated in device-independent metrics. Uniform nearest/bilinear scaling of a finished 100% framebuffer is not an acceptable HiDPI implementation.

## 30. Visual trace/statistics usability

Trace/statistics views must remain readable while Play is paused and shall not cover all of the playfield by default. Opening/closing them is view-only and must not change deterministic state or authored dirty state.


## 30. Windows framebuffer presentation

The visual result SHALL be produced into an application-owned pixel buffer. GDI may transfer/present that completed buffer to the client area and may provide the narrow glyph acquisition services permitted by document 32; it SHALL NOT be used to draw the required buttons, panels, gradients, shadows, rounded rectangles, ripple, glow, blur, pinball canvas primitives, or application text directly into the window as a substitute for the software renderer.

The application SHALL repaint correctly under WM_PAINT and after uncover/minimize/restore/resize without depending on preserved screen pixels.
