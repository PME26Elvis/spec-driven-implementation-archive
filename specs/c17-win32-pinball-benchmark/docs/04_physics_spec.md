# 04 — Deterministic 2D Physics Specification

## 1. Core principle

The physics engine is a required production subsystem implemented by the assignee. Interactive Play, Simulation Preview, replay playback, and headless verification MUST share the same production physics code.

A separate simplified headless solver is prohibited.

## 2. Numeric representation

Physics calculations SHALL use double precision. Non-finite scene values are rejected before active simulation. NaN/infinity MUST NOT propagate silently.

## 3. Coordinate and units

- +X right;
- +Y down;
- position: logical units;
- velocity: logical units/s;
- acceleration: logical units/s²;
- time: seconds.

## 4. Fixed timestep

Normative timestep:

`dt = 1 / 240 second`

Simulation advances only in integer fixed steps. Rendering may interpolate/display state but MUST NOT alter physics timestep.

## 5. Real-time accumulator

Interactive mode accumulates monotonic elapsed real time multiplied by selected simulation-speed multiplier and performs zero or more fixed steps.

Catch-up steps per render frame MAY be capped to avoid spiral-of-death, but implementation MUST:

- expose falling-behind diagnostics;
- never silently enlarge `dt`;
- preserve deterministic results for an explicit sequence of fixed steps.

## 6. Simulation-speed multipliers

Required: 0.25×, 0.5×, 1×, 2×, 4×.

They change scheduling relative to wall time only. They MUST NOT change gravity, impulses, formulas, or `dt`.

Step-indexed replay outcome therefore remains identical regardless of playback multiplier.

## 7. Integration

Ball translation SHALL use semi-implicit Euler:

1. apply acceleration/forces to velocity;
2. apply damping;
3. enforce maximum-speed safety policy;
4. advance position through collision/CCD pipeline.

Exact damping formula must be documented and based on elapsed seconds, not a frame-rate-specific magic multiplier.

## 8. Gravity

Default `(0, 980)` and applies to each active normal ball.

## 9. Ball runtime state

Each ball has at least:

- unique runtime ID;
- position;
- velocity;
- radius;
- mass;
- restitution;
- friction;
- damping;
- max speed;
- alive/drained state;
- previous position;
- sensor occupancy state;
- last collision diagnostic information.

## 10. Required collision shapes

Solid:

- static capsule/rounded segment for Wall/Ramp/Gate/Slingshot;
- circle for Bumper;
- moving capsule for Flipper.

Non-solid triggers:

- axis-aligned Sensor rectangle;
- axis-aligned Drain rectangle.

## 11. Ball vs static capsule

Required calculations include closest point, separation vector, contact normal, penetration depth, relative normal velocity, normal impulse, and tangential/friction response.

Zero-length segments are invalid and never passed unchecked into normalization.

## 12. Ball vs bumper circle

Use circle-circle geometry. After normal collision response, a qualified impact applies additional bumper impulse away from bumper center.

Coincident centers require a deterministic fallback normal instead of division by zero.

## 13. Ball vs ball

Multiball requires dynamic circle-circle collision accounting for both masses, relative normal velocity, restitution, penetration correction, and deterministic pair ordering.

A pair already separating must not receive a new closing impulse merely because tiny overlap remains.

## 14. Flipper motion

A flipper is a rotating capsule around fixed pivot.

- input pressed: move toward active angle at configured engage speed;
- released: move toward rest angle at return speed;
- clamp at endpoints;
- angular velocity becomes zero at commanded endpoint.

## 15. Ball vs moving flipper

Collision response MUST consider flipper surface velocity at contact point due to angular velocity. Relative contact velocity subtracts the moving surface velocity.

This is required so a moving flipper transfers energy to a ball. Endpoint teleportation with only static overlap response fails.

## 16. One-Way Gate

Gate stores normalized allowed direction/side test.

- approach from allowed direction: passes through;
- approach from blocked direction: capsule collision;
- near-parallel motion handled deterministically;
- residual penetration corrected without explosive impulse.

