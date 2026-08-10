# 20 — Runtime Timing, Determinism, Collision Ordering, and Numeric Safety

This document is normative where real-time rendering, deterministic simulation, simultaneous contact processing, randomness, and invalid numeric states interact.

## 1. Separate clocks

The implementation SHALL conceptually separate:

- monotonic wall clock used for UI animation and accumulator scheduling;
- fixed simulation step index used for physics/gameplay/replay;
- simulated elapsed time defined as `step_index * fixed_dt` plus any explicitly defined sub-step event time.

Wall-clock timestamps SHALL NOT be used to order deterministic gameplay events.

## 2. Fixed timestep

`fixed_dt = 1/240 s` is immutable during a running simulation.

Simulation speed controls how many fixed steps are consumed per unit wall time and never changes `fixed_dt`.

## 3. Accumulator

A real-time implementation SHALL use an accumulator or equivalent scheduling method that produces the same ordered fixed steps.

At each rendered frame:

1. obtain finite non-negative monotonic wall delta;
2. clamp/handle it under the stall policy below;
3. multiply scheduling consumption by selected simulation-speed multiplier;
4. accumulate simulation time budget;
5. execute complete fixed steps while budget permits and catch-up budget is not exceeded;
6. render from current state without changing physics merely to interpolate visuals.

## 4. Frame-stall policy

A single wall-clock frame delta larger than 250 ms is a stall.

Required behavior:

- never feed the entire large delta directly into physics;
- execute at most 60 catch-up fixed steps for one rendered frame;
- preserve deterministic fixed-step order for executed steps;
- if backlog remains after 60 steps, drop excess wall-clock backlog and expose a `simulation_backlog_dropped` diagnostic counter;
- dropping real-time backlog SHALL NOT fabricate a larger physics timestep;
- replay/headless execution is not subject to real-time catch-up dropping.

This rule prevents spiral-of-death behavior while keeping fixed-step physics valid.

## 5. Pause and Single Step

While paused:

- accumulator consumption for physics is suspended;
- UI animation may continue;
- no gameplay timers expressed in simulated time advance;
- Single Step executes exactly one complete fixed step and returns to paused state.

## 6. Deterministic authored order

Every authored object/event has a stable authored sequence index derived from scene order. Runtime-created balls/events also receive monotonically increasing runtime IDs from deterministic counters reset at fresh simulation start.

Pointer addresses, hash-table bucket order, allocation order variations, thread scheduling, or operating-system enumeration order SHALL NOT determine simulation results.

## 7. Collision candidate ordering

Broad-phase acceleration is implementation-defined, but candidate pairs SHALL be normalized and sorted before resolution by a deterministic key.

Canonical key priority:

1. earliest time-of-impact within current fixed step;
2. collision class rank;
3. lower canonical object/runtime ID;
4. higher canonical object/runtime ID;
5. stable feature index for compound shapes.

Collision class ranks in v1.0.0:

1. Drain/Sensor swept crossings that occur strictly before a solid impact;
2. ball vs static solid;
3. ball vs moving flipper;
4. ball vs active bumper/slingshot/target solid;
5. ball vs ball.

Ties within `1e-12 s` sub-step time are treated as simultaneous and resolved using the stable key, followed by iterative solver passes.

## 8. Simultaneous contacts

A ball may participate in multiple contacts at the same sub-step time. Implementations SHALL NOT resolve only the first and discard the rest.

Required behavior:

- collect all contacts whose TOI falls within simultaneous-time tolerance;
- sort by canonical key;
- run the normative number of solver iterations over the active contact set;
- maintain finite state after every impulse;
- re-evaluate separating contacts before later iterations;
- cap pathological work under the per-step impact limits.

## 9. Corner/wedge acceptance

Canonical tests SHALL include:

- 90-degree corner impact;
- acute wedge;
- ball initially touching two surfaces;
- ball contacting wall and bumper in the same step;
- three-ball near-simultaneous contact.

