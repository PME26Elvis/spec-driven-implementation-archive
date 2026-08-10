# 15 — Advanced Physics Validation, Invariance, Stress, and Numerical Quality

## 1. Purpose

This document extends the mandatory validation requirements beyond visually plausible motion.

A 2D rigid-body engine can appear convincing while containing errors in coordinate transforms, mass properties, contact generation, impulse signs, friction limits, fixed-step scheduling, constraint drift, sleeping, or broad-phase lifecycle handling. The cases below are therefore release-blocking correctness checks.

The cases in this document are normative and are part of the physics validation suite executed by the production-engine-backed verification tool required in `10_DEV_TOOLS.md`.

They do not expand the supported product feature set. In particular, they do not require concave polygons, continuous collision detection, soft bodies, fluids, or 3D physics.

## 2. Validation philosophy

The advanced suite must combine multiple oracle families rather than depending on one style of expected output.

Required oracle families:

1. **Analytic reference** — compare against a closed-form or directly calculated physical result where one exists.
2. **Conservation/invariant check** — verify quantities that should remain constant or monotonic in an isolated configuration.
3. **Metamorphic check** — transform a valid scenario in a way that should preserve the physical result, then compare the transformed result.
4. **Independent structural oracle** — compare production subsystem behavior against a simpler independent reference where practical, such as brute-force AABB overlap enumeration.
5. **Bounded-stability check** — for iterative systems without a useful exact trajectory, verify finite state, bounded error, non-divergence, and expected qualitative direction of change.
6. **Deterministic repetition** — replay identical initial state and inputs and require identical or explicitly tolerance-equivalent state.

A test suite made only of screenshots or only of hard-coded final coordinates is insufficient.

## 3. Numeric comparison rules

### 3.1 Finite values first

Every numeric validation comparison must first assert that all compared values are finite.

A NaN must never accidentally pass because a normal greater-than/less-than comparison evaluates false.

### 3.2 Absolute plus relative tolerance

Unless a case defines a stricter rule, scalar comparisons should use an absolute-plus-relative form equivalent to:

`abs(actual - expected) <= abs_tol + rel_tol * max(abs(expected), characteristic_scale)`

The report must print:

- actual value.
- expected/reference value.
- absolute error.
- relative error when meaningful.
- threshold used.

### 3.3 Characteristic scale

Each fixture must document its characteristic length, speed, mass, and/or energy scale when those quantities are used to normalize error.

Validation must not hide poor results by selecting an arbitrarily enormous normalization scale.

### 3.4 Angle comparison

Angles representing equivalent orientations must be compared modulo a full rotation.

### 3.5 Vector comparison

Vector comparisons must report both:

- component error.
- magnitude of vector difference.

### 3.6 Near-zero reference

When a reference quantity is near zero, an absolute tolerance must be used instead of dividing by the near-zero value.

## 4. Fixture discipline

All advanced cases must use deterministic fixtures.

For every case, the machine-readable report must record at least:

- case ID.
- fixture version/name.
- random seed when randomized data is involved.
- fixed time step.
- velocity-iteration count.
- position/stabilization iteration count when applicable.
- gravity vector.
- relevant material coefficients.
- body count.
- joint count.
- simulated duration or step count.
- measured metrics.
- acceptance threshold.
- pass/fail.

A failure report must preserve enough information to reproduce the exact fixture.

## 5. VAL-13 — Force-free translation and torque-free rotation

Configuration:

- one dynamic body.
- gravity disabled.
- no damping.
- no contacts.
- no joints.
- non-zero initial linear velocity.
- non-zero initial angular velocity.
- no forces or torques after initialization.
- run for 10 simulated seconds.

Required checks:

- linear velocity remains constant.
- angular velocity remains constant.
- center-of-mass position matches constant-velocity motion.
- angle matches constant-angular-velocity motion modulo full rotations.

Acceptance:

