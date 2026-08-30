# 04 — Canonical Time Model and Synchronization

## 1. Goal

Every representation of time must derive from one authoritative model.

The analog clock, digital editor, playback engine, undo history, state fixtures, and reset behavior must never maintain conflicting independent time values.

## 2. Canonical time representation

The canonical simulated time is a finite real value representing seconds within one 24-hour day.

Conceptually:

```text
0.0 <= t < 86400.0
```

The implementation may store this as `double`, fixed-point integer ticks, or another explicit representation, but it must support smooth sub-second animation.

If floating point is used, all external assignments must normalize and reject non-finite values.

## 3. Normalization

Every committed time update must normalize modulo `86400` seconds.

The normalization function must handle positive and negative inputs.

Examples:

```text
86400     -> 0
86401     -> 1
-1        -> 86399
172800.25 -> 0.25
```

Floating-point negative-zero artifacts must not appear in user-visible formatting.

## 4. Simulation advancement

When playing and not temporarily suspended by an edit gesture:

```text
canonical_time += real_elapsed_seconds * playback_rate
canonical_time = normalize(canonical_time)
```

`real_elapsed_seconds` must be measured with a monotonic clock.

Wall-clock adjustments must not jump the simulation.

## 5. Large elapsed gaps

If the application is stalled, minimized, debug-paused, or otherwise receives a very large frame delta, it must remain numerically safe.

The required policy is:

- UI animations are evaluated from absolute monotonic `now - animation_start`; after a long stall an animation whose duration has elapsed completes at its endpoint rather than advancing in many clamped integration steps.
- simulated time advancement must use the measured elapsed monotonic duration up to `playback.large_gap_clamp_seconds` per event-loop wakeup (default 5 seconds).
- elapsed gaps beyond that configured bound are clamped to the bound to prevent surprising jumps after suspension.

This policy must be testable.

## 6. Playback-rate state

Playback rate is one finite real scalar.

Default is `1.0`.

Allowed runtime/configured range is fixed at `[-100, +100]`.

Every playback-rate source has fixed handling: configuration/state/history input outside `[-100,+100]` or non-finite is rejected as invalid; the UI slider mapping produces values intrinsically inside the range; keyboard changes clamp to the endpoints. Silent NaN/Infinity propagation is prohibited.

## 7. Play/Pause state

Play/Pause is a boolean independent of playback rate.

Temporary direct-manipulation suspension is a separate transient flag/counter.

Effective advancement occurs only when:

```text
is_playing && !edit_suspended && playback_rate != 0
```

## 8. Derived digital time

The digital display derives hours, minutes, seconds, and displayed sub-second behavior from canonical time.

For mandatory `HH:MM:SS` display, seconds are shown as an integer using floor semantics for positive normalized canonical time.

Example:

```text
10:04:09.999 -> 10:04:09
```

The analog second hand may still be between 9 and 10 seconds in smooth mode.

## 9. Smooth second-hand mode

In `smooth` mode:

```text
second_angle = 2π * (t mod 60) / 60
minute_angle = 2π * (t mod 3600) / 3600
hour_angle   = 2π * (t mod 43200) / 43200
```

Zero angle corresponds to 12 o'clock, increasing clockwise.

## 10. Tick second-hand mode

In `tick` mode, the second hand displays the integer second position while minute and hour hands remain continuous from canonical time.

Required default:

- second hand snaps to whole-second marks;
- minute hand remains continuous from canonical time;
- hour hand remains continuous from canonical time.

Tick mode is a rendering policy, not a lower-resolution canonical-time model. While the **second hand itself is actively pointer-dragged**, that hand is rendered at the continuous drag candidate angle so it remains directly attached to the pointer; the rest of the clock still derives from the same candidate canonical time. On commit/cancel the ordinary `tick` rendering policy resumes immediately. This drag-only presentation exception does not quantize canonical time and is independent of `clock.drag_snap`.

## 11. Direct hand drag — general

Dragging a clock hand writes a candidate canonical time derived from the pointer angle and the selected hand.

The application must preserve enough coarser/finer components to make the drag intuitive.

A drag begins with a snapshot of canonical time and playback suspension state.

A drag ends on mouse release, explicit cancel, or loss of capture/focus according to the interaction specification.

## 12. Angle extraction

Given pointer position `(x,y)` and clock center `(cx,cy)`, the drag angle must be computed using both axes with quadrant-correct behavior.

The 12 o'clock direction maps to zero and angle increases clockwise.

Near the exact clock center, pointer angle becomes unstable. The mandatory dead radius is **12 logical pixels** from the current clock center. Pointer moves inside that radius do not update the candidate angle/time and do not reset the last valid unwrapped angle. When the pointer leaves the dead radius, tracking continues from the last valid angular sample without inventing a jump.

## 13. Revolution tracking

Hand dragging must track continuous angular movement across the 12 o'clock discontinuity.

For example, dragging clockwise from 59 seconds to 1 second must be interpreted as a small forward movement, not a 58-second backward jump.

The implementation must maintain unwrapped drag angle or equivalent previous-angle logic. Let `delta_turns` be the accumulated unwrapped pointer-angle change from the first valid pointer angle at pointer-down divided by one full turn. Before optional snapping, the candidate time is fixed as:

```text
second hand: candidate = drag_start_time + delta_turns * 60
minute hand: candidate = drag_start_time + delta_turns * 3600
hour hand:   candidate = drag_start_time + delta_turns * 43200
```

