# 05 — Interaction, Gestures, Keyboard, Undo and Redo

## 1. Interaction design principle

The interface must behave like a direct-manipulation desktop application rather than a collection of disconnected demo widgets.

Pointer, keyboard, focus, modal, and history behavior must be deterministic.

## 2. Pointer states

Every interactive control must support the states relevant to it:

- idle;
- hovered;
- pressed;
- focused;
- disabled;
- active/selected where applicable.

Visual state changes must not change control hit boxes unexpectedly.

## 3. Pointer capture for drags

When a draggable control begins a drag, it must continue receiving logical drag updates even when the pointer leaves the original hit box.

This applies to:

- hour/minute/second hands;
- digital hour/minute/second scrub gestures;
- playback-rate slider thumb;
- scrollbars if application-rendered;
- any other required continuous drag control.

A drag ends on button release or cancellation. On Windows, unexpected capture loss (`WM_CAPTURECHANGED`) or `WM_CANCELMODE` is cancellation, not a successful release. Time/ rate/Settings-slider gestures restore their gesture-start logical value and create no history entry; custom scrollbar dragging may retain the already-applied scroll offset but must clear drag/pressed state. The app must never continue believing it owns capture after Win32 reports otherwise.

## 4. Clock-hand hover target

The visible hand geometry must have a slightly expanded interaction tolerance so that thin hands are practical to grab.

The hit tolerance must not make a distant hand selectable.

The mandatory interaction expansion is 8 logical pixels beyond the rendered shaft/hand geometry, evaluated in logical coordinates before UI-scale conversion. A press inside the clock-center 12-logical-pixel dead radius does not start a hand drag even if expanded hand regions overlap there.

## 5. Overlapping hand selection

When multiple hands are under the pointer:

1. compute distance to each interactive hand shaft/endpoint;
2. ignore candidates outside tolerance;
3. select the nearest candidate;
4. if effectively tied, prefer second over minute over hour because thinner foreground hands are generally visually on top.

The tie-break policy must be consistent with visual z-order.

## 6. Drag feedback

During a clock-hand drag:

- selected hand receives a distinct glow or emphasis;
- pointer movement updates time continuously;
- digital display updates in the same frame;
- status strip may show `Editing hour hand`, etc.;
- playback advancement is temporarily suspended;
- release commits one transaction.

## 7. Slider drag feedback

During playback-rate slider drag:

- thumb follows pointer continuously;
- filled track updates;
- rate label updates;
- time simulation immediately uses the new rate;
- releasing commits one history transaction if the final rate differs meaningfully from the initial rate.


## 7A. Playback-rate drag cancellation

Rate-slider pointer-down captures the initial playback rate. Escape or the mandatory focus-loss cancellation restores that initial rate and creates no history entry. Because rate-slider manipulation does not suspend simulation, canonical time is **not** rewound for the interval that temporarily ran at previewed rates; only the rate value is restored. Ordinary pointer release commits the final rate as one history entry when it differs from the initial rate.

A primary click on the slider track outside the thumb maps that pointer position through the required piecewise slider function, moves the thumb to that rate, and commits one rate history transaction on release.

## 8. Click versus drag threshold

Controls that support dragging must distinguish tiny pointer jitter from intentional drag.

For controls where click and drag have different semantics, the drag threshold is exactly 4 logical pixels measured as Euclidean displacement from pointer-down in logical coordinates. Digital time fields use this threshold before entering scrub mode. Clock hands and slider thumbs have no competing click action and may begin continuous manipulation immediately after a valid press; history still records one gesture.

## 9. Button activation

A button press begins on primary-pointer down inside the hit region.

Activation occurs on primary-pointer release while the pointer is inside the button, unless the button's semantic is a press-and-hold action.

Dragging out cancels activation but retains pressed visual feedback only while appropriate.

## 10. Ripple effect semantics

Every primary/secondary button must spawn a click ripple from the local press point.

The ripple:

- begins on pointer down;
- expands radially;
- fades out;
- is clipped to the button shape;
- does not block input;
- continues briefly even if activation is cancelled by dragging out.

Keyboard activation may originate the ripple from button center.

## 11. Hover elevation

Hovering a button smoothly raises its apparent elevation using shadow offset/blur/intensity and/or a small vertical transform.

The control's logical layout position and hit region must not oscillate due to this visual transform.

## 12. Focus navigation

Tab moves focus forward through visible, enabled controls.

Shift+Tab moves backward.

Hidden controls and controls behind a modal are skipped. If a state/config change makes the currently focused control hidden or disabled (for example removing the 12-hour AM/PM control by returning to 24-hour mode), focus must be repaired in the same logical update to the nearest sensible visible enabled control; hidden controls must never keep consuming keys.

In the scrollable Settings page, keyboard focus traversal must minimally auto-scroll the content so the newly focused control is fully visible below the anchored navigation bar. This scrolling participates in the same nav frost/collapse calculations as wheel scrolling.