- final linear-speed error <= **0.1%** plus a small documented absolute tolerance.
- final angular-speed error <= **0.1%** plus a small documented absolute tolerance.
- position error <= **0.1% of total expected travel distance + 0.001 world units**.
- angular-position error <= **0.1% of total expected swept angle + 0.001 rad**.

This case must be run for at least one circle and one non-square convex body.

## 6. VAL-14 — Constant torque angular acceleration

Configuration:

- one dynamic rigid body with known inertia `I`.
- gravity disabled.
- no damping, contacts, or joints.
- initial angular velocity zero.
- known constant torque applied for an exact number of fixed steps spanning at least 2 simulated seconds.

Reference:

- angular acceleration is `tau / I`.
- angular velocity follows constant angular acceleration.
- angular position is compared against the expected semi-implicit-Euler result or analytic trajectory with the integrator truncation error explicitly accounted for.

Acceptance:

- final angular-velocity error <= **0.5%**.
- angular-position error <= **1.5%** of total expected angular displacement + **0.001 rad**.
- stopping torque application causes no further acceleration on later steps.

## 7. VAL-15 — Projectile trajectory

Configuration:

- one dynamic body.
- gravity enabled with known vector.
- no damping.
- no collisions during measurement.
- non-zero initial velocity with both X and Y components.
- run for at least 2 simulated seconds.

Required checks against constant-acceleration reference:

- horizontal/orthogonal-to-gravity velocity component remains constant.
- gravity-parallel velocity changes linearly.
- position follows the expected quadratic trajectory within semi-implicit-Euler error.

Acceptance at default 1/120 s step:

- constant velocity component error <= **0.2%**.
- accelerated velocity component error <= **0.5%**.
- position error in either axis <= **1.0% of characteristic traveled distance + 0.002 world units**.

## 8. VAL-16 — Analytic and representation-consistent mass properties

Required fixtures:

1. solid circle of known radius and density.
2. solid rectangle of known width/height and density.
3. the same rectangle represented as a generic convex polygon.
4. an asymmetric convex polygon tested under translation and rotation of its local/world transform.

Required checks:

- circle mass matches area × density.
- circle center inertia matches the solid-disk formula.
- rectangle mass matches width × height × density.
- rectangle center inertia matches the standard rectangular-lamina formula.
- rectangle-as-polygon mass, centroid, and inertia agree with rectangle-shape values.
- translating/rotating the rigid body transform does not alter local mass or center inertia.
- rotating the local vertex list about its centroid preserves area and inertia.

Acceptance:

- circle/rectangle analytic mass and inertia relative error <= **0.01%**.
- equivalent rectangle-vs-polygon relative mass/inertia error <= **0.1%**.
- transformed equivalent polygon mass/inertia relative error <= **0.1%**.

## 9. VAL-17 — Unequal-mass one-dimensional restitution reference

Use isolated, frictionless, gravity-free head-on collisions with analytically computable final velocities.

Required parameter coverage:

- mass ratio 1:3.
- mass ratio 3:1.
- restitution 0.0.
- restitution 0.5.
- restitution 1.0.
- at least two non-trivial incoming velocity combinations.

Acceptance after contact resolves:

- each body's final normal velocity differs from analytic reference by <= **2.0% or 0.02 world units/s**, whichever tolerance is larger.
- total linear momentum error <= **0.5%**.
- restitution 1 cases retain kinetic energy within the existing elastic-collision tolerance.
- restitution 0 cases do not gain kinetic energy from the collision.

## 10. VAL-18 — Contact action/reaction consistency

Configuration:

- isolated two-body collision.
- gravity disabled.
- no damping.
- friction disabled for the primary normal-impulse subcase.

Required checks:

- the net change in the pair's linear momentum from internal contact impulses is approximately zero.
- if per-contact accumulated impulses are exposed to diagnostics, application to body A and B must be equal in magnitude and opposite in direction.
- swapping body A/B order must reverse the reported contact normal consistently without changing the physical outcome.

Acceptance:

- pair linear momentum relative error <= **0.5%**.
- no contact impulse attracts bodies together when they are separating and no penetration correction requires such attraction.

