# 03 — Math, Rigid Bodies, Kinematics, and Time Stepping

## 1. Coordinate convention

The application must define and document one world convention and use it consistently.

For acceptance, the default convention is:

- +X points right.
- +Y points down.
- Positive angles rotate clockwise on screen.
- Distances are abstract world units.
- Mass uses abstract mass units.
- Time uses seconds.

## 2. Required math primitives

The physics engine must provide explicit C implementations of:

### 2.1 2D vector

Operations include:

- create/set
- add/subtract
- scalar multiply/divide
- component-wise operations where needed
- dot product
- 2D scalar cross product `cross(a,b)`
- scalar-vector perpendicular cross variants needed by angular velocity math
- squared length
- length
- normalization with zero-length guard
- perpendicular vector
- min/max
- finite-value validation

### 2.2 2×2 rotation matrix

Required capabilities:

- construct from angle using sine/cosine
- multiply vector
- transpose/inverse rotation for orthonormal matrix
- multiply matrices if used

A full generic matrix package is not required.

### 2.3 Transform

A transform must combine:

- translation
- rotation

Required conversion:

- local point → world point
- world point → local point
- local vector → world vector
- world vector → local vector

## 3. Rigid-body state

Each rigid body must store enough state to represent:

- stable numeric ID
- body type: static/dynamic/kinematic
- transform position
- rotation angle
- previous transform or equivalent interpolation state
- linear velocity
- angular velocity
- accumulated force
- accumulated torque
- mass
- inverse mass
- rotational inertia
- inverse inertia
- center of mass
- linear damping
- angular damping
- gravity participation if implemented as a body flag
- awake/sleep state
- sleep timer
- rotation-lock state
- material properties
- collision filter properties
- shape reference/data
- broad-phase proxy reference

## 4. Body type semantics

### 4.1 Static

Static bodies:

- have zero inverse mass and inverse inertia.
- are not integrated by forces.
- do not gain velocity from collisions.
- can participate in collision manifolds.
- can anchor joints.

### 4.2 Dynamic

Dynamic bodies:

- have finite positive mass.
- respond to forces, gravity, impulses, contacts, and joints.
- can sleep.

### 4.3 Kinematic

Kinematic bodies:

- are moved by user-assigned linear/angular velocity.
- are not accelerated by forces, gravity, or impulses.
- influence dynamic bodies through collision.
- do not need to sleep.

## 5. Mass and inertia

Mass and inertia must be physically derived from shape geometry and density unless the user explicitly selects direct mass editing.

At minimum support correct formulas for:

- solid circle about center.
- solid rectangle about center.
- uniform-density convex polygon about centroid.

Convex polygon mass properties must compute:

- signed area.
- centroid.
- rotational inertia around the centroid.

Degenerate or near-zero area polygons must be rejected.

Changing geometry or density must update mass properties.

## 6. Forces and impulses

The engine must expose internal operations equivalent to:

- apply force at center of mass.
- apply force at world point, producing torque.
- apply linear impulse at center.
- apply impulse at world point, changing both linear and angular velocity.
- apply torque.

Static and kinematic bodies must ignore force/impulse operations that would incorrectly change their physically controlled state.

## 7. Fixed time step

The simulation must use a fixed physics time step independent of render-frame time.

Default fixed step: **1/120 s**.

The application may offer other fixed-step choices only if diagnostics clearly indicate the active value.

The render loop may accumulate wall-clock time and execute zero or more fixed physics steps.

To prevent runaway catch-up, no rendered frame may execute more than **8** accumulated physics steps before excess lag is clamped/dropped and recorded in diagnostics.

## 8. Integrator

The required base integrator is **semi-implicit Euler**.

For dynamic bodies, each step must conceptually perform:

1. update linear velocity from gravity and accumulated force.
2. update angular velocity from accumulated torque.
3. apply damping in a stable time-step-aware form.
4. solve velocity constraints/contacts at the correct stage.
5. update position from the resulting velocity.
6. update angle from the resulting angular velocity.
7. perform required contact/position stabilization stage.
8. clear force/torque accumulators.

Exact solver staging may differ, but the implementation must remain consistent with sequential-impulse constraint solving.

## 9. Damping

Linear and angular damping must:

- be non-negative.
- not reverse velocity due solely to damping.
- be time-step aware.
- default to small or zero values so they do not hide unstable collision behavior.

## 10. World boundaries

The Free Fall & Boundary Bounce milestone requires world-boundary collision behavior.

World boundaries may be represented as four static bodies or equivalent engine-owned static collision planes/boxes.

Acceptance requires the boundary path to use actual collision/contact response once the collision engine exists; the final release must not keep an early “if coordinate exceeds border, reverse velocity” prototype as the final implementation.

## 11. Numerical validity

Every physics step must guard against propagation of invalid state.

If a body produces NaN or infinity:

- diagnostics must record the body ID and subsystem stage.
- the application must pause simulation automatically.
- the user must be shown a visible error state.
- the process must not continue silently with corrupted state.

## 12. Interpolation for rendering

Rendering may interpolate between previous and current fixed-step states to appear smooth at render rates not equal to physics rate.

Interpolation must be visual only and must not feed interpolated values back into physics state.

## 13. Required math/kinematics tests

Automated tests must cover at least:

- vector add/subtract/dot/cross.
- normalization and zero vector behavior.
- rotation matrix round-trip.
- transform local/world round-trip.
- circle mass/inertia.
- rectangle mass/inertia.
- convex polygon area/centroid/inertia.
- force at center.
- force at offset producing torque.
- impulse at offset producing angular response.
- static body immobility.
- kinematic body constant-velocity movement.
- semi-implicit free-fall trajectory within documented tolerance.
- damping monotonic energy reduction.
- fixed-step single-step exactness.

## 14. External force and impulse API semantics

The rigid-body core must expose internal engine operations equivalent to:

- add force at center of mass;
- add force at a world/local application point;
- apply impulse at center of mass;
- apply impulse at a world/local application point.

Off-center force must contribute torque from the 2D cross product between lever arm and force. Off-center impulse must change angular velocity through inverse inertia.

Force accumulators are transient per-step state and must be cleared exactly once according to the fixed-step lifecycle. A one-shot impulse must not enter the persistent force accumulator.

The GUI tools defined in `14_FORCE_TRAJECTORY_TOOLS.md` must call these real engine paths rather than containing separate UI-only motion equations.

## 15. v1.0 body state additions

Rigid-body state/configuration must include the deterministic collision-detection mode required by `19_CCD_TOI_SHAPE_CAST.md`:

- `DISCRETE`, or
- `BULLET`.

The mode is not an integration state variable, but it is physics-affecting configuration and therefore participates in scene persistence, checkpoint reconstruction, replay commands, and canonical physics state digest.

For TOI evaluation, transformed shape poses at fractional step time must use finite interpolation of position and angle from the current step start toward the predicted unconstrained end transform. Fraction evaluation must not mutate the committed world state until the selected impact fraction is accepted.

