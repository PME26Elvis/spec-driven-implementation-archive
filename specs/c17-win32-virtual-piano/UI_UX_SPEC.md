# UI / UX Specification

## 1. Design Goal

The GUI shall look like a deliberate modern dark desktop application, not a raw Win32/GDI sample.

The required effects are functional requirements because they exercise the hand-written UI engine.

---

## 2. Logical Design Coordinate System

At 96 DPI the design uses logical pixels.

Reference sizes:

- default client: 1280 × 720;
- minimum client: 960 × 600;
- outer content margin at default: 24;
- primary control corner radius: 12;
- card corner radius: 18;
- pill/capsule radius: half its height.

At other DPI values these logical dimensions scale according to `DPI_SCALING.md`.

---

## 3. Mandatory Dark Theme Tokens

The implementation shall use these v1.0 design colors or values visually indistinguishable within ordinary 8-bit rounding:

- app background: `#0B0F17`;
- primary surface: `#131A26`;
- elevated surface: `#192334`;
- primary text: `#F5F7FB`;
- secondary text: `#A7B0C0`;
- subtle border: `#2A3445`;
- accent: `#7C6CFF`;
- accent secondary/glow: `#46D6D6`;
- destructive/error: `#FF5C72`;
- recording indicator: `#FF3B5C`;
- white piano key base: `#F2F4F8`;
- white piano key pressed tint: `#CFC9FF`;
- black piano key base: `#111722`;
- black piano key pressed tint: `#5146A8`.

Small antialiasing/alpha differences are acceptable; replacing the palette with an unrelated default gray Win32 look is not.

---

## 4. Main Layout

The default 1280×720 layout contains:

1. top title/navigation bar: 64 logical px high;
2. a central status/control region containing chord label and musical controls;
3. the piano keyboard occupying the lower performance region;
4. settings button at top-right;
5. visible recording control/status.

The exact horizontal grouping may adapt during resize, but no required control may disappear at the minimum size.

Piano keyboard container geometry at any supported size:

- left/right margin: 24 logical px;
- bottom margin: 24 logical px;
- keyboard height: `clamp(client_height * 0.38, 220, 300)` logical px;
- keyboard width: `client_width - 48` logical px.

---

## 5. Piano Geometry

### UI-KEY-001 White Keys

The keyboard contains 14 equal logical white-key spans covering the keyboard container from left edge to right edge.

Repeated-key boundaries shall be derived cumulatively from the total container width so physical-pixel rounding cannot drift.

### UI-KEY-002 Black Keys

Each black key has:

- width = `0.62 * white_key_width`;
- height = `0.62 * white_key_height`.

Each black key is horizontally centered on the corresponding boundary between adjacent white keys.

There are no black keys at E-F or B-C boundaries.

### UI-KEY-003 Hit Testing

Black-key hit regions are tested before white-key hit regions wherever they overlap.

The hit geometry shall be derived from the same scaled rectangles used for rendering.

---

## 6. Piano Visual States

Every key shall support these mandatory interaction states:

- idle;
- hover;
- physically/logically held.

Under all legal v1.0 octave/transpose settings, displayed piano keys are never disabled.

Held state must be unambiguous through at least two simultaneous cues, for example color plus vertical/inset shift.

A sustain-only/releasing visual hint is optional. If implemented, it shall be visually weaker than the physically/logically held state and shall not be used by chord recognition as evidence that the note is still held.

---

## 7. Buttons and Controls

The button interaction contract in this section applies to every application-owned button-like action control, including Octave/Transpose actions, Settings, Record/Stop, Save/Cancel/Restore, Rebind/Clear, and conflict-resolution actions. Piano keys, sliders, scrollbars, and segmented selectors follow their own dedicated interaction rules.

Disabled button-like controls do not activate and need not create a ripple.

### UI-BTN-001 Hover Elevation

On pointer enter, an enabled button-like action control transitions over `140 ms` from:

- y offset 0 -> -2 logical px;
- shadow opacity baseline -> baseline + 0.12.

On leave it returns over `140 ms`.

### UI-BTN-002 Press

On pointer-down, the button reaches y offset `+1 logical px` relative to resting position within `80 ms` and visually reduces elevation.

### UI-BTN-003 Ripple

Pointer-down creates a ripple centered at the local pointer position.

- duration: `360 ms`;
- initial radius: 0;
- final radius: at least the distance from origin to the farthest control corner;
- alpha: 0.24 -> 0;
- clipped to control shape;
- multiple ripples may coexist without corrupting state.