## 11. VAL-19 — Static-friction threshold and dynamic-friction deceleration

This case strengthens `VAL-06` with numeric expectations.

### 11.1 Static threshold

For a block on an incline with configured static friction `mu_s`:

- choose one slope safely below `atan(mu_s)`.
- choose one slope safely above `atan(mu_s)`.
- use a margin of at least 5 degrees from the theoretical threshold when practical.

Acceptance:

- below-threshold body remains effectively at rest after settling.
- above-threshold body acquires sustained downhill motion.

### 11.2 Dynamic deceleration

On a horizontal surface with known `mu_k` and gravity:

- give a block a known initial horizontal velocity.
- disable other damping.
- measure while it is still sliding.

Acceptance:

- measured deceleration magnitude agrees with `mu_k * |g|` within **10%** over the selected sliding interval.
- acceleration direction opposes tangential velocity.
- kinetic energy does not increase from friction.

## 12. VAL-20 — Dissipative systems must not self-energize

Required subcases:

1. horizontal sliding with dynamic friction and no external work.
2. free motion with positive linear/angular damping and no external work.
3. rolling body with rolling resistance and no external work.

Acceptance:

- final relevant kinetic energy is lower than initial kinetic energy.
- no sustained sequence of samples shows an unexplained energy increase larger than **1% of the initial energy**.
- rolling resistance never increases the magnitude of rolling angular motion in its current direction.

Small one-step numerical fluctuations may occur, but repeated or unbounded energy creation fails.

## 13. VAL-21 — High mass-ratio collision stability

Configuration:

- two dynamic bodies.
- isolated collision.
- mass ratio at least **1:1000**.
- no gravity or damping.
- frictionless normal collision.
- use bounded moderate incoming speed.

Required checks:

- all state remains finite.
- light body response has the physically expected sign.
- heavy body does not receive an implausibly enormous velocity change.
- total momentum remains approximately conserved.

Acceptance:

- linear momentum error <= **1%**.
- final velocities agree with analytic one-dimensional reference within **5% or 0.05 world units/s**.
- no NaN/infinity or solver blow-up.

## 14. VAL-22 — Deep initial-overlap recovery

Configuration:

- start two dynamic or dynamic/static convex bodies with a deliberate overlap equal to roughly **10–20% of the smaller characteristic dimension**.
- initial relative velocity is zero.
- no external force that would push the bodies further together.

Acceptance:

- penetration decreases rather than diverges.
- within 2 simulated seconds, residual penetration is <= **2% of the smaller characteristic dimension**, unless the fixture intentionally geometrically traps the body.
- no NaN/infinity.
- no body is displaced by more than **3 characteristic body lengths** solely from correction of the initial overlap.
- diagnostics may report severe initial overlap, but the engine must remain usable.

## 15. VAL-23 — Tangency, epsilon separation, and grazing robustness

For each of circle-circle, circle-polygon, and polygon-polygon:

Create deterministic subcases for:

- separated by a small positive epsilon.
- exactly tangent within the engine's documented contact tolerance.
- overlapping by a small epsilon.
- nearly parallel/grazing motion.

Acceptance:

- positive-epsilon separated shapes do not produce a false penetrating manifold.
- exact tangency at rest with gravity disabled does not create a large spontaneous velocity.
- small negative separation produces finite, consistently oriented contact data.
- grazing contact does not produce NaN, an inverted normal, or an extreme impulse unrelated to the bounded incoming speed.

The epsilon values used must be recorded in the report.

## 16. VAL-24 — Translation invariance metamorphic test

Create a deterministic multi-body fixture with collisions and at least one joint.

Run version A normally.

Run version B with every world-space position and anchor translated by the same non-trivial constant vector, for example tens of world units in both axes.

Gravity, velocities, angular quantities, shape dimensions, and material properties remain unchanged.

After subtracting the translation from B:

- relative body trajectories.
- velocities.
- angles.
- angular velocities.
- joint errors.

