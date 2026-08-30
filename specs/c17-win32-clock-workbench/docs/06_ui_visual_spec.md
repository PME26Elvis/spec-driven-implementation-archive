# 06 — UI and Visual Specification

## 1. Visual objective

The UI must look like a deliberate modern desktop interface rather than default Win32 controls or a debugging control panel.

Visual quality is a functional requirement because several rendering and animation features are explicitly part of the assignment.

## 2. Rendering ownership

Every mandatory visual component is drawn by the application into its own pixel buffer.

No native Win32 button, edit control, menu widget, scrollbar, common control, or dialog may substitute for the custom UI.

## 3. Base style

Required design characteristics:

- dark neutral background by default;
- layered surfaces with restrained depth;
- rounded cards and controls;
- high-contrast primary text;
- softer secondary text;
- one configurable accent hue;
- consistent spacing scale;
- consistent corner-radius scale;
- shadows that communicate elevation rather than heavy outlines.

Exact colors are implementation choices unless configured.

## 4. Main Clock layout

At the reference window size `1100×760`:

- top navigation occupies approximately 64–76 logical pixels;
- main content is centered with generous margins;
- analog clock takes the dominant visual area;
- digital editor and controls form a coherent side panel or lower panel;
- status strip remains visible without obscuring content.

The layout may differ proportionally while preserving hierarchy.

## 5. Clock card

The analog clock is placed on a distinct surface/card.

The card must include:

- rounded corners;
- subtle border or inner highlight;
- soft shadow;
- sufficient padding so the hand tips never clip under normal scale.

## 6. Clock face

The face must be circular with anti-aliased edges.

Required tick hierarchy:

- 12 major hour ticks;
- 60 total minute positions;
- major ticks visibly stronger than minor ticks.

Major numerals may be `12`, `3`, `6`, `9` at minimum, though all twelve are allowed.


## 6A. Face-style presets

`clock.face_style` must produce visibly distinct geometry rather than only changing a label:

- `minimal`: single outer face boundary, clean tick hierarchy, no decorative inner scale ring;
- `classic`: double/rimmed boundary and stronger hour ticks with restrained center/ring detail;
- `technical`: adds a distinct inner scale ring plus small five-minute marker dots or equivalent instrumentation detail.

`display.show_all_hour_numbers` independently controls whether all twelve hour numerals are shown. When false, at least `12`, `3`, `6`, and `9` are shown in every face style.

## 7. Hand styling

The three hands must differ clearly in length, thickness, or color.

Required order:

- hour hand shortest and heavier;
- minute hand longer;
- second hand longest and visually finer/accented.

Hover/drag emphasis must not make the active hand indistinguishable from the others.

## 8. Digital display card

Digital time uses large fixed-width or visually aligned glyphs.

Hour, minute, and second fields must look separately editable while still reading as one time string.

Separators `:` remain visible but are not editable fields.

Focused field receives a clear focus ring/glow or background change.


## 8A. Digital scrub feedback

A numeric field that crosses the scrub threshold must visibly enter a scrub state distinct from ordinary keyboard focus. The field receives stronger glow/emphasis and may show a small up/down affordance. During scrub, the visible numeric value and analog clock update continuously from canonical time. The UI must not leave an invisible drag active after release/cancel/focus loss.

## 9. Caret behavior

A traditional text caret is optional because fields are fixed-format numeric editors.

If no caret is shown, the active edit buffer must still be unambiguous.

## 10. Playback control card

The playback controls must visually group:

- Play/Pause button;
- rate slider;
- current numeric rate;
- `1×` reset button/affordance.

The sign of negative rates must be visually obvious.

## 11. Rate slider geometry

The rate slider includes:

- background track;
- zero point indicator;
- filled/active segment from zero to current rate;
- draggable thumb;
- optional key labels such as `-100`, `-1`, `0`, `1`, `100`.

The zero point must be visually discoverable.

## 12. Slider thumb interactions

Thumb states:

- idle;
- hover enlarged/emphasized;
- pressed/dragging emphasized;
- keyboard-focused with focus ring.

The thumb must remain easy to grab at supported UI scales.

## 13. Buttons

Mandatory buttons share a consistent custom component implementation.