## 17. Slingshot

Performs normal surface response, then adds configured normal impulse on qualified new impact. Per-ball/object cooldown prevents every-step retrigger.

## 18. Restitution combination

Normative combination rule:

`e = min(e_a, e_b)`

Alternative documented combination is allowed only if all normative fixtures with equal material values still pass.

## 19. Friction

Collision friction opposes tangential relative motion. Full ball spin is not required in v1.

Tangential impulse must be bounded and must not reverse the tangential component beyond zero in a simple passive contact.

## 20. Penetration correction

Residual overlaps require bounded positional correction with tolerance/slop and deterministic solver iteration.

Implementation must document chosen constants.

Required constraints:

- slop roughly 0.01–0.1 logical units or equivalently justified;
- bounded correction fraction;
- at least 4 contact-solver iterations for dense contact frames or equivalent robust method;
- no unbounded velocity injection.

## 21. Continuous collision detection

Fast balls MUST NOT tunnel through thin static walls in acceptance scenarios. Discrete-overlap-only simulation fails.

CCD at minimum covers:

- moving circle vs static capsule/segment;
- moving circle vs Bumper circle;
- swept Sensor crossing;
- swept Drain crossing.

Moving-flipper collision may use conservative substeps or swept strategy but must pass required flipper stress tests.

## 22. CCD method freedom

Allowed approaches include analytic time-of-impact, swept circle intersection, conservative advancement, or bounded adaptive substeps combined with explicit static CCD.

Method is free; behavior is prescribed.

## 23. Multiple impacts inside one step

A ball may collide multiple times during one fixed step. Engine must resolve earliest/equivalent robust impact, advance remaining step time, bound impact iterations, and terminate safely in pathological traps.

Hitting an impact-iteration cap must increment diagnostics.

## 24. Maximum speed

After forces/impulses, speed is clamped to configured maximum while preserving direction. The cap is only a safety bound and cannot substitute for stable collision response.

## 25. Bumper impulse

Default additional impulse magnitude: 500 for mass=1 ball, applied along bumper-to-ball normal. Velocity delta follows impulse/mass.

## 26. Slingshot impulse

Default additional impulse magnitude: 350, same mass/normal semantics.

## 27. Launcher charge

Defaults:

- minimum launch speed: 400;
- maximum: 1800;
- full charge: 1.2 simulation seconds;
- linear charge fraction `clamp(held_time / 1.2, 0, 1)`;
- launch speed linearly interpolates min→max;
- holding beyond full charge does not increase speed;
- direction normalized before launch.

## 28. Launcher input semantics

LAUNCH press begins/continues charge when a valid launchable ball is associated with launcher/spawn lane. Release launches once. Release without prior valid charge does not create repeated free launches.

## 29. Launcher-to-spawn ownership

Every Launcher references exactly one authored Ball Spawn through its `spawn` field. The referenced object MUST exist and MUST be a Ball Spawn. At most one enabled Launcher may target a given Ball Spawn; otherwise scene validation is an Error.

When a turn enters READY and its primary launch path uses an enabled Launcher, one launcher-managed ball is instantiated at the referenced Ball Spawn with zero velocity and is held at the spawn center until launch. Authored `initial_velocity` on that Ball Spawn is ignored for this launcher-managed READY ball. On LAUNCH release, the held ball receives velocity `normalized(launcher.direction) * launch_speed`.

A Ball Spawn not owned by an enabled Launcher uses its authored initial velocity when a runtime/gameplay action explicitly spawns a ball from it. Headless scenarios that directly initialize a ball at a spawn likewise use the scenario/requested initial velocity and do not implicitly create launcher-hold state unless launcher interaction is part of that scenario.

The hold is a gameplay constraint, not an overlapping dynamic collision. The held ball MUST NOT accumulate gravity, damping, contacts, Sensor events, or score before release.

## 30. Spawn collision safety

