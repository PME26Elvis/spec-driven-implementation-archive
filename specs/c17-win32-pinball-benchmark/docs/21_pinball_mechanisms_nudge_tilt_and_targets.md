# 21 — Pinball Mechanisms, Nudge/Tilt, and Target Components

This document adds the pinball-specific mechanisms required for v1.0.0 beyond the original core wall/bumper/flipper/sensor set.

## 1. Required additional object types

The editor, scene format, renderer, validation, simulation, Inspector, copy/paste, Undo/Redo, and test suite SHALL support:

- `DROP_TARGET`;
- `STANDUP_TARGET`;
- `ROLLOVER`;
- `SPINNER`;
- `KICKOUT`.

These bring the total mandatory authored gameplay object types to 15.

## 2. DROP_TARGET

A Drop Target is a finite capsule-like solid target that can transition from raised to dropped after a qualified ball hit.

Authored properties:

- start/end;
- thickness;
- restitution;
- friction;
- minimum qualifying normal speed;
- base score;
- cooldown;
- initially_raised;
- reset mode;
- enabled.

Runtime states:

- `RAISED`;
- `DROPPED`.

Only RAISED participates as a solid collider. DROPPED remains event-addressable and visible using a lowered/dim state.

## 3. Drop-target qualification

A hit qualifies when:

- target is enabled and RAISED;
- ball is not within target cooldown;
- incoming normal relative speed is at least `min_hit_speed`;
- contact is an actual approaching collision, not penetration correction alone.

Qualified hit ordering:

1. passive collision response;
2. scoring qualification;
3. transition to DROPPED;
4. queue `TARGET_DROPPED` event;
5. start cooldown.

## 4. Drop-target reset

Required reset modes:

- `MANUAL_EVENT`: remains dropped until `RESET_TARGET` action;
- `AFTER_DELAY`: automatically raises after authored simulated-time delay;
- `ON_NEW_BALL`: raises when a new turn begins.

Automatic raising that would overlap a ball SHALL be deferred until the collider can become solid without initial penetration greater than correction slop. Deferred retry occurs once per fixed step deterministically.

## 5. STANDUP_TARGET

Stand-up Target is always solid while enabled and does not drop.

Authored properties parallel DROP_TARGET without reset fields.

A qualified hit emits `TARGET_HIT` and awards base score subject to cooldown.

## 6. ROLLOVER

Rollover is a non-solid sensor strip designed for pinball lanes.

Authored properties:

- start/end;
- width;
- base score;
- activation mode;
- enabled;
- indicator style/state.

Required activation mode v1: `ON_ENTER`.

Each ball crossing into the rollover from outside produces one qualified activation until that ball exits the rollover region.

High-speed swept crossing SHALL still produce activation.

## 7. SPINNER

Spinner is a rotating pinball target around a fixed pivot.

Authored properties:

- pivot;
- half_length;
- thickness;
- rest_angle;
- angular_damping;
- inertia scalar;
- restitution;
- friction;
- score_per_tick;
- tick_angle_deg;
- enabled.

Runtime properties include angle, angular velocity, and accumulated tick phase.

## 8. Spinner collision

Ball contact applies normal/tangential impulse to both ball translational motion and spinner angular velocity using the authored inertia scalar.

Spinner does not have a motor. Angular damping gradually reduces angular velocity.

Every signed rotation crossing of another `tick_angle_deg` boundary in either direction emits one `SPINNER_TICK` and awards `score_per_tick`. Multiple tick crossings inside one fixed step SHALL all be counted in deterministic order, subject to the maximum event budget.

## 9. KICKOUT

Kickout is a circular capture hole that temporarily captures a ball and later ejects it.

Authored properties:

- center;
- capture_radius;
- eject_direction;
- eject_speed;
- hold_time;
- base_score;
- enabled.

## 10. Kickout capture

On swept entry of an eligible ball center into capture radius:

- remove ball from free collision simulation at capture sub-time;
- preserve its runtime ID;
- set state `CAPTURED` by this Kickout;
- zero translational velocity;
- queue `KICKOUT_CAPTURE`;
- optionally score authored base score once;
- begin simulated hold timer.

Only one ball can be held by one Kickout in v1.0.0. Additional balls interact with its boundary as a passive circular solid until the held ball is ejected.

## 11. Kickout ejection

At hold timer expiry, captured ball is placed at deterministic ejection point outside capture radius along normalized eject direction and receives `eject_speed`.

If placement is blocked by another ball/solid beyond allowed penetration, ejection is deferred one fixed step and retried. After 240 consecutive deferred steps, runtime error `KICKOUT_EJECT_BLOCKED` pauses simulation for inspection rather than deleting/teleporting the ball.

Ejection queues `KICKOUT_EJECT`.

## 12. Required event triggers

Add trigger types:

- `TARGET_HIT`;
- `TARGET_DROPPED`;
- `ROLLOVER_ENTER`;
- `SPINNER_TICK`;
- `KICKOUT_CAPTURE`;
- `KICKOUT_EJECT`;
- `TILT_STARTED`;
- `TILT_CLEARED`.