Focus wraps within a modal while the modal is open. When a modal opens, the previously focused background control is remembered; when the modal closes, focus returns to that control if it is still visible/enabled, otherwise to a deterministic visible fallback.

## 13. Keyboard button activation

Focused buttons activate with Enter or Space.

Focused toggles may toggle with Space.

Keyboard activations that are discrete actions—buttons, toggles, Play/Pause, Reset, navigation, and global Undo/Redo shortcuts—must ignore auto-repeat `keydown` messages and activate at most once per physical key press. Repetition is intentional only for controls whose specification explicitly defines held-key adjustment (digital fields and sliders).

The global Space Play/Pause shortcut must not steal Space from a focused button or editable field.

## 14. Escape priority

Escape follows this priority:

1. cancel active digital edit buffer;
2. cancel active hand/slider drag when cancel semantics exist;
3. close topmost dismissible modal;
4. leave Settings only according to unsaved-change policy;
5. otherwise no action.

## 15. History model

The application must implement bounded undo and redo stacks.

Minimum capacity is 100 committed user actions.

When capacity is exceeded, oldest undo entries may be discarded.

## 16. History entry structure

Each undoable action must contain enough before/after state to restore the affected domain reliably.

A history entry must include at least:

- action kind;
- affected state domain;
- before value/state;
- after value/state;
- optional metadata for diagnostic labels.

Raw pointers into transient UI objects must not be persisted in history entries.

## 17. Required undoable actions

At minimum these actions are undoable:

- completed hour-hand drag;
- completed minute-hand drag;
- completed second-hand drag;
- completed digital hour scrub;
- completed digital minute scrub;
- completed digital second scrub;
- committed digital hour edit;
- committed digital minute edit;
- committed digital second edit;
- AM/PM change;
- reset-time action;
- completed playback-rate slider drag;
- reset playback rate to `1×`;
- Play/Pause toggle;
- each completed GUI-editable setting control gesture that materially changes `active_config`;
- successful configuration Reload when it materially changes `active_config`;
- Settings Revert/Discard when it materially restores `saved_config`.

`Apply` itself does **not** create a duplicate history entry because it changes persistence baseline, not the already-previewed logical setting values. Undo after Apply may therefore make Settings dirty relative to the newly saved baseline; it must not silently rewrite the config file.

## 18. Non-history actions

The following do not create history entries by themselves:

- hover;
- focus movement;
- switching Clock/Settings views without state mutation;
- opening/closing help;
- cancelled edits;
- failed config reload;
- transient status messages;
- automatic simulation advancement due only to elapsed time.

## 19. Simulation time and history

Automatic passage of simulated time is not continuously recorded as history.

When undoing a manual time edit after playback has continued, required behavior is:

- restore the time value from immediately before that edit, adjusted by **no** automatic elapsed-time compensation;
- the restored value becomes current canonical time at undo instant;
- playback then continues from that restored value if playing.

This intentionally makes Undo a state-edit operation, not a historical timeline replay.

## 20. Gesture transaction coalescing

A pointer drag that emits hundreds of move events must create only one history entry.

The `before` state is captured at gesture start.

The `after` state is captured at successful release.

If before and after are equivalent within the domain's comparison tolerance, no history entry is created.

## 21. Key-repeat coalescing

Holding Up/Down on one digital field may generate repeated increments.

All repeats belonging to one continuous key-down/key-repeat/key-up sequence must be coalesced into one undo transaction.

Separate key presses create separate transactions.

## 22. Typed-entry coalescing

Typing the two digits of one digital field edit is one history transaction upon commit.

An invalid/reverted edit creates none.


## 22A. Digital scrub transaction

A digital-field scrub gesture follows the same transaction rule as a hand drag: capture canonical time at pointer-down, update from cumulative displacement after the scrub threshold, and create at most one time-history entry on release. Pointer-move count must not affect history count. Escape/focus-loss cancellation restores the captured time and creates no entry.

## 23. Settings history

Settings history policy is fixed to one entry per completed control gesture.

Examples:

- dragging blur-strength slider -> one entry;
- toggling smooth/tick -> one entry;
- dragging animation-speed slider -> one entry.

`Apply` only updates the persisted `saved_config` baseline after a successful write and does not add a second logical-state entry. `Revert`, `Discard`, and successful `Reload` each create one configuration history entry when they actually change `active_config`. Undo/redo changes active settings only; it never performs hidden file I/O.

## 24. Undo behavior

`Ctrl+Z` restores the previous history state and moves the entry to redo stack.

Undo must update all derived views immediately.

Undo itself must not append a new ordinary undo entry.

## 25. Redo behavior

`Ctrl+Y` and `Ctrl+Shift+Z` reapply the most recently undone entry and move it back to undo stack.