Repeated runs must produce identical checkpoints within same-build tolerance.

## 10. Event ordering relative to contacts

For each fixed step, events are queued with `(step_index, sub_time, source_order, trigger_rank, sequence)`.

Trigger rank:

1. `SENSOR_ENTER`;
2. `SENSOR_EXIT`;
3. target/bumper/slingshot qualified hit;
4. drain;
5. timer-expiry actions;
6. explicitly queued follow-up actions.

The action queue is processed after physics state reaches the relevant sub-time. Actions that create new collisions do not retroactively change already-resolved earlier sub-times.

## 11. Sensor edge semantics

Sensors maintain per-ball occupancy state.

Required triggers:

- ENTER: outside at previous evaluated state and crossing/inside at current swept state;
- STAY: occupied at both previous and current fixed-step end state;
- EXIT: occupied previously and exits/crosses completely during current step.

A high-speed ball that enters and exits the Sensor within one fixed step emits exactly one ENTER then one EXIT, ordered by swept crossing time. It does not emit STAY for that step.

A ball remaining inside emits at most one STAY per fixed step.

## 12. Drain edge semantics

Drain is a terminal swept volume for that ball. When drain crossing occurs:

- the ball is marked pending removal at that sub-time;
- later solid contacts at later sub-times in the same step do not apply to that ball;
- drain event executes according to event ordering;
- actual container removal occurs at deterministic end-of-step removal phase.

## 13. Event budget and loop protection

The total number of event actions executed in one fixed step SHALL NOT exceed the normative action budget.

On budget exhaustion:

- stop processing further actions for that step;
- set deterministic runtime error `EVENT_BUDGET_EXCEEDED`;
- pause simulation at end of the current fixed step;
- preserve inspectable state;
- do not crash or spin indefinitely;
- replay/headless return a non-success verification status.

## 14. Direct cycle detection

In addition to the action budget, the event engine SHOULD track active causal chains. A direct synchronous causal cycle may be diagnosed earlier, but early termination MUST be deterministic and produce the same result in GUI/headless modes.

## 15. Randomness policy

The required v1.0.0 table mechanisms do not require nondeterministic randomness.

If an implementation exposes optional random behavior that can affect simulation/gameplay:

- it must use a deterministic PRNG owned by simulation state;
- initial seed is stored in replay metadata;
- fresh non-replay runs use the scene's authored seed or a generated seed recorded before first fixed step;
- wall-clock reads after simulation start SHALL NOT directly influence random draws;
- number/order of PRNG draws is deterministic.

Optional randomness must not alter canonical acceptance fixtures unless explicitly enabled there.

## 16. Finite-value barrier

Every externally parsed floating value SHALL be checked for finiteness before entering authored state.

At runtime, the following SHALL remain finite after every fixed step:

- positions;
- velocities;
- accelerations;
- flipper angles/angular velocities;
- contact normals;
- impulses;
- time values;
- derived score multipliers represented as floating values, if any.

## 17. Runtime non-finite failure

If any required runtime numeric becomes NaN or infinity:

- stop/pause simulation deterministically at the first detected fixed step;
- record object/runtime ID and field;
- expose `NON_FINITE_STATE` diagnostic;
- headless mode returns non-zero;
- do not clamp NaN/Inf to zero and continue silently.

## 18. Extreme-but-finite values

Values beyond authored validation ranges SHALL be rejected before Play. Runtime impulses/speeds are bounded by explicit caps where defined.

The solver SHALL avoid operations that divide by nearly zero geometry length/mass. Degenerate geometry is invalid authored data and SHALL not enter Play.

## 19. Coincident centers

Coincident ball centers and ball/bumper centers use the fallback normal rules in `15_normative_physics_math.md`. Fallback selection must depend only on stable IDs/known velocity, never random bits or memory address.

## 20. World escape policy

A ball center farther than one ball radius beyond every side of the world bounds without having crossed a valid Drain is an out-of-bounds runtime failure in canonical scenes.