Authored Ball Spawn inside solid object is validation Error. Runtime event spawning at currently blocked location is rejected safely and diagnosed; it MUST NOT create an explosive overlapping ball.

## 31. Active-ball capacity

Runtime enforces configured maximum. Excess spawn attempts are rejected non-fatally while existing balls remain valid.

## 32. Sensor semantics

Sensor tracks outside→inside ENTER, inside→inside no repeat, inside→outside LEAVE.

High-speed crossing that enters and leaves within one fixed step must still generate deterministic trigger behavior as defined by fixture.

## 33. Drain semantics

A swept crossing drains the ball even if final step position lies beyond Drain region. High-speed skipping is prohibited.

## 34. Deterministic ordering

Results MUST NOT depend on memory addresses or unspecified collection traversal order.

Use a stable deterministic policy for:

- balls;
- static colliders;
- ball pairs;
- event processing;
- action processing.

Recommended: runtime ID/object authored order plus action list index.

## 35. Randomness

Core physics/gameplay requires no uncontrolled randomness. Any optional random behavior must use explicit seed included in replay state.

## 36. Compiler floating-point behavior

Release builds used for deterministic acceptance MUST NOT enable unsafe fast-math transformations that break reproducibility.

## 37. Determinism tolerance

Within the same supported build/platform, replay self-comparison checkpoints require:

- position absolute error ≤ 1e-7;
- velocity absolute error ≤ 1e-7;
- discrete state/score/event counts exact.

Cross-build/package analytic fixtures may declare looser tolerance, normally 1e-5 to 1e-3.

## 38. Physics Inspector data

Selected runtime ball exposes:

- position;
- velocity vector;
- scalar speed;
- radius;
- mass;
- active sensor IDs;
- last solid collision object ID;
- last collision normal;
- current/last contact count;
- fixed-step index.

## 39. Debug overlay

Toggleable overlay renders at least:

- collision shapes;
- ball bounds;
- Sensor/Drain bounds;
- contact points;
- contact normals;
- velocity vectors;
- optional object IDs;
- selected-ball speed.

Debug overlay MUST NOT modify physics.

## 40. Runtime diagnostics

Expose counters for:

- fixed steps;
- CCD impacts;
- penetration corrections;
- collision-iteration cap hits;
- rejected spawns;
- active balls;
- simulation time;
- falling-behind/catch-up condition.

## 41. Stability requirements

Required stress scenes must produce:

- no NaN/infinity;
- no memory corruption;
- no unexplained teleport from solver divergence;
- no sustained passive energy explosion;
- bounded contact/event processing.

## 42. Passive energy sanity

In zero-gravity scene with only static passive surfaces, restitution ≤1, no bumpers/slingshots/flippers/launchers, and damping 0, translational kinetic energy may decrease but MUST NOT systematically increase beyond small numeric tolerance.

## 43. Headless isolation

Physics core SHALL be callable without creating an Win32 window. UI consumes physics state; physics MUST NOT depend on UI widgets.

## 36. Simultaneous-contact and runtime determinism

Collision candidate normalization, same-TOI contact collection, stable deterministic ordering, solver passes, Sensor edge semantics, frame-stall catch-up policy, numeric safety, world-escape handling, fingerprints, golden checkpoints, and invariant requirements are normative in `20_runtime_timing_determinism_and_numeric_safety.md`.

## 37. Additional dynamic mechanisms

Spinner collision/angular response, Kickout capture/ejection, target collision/state behavior, Nudge apparent impulse, and Tilt physical/control interaction are defined in document 21 and use the same production fixed-step solver.

## 38. No render-clock physics

Render interpolation, UI animation, HiDPI changes, trace visibility, and frame rate SHALL NOT alter fixed-step physical outcomes. Backlog dropping uses the explicit real-time scheduling policy and never substitutes a larger `dt`.

## 39. Numeric failure visibility

NaN, infinity, impossible out-of-world escape, or required blocked-ejection failure must transition to inspectable runtime error/pause behavior rather than silent clamping/teleportation.