Buttons must have:

- rounded shape;
- hover elevation;
- pointer-down ripple;
- pressed feedback;
- focus indication;
- disabled appearance;
- animated border glow on hover/focus for every enabled mandatory button.

## 14. Border glow

The glow is a soft outer/inner luminance around the button border. Every enabled mandatory button must exhibit it on hover and keyboard focus; destructive/secondary buttons may use lower intensity but may not omit the effect.

It must animate in/out rather than appear as an abrupt hard stroke.

The glow must be clipped/combined so that it does not contaminate unrelated controls.

## 15. Capsule navigation

The Clock/Settings selector is a pill-shaped track with an animated active capsule.

When switching tabs:

- the capsule slides to the new item;
- its width interpolates if labels differ;
- label foreground states crossfade or interpolate;
- page content transition coordinates with the capsule movement.

The capsule must not teleport instantly when animations are enabled.

## 16. Dynamic navigation collapse

When Settings content scrolls downward, the navigation bar enters a compact/collapsed state.

Required behavior:

- expanded nav height at the reference scale is 72 logical pixels;
- compact nav height is 56 logical pixels;
- height interpolates continuously over `effects.frost_scroll_range`;
- the title translates upward by 4 logical pixels and scales visually to approximately 92% at full collapse;
- controls remain usable and their hit targets remain at least 32 logical pixels high;
- nav height never becomes smaller than 56 logical pixels before UI scaling.

Scrolling back up reverses the transition.

## 17. Dynamic frosted navigation

When scroll offset increases from zero, the top navigation surface must progressively increase its software-rendered frosted-glass appearance.

Required behavior over `effects.frost_scroll_range`:

- with `p = clamp(scroll_y / effects.frost_scroll_range, 0, 1)`, effective nav blur radius is `p * effects.nav_blur_radius`;
- bottom-shadow strength is `p * effects.shadow_strength`;
- nav height is `72 - 16*p` logical pixels;
- title translation is `-4*p` logical pixels and title scale is `1 - 0.08*p`;
- translucent overlay/tint opacity must also increase continuously with `p` and may remain a visual-design choice;
- no required effect may switch by a binary threshold.

## 18. Frosted navigation scope

The blur must sample previously rendered application content underneath the navigation bar.

It does not need to blur the desktop or other applications.

The implementation may use an offscreen buffer or retained pre-nav layer.

## 19. Settings content

Settings controls are arranged in labeled sections/cards.

Suggested sections:

- Display;
- Clock behavior;
- Animation;
- Playback;
- Configuration/Persistence.

Each section must have consistent vertical rhythm. With the shipped default configuration at the reference `1100×760` size, the Settings content extent must be tall enough to permit at least `effects.frost_scroll_range` logical pixels of real scroll travel; the required frosted/collapse behavior must therefore be reachable without resizing the window or fabricating test-only content.

## 20. Settings controls

Settings require application-rendered versions of:

- toggle switch;
- slider;
- segmented/capsule choice;
- numeric field;
- time field;
- action button;
- scrollable panel.

A traditional drop-down is optional; segmented controls may be used where choice count is small.

## 21. Toggle animation

Toggle switch thumb moves with eased interpolation.

Track fill/opacity transitions simultaneously.

Keyboard toggling must trigger the same animation.

## 22. Modal windows

Confirmation/error/help surfaces that require modal behavior are in-app modal cards.

Opening transition combines:

- opacity from exactly 0 to 1;
- scale from exactly 0.96 to 1;
- no mandatory translation; implementations may add at most 4 logical pixels of vertical travel as decorative polish without changing hit/layout geometry;
- the fixed open cubic Bézier easing defined below.

Closing starts from the current interpolated state when interrupted and targets opacity 0 / scale 0.96 using the fixed close curve.

Closing reverses appropriately.


## 22A. Top-level scene open/close transition

The visible application scene must also use scale + opacity transitions on ordinary launch and graceful close. This is application-rendered content animation, not a DWM/layered-window/compositor effect. The scene remains centered while scaling so controls do not appear to slide from a corner. The exact state/input/shutdown contract is defined in `07_rendering_and_animation.md`.

## 23. Modal easing

