# 23 — Text Input, Focus, UTF-8, HiDPI, and Reduced Motion

This document defines desktop-quality interaction requirements for the custom UI engine.

## 1. Focus model

Exactly zero or one interactive UI control owns keyboard focus at a time.

Focus is distinct from canvas selection.

Focus state SHALL be visibly indicated for keyboard-navigated controls.

## 2. Tab traversal

Tab moves focus forward through enabled visible controls in a deterministic logical order. Shift+Tab moves backward.

Traversal SHALL include:

- top toolbar controls;
- sidebar/palette controls;
- Inspector fields;
- modal controls;
- file-picker controls;
- command-palette field/results when open;
- relevant trace/statistics controls.

Hidden, disabled, or clipped-off controls are not focus stops.

## 3. Focus scopes

Modal dialog creates a focus scope:

- keyboard focus is trapped inside modal;
- Tab wraps within modal;
- background controls cannot receive pointer or keyboard activation;
- on close, focus returns to the logical control that opened the modal if it still exists/enabled, otherwise the nearest valid ancestor/default control.

Popups such as dropdowns form a temporary focus scope under the owning control.

## 4. Keyboard activation

For focused button-like controls:

- Enter activates;
- Space press/release provides pressed state and activates once on release while focus remains valid.

Escape closes the topmost dismissible popup/modal according to hierarchy before affecting editor selection.

## 5. Pointer focus

Clicking an interactive control gives it focus unless that control is explicitly non-focusable.

Clicking canvas transfers focus to canvas and commits/cancels pending text edits according to field validity rules.

## 6. Focus loss during held gameplay input

When the top-level window receives focus loss/deactivation (for example WM_KILLFOCUS/WM_ACTIVATE inactive):

- synthesize release of held flipper/launcher gameplay actions;
- prevent stuck controls;
- do not synthesize nudge;
- replay records only logical actions that actually occurred before focus loss plus deterministic release actions where required.

## 7. UTF-8 storage

User-facing scene/table/layer/group/object display names SHALL be stored as valid UTF-8.

Identifiers that participate in the scene grammar remain restricted ASCII by the ID rules.

Invalid UTF-8 loaded from external files is rejected with precise parse diagnostics.

## 8. Required Unicode editing baseline

Custom text fields SHALL correctly edit UTF-8 by Unicode scalar value boundaries, never delete/cursor into the middle of a UTF-8 byte sequence.

Required operations:

- insertion of valid text received from the Win32 Unicode/IME text input path, converting UTF-16 surrogate pairs correctly into UTF-8 scalar sequences;
- Left/Right by Unicode scalar value;
- Ctrl+Left/Ctrl+Right by defined word boundaries;
- Home/End;
- Shift selection variants;
- Ctrl+A/C/X/V;
- Backspace/Delete;
- mouse click cursor placement;
- mouse drag selection;
- horizontal scrolling for long single-line fields.

## 9. Word-boundary rule

For v1.0.0 text-field navigation/search-token purposes:

- ASCII letters/digits/underscore form word runs;
- each contiguous run of CJK Unified Ideographs/Hiragana/Katakana/Hangul is treated as a text word run for Ctrl-navigation;
- whitespace and punctuation delimit runs;
- exact Unicode linguistic segmentation is not required.

This rule is deterministic and sufficient for Chinese-capable field editing.

## 10. Chinese text

The application SHALL preserve, render when supported by available platform font path, search, save, load, copy, and paste Chinese UTF-8 strings without corruption.

Full custom IME composition rendering is not required; however, committed text from the Windows input method SHALL be accepted. A valid implementation may consume Unicode character messages and/or retrieve `GCS_RESULTSTR` from the IMM composition result path. UTF-16 surrogate pairs SHALL be combined correctly; an isolated invalid surrogate SHALL NOT corrupt internal UTF-8.

If the runtime font lacks a glyph, use visible missing-glyph fallback while preserving underlying text.

## 11. Search matching

Any editor command/object search that is required to support names SHALL match UTF-8 byte-preserving strings and ASCII case-insensitive search for ASCII letters. Chinese substring search SHALL work by exact UTF-8 scalar sequence.

Locale-specific case folding beyond ASCII is not required.

## 12. Text rendering safety

Malformed internal UTF-8 must never reach rasterization. If an internal invariant is violated, render a replacement marker and raise a diagnostic rather than reading past buffers.

## 13. Clipboard text

Clipboard copy/paste for text fields SHALL preserve UTF-8 exactly. Embedded NUL is not accepted into single-line text fields.

Pasted line breaks into single-line fields are normalized to spaces or rejected with explicit behavior documented by implementation; they must not corrupt layout.

## 14. User UI scale settings

Required user-selectable UI scales:

- 100%;
- 125%;
- 150%;
- 200%.

The user UI scale affects control metrics, text rasterization target size, hit areas, shadow/blur radii, and device-independent layout. It is independent of the Windows monitor DPI scale defined below.

## 15. Device-independent geometry

UI layout SHALL be computed in device-independent UI units. Physical framebuffer mapping SHALL apply both the current Windows monitor DPI factor and the selected user UI scale; the implementation shall not depend on DPI virtualization.

Required controls SHALL not become blurrier merely because the final framebuffer is uniformly scaled from a 100% render. Re-rasterization at target scale is required for text and vector/software-drawn controls.

