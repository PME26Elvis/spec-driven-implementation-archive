# 05 — Contact Solver, Friction, Sleeping, and Joint Constraints

## 1. Solver model

The engine must use an iterative **sequential impulse** solver for contact and joint velocity constraints.

The solver must not resolve final collisions merely by directly separating shapes and reversing velocities.

## 2. Contact velocity constraint

For each contact point, the solver must account for:

- relative linear velocity.
- angular velocity contribution at contact anchors.
- inverse mass.
- inverse inertia.
- contact normal.
- effective mass.
- accumulated impulse.

Normal impulses must be non-negative after accumulation.

## 3. Restitution

Each body has a restitution coefficient in `[0,1]`.

Pair restitution combination rule must be deterministic and documented. Required default: **maximum of the two coefficients**.

Restitution must be suppressed below a configurable low relative-normal-velocity threshold to avoid jittering stacks.

Default restitution threshold: **1.0 world units/s**.

## 4. Coulomb friction

Each body must support:

- static friction coefficient.
- dynamic friction coefficient.

Coefficients must be non-negative.

Pair combination default: geometric mean for each coefficient.

At each contact point:

- compute tangent direction from relative velocity or a deterministic perpendicular fallback.
- solve tangent effective mass.
- accumulate tangent impulse.
- clamp according to Coulomb friction limit based on accumulated normal impulse.

The solver must distinguish the intended static-versus-dynamic limit behavior or implement a well-documented equivalent Coulomb approximation.

A constant arbitrary horizontal damping force does not satisfy friction requirements.

## 5. Rolling resistance

Rolling resistance is a separate required physical effect.

Each dynamic body exposes a non-negative rolling-resistance coefficient.

The implementation must apply a contact-dependent resisting angular impulse/torque with a bounded magnitude related to contact normal loading.

It must:

- oppose relative rolling/angular motion.
- never accelerate rolling in its current direction.
- avoid sign-flip oscillation at tiny angular velocities.

Simple global angular damping alone does not satisfy rolling resistance.

## 6. Warm starting

Before iterative velocity solving, persistent contacts and joints must re-apply cached impulses from the previous step, subject to validity of contact feature matching.

Warm starting can be disabled only as a diagnostics option; normal release behavior must enable it.

## 7. Solver iteration counts

Default velocity iterations: **10**.

Default stabilization/position iterations: **4** if a separate stage exists.

User-adjustable range must include at least 1–30 iterations.

Changes must affect the real solver and be visible in Diagnostics.

## 8. Baumgarte-style stabilization

Penetration stabilization must include:

- penetration slop/tolerance.
- proportional correction factor.
- time-step-aware bias.
- clamping to prevent explosive corrections.

Suggested/default values may be tuned, but the implementation must expose source constants and diagnostics.

The solver must not rely solely on teleporting bodies completely apart after every collision.

## 9. Deep-overlap behavior

For bodies created in overlap or loaded from a scene with overlap:

- simulation must not produce NaN/infinity.
- penetration should reduce over subsequent steps.
- corrections must remain bounded.
- diagnostics may flag severe initial overlap.

## 10. Sleeping and waking

Dynamic bodies must support sleeping.

### 10.1 Sleep eligibility

A body can sleep after remaining below both:

- linear speed threshold.
- angular speed threshold.

for a continuous sleep-time threshold.

Required defaults:

- linear threshold ≤ 0.08 world units/s.
- angular threshold ≤ 0.08 rad/s.
- sleep time ≥ 0.4 s.

Exact chosen values must be documented.

### 10.2 Island behavior

Connected contact/joint islands should sleep coherently so one body does not remain actively jittering inside an otherwise stable stack.

### 10.3 Wake triggers

At minimum wake on:

- significant collision/impulse.
- direct mouse constraint grab.
- force/impulse application.
- body property edit affecting dynamics.
- connected joint partner waking when necessary.

### 10.4 Sleeping effect

Sleeping bodies must be excluded from ordinary integration and unnecessary solver work until woken.

A sleeping flag used only for tinting is invalid.

## 11. Constraint framework

Joints must be represented as actual constraints solved through Jacobian/effective-mass logic or mathematically equivalent sequential impulses.

Each joint must have:

- stable joint ID.
- body A and body B.
- local anchors where applicable.
- cached impulses for warm starting.
- validity lifecycle.
- debug rendering.

## 12. Distance joint

Required behavior:

- maintains target distance between two body anchor points.
- works for dynamic-static and dynamic-dynamic combinations.
- supports a rigid mode.
- supports optional spring frequency/stiffness and damping behavior.

Inspector fields:

- anchor A.
- anchor B.
- target length.
- spring enable.
- stiffness/frequency representation.
- damping ratio or equivalent.
- collide-connected toggle.

Distance error must remain bounded in the validation scene.

## 13. Revolute/hinge joint

Required behavior:

- constrains two anchor points to coincide while allowing relative rotation.
- solves two translational constraint dimensions.

Required options:

- angular limit enable.
- lower angle.
- upper angle.
- motor enable.
- motor target speed.
- maximum motor torque.
- collide-connected toggle.

The motor must be solver-driven, not direct angle assignment.

The limit must resist motion beyond bounds without creating unbounded impulse explosions.

## 14. Mouse joint

The pointer-drag interaction must be implemented as a constraint.

Required properties:

- target follows pointer world position.
- configurable maximum force.
- spring-like stiffness/frequency.
- damping.
- body remains collidable while dragged.

Teleporting the body transform each pointer event is invalid for the running-simulation drag mode.

## 15. Constraint islands

The engine must identify enough connectivity between awake dynamic bodies, contacts, and joints to solve interacting groups coherently.

A sophisticated graph package is not required, but the solver must avoid obvious double-solving or stale references.

## 16. Stability requirements

The following scenarios must remain stable for their acceptance duration:

- five-block tower.
- pendulum.
- linked chain/ragdoll.
- suspension bridge under impact.
- resting circle on flat floor.
- mixed circle/box stack.

Stability means no NaN, runaway velocity, spontaneous large energy gain, persistent deep penetration, or joint separation beyond acceptance tolerance.

## 17. Physics conservation validation

Separate isolated validation scenarios must test conservation behavior where applicable.

Required reference case:

- gravity disabled.
- damping disabled.
- friction disabled.
- restitution = 1.
- two-body collision.

The validation report must calculate before/after total linear momentum and kinetic energy and report relative error.

Angular-momentum checks are required for at least one off-center collision case.

The acceptance thresholds are defined in `09_TEST_VERIFICATION.md`.

## 18. Required solver/joint tests

Tests must include:

- normal impulse prevents closing velocity after collision.
- restitution increases separation velocity in elastic case.
- tangent impulse clamps to friction cone/limit.
- friction opposes tangent motion.
- rolling resistance opposes rolling.
- warm-start cache reuse.
- penetration bias bounded.
- body enters sleep.
- sleeping body wakes on impact.
- distance joint static-dynamic.
- distance joint dynamic-dynamic.
- distance joint spring damping.
- hinge anchor coincidence.
- hinge limit lower/upper.
- hinge motor torque clamp.
- mouse joint fast target movement.
- deleting connected body invalidates/removes joints safely.

## 19. User-applied external interaction

Externally applied forces and impulses from the Sandbox are independent of contact and joint constraint impulses but must participate in the same rigid-body state.

Required ordering must be deterministic and documented in the engine step contract so that automated tests can reproduce results. At minimum:

- accumulated external forces affect velocity through the normal integration path;
- one-shot impulses modify velocity exactly once when committed;
- contact/joint solving subsequently sees the resulting velocities on the appropriate simulation step;
- sleeping bodies wake before an externally induced state change is discarded by sleep logic.

The contact solver must not silently erase or replace externally produced velocity changes except as physically required by contacts/constraints.

## 20. v1.0 TOI solver integration

Contacts produced by CCD/TOI use the production sequential impulse/contact machinery rather than a separate simplified bounce rule.

At a TOI sub-step:

- construct the real manifold at the impact pose;
- apply restitution/friction/material mixing consistently with discrete contacts;
- maintain deterministic constraint ordering;
- preserve joint constraints for affected bodies/islands;
- continue only the bounded remaining step fraction;
- expose the TOI-origin contact to Solver Inspector.

A TOI path that directly overwrites velocity/position with a special-case formula and bypasses production contact solving is prohibited.