The implementation must provide a reusable cubic Bézier easing evaluator. For this assignment the default modal/top-level scene curves are fixed:

- open/entrance: cubic Bézier control points `(0.20, 0.90, 0.25, 1.05)`;
- close/exit: cubic Bézier control points `(0.40, 0.00, 0.60, 1.00)`.

The evaluator treats normalized wall-time progress as the x-coordinate and must solve/invert the Bézier x dimension. Eased y may slightly exceed `1` on the opening curve to create elastic character. Opacity is clamped to `[0,1]`; scale may use the small overshoot but must remain finite and visually bounded. These constants must be defined in source and covered by tests.

## 24. Modal background treatment

While modal is open:

- background content gradually darkens;
- background content gradually blurs;
- modal remains sharp;
- input to background is blocked.

Let `q` be the clamped eased modal-open progress (`0` fully closed, `1` fully open). Effective modal background blur radius is `q * effects.modal_blur_radius`. The mandatory black/dark dim overlay has maximum alpha `0.42`, so its effective alpha is `0.42*q`. Closing uses the continuously retargeted reverse progress. This treatment must not pop abruptly.

## 25. Modal blur implementation

The blur must be a real spatial blur or documented separable approximation applied to the application's background pixel content.

Replacing blur with only a translucent dark rectangle does not satisfy the requirement.

## 26. Error surfaces

Recoverable errors such as config parse failure must use a non-destructive modal or prominent inline panel.

Errors must contain:

- concise title;
- explanation;
- relevant location/path when safe;
- dismiss action;
- retry/reload action when relevant.

## 27. Toast/status messages

Short success messages may appear as transient toasts or in the status strip.

Examples:

- `Configuration saved`;
- `Configuration reloaded`;
- `Time reset`.

A toast must not replace a modal for an action requiring user decision.

## 28. Animation disabled mode

When UI animations are disabled in Settings/config:

- final visual states must still be correct;
- interactions must remain functional;
- no component may depend on animation completion to finalize essential logical state;
- ripples may be omitted or immediately completed;
- modal background still needs a valid final dim/blur state.

## 29. Reduced rendering load

The application may skip rendering frames while completely idle.

Animations and smooth-time movement must wake rendering as required.

## 30. Logical pixel scaling

The renderer must separate logical layout units from physical framebuffer pixels sufficiently to support at least one configurable UI scale factor.

Mandatory supported scale settings:

- `1.0`;
- `1.25`;
- `1.5`.

The application must be DPI-aware as required by the Windows platform contract. Native monitor-DPI changes must be detected/handled sufficiently to avoid DPI virtualization and keep framebuffer pixels aligned with hit testing; however, monitor DPI must not automatically rewrite the user-configured `window.ui_scale`. The manual scale settings below remain application-level controls distinct from OS DPI.

## 31. Scaling quality

At supported scale factors:

- hit boxes align with rendered controls;
- text remains legible;
- clock remains circular;
- borders do not disappear due to integer truncation;
- pointer drag geometry uses logical coordinates consistently.

## 32. Text renderer

The application must contain its own renderer for the glyph set needed by the mandatory UI.

An embedded bitmap/vector glyph atlas encoded in source data is allowed.

Minimum mandatory glyph coverage:

- ASCII letters used by the shipped English UI;
- digits `0–9`;
- punctuation used in labels/errors/config paths;
- multiplication sign may be drawn or represented as `x` if necessary, but UI should remain polished.

## 33. UTF-8 configuration labels

Custom user labels stored in config are optional. If implemented, unsupported glyphs may render a replacement box, but raw UTF-8 must remain preserved in files.

## 34. Clipping

The renderer must support rectangular clipping for at least:

- scrollable Settings viewport;
- button ripple clipping;
- modal/page transitions where content should not bleed outside bounds.

## 35. Visual artifact prohibition

The following are release-blocking visual defects when reproducible in normal use:

- uninitialized framebuffer garbage;
- trails from moving hands;
- modal background not restored after close;
- clipping into unrelated controls;
- hover state stuck after pointer leaves;
- ripple drawing outside intended control;
- clock hand not anchored at center;
- text visibly corrupt after resize.
