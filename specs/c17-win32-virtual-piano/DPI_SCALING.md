# DPI Scaling Specification

## 1. Awareness

The GUI shall establish Per-Monitor DPI awareness compatible with handling `WM_DPICHANGED` on supported modern Windows versions.

OS bitmap stretching of a DPI-unaware 96-DPI client is not acceptable.

---

## 2. Required DPI Values

Mandatory:

- 96 DPI / 100%;
- 120 DPI / 125%;
- 144 DPI / 150%.

Robustness-only:

- 168 DPI / 175%;
- 192 DPI / 200%.

---

## 3. Coordinate Model

Canonical logical units are defined at 96 DPI.

`scale = dpi / 96.0`

Layout should retain floating-point logical geometry through layout and convert to physical pixels only for rasterization/native-window boundaries.

A single central DPI/layout module shall expose equivalent operations for:

- current DPI;
- scale factor;
- logical -> physical;
- physical -> logical when needed;
- scaled font/effect metrics.

---

## 4. Initial Layout

Before final first-frame layout, obtain the effective DPI for the target window/monitor and size the backbuffer/layout accordingly.

Default logical client size remains 1280×720.

Minimum logical client size remains 960×600.

---

## 5. `WM_DPICHANGED`

On DPI change:

1. end/cancel transient pointer capture if keeping it would create stale coordinate ownership;
2. update DPI and scale;
3. apply the suggested window rectangle from Windows unless doing so would violate the specified minimum size;
4. recompute all layout and hit-test geometry;
5. recreate scale-dependent fonts/resources;
6. resize/recreate backbuffer;
7. invalidate/redraw.

Musical state is preserved:

- active voices continue;
- held-key ownership remains logically valid except pointer capture may be safely released;
- transpose/octave/sustain/volume unchanged;
- recording continues;
- chord state remains based on held note instances.

---

## 6. Piano Geometry Rounding

White-key boundary `i` for `i=0..14` shall be derived from the full keyboard width using cumulative normalized position, e.g. equivalent to:

`x_i = round(left + width * i / 14.0)`

Repeatedly adding one already-rounded key width is prohibited if it causes cumulative drift.

Black key geometry derives from the resulting white-key span and the 0.62 ratios in `UI_UX_SPEC.md`.

---

## 7. Hit Testing

Pointer coordinates and draw rectangles must be converted into the same coordinate space.

At every required DPI:

- center of each visible key hits that key;
- points just outside the keyboard do not hit a key;
- overlap points belonging to black keys select black keys first;
- buttons/sliders/modal rows use current-DPI hit geometry.

---

## 8. Text

Font logical sizes scale with DPI.

Text measurement shall use the actual scaled font.

At minimum supported client size and 150% DPI:

- chord label readable;
- musical control values readable;
- recording UI readable;
- settings rows/actions reachable;
- no required control is entirely clipped offscreen.

---

## 9. Effects

Unless a section explicitly says otherwise, these logical values scale with DPI:

- corner radius;
- border width;
- glow radius;
- shadow offset/extent;
- blur radius;
- ripple radius;
- layout gaps/padding.

Opacity and animation duration do not scale with DPI.

Repeated DPI transitions must not multiply an already-scaled value again.

---

## 10. Modal / Scroll During DPI Change

If Settings is open:

- preserve selected tab and logical scroll offset;
- recompute modal/header/list geometry;
- recompute blur/backdrop region;
- capture state remains logically targeted to the same mapping position;
- stale pre-DPI hit rectangles are discarded.

If a conflict/capture UI is active, it remains visible after relayout unless the implementation must cancel pointer capture; keyboard binding state itself is preserved.

---

## 11. Backbuffer Failure

If new physical backbuffer allocation fails during DPI/resize transition:

- do not draw through a null/undersized buffer;
- preserve old resources until safe where possible;
- report a diagnosable fatal or recoverable rendering error;
- never write beyond allocated memory.

---

## 12. Acceptance

Automated geometry tests shall be able to inject synthetic DPI values 96/120/144 without requiring three real monitors.

Visual evidence is still required at those DPI values when the environment can produce it.

Cross-monitor live `WM_DPICHANGED` verification may be `UNVERIFIED` if the execution environment lacks a suitable display setup; synthetic transition/state tests remain mandatory.