must agree with A within normal solver tolerance.

Acceptance:

- normalized position/velocity differences <= **1%** for the compared non-chaotic fixture.
- contact/joint topology matches by stable semantic identity.

## 17. VAL-25 — 90-degree rotation invariance metamorphic test

Create a deterministic non-degenerate fixture.

Run version A normally.

Create version B by rotating by exactly 90 degrees:

- all body positions around the chosen origin.
- body orientations.
- linear velocities.
- gravity vector.
- force vectors.
- joint anchors and axes.

After inverse-rotating B's results, the trajectories and states must agree with A within solver tolerance.

Acceptance:

- normalized position and velocity differences <= **1%**.
- angular velocities agree in sign/magnitude under the documented coordinate convention.
- corresponding contact normals rotate consistently.

## 18. VAL-26 — Insertion order and stable-ID permutation

Use a fixture whose important contacts occur at distinct times rather than an intentionally ambiguous simultaneous pileup.

Create equivalent worlds with:

- different body creation order.
- different stable numeric IDs.
- equivalent shape/material/joint data.

Acceptance:

- semantic physical outcomes agree within **1%** for compared positions/velocities.
- changing a body's numeric ID does not alter its mass, inertia, collision filtering, or material behavior.
- no cache or broad-phase logic incorrectly depends on accidental pointer or creation ordering for correctness.

Exact bitwise equality is not required for this metamorphic case.

## 19. VAL-27 — Fixed-step render-cadence invariance

Drive the same application/world simulation using different render-frame timing sequences while causing the same fixed physics steps to execute and avoiding the catch-up clamp.

Required timing patterns:

- nominal 30 Hz render cadence.
- nominal 60 Hz render cadence.
- deterministic jittered cadence.

Acceptance:

- total physics step count is identical for the controlled interval.
- final physics state is identical for a deterministic implementation, or differs only within **0.01%** numeric tolerance if representation details prevent bitwise equality.
- render cadence must not be substituted for physics `dt`.

## 20. VAL-28 — Same-input deterministic repetition

Run the same deterministic fixture at least **5 times** in the same executable/platform build with:

- same initial scene.
- same seed.
- same fixed step.
- same ordered user/force events.
- same solver settings.

The verifier must compute a canonical final-state digest over semantic physics state, including at least body transforms/velocities, sleep state, and joint state.

Acceptance:

- all five runs produce identical canonical state bytes/digest when the implementation promises deterministic same-platform execution.
- if the implementation documents unavoidable non-deterministic system timing outside physics, the physics state after an explicitly fixed step count must still be deterministic.

Wall-clock timestamps must not be included in the canonical state digest.

## 21. VAL-29 — Time-step convergence

Use a constant-acceleration or other analytic fixture that is valid under all compared fixed steps.

Required fixed steps:

- 1/60 s.
- 1/120 s.
- 1/240 s.

Run each to the same simulated time.

Acceptance for the semi-implicit Euler integration error metric:

- the 1/120 result must be more accurate than the 1/60 result.
- the 1/240 result must be more accurate than the 1/120 result.
- for the primary position error, halving `dt` should reduce error to approximately first-order behavior; require each halving to produce an error ratio <= **0.70**, unless the error has already reached the declared floating-point floor.

The verifier must report all three errors and ratios.

## 22. VAL-30 — Hinge motor speed and torque limit

Configuration:

- hinge joint with enabled motor.
- known target angular speed.
- known maximum motor torque.
- low-friction/unloaded subcase.
- loaded subcase that requires the torque cap to matter.

Acceptance:

- unloaded motor approaches target speed without sign inversion or runaway.
- when target speed is reachable, steady speed is within **10%** of target.
- accumulated/per-step motor impulse respects the configured torque limit within numeric tolerance.
- loaded case does not secretly exceed the torque cap to reach target speed.
- combining motor with angular limits remains finite and does not persistently violate the limit beyond the existing hinge tolerance.

## 23. VAL-31 — Damped distance-spring response