### UI-BTN-004 Border Glow

Enabled button-like action controls shall be capable of the custom accent border/glow state. Hover, keyboard focus where supported, selected, or active state shall expose the effect according to the control's state design.

Reference glow geometry:

- 1 logical px accent border;
- soft glow extent target 8 logical px; 6–10 logical px is acceptable;
- glow alpha peak target 0.28; 0.23–0.33 is acceptable.

A stock dotted focus rectangle does not satisfy this requirement.

---

## 8. Segmented Release Control / Sliding Capsule

The Short/Medium/Long release selector is a three-segment control.

The active selection is shown by a rounded capsule that moves from its old segment to the new segment rather than teleporting.

Animation:

- duration: `180 ms`;
- easing: cubic Bézier `(0.20, 0.80, 0.20, 1.00)`.

At completion the capsule bounds equal the selected segment bounds after inner padding.

---

## 9. Modal Settings Surface

Opening Settings presents a custom modal/sheet over the application.

Target modal geometry in logical pixels:

- width = `min(780, client_width - 64)`;
- height = `min(620, client_height - 64)`;
- centered in the client area.

Its content viewport is scrollable. The combined Keyboard + Help content shall exceed the available viewport height by at least 240 logical px at the minimum supported window size so the frosted/collapse behavior is genuinely exercisable.

### UI-MOD-001 Open

Duration: `220 ms`.

Modal content:

- opacity 0 -> 1;
- scale 0.96 -> 1.00;
- easing: standard cubic Bézier `(0.18, 1.18, 0.28, 1.00)`.

The cubic Bézier is evaluated as an easing function: normalized time/progress is the x-coordinate input, and the corresponding y value is the eased output. Because the first y control point is greater than 1, the eased output is allowed to exceed 1 briefly before returning to exactly 1 at the endpoint. Scale uses that eased value and therefore preserves the intended small elastic overshoot. Opacity uses `clamp(eased, 0, 1)` so alpha never exceeds its legal range. The eased y output is **not** required to be monotonic; normalized time progress is.

### UI-MOD-002 Close

Duration: `160 ms`.

- opacity 1 -> 0;
- scale 1.00 -> 0.98;
- easing: cubic Bézier `(0.40, 0.00, 1.00, 1.00)`.

Input to obscured background controls is blocked while modal state is OPENING, OPEN, or CLOSING.

### UI-MOD-003 Backdrop

During open:

- background dim alpha: 0 -> 0.52;
- application-content blur radius: 0 -> 12 logical px.

During close the values return to zero.

Blur shall operate on the application's own composed background image. Replacing it with a flat translucent rectangle alone does not satisfy blur.

---

## 10. Blur Requirement

A project-owned separable blur or documented equivalent image-space blur is required.

The normative blur is a separable box blur. For animated logical blur radius `r_logical`:

`R = round(r_logical * dpi / 96.0)`

- R=0 copies the source unchanged;
- horizontal pass averages source pixels from x-R through x+R;
- vertical pass averages the horizontal result from y-R through y+R;
- samples beyond an image edge clamp to the nearest edge pixel;
- BGRA color channels are averaged independently using sufficient accumulator width;
- a sliding-window optimization is allowed and encouraged, but output must be equivalent within 1 channel value of the direct box average;
- no read/write beyond allocated buffers.

The implementation need not blur the desktop or other windows.

### UI-BLUR-002 Edge Antialiasing

Rounded rectangles, circular/ripple edges, and other curved custom shapes shall use antialiased edge coverage.

At boundary pixels, use at least four subpixel coverage samples or an analytically equivalent coverage method. A visibly staircase/1-bit curved edge does not satisfy the modern custom-rendered requirement.

GDI font rasterization may provide text antialiasing.

---

## 11. Settings Navigation, Collapse, and Frosted Glass

The Settings modal contains an internal vertically scrollable content surface with at least two navigation destinations:

- `Keyboard`;
- `Help`.

A sticky settings header contains the tab/navigation selector and uses a sliding capsule for active destination.

### UI-NAV-001 Scroll Progress

Define:

`p = clamp(scroll_offset / 96.0, 0, 1)`

where scroll offset is in logical pixels.

### UI-NAV-002 Dynamic Header Height

Header height interpolates:

- 72 logical px at `p=0`;
- 52 logical px at `p=1`.

### UI-NAV-003 Frosted Backdrop

As `p` changes:

- header fill alpha interpolates 0.55 -> 0.90;
- blur radius interpolates 0 -> 14 logical px;
- bottom shadow alpha interpolates 0 -> 0.28;
- bottom shadow vertical extent interpolates 0 -> 10 logical px.

The change is continuous, not a threshold toggle.

### UI-NAV-004 Scroll Interaction

The Settings content supports:

- mouse wheel scrolling; one standard 120-unit wheel notch changes target offset by 48 logical px before clamping;
- a custom-rendered vertical scrollbar when content overflows;
- dragging the scrollbar thumb with pointer capture;
- scroll offset clamped to `[0, content_height - viewport_height]`.

The Help destination shall contain at minimum the factory keyboard map explanation, Space sustain shortcut, octave/transpose/release/recording descriptions, and enough content for the required overflow.

---

## 12. Volume Slider

The custom volume slider shall include:

- track;
- filled accent portion;
- thumb;
- visible numeric percent;
- pointer drag;
- pointer capture until release.

Dragging outside the original slider bounds while captured still updates/clamps the value until pointer-up/capture loss.

Value is integer 0–100.

---

## 13. Recording UI

Idle state shows `Record`.

Recording state shows:

- `Stop` action;
- recording-color dot/icon;
- literal `REC`;
- elapsed `MM:SS`.

The indicator may pulse, but pulsing is optional and cannot be the only indication.

Recording error is shown using the custom UI, not only a debug log.

---

## 14. Keyboard Mapping UI

The `Keyboard` settings section lists all 24 positions in pitch order.

Each row shows:

- displayed base position/pitch name;
- current human-readable key label;
- Rebind/Clear affordance or equivalent row action.

Mandatory states:

- normal;
- selected;
- capturing;
- conflict;
- unbound candidate;
- invalid candidate preventing Save.

The footer exposes Restore Defaults, Cancel, and Save.

---

## 15. Text

Required UI font family is `Segoe UI` when available, with a sensible Windows sans-serif fallback only if unavailable.

Reference logical sizes:

- chord label: 32 px equivalent;
- primary section/title: 20;
- button/control label: 14–16;
- secondary/help text: at least 13.

Text metrics shall be measured from the actual DPI-scaled font rather than guessed from character count.

---

## 16. Animation Engine

Animations are elapsed-time based using a monotonic high-resolution clock such as `QueryPerformanceCounter`. System wall-clock changes must not reverse animation progress.

While one or more visible animations are active, the application shall schedule/request repaint ticks at a nominal period no greater than 17 ms (approximately 60 Hz). Actual OS scheduling jitter does not change time-based endpoints. When no animation/state change requires redraw, continuous 60 Hz repainting is not mandatory.

The engine shall provide at least:

- linear interpolation;
- clamp;
- cubic Bézier easing evaluation;
- normalized progress 0..1.

A large frame gap of more than 250 ms shall clamp progress to valid bounds and may settle completed animations; it must not extrapolate to invalid geometry/opacity.

Animation durations are independent of frame rate and DPI.

---

## 17. Double Buffering and Flicker

Every normal custom frame is composed off-screen before presentation.

No normal resize/animation path may intentionally erase the window to a stock background between frames in a way that causes visible white/default flicker.

---

## 18. Interaction Capture Rules

For any custom clickable control:

- activation target is chosen at pointer-down;
- pointer-up activates it only if the control's policy allows release-outside; ordinary buttons require release inside;
- piano key release always ends its captured note even if outside;
- slider remains captured and clamps position;
- disabled controls never activate;
- opening a modal cancels background hover/press capture safely.

---

## 19. DPI

All geometry, visual effect dimensions, text, and hit testing obey `DPI_SCALING.md`.

A 150% rendering that is merely an OS-stretched 100% bitmap fails acceptance.

---

## 20. Required Visual Evidence States

When screenshots are possible, evidence shall include:

1. main idle state;
2. at least three simultaneous held piano keys with a recognized chord;
3. button hover/elevation;
4. active ripple/press state;
5. non-zero transpose;
6. sustain latched on;
7. release capsule on a non-default segment;
8. recording active with REC/time;
9. settings keyboard mapping list;
10. binding-capture state;
11. duplicate-binding conflict state;
12. modal open with dim + blur;
13. settings header at scroll 0;
14. settings header at scroll >=96 logical px;
15. equivalent main state at 100%, 125%, and 150% DPI.

The capture mechanism is intentionally not specified.