This formula, followed by the specified snap policy and final modulo-day normalization, is the normative drag mapping. It preserves the drag-start phase while allowing carry/borrow and multiple revolutions. A hand drag may start only from a valid hand hit outside the 12-logical-pixel dead radius. The pointer-down polar angle itself becomes the drag angular reference while the canonical time at pointer-down becomes `drag_start_time`; therefore a slightly off-center hit inside the hand tolerance cannot create an initial time jump.

## 14. Second-hand drag

Dragging the second hand modifies seconds/subseconds while preserving the current minute/hour context and allowing natural minute carry/borrow when crossing a full revolution.

One clockwise revolution advances the canonical time by 60 simulated seconds.

One counterclockwise revolution subtracts 60 simulated seconds.

## 15. Minute-hand drag

Dragging the minute hand modifies minutes and lower-order time coherently.

Required behavior:

- pointer angle determines minute-plus-fraction position;
- seconds become the fractional minute implied by the angle in smooth manipulation mode;
- one full clockwise revolution advances one hour;
- counterclockwise revolution subtracts one hour.

Snapping behavior is controlled only by `clock.drag_snap`: `off` keeps the continuous candidate; `on_release` keeps motion continuous and quantizes only the release candidate; `live` quantizes each move candidate. Quantization is defined in Section 16A below.

## 16. Hour-hand drag

Dragging the hour hand modifies time at the hour-cycle scale.

One full clockwise revolution advances 12 hours.

The minute/second fraction is derived from the continuous angle.

The drag must remain unambiguous across 12/24-hour representation by anchoring to the canonical time at drag start and tracking revolution direction.


## 16A. Hand-drag snapping

When `clock.drag_snap` is not `off`, quantization applies to the candidate produced by the selected analog hand:

- second hand: nearest multiple of `clock.snap_seconds` seconds;
- minute hand: nearest multiple of `clock.snap_minutes` minutes;
- hour hand: nearest multiple of `clock.snap_hours` hours.

Quantization is applied to the unwrapped candidate time before final modulo-24-hour normalization so crossing midnight or 12 o'clock remains continuous. The quantization steps are exactly `snap_seconds` seconds, `snap_minutes * 60` seconds, and `snap_hours * 3600` seconds for the three hands respectively; the candidate is rounded to the nearest integer multiple of that step on the unwrapped time axis. Exact halfway ties round in the current drag direction; if there has been no nonzero angular direction yet, ties round toward the numerically larger multiple. `on_release` shows the continuous unsnapped candidate until release; `live` shows the quantized candidate during movement.

Snapping affects analog-hand dragging only. Typed edits, arrow increments, digital scrub-drag, playback, and Reset Time do not use these snap increments.

## 17. Dragging through multiple revolutions

A continuous hand drag may cross more than one revolution.

The implementation must not clamp the unwrapped drag at one circle.

Because canonical time wraps daily, multiple revolutions may eventually normalize, but interaction must remain locally continuous.

## 18. Drag cancel

Pressing Escape during a hand drag restores the canonical time to its value at drag start.

The cancellation creates no undo-history entry.

Playback resumes if it had been playing before the drag.

## 19. Digital edit commit

A committed digital field edit computes a new canonical time using unchanged components from the current edit snapshot plus the new field.

In 24-hour mode:

- hour valid `00..23`;
- minute valid `00..59`;
- second valid `00..59`.

Digital integer-edit subsecond policy is fixed: hour and minute edits preserve the current subsecond fraction; a typed second-field commit sets the selected integer second and clears the subsecond fraction to exactly zero. Arrow-key and scrub adjustments add/subtract whole units and therefore preserve the existing subsecond fraction.

## 20. 12-hour conversion

In 12-hour mode:

- `12 AM` maps to hour 0;
- `12 PM` maps to hour 12;
- `1 PM` maps to hour 13;
- AM/PM changes are undoable time edits.

The display must never show hour `00` in 12-hour mode.

## 21. Increment/decrement keyboard edits

Up/Down on a focused field adjust canonical time by the logical unit:

- hours: ±1 hour;
- minutes: ±1 minute;
- seconds: ±1 second;
- AM/PM toggle: ±12 hours or direct half-day switch.

Wrapping is allowed and must be natural.

Examples:

- `23:59:59` + one second -> `00:00:00`;
- `00:00:00` - one second -> `23:59:59`.

## 22. Reset synchronization

Reset-time action writes exactly one canonical time value.

Every visible clock representation must reflect it within the same UI frame.

No temporary mismatch is permitted between analog and digital views after a committed user action.

## 23. Slider and time independence

Changing playback rate must not itself change canonical time except for ordinary elapsed simulation time while the slider is manipulated.

Undoing a rate change restores the prior rate without rewinding the elapsed time that naturally passed during the gesture.

## 24. Settings effects

Changing 12/24-hour mode is presentation-only and must not alter canonical time.

Changing smooth/tick mode is presentation-only and must not alter canonical time.

Changing snapping settings affects future direct manipulations, not current time.

## 25. Frame consistency

Within one rendered frame, all time-derived components must use the same canonical-time snapshot.

Do not compute analog hands before simulation advancement and digital text after advancement in a way that displays two different instants in one frame.

## 26. Precision expectations

At `1×`, after 60 seconds of uninterrupted execution under ordinary load, simulated time error relative to accumulated monotonic elapsed time should remain below 50 ms, excluding the explicit large-gap clamp policy.

At `100×`, arithmetic must remain stable across repeated day wrapping.

## 27. Required invariant checks

Debug/test builds must assert or explicitly check/report the following invariants during the relevant mandatory tests:

- canonical time is finite;
- canonical time is in normalized range;
- playback rate is finite and in configured range;
- formatted digital fields are valid;
- derived angles are finite;
- no active drag references an invalid hand ID;
- edit suspension cannot become permanently stuck after a gesture ends.