Configuration:

- one distance joint configured as a spring/damper.
- initial extension at least 20% from rest length.
- no external periodic driving.
- run for at least 10 simulated seconds.

Acceptance:

- oscillation remains finite.
- with positive damping, successive displacement peaks trend downward over time.
- late-window RMS displacement from rest length is lower than early-window RMS displacement.
- the system does not gain mechanical energy without external input.
- the joint does not numerically explode when crossing its rest length.

## 24. VAL-32 — Long chain constraint stress

Create a chain of at least **20 dynamic links** connected by the production joint system and anchored at one end or both ends.

Run for at least **30 simulated seconds** under gravity with one deterministic disturbance.

Acceptance after initial transient:

- no joint/reference is lost.
- no NaN/infinity.
- RMS anchor/constraint error <= **3% of characteristic link length**.
- maximum sustained error <= **10% of characteristic link length**.
- motion remains bounded after the deterministic disturbance.

This is separate from the bridge visual scene and is intended as numerical stress validation.

## 25. VAL-33 — Sleep threshold and wake propagation boundaries

Required subcases:

1. an eligible isolated body below both sleep thresholds long enough to sleep.
2. a body intentionally maintained above the documented linear threshold that must not sleep.
3. a body intentionally maintained above the documented angular threshold that must not sleep.
4. a sleeping contact/joint island receiving a significant impulse.
5. a sleeping body grabbed by the mouse constraint.

Acceptance:

- sleep does not occur before the documented sleep-time threshold.
- above-threshold bodies do not sleep merely because they have existed for a long time.
- required wake triggers wake the appropriate body/island before subsequent dynamic response is solved.
- waking one constrained/contact-connected region does not leave a directly affected partner incorrectly frozen.

## 26. VAL-34 — Dynamic AABB-tree lifecycle oracle

This expands `VAL-12` from static query sets to mutation-heavy operation sequences.

Across at least **25 deterministic seeds**, perform at least **1000 operations per seed** drawn from:

- insert proxy.
- move/update proxy.
- remove proxy.
- query overlapping region.
- pair generation.

At regular checkpoints:

- validate parent/child links.
- validate stored heights.
- validate AABB containment of children by parents.
- validate root reachability.
- validate proxy count.
- compare overlap/pair results with an independent brute-force AABB oracle over the currently live proxies.

Acceptance:

- no missing true overlap pair.
- no reference to removed proxy.
- no duplicate normalized pair in final candidate output.
- all tree invariants pass at every checkpoint.

## 27. VAL-35 — Collision-filter matrix and live filter update

Create bodies spanning at least four category bits and multiple masks.

The fixture must include:

- allowed pair.
- pair blocked from A's mask.
- pair blocked from B's mask.
- mutually blocked pair.
- live runtime edit that changes an allowed pair to blocked while bodies remain geometrically overlapping.
- live runtime edit that changes a blocked pair to allowed.

Acceptance:

- only allowed pairs reach the narrow-phase/contact solver.
- changing a pair to blocked removes or invalidates its persistent contact safely.
- changing a pair to allowed permits contact on the next appropriate collision update.
- filtering does not leave stale impulses applied from an invalid contact.

## 28. VAL-36 — Convex polygon representation invariance

Represent the same convex polygon using:

- each possible cyclic starting vertex for at least one 5+ vertex polygon.
- clockwise input winding.
- counter-clockwise input winding where accepted and normalized.

Required checks:

- normalized geometry represents the same shape.
- mass, centroid, and inertia are equivalent.
- AABB is equivalent under the same transform.
- collision classification against a fixed test body is equivalent.
- contact normal/contact-point set is geometrically equivalent within tolerance.

Acceptance:

- mass/inertia relative differences <= **0.1%**.
- no representation produces a missing collision that exists for another equivalent representation.

## 29. VAL-37 — Thin-shape and high-aspect-ratio robustness

Use valid convex bodies with aspect ratios up to approximately **100:1**, such as a 10 × 0.1 rectangle and an equivalent convex polygon.