Production behavior SHALL:

- report `BALL_OUT_OF_WORLD`;
- pause simulation;
- preserve state for inspection.

The application SHALL NOT silently teleport escaped balls back into the table.

## 21. Energy-explosion monitor

Canonical soak tests SHALL monitor total translational kinetic energy and flag an energy explosion if, in a scene with no active energy source during the measured interval, energy increases above the specified invariant tolerance.

In scenes containing active flippers/bumpers/slingshots/launcher, energy growth is allowed only when correlated with those mechanisms. A global sanity cap remains required to catch numeric runaway.

## 22. Same-build determinism

Given identical:

- normalized scene semantic model;
- physics version;
- PRNG seed;
- initial runtime ID counters;
- fixed-step input stream;

same executable build SHALL produce equivalent ordered checkpoints and final fingerprint regardless of:

- window size;
- UI scale;
- render frame rate;
- Play speed multiplier;
- visibility of debug overlays;
- whether simulation is GUI or headless.

## 23. Deterministic fingerprint content

State fingerprint SHALL include at minimum:

- fixed step index;
- active balls ordered by runtime ID with position/velocity/radius/state;
- flipper runtime angles/state ordered by object ID;
- runtime-enabled state of mutable objects;
- drop-target states;
- gate states/timers;
- sensor occupancy sets ordered by sensor then ball;
- score/combo/multiplier;
- turn state;
- tilt state;
- PRNG state if used;
- pending deterministic timers/events.

UI state is excluded.

## 24. Golden checkpoints

At least five canonical scenarios SHALL include intermediate golden checkpoints in addition to final expected results. Required minimum checkpoint steps are 1, 10, 60, 240 where the scenario duration allows, plus scenario-specific impact/event steps.

A test that validates only the final position while intermediate states diverge is insufficient.

## 25. Divergence reporting

When two expected deterministic traces differ, verification tooling SHALL report:

- first mismatching fixed step;
- first mismatching entity/field by canonical comparison order;
- expected value;
- actual value;
- absolute/relative error when numeric;
- fingerprints immediately before and at divergence.

## 26. Invariant suites

Required invariant categories:

- no-force stationary ball remains stationary;
- zero-gravity zero-damping free ball preserves velocity;
- elastic equal-mass head-on ball collision conserves linear momentum and kinetic energy within tolerance;
- frictionless static-wall bounce preserves tangential velocity;
- perfectly inelastic normal component with restitution 0 does not reverse away with positive normal speed beyond correction tolerance;
- no active energy source causes spontaneous kinetic-energy growth beyond tolerance;
- all canonical long-run states remain finite.

## 27. Invariant tolerances

For controlled invariant fixtures without CCD ambiguity:

- momentum relative error <= `1e-8` after a single isolated collision;
- kinetic-energy relative error <= `1e-8` for restitution 1 and friction 0 isolated collision;
- stationary drift <= `1e-10` logical units over 10,000 steps with gravity/damping disabled;
- free-flight velocity drift <= `1e-10` logical units/s over 10,000 steps with forces/damping disabled.

If floating implementation differs internally, externally observed double-precision outputs still must meet these acceptance tolerances.

## 28. Soak tests

Required soak runs include:

- 1,000,000 fixed steps of a bounded canonical scene;
- 16-ball active-play scene for at least 30 simulated seconds;
- 64-ball physics stress fixture for at least 10 simulated seconds in headless mode;
- repeated start/stop/restart cycles;
- repeated event-heavy scene under maximum valid action load below budget.

No soak run may exhibit crash, hang, non-finite state, out-of-bounds escape in valid fixture, or unbounded resource growth.

## 29. Headless timing

Headless simulation SHALL advance by requested fixed-step count, not wall time. Headless performance measurement may report wall time separately but shall never change simulation state based on it.

## 30. Acceptance minimums

The Determinism/Physics Release Gates SHALL cover every section above with a named automated test, trace comparison, or canonical fixture where objectively testable.