## 13. Required new actions

Add actions:

- `RESET_TARGET target=<id>`;
- `SET_TARGET_DROPPED target=<id> dropped=<bool>`;
- `EJECT_KICKOUT target=<id>`;
- `CLEAR_TILT`.

Actions obey the deterministic event ordering and action-budget rules.

## 14. Nudge input

Play Mode SHALL provide three logical nudge actions:

- `NUDGE_LEFT`;
- `NUDGE_RIGHT`;
- `NUDGE_UP`.

Keyboard bindings are implementation-selectable but SHALL be documented and discoverable.

Each accepted nudge applies the same instantaneous table-frame velocity impulse to every free active ball:

- left nudge: ball velocity gains `(+nudge_impulse, 0)`;
- right nudge: ball velocity gains `(-nudge_impulse, 0)`;
- up nudge: ball velocity gains `(0, +nudge_impulse)` because pushing table upward relative to player produces an apparent downward ball-frame impulse under the chosen screen coordinate convention.

The exact convention is less important than being fixed and covered by acceptance fixtures; the values above are normative for v1.0.0.

Captured Kickout balls do not receive nudge velocity.

## 15. Nudge cooldown

Repeated key-repeat events SHALL NOT create uncontrolled nudges.

One logical nudge is accepted only on key/button press edge and subject to minimum `nudge_cooldown = 0.08 s` simulated time per direction.

## 16. Tilt meter

Game state includes floating `tilt_meter` in range `[0, tilt_threshold]`.

Each accepted nudge adds `nudge_tilt_cost`.

Between nudges, meter decays linearly at `tilt_decay_per_second` based on simulated time, clamped at zero.

## 17. Tilt trigger

When a nudge causes meter to reach or exceed threshold:

- transition to `TILTED` at that fixed step;
- clamp meter to threshold;
- queue `TILT_STARTED` once;
- disable player flipper and launcher actuation;
- disable score awards while Tilt is active;
- continue ball physics and drains;
- visible Tilt state must be unmistakable.

Bumpers/slingshots may continue physical impulses, but their scoring is suppressed while tilted.

## 18. Tilt lifecycle

Default v1 behavior: Tilt persists until all currently active balls have drained and next turn is initialized.

At next turn start:

- clear tilt meter to zero;
- transition out of TILTED;
- queue `TILT_CLEARED`;
- re-enable normal controls/scoring.

`CLEAR_TILT` exists for authored event/test use and clears immediately only when explicitly invoked.

## 19. Tilt and multiball

Tilt affects every active ball in the current turn. It does not end multiball by itself. All balls continue until drained.

## 20. Nudge determinism/replay

Nudge actions are replay logical inputs at fixed-step indices. Replay does not store resulting velocity directly.

Recorded/replayed tilt trigger step MUST match exactly in same-build determinism tests.

## 21. Defaults

Normative defaults:

- `nudge_impulse = 85 logical units/s`;
- `nudge_tilt_cost = 1.0`;
- `tilt_threshold = 3.0`;
- `tilt_decay_per_second = 0.75`;
- `nudge_cooldown = 0.08 s`.

These are table-level authored settings within validation ranges defined in `17_reference_defaults_and_limits.md`.

## 22. Rendering requirements

Each additional mechanism SHALL have visually distinct authored and runtime states.

At minimum:

- raised vs dropped target;
- rollover inactive/activated indicator;
- spinner visible orientation and motion;
- kickout empty vs ball-held indication;
- Tilt overlay/state indicator.

Visual styling remains within the custom software renderer requirement.

## 23. Editor requirements

All new mechanism objects SHALL support:

- palette creation;
- selection;
- move;
- applicable rotate/resize handles;
- exact Inspector fields;
- duplicate/copy/paste;
- group/layer assignment;
- lock;
- delete/reference validation;
- Undo/Redo;
- serialization/round trip.

## 24. Validation errors

Required errors include:

- zero/negative target/spinner dimensions;
- non-finite fields;
- invalid score/cooldown ranges;
- zero eject direction;
- Kickout hold time out of range;
- spinner inertia <= 0;
- tick angle outside supported range;
- target reset delay invalid;
- malformed target references in actions.

## 25. Canonical tests

Required named tests include:

- drop target qualifies once and becomes non-solid;
- drop target cooldown prevents duplicate score;
- delayed reset and blocked-reset deferral;
- stand-up target remains solid;
- high-speed rollover crossing activates once;
- spinner tick count for known angular motion;
- spinner bidirectional ticks;
- Kickout capture/hold/eject timing;
- Kickout second-ball behavior;
- blocked Kickout ejection failure;
- each nudge direction velocity change;
- nudge cooldown;
- tilt meter decay boundary;
- third rapid default nudge causes Tilt;
- Tilt suppresses score/flippers/launcher;
- next-turn Tilt clear;
- deterministic replay including nudge/tilt.