Required subcases:

- static thin floor/wall contact.
- rotated thin dynamic body resting/colliding at moderate speed.
- thin polygon against circle.

Acceptance:

- no NaN/infinity.
- normals remain finite and approximately unit length.
- no persistent penetration deeper than **20% of thin-body thickness** after settling.
- contact generation does not create obviously duplicate manifold points at the same location.

This case does not require continuous collision detection; incoming speeds must remain within a range where discrete collision detection can observe the overlap during a fixed step.

## 30. VAL-38 — Geometry-scale robustness

Run collision-geometry fixtures at characteristic lengths of approximately:

- 0.1 world units.
- 1 world unit.
- 10 world units.

Use dimensionless-equivalent configurations for:

- clearly separated pair.
- shallow overlap.
- deeper overlap.
- one-contact polygon case.
- two-contact face case.

Acceptance:

- separated/overlap classification remains consistent across scale.
- contact normal direction remains consistent.
- penetration normalized by characteristic size agrees within **2%**.
- no fixed hard-coded epsilon causes one of these moderate scales to become unusable.

The requirement is moderate scale robustness, not arbitrary astronomical coordinate ranges.

## 31. VAL-39 — Deterministic randomized finite-state fuzz

Using `fixturegen` or equivalent, execute at least **100 deterministic seeds** of bounded randomized physics worlds.

Each fixture must include a mix selected from:

- circles.
- rectangles.
- convex polygons.
- static and dynamic bodies.
- at least some joints across the full campaign.
- friction/restitution variation within valid ranges.
- sleeping enabled for part of the campaign.

Use bounded, documented input ranges that intentionally exclude unsupported continuous-collision-detection conditions.

Run each fixture for at least **600 fixed steps**.

Every step or regular checkpoint must assert:

- all body state finite.
- all AABBs finite and valid.
- all live broad-phase proxies refer to live bodies.
- all contacts refer to live bodies/shapes.
- all joints refer to live bodies.
- inverse mass/inertia remain non-negative.
- no normalized contact normal has invalid magnitude.
- no body exceeds a documented sanity speed cap unless an input could physically justify it.

Acceptance:

- all mandatory seeds pass.
- failing seed and step index are printed and retained.

## 32. VAL-40 — Contact-manifold geometric validity

Across deterministic circle-circle, circle-polygon, and polygon-polygon collision fixtures, validate production manifolds directly.

Required checks where applicable:

- contact normal is finite.
- normal magnitude is approximately 1.
- penetration/separation metadata uses the documented sign convention consistently.
- contact point count is within the supported manifold cardinality.
- convex polygon-polygon manifold has no more than two final contact points.
- final contact points are not numerically duplicate.
- each contact point lies within the documented geometric tolerance of the relevant contact features/surfaces.
- swapping shape order yields equivalent contact geometry with the normal direction reversed consistently.
- manifold contains no point derived merely from object-center midpoint when that point is not on the actual contacting features.

Acceptance:

- normal magnitude error <= **0.1%** for non-degenerate contacts.
- contact-point surface distance <= **0.5% of characteristic body length + 0.001 world units**.
- no mandatory fixture violates cardinality, finiteness, or symmetry rules.

## 33. Additional fixed-step and application semantics checks

The application/E2E suite must additionally verify:

### E2E-PHY-01 Exact single step

When paused, activating Single Step exactly once:

- advances simulated time by exactly one active fixed step.
- executes exactly one physics step.
- does not accidentally execute a render-time-derived extra step.

### E2E-PHY-02 Pause stability

While paused for a measurable wall-clock interval:

- simulated time does not advance.
- dynamic transforms do not drift.
- recorder adds no physics samples.
- UI animations may continue using UI time without changing physics state.

### E2E-PHY-03 Runtime resize

Resize the X11 window repeatedly while a mixed physics scene runs.

Acceptance:

- physics fixed step remains independent of resize frequency.
- no body state is reset or teleported because of viewport resize.
- camera/world mapping remains valid after resize.
- no crash or invalid framebuffer write.

### E2E-PHY-04 Debug-toggle non-interference

Run equivalent fixed-step simulations with collision/joint/AABB/trajectory debug overlays enabled and disabled.

Acceptance:

- debug drawing does not mutate physics state.
- final physics state after a fixed step count is identical or within deterministic numeric tolerance.

## 34. Additional performance/stress fixtures

Performance reporting must add the following workload families. They are required reports even when no absolute hardware timing threshold is specified.

### PERF-DENSE-500

- approximately 500 active bodies.
- dense stacking/contact workload.
- report broad phase, narrow phase, solver time, contact count, and iteration count.

### PERF-JOINT-200

- at least 200 dynamic bodies linked through a meaningful number of joints.
- report joint-solver contribution separately when instrumentation permits.

### PERF-CHURN-5000

- up to 5000 broad-phase proxies in a sparse world.
- a meaningful subset moves every step so fat-AABB updates/reinsertions occur.
- report proxy updates, tree reinsertion count if available, candidate count, median, and p95 broad-phase time.

These workloads must execute production algorithms and may not disable required solver/collision work merely to improve the report.

## 35. Lifecycle and memory stability

In addition to the 120-second simulated soak test, provide a repeated-lifecycle stress test.

Required operations across at least **250 cycles**:

- create/load a deterministic scene.
- run a bounded number of steps.
- create/delete bodies and joints.
- clear/reset or replace the world.

Acceptance:

- no crash.
- no stale body/joint/contact/proxy references.
- no monotonic unbounded growth in live engine object counts after each world is cleared.
- if process memory is measured, the report must distinguish allocator retention from increasing live allocations; live allocation/accounting instrumentation is preferred over relying only on resident-set size.

## 36. Regression-corpus minimum expansion

The deterministic regression corpus required by `09_TEST_VERIFICATION.md` must contain at least **60 stable fixtures** for v1.0, excluding the twelve Golden scenarios.

Coverage must include at minimum:

- all supported shape-pair classes.
- one-contact and two-contact manifolds.
- near-tangent cases.
- deep overlap.
- high mass ratio.
- thin geometry.
- friction threshold.
- restitution reference.
- distance joint.
- hinge limit/motor.
- spring/damping.
- sleep/wake.
- collision filtering.
- broad-phase lifecycle.
- translated metamorphic scene.
- rotated metamorphic scene.
- polygon winding/start-index representation variants.
- force/impulse and trajectory-recorder fixtures from `14_FORCE_TRAJECTORY_TOOLS.md`.

Each fixture must have a stable ID and documented expected invariant/result.

## 37. Validation quality anti-substitution requirements

The advanced suite fails its purpose if the verifier merely repeats production calculations with the same code path and calls the result correct.

Therefore:

- analytic cases must calculate expected values independently from the state-update routine under test.
- broad-phase oracle must use independent brute-force enumeration, not the dynamic tree itself.
- metamorphic cases must create independently transformed input fixtures and compare semantic results.
- finiteness checks must explicitly test `isfinite`-equivalent behavior.
- a machine-readable report must include measured values, not only `pass: true`.
- a case with no executed simulation steps must not pass a dynamic validation accidentally.
- a missing metric, NaN metric, or malformed report field required for acceptance counts as test failure.
- randomized tests must print failing seeds and must include fixed mandatory seed sets in the repository.
- verification code must execute the same production physics engine library/modules used by the GUI.

## 38. Required advanced validation report summary

The aggregate physics validation report must contain separate totals for:

- base validation (`VAL-01` through `VAL-12`).
- force/impulse/recorder validation from `14_FORCE_TRAJECTORY_TOOLS.md`.
- advanced validation (`VAL-13` through `VAL-40`).
- randomized seeds executed.
- metamorphic comparisons executed.
- performance/stress fixtures executed.

A final release has **zero skipped mandatory cases** across all three validation groups.
