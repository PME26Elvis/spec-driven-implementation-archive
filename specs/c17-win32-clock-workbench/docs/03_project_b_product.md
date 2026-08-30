# 03 — Project B: Analog Clock Workbench Product Requirements

## 1. Product concept

Analog Clock Workbench is an interactive clock simulation environment centered around a single simulated clock.

The application is designed for direct manipulation: a user can drag clock hands, edit digital time, move the playback-rate slider, pause/resume time, and change settings while every representation remains synchronized.

## 2. Main window regions

The default main window contains five conceptual regions:

1. **Top navigation bar**.
2. **Analog clock stage**.
3. **Digital time editor**.
4. **Simulation controls**.
5. **Context/status strip**.

The exact pixel dimensions may scale with the window, but the visual hierarchy must remain recognizable.

## 3. Top navigation bar

The top navigation bar contains:

- application title;
- a two-item capsule selector: `Clock` and `Settings`;
- undo button;
- redo button;
- optional compact help/about button.

Switching `Clock`/`Settings` must use an animated sliding capsule indicator.

The active section must be visually unmistakable without relying only on text color.

## 4. Clock stage

The analog clock stage is the primary content area.

It must contain:

- clock face;
- hour tick marks;
- minute tick marks;
- numerals for at least the major hour positions;
- hour hand;
- minute hand;
- second hand;
- central hub;
- optional decorative inner ring provided it does not interfere with hit testing.

The hands must be rendered as geometry, not pre-rendered image sprites.

## 5. Hand semantics

The hour, minute, and second hands are all interactive.

The user must be able to grab a hand at a natural point along its visible shaft and drag it around the clock center.

Hit testing must prioritize the hand that is visually closest to the pointer when hit regions overlap.

The UI must make the hovered/active hand apparent.

## 6. Continuous hand positions

The displayed hand angles derive continuously from canonical simulated time.

For canonical time expressed as seconds-of-day `T`:

- second hand uses the second fraction including subsecond time;
- minute hand includes seconds and subseconds;
- hour hand includes minutes, seconds, and subseconds.

The hour hand must not jump once per hour.

## 7. Digital time editor

A large digital display appears adjacent to or below the analog stage depending on width.

It must show at least:

```text
HH:MM:SS
```

In 24-hour mode `HH` ranges `00..23`.

In 12-hour mode the UI must include a clear AM/PM indicator and valid 12-hour formatting.

The digital editor is not read-only.

## 8. Digital field editing

The hour, minute, and second fields are individually focusable.

The user must be able to:

- click a field to focus it;
- type numeric digits;
- use Up/Down to increment/decrement;
- use Left/Right or Tab/Shift+Tab to move between fields;
- commit a valid edit;
- cancel an uncommitted edit with Escape.

Editing a valid field must update the canonical time and therefore the analog hands.

## 9. Numeric entry behavior

The buffered-entry model is fixed:

- focusing a field by itself does not modify time or suspend playback;
- typing the first digit snapshots the field's pre-edit canonical time, starts a one-digit pending buffer, and temporarily suspends automatic playback advancement;
- a second digit appends to make a two-digit buffer; if the two-digit value is valid for the field/mode, it commits immediately;
- a two-digit invalid value remains visibly pending/error-marked and does not alter canonical time;
- when an invalid two-digit buffer exists, typing another digit replaces the entire buffer with that new single digit so correction does not require mouse interaction;
- Backspace removes the last pending digit; an empty buffer is allowed only transiently and never becomes canonical state;
- Enter with one or two valid pending digits commits the numeric value (a single `6` means value 6 and displays padded as `06` afterward);
- Enter on empty/invalid input does not commit;
- Left/Right, Tab/Shift+Tab, pointer focus transfer, or ordinary focus loss commit a valid non-empty buffer, otherwise revert to the pre-edit canonical time and show validation feedback before moving focus;
- Escape cancels and restores the pre-edit canonical time;
- successful commit, revert, or cancel ends the temporary playback suspension.

The application must not expose a pending numeric buffer as canonical time before commit.


## 9A. Digital field scrub-drag

Each `HH`, `MM`, and `SS` field is both typable and directly scrub-draggable. This is a mandatory pointer interaction, not an optional enhancement.

Behavior:

- primary-pointer down on a field captures its starting canonical time and pointer position;
- movement smaller than 4 logical pixels remains a click/focus gesture;
- after the threshold is crossed, the gesture becomes a vertical scrub;
- cumulative upward movement increases the selected unit and downward movement decreases it;
- once the 4-logical-pixel Euclidean drag threshold is crossed, scrub mode is latched for the rest of that pointer gesture even if the pointer later returns near its start;
- with logical pointer-down coordinate `start_y` and current coordinate `y`, the signed whole-unit delta is `trunc((start_y - y) / 12)` toward zero; every 12 logical pixels of cumulative vertical displacement therefore corresponds to one integer unit from the drag-start value, while partial units do not commit fractional hours/minutes/seconds;
- hour scrub changes canonical time by whole hours, minute scrub by whole minutes, second scrub by whole seconds; lower-order components and subsecond fraction are preserved;
- values wrap naturally across minute/hour/day boundaries;
- automatic playback advancement is suspended during the scrub;
- pointer capture continues outside the field/window content region;
- release commits exactly one undo transaction;
- Escape or focus-loss cancellation restores the drag-start canonical time and creates no history entry.

If scrub mode is entered on a field that already has an uncommitted typed buffer, that pending typed edit is cancelled/reverted to its pre-edit canonical snapshot before scrub mutation begins; the scrub then uses the canonical time captured at pointer-down as its own start value. A simple click that never crosses the scrub threshold does not cancel the existing buffer merely because the same field was clicked.

The scrub must update analog hands and the digital display from the same canonical-time snapshot every frame.

## 10. Playback controls

Simulation controls include:

- Play/Pause toggle button;
- playback-rate slider;
- numeric playback-rate label;
- reset-to-`1×` affordance.

The slider must support negative values, zero, and positive values.

## 11. Playback-rate range

Mandatory supported range:

```text
-100× .. +100×
```

The exact mapping from slider position to rate must provide useful precision near `0×` and `±1×`.

The slider mapping is fixed for comparability. Let normalized thumb displacement from center be `u` in `[-1,1]`, let `a = abs(u)`, and let `s = sign(u)`. The playback rate is:

```text
a in [0, 0.25] : rate = s * (4*a)
a in (0.25,0.5] : rate = s * (1 + 36*(a-0.25))
a in (0.5,1]    : rate = s * (10 + 180*(a-0.5))
```

Therefore the anchors are exactly `-100, -10, -1, 0, +1, +10, +100` at normalized positions `-1, -0.5, -0.25, 0, +0.25, +0.5, +1`. The inverse mapping must be used when positioning the thumb from an existing rate. Track clicks and keyboard adjustment must update the same playback-rate state used by dragging.

## 12. Playback-rate display

Rate formatting must be readable and stable. The displayed decimal precision is at most three fractional digits and trailing fractional zeroes are removed. Values whose formatted magnitude rounds to zero display exactly `0×`, never `-0×`. Examples include `-100×`, `-10×`, `-1×`, `-0.25×`, `0×`, `0.5×`, `1×`, `10×`, and `100×`. The stored rate remains the full finite internal value; display rounding must not feed back into simulation state.

## 13. Pause semantics

Pause is a simulation-state flag independent of the stored playback-rate value.

Pressing Pause does not force the rate to zero.

Pressing Play resumes using the previous rate.

A rate of `0×` also produces no time movement, but the UI still distinguishes `playing at 0×` from `paused` internally.

## 14. Negative playback

At negative playback rates, simulated time runs backward.

All analog and digital representations must move backward naturally.

Crossing midnight backward wraps from `00:00:00` to just before `24:00:00` without error.

## 15. Day wrapping

The product simulates time-of-day only.

Canonical time wraps modulo 24 hours.

No date counter is required.

Forward and reverse playback may wrap through midnight any number of times.

## 16. Manual manipulation while playing

When a user begins a direct time-edit gesture while playback is active, automatic time advancement must be temporarily suspended for the duration of that gesture.

Examples:

- dragging a clock hand;
- typing a digital field;
- holding Up/Down over a digital field.

After the gesture commits or cancels, playback resumes automatically if it was playing before the gesture.

This temporary suspension must not alter the Play/Pause flag.

## 17. Rate-slider manipulation while playing

Dragging the rate slider does not suspend time simulation.

The new rate applies continuously during the drag.

The complete drag is one undoable settings/simulation transaction as defined in the undo specification.

## 18. Reset time action

The Clock view must provide a deliberate reset-time action.

Default reset time is configurable and defaults to `10:10:30`.

Resetting time is undoable.

Resetting time does not alter playback rate or Play/Pause state.

## 19. Settings view

The Settings view must contain actual editable controls for every mandatory configuration setting identified in the config specification that is marked GUI-editable.

At minimum settings must expose:

- 12/24-hour display mode;
- second-hand movement style: `smooth` or `tick`;
- clock-face appearance preset;
- UI animation enabled/disabled;
- animation speed multiplier;
- modal/background blur strength;
- clock-hand snapping behavior;
- default reset time;
- default playback rate;
- persistence behavior.

## 20. Settings apply behavior

Settings uses one required active/saved model:

- `active_config` is the fully validated configuration currently driving the UI;
- `saved_config` is the last configuration successfully loaded from or written to the active config path, except that an absent implicit default config initializes it to the normalized built-in-default baseline as defined in the configuration specification;
- changing a Settings control updates `active_config` immediately after local validation so safe visual/behavioral settings are live-previewed;
- `Apply` validates the entire `active_config`, atomically writes it, and on success replaces `saved_config` with the written configuration;
- `Revert` restores `active_config` from `saved_config`;
- dirty state is exactly `active_config != saved_config` by logical normalized values; file presence is tracked separately, so an absent implicit default file can be clean while `Apply` is still available to create it.

Leaving Settings, switching to Clock, pressing window close, or otherwise abandoning Settings while dirty must show an in-app `Save / Discard / Cancel` confirmation. `Save` performs Apply and proceeds only if save succeeds. `Discard` restores `saved_config` and proceeds. `Cancel` remains in Settings.


## 20A. Required setting-effect timing

The following semantics are fixed so that a control cannot be considered wired merely because its stored value changes:

- `display.hour_mode`, `clock.second_hand`, `clock.face_style`, `clock.drag_snap`, `window.ui_scale`, `animation.enabled`, `animation.speed_multiplier`, `effects.modal_blur_radius`, and `effects.nav_blur_radius` affect current UI behavior immediately after a valid control gesture;
- `clock.reset_time` changes the value used by the next Reset Time action and does not itself change canonical time;
- `playback.default_rate` changes the startup/default rate used when no remembered runtime rate is applied; it does not overwrite the current runtime playback rate;
- persistence/remember toggles affect subsequent runtime-state load/save semantics and do not fabricate an immediate state mutation;
- `window.remember_geometry` controls whether dimensions are restored/saved in the runtime-state file.

Changing a configuration setting never silently rewrites canonical time except when the setting is itself a deliberate time action (none of the mandatory settings are).

## 21. Configuration reload

Settings must include `Reload config`.

Reload reads the configured file again. On success, the fully validated loaded configuration becomes both `active_config` and the new `saved_config` baseline, so the application is clean immediately after Reload. If the loaded values materially differ from the pre-Reload `active_config`, the logical change is still recorded as one undoable configuration transaction; Undo changes only `active_config` and therefore makes the application dirty relative to the newly loaded `saved_config` without rewriting the file.

If parsing or validation fails:

- the currently active valid configuration remains unchanged;
- the UI displays a non-destructive error surface;
- the error includes line/column and reason when available.

## 22. Save configuration

The Settings `Apply` action is the required `Save config` behavior and must write the current active configuration.

Save must be failure-safe: an I/O failure must not destroy the previous valid config file.

## 23. Status strip

When `display.show_status_strip` is true (the default), a compact status strip must show at least:

- current playback state (`Playing` / `Paused`);
- current playback rate;
- whether an edit gesture is active when relevant;
- a transient message area for validation/save/reload feedback.

The status strip must not be the only location where important errors are shown.

## 24. Window resizing

The application window must be resizable.

Required minimum supported content size is `720×540` logical pixels. The supported client-area physical maximum is `3840×2160`.

The application must request/enforce the scaled native minimum and physical maximum described in the config specification. If Windows nevertheless delivers a smaller/larger client area transiently (for example during DPI/restore transitions), the application must use a constrained memory-safe fallback; controls must not overlap unpredictably or cause out-of-bounds rendering.

At larger supported sizes, the analog clock must scale while maintaining aspect ratio.

## 25. Responsive layout

At wide aspect ratios, analog stage and digital/control panel may sit side by side.

At narrower supported sizes, the layout may stack vertically.

All mandatory controls must remain reachable without clipping.

If scrolling is needed in Settings, it must be application-implemented and must satisfy the frosted navigation behavior requirements.

## 26. Keyboard shortcuts

Mandatory global shortcuts:

- `Ctrl+Z`: Undo.
- `Ctrl+Y`: Redo.
- `Ctrl+Shift+Z`: Redo as an alternate shortcut.
- `Space`: Play/Pause when no text field is actively consuming Space.
- `Esc`: cancel current edit/drag when cancellable; otherwise close topmost modal.
- `Ctrl+,`: open Settings.

Shortcuts must not trigger through a modal that intentionally captures them unless documented.

## 27. Focus indication

Every keyboard-focusable control must have a visible focus state.

Focus indication must remain distinguishable from hover and pressed states.

## 28. No hidden duplicate state

Analog time, digital time, and simulation progression must not diverge because of independently maintained UI values.

The architecture must use a single authoritative simulated-time state as specified separately.

## 29. No placeholder controls

Every visible mandatory button, toggle, slider, and settings control must be wired to the required behavior.

Controls that look interactive but do nothing are release-blocking defects.