## 16. Canvas vs UI scaling

Neither user UI scale nor Windows monitor DPI changes world zoom percentage definition. A 100% canvas zoom maps one logical world unit to the defined baseline canvas device-independent unit, then the current device/DPI mapping produces physical framebuffer pixels.

World measurements remain unchanged across UI scales.

## 17. HiDPI acceptance sizes

At user UI scale 100/125/150/200%, including at non-96-DPI monitors:

- controls remain fully clickable;
- no required label is clipped at minimum supported window size adjusted for device pixels;
- Inspector remains scrollable;
- hit-test geometry matches rendered geometry within 1 device-independent pixel;
- modal remains centered within usable window;
- canvas interaction coordinates remain correct.

## 18. Reduced Motion setting

Settings SHALL expose `Reduced Motion` boolean.

When enabled:

- non-essential transition durations are reduced to <= 80 ms;
- hover elevation may change instantly or within <= 80 ms;
- ripple may be replaced by brief opacity/highlight feedback;
- modal still uses visible state transition but no pronounced scale overshoot;
- blur/dim final visual state remains present;
- functional state changes and focus feedback are unchanged.

Reduced Motion is a UI preference and SHALL NOT affect simulation/replay determinism.

## 19. Animation state decoupling

Business/editor state SHALL transition independently from visual animation completion. For example, modal input capture begins when modal state opens, not only after fade-in completes.

Reduced Motion must not reveal bugs where logic was accidentally driven by animation duration.

## 20. Popup dismissal

Dropdown/popup behavior:

- click owner toggles popup;
- click inside interacts without background leakage;
- click outside closes and consumes the click if activating background would be destructive or state-changing;
- Escape closes;
- resize/reflow repositions or closes popup if it cannot remain validly anchored;
- focus returns to owner on keyboard dismissal.

## 21. Animation interruption

For every reversible required animation, current interpolated state is the new starting point when direction changes.

Required tests:

- hover enter/leave/enter rapidly;
- modal open then close before 50% duration;
- close then reopen before close finishes;
- sidebar collapse/expand repeatedly;
- capsule mode switch A->B->A before first slide finishes;
- popup open while panel is resizing.

No test may show endpoint snapping, stale input capture, or duplicate callback activation.

## 22. Damage/clip correctness

Software renderer SHALL correctly repaint invalidated areas after:

- moving modal;
- closing popup;
- ripple ending;
- shadow/blur transition;
- panel collapse;
- WM_PAINT/expose or uncover;
- resize;
- UI scale change.

No persistent trails, stale pixels, or rendering outside required clipping bounds are accepted.

## 23. Acceptance evidence

Static/transition evidence SHALL include:

- keyboard focus ring on toolbar and Inspector;
- modal focus trap demonstration;
- Chinese scene/object name visible after save/reload;
- 100/125/150/200% representative screenshots;
- Reduced Motion state;
- interrupted modal/sidebar/capsule animation sequence;
- resize/expose with no stale framebuffer artifacts.


## 24. Windows Per-Monitor DPI contract

The Windows release SHALL be DPI aware and SHALL NOT rely on Windows bitmap DPI virtualization. The preferred/required release behavior is **Per-Monitor DPI Awareness v2** through the executable manifest or equivalent process-default configuration established before UI creation.

For a window on monitor DPI `D`, the platform device factor is `D / 96`. The application SHALL obtain the effective DPI for the top-level window and react to runtime DPI changes, including the `WM_DPICHANGED` transition path.

A DPI change SHALL:

1. update the platform device factor exactly once;
2. accept/reconcile the OS-recommended new window rectangle or otherwise keep the window fully usable on the destination monitor;
3. recompute client pixel dimensions;
4. resize/recreate the application-owned framebuffer safely;
5. rerasterize text and all software-drawn UI at the new target resolution;
6. preserve authored world coordinates, simulation state, canvas world zoom, selection, and replay state;
7. preserve logical focus and popup/modal ownership where the associated control still exists.

The mandatory DPI acceptance set is 96, 120, 144, and 192 DPI (100%, 125%, 150%, 200% OS scale) where the evaluation environment can expose those values.

## 25. Effective UI scale composition

Let `S_os = dpi/96` and user setting `S_user` be one of 1.00, 1.25, 1.50, 2.00. Device-independent UI geometry is multiplied by `S_user` in logical UI layout and then mapped to device pixels by `S_os`.

This composition is deliberate. The user setting is an accessibility/product preference; Windows DPI is physical display scaling. Neither may be silently ignored or collapsed into a blurred post-scale.

## 26. Windows clipboard text

Required text-field copy/paste interoperates with the Windows system clipboard using Unicode text semantics equivalent to `CF_UNICODETEXT`. Internal authored-object clipboard remains the structured in-process model from document 19.

Clipboard acquisition failure, clipboard contention, or malformed external text SHALL fail safely without clearing the application's internal text selection or corrupting UTF-8. CR/LF text pasted into a single-line field follows section 13 normalization/rejection semantics.

## 27. Windows IME boundary

The implementation is not required to draw a full custom composition candidate UI. It SHALL, however, accept committed Chinese/Japanese/Korean text delivered through normal Windows desktop IME services. Calling IMM solely to retrieve composition/result strings is an explicitly permitted low-level platform service.

No hidden native EDIT control may be used as an input surrogate for the custom text field.