## 26. Redo invalidation

After one or more undos, performing a new undoable user action clears the redo stack.

Non-mutating actions do not clear redo.

## 27. Undo during active gesture

Global undo/redo is disabled while a pointer drag or uncommitted digital edit is active.

The user must first commit or cancel the active gesture.

Undo/redo buttons must visually show disabled state whenever their corresponding operation is unavailable.


## 27A. Global shortcuts during active edits

While a pointer drag, digital scrub, uncommitted typed digital edit, or open coalesced keyboard-adjustment transaction is active, global state-mutating shortcuts other than Escape are blocked. This includes Play/Pause, Undo/Redo, Settings navigation shortcuts, and reset actions. Field-local editing/navigation keys remain available. This prevents one transaction from being interleaved with an unrelated mutation.

## 28. Undo/redo buttons

The top navigation undo and redo buttons must:

- mirror keyboard shortcuts;
- disable when their stack is empty;
- provide hover/pressed/focus states;
- optionally expose a short label/tooltip for the next action.

## 29. Modal interaction isolation

When a modal is open:

- background controls do not receive pointer input;
- keyboard focus is constrained to modal controls;
- background scrolling is disabled;
- global shortcuts that would mutate hidden background state are blocked unless explicitly allowed;
- Escape dismisses only if the modal allows cancellation.

## 30. Lost focus during drag

If the top-level window loses focus during any active hand drag, digital scrub, playback-rate slider drag, or mandatory Settings-slider drag, the gesture must be cancelled: restore its gesture-start logical value, release capture, clear pressed/drag state, resume playback if it had been temporarily suspended, and create no history entry. Capture loss / cancel-mode uses the same policy as specified above. A custom scrollbar drag simply ends safely at its current scroll offset because scrolling is transient and non-history state.

If focus is lost while a coalesced keyboard-adjustment sequence is active (for example held Up/Down on a digital field or held arrow on a slider), the values already applied are **committed as one history transaction** if they differ from the sequence-start value, the sequence is closed, key/modifier state is cleared, and any temporary playback suspension ends. A missing key-up must never leave edit suspension or a history transaction stuck. This exact policy must be covered by tests.

## 31. Resize during drag

If the window is resized while dragging a hand, interaction must remain coherent.

The clock center/radius may change, but the pointer must continue mapping against the current clock geometry without teleporting time unpredictably.

## 32. Double click

Double click has no mandatory special semantics.

The application must not accidentally trigger two destructive/reset actions due to double click.

## 33. Right click

Right click/context menus are out of scope unless implemented as optional behavior.

They must not be required for any mandatory function.

## 34. Pointer-wheel behavior

On Settings scrollable content, wheel scrolls the panel.

Over the playback slider, wheel adjustment is optional.

If implemented, wheel changes must be undoable and use predictable increments.


## 34A. Slider keyboard behavior

When the playback-rate slider is keyboard-focused:

- Left/Down decreases rate; Right/Up increases rate;
- the step is `0.1×` while `abs(rate) < 2`, `1×` while `2 <= abs(rate) < 10`, and `5×` at `abs(rate) >= 10`;
- the result is clamped to `[-100,+100]`;
- if a step crosses zero, the mathematically stepped value is used rather than sticking at one sign;
- each discrete key press is one history transaction; one held key-repeat sequence is coalesced as specified earlier;
- Home sets `-100×`, End sets `+100×`, and the `1×` reset action remains a separate button/keyboard-focusable control.

Settings sliders must likewise support arrow-key adjustment using a documented fixed step appropriate to their schema range; pointer-only settings controls do not satisfy keyboard completeness.

## 34B. Mandatory Settings-slider keyboard steps

For mandatory GUI-editable settings that are presented as sliders, keyboard behavior is fixed as follows:

- `animation.speed_multiplier`: arrow step `0.25`, clamped to `0.25..4.0`;
- `effects.modal_blur_radius`: arrow step `1`, clamped to `0..32`;
- `effects.nav_blur_radius`: arrow step `1`, clamped to `0..32`;
- if `playback.default_rate` is presented as a slider, it uses the same keyboard rate-step policy and `[-100,+100]` range as the runtime playback slider;
- `window.ui_scale` has exactly the three schema values `1.0`, `1.25`, `1.5`; a segmented choice is natural, and if presented as a slider its pointer positions and arrow keys must select only those three values.

A Settings control implemented as a numeric/time editor or segmented choice follows its own field/choice keyboard semantics instead; it need not impersonate a slider. Held arrow-key repeats on a slider are coalesced into one history transaction under the existing key-repeat rule.

## 35. Accessibility-oriented keyboard completeness

All mandatory actions except the literal direct hand-drag gesture must have a keyboard-accessible path through focusable controls.

Direct hand dragging remains mandatory as a pointer interaction and cannot be replaced by keyboard controls.
