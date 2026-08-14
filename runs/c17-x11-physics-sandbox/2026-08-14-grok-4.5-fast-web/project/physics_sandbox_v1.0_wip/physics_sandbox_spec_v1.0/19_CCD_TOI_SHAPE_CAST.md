# Continuous Collision Detection, Time of Impact, and Shape Cast

Version: **1.0**
Status: **Normative**

This document defines the mandatory continuous-collision and swept-query scope for the 1.0 release. It closes the intentional discrete-collision gap from earlier drafts.

## 1. Purpose

The engine must not rely exclusively on end-of-step overlap tests for bodies marked for continuous collision detection. A sufficiently fast rigid body must not be able to cross a thin collidable body between two fixed simulation steps without the collision system detecting and resolving the event.

The same geometric sweep core is also exposed to the product as a Shape Cast query tool.

## 2. Required body mode

Every rigid body has a collision detection mode:

- `DISCRETE`
- `BULLET`

`DISCRETE` uses the ordinary broad-phase + narrow-phase path.

`BULLET` additionally participates in continuous collision detection before committing the full transform for a fixed step.

The mode must be editable in the Inspector, persisted in scene files, supported by undo/redo, deterministic replay, checkpoints, and E2E automation.

## 3. Supported CCD geometry

CCD is mandatory for all required rigid-body shape types:

- circle,
- rectangle,
- arbitrary convex polygon within the project vertex-count limits.

Concave polygons remain unsupported.

CCD must support BULLET bodies against:

- static bodies,
- kinematic bodies,
- dynamic bodies,
- sensor shapes for event detection.

A sensor TOI event must not generate a physical impulse.

A potentially colliding pair enters the continuous path when at least one member is in BULLET mode and the pair passes collision filtering. If both are DISCRETE, only ordinary discrete collision semantics are required. Relative motion of both bodies must be considered for BULLET-vs-dynamic/kinematic pairs.

## 4. Continuous transform model

For one fixed step, a candidate body transform is defined by interpolation between start-of-step state and the unconstrained predicted state.

The continuous collision system must account for both:

- linear translation,
- angular rotation.

A solution that sweeps only the center of mass while ignoring shape rotation does not satisfy polygon/rectangle CCD.

## 5. Swept broad phase

Before narrow TOI work, the engine must derive a conservative swept AABB for each BULLET body.

The swept AABB must include:

- start transform bounds,
- predicted end transform bounds,
- conservative angular-motion expansion based on the shape's maximum radius from center of mass.

The swept AABB is queried against the production Dynamic AABB Tree.

A brute-force all-body CCD scan may exist only as a verification oracle and may not replace production broad-phase querying.

## 6. Time-of-impact solver

The production TOI algorithm must be an actual continuous root/separation search, not fixed micro-stepping disguised as CCD.

The required approach is:

1. compute a conservative candidate interval `[0, 1]` for the current fixed step,
2. evaluate transformed shapes at a candidate fraction,
3. compute signed separation / overlap using the production convex geometry primitives,
4. bracket the first transition from separated to touching/overlapping,
5. refine the first-impact fraction with conservative advancement and/or bracketed bisection,
6. stop when the TOI fraction or separation tolerance is met,
7. advance to the safe impact fraction,
8. build a real contact manifold at the impact configuration,
9. solve the impact using the production sequential impulse solver,
10. continue the remaining fractional time if safe and within the sub-step budget.

Equivalent internal factoring is allowed only if the observable behavior and all numerical acceptance tests are met.

## 7. Prohibited CCD substitutes

The following do not satisfy CCD:

- multiplying the global physics frequency until tunneling happens less often,
- running an arbitrary fixed number of small discrete substeps for every frame,
- expanding thin walls into visually invisible giant colliders,
- clamping velocity below a tunneling threshold,
- teleporting a body back after it is observed past a wall,
- ray casting only the center point of a polygon,
- treating every body as a circle with a bounding radius for final collision response,
- hard-coding special behavior for the acceptance fixtures.

## 8. TOI tolerances

The implementation must expose constants for:

- separation tolerance,
- TOI fraction tolerance,
- maximum TOI iterations,
- maximum CCD sub-steps per fixed step.

The default mandatory limits are:

- separation tolerance <= `1e-4` world units for normalized acceptance fixtures,
- fraction tolerance <= `1e-5`,
- maximum TOI search iterations >= 20,
- maximum CCD sub-steps per body per fixed step between 4 and 16 inclusive.

Reaching an iteration/sub-step cap must be observable through diagnostics and acceptance reports. Silent fallback to tunneling is prohibited.

## 9. Impact ordering

When multiple candidate TOIs exist in one step:

- the smallest valid TOI fraction must be processed first,
- ties within the defined tie epsilon must use deterministic stable ordering,
- ordering may not depend on raw pointer addresses, hash-table iteration order, or current render frame timing.

The tie-break key must be documented and included in deterministic validation.

## 10. Initial overlap behavior

CCD does not replace ordinary penetration recovery.

If a BULLET body starts a step overlapping another body:

- the pair enters the ordinary manifold/stabilization path,
- TOI is not allowed to report a fictitious negative time,
- the solver must remain finite,
- the anomaly monitor must flag pathological deep overlap according to existing thresholds.

## 11. Restitution and friction at TOI

TOI contacts use the same material semantics as discrete contacts:

- restitution,
- static friction,
- dynamic friction,
- rolling resistance where applicable.

CCD is not allowed to silently force perfectly inelastic contact to make stability easier.

## 12. Sleeping and waking

A BULLET impact with a sleeping body must wake the affected island when the ordinary wake criteria are met.

A sleeping BULLET body does not need continuous sweeping until a wake condition exists.

A body switched from DISCRETE to BULLET at runtime takes effect before the next fixed step.

## 13. Joints and CCD

Joint constraints remain solved by the ordinary solver.

When a jointed BULLET body reaches TOI:

- the impact must not permanently detach or bypass the joint,
- the remaining sub-step must include the joint constraints,
- joint error must remain inside the v1.0 acceptance envelopes.

The engine is not required to solve a global exact multi-body TOI optimization problem.

## 14. Sensor interaction

BULLET bodies must not tunnel through mandatory sensor fixtures.

A continuous sensor crossing within one fixed step must create the same deterministic event lifecycle semantics as a discrete crossing:

- `BEGIN`,
- zero or more `STAY` events when overlap persists across later steps,
- `END`.

For a crossing that enters and exits entirely within one fixed step, the engine must emit a deterministic transient crossing representation documented by the implementation. The required product representation is a `BEGIN` followed by `END` ordered at the same fixed-step index with TOI fractions attached.

No physical impulse is generated.

## 15. CCD diagnostics

The Diagnostics view must expose at least:

- number of BULLET bodies,
- swept-tree candidates this step,
- TOI searches this step,
- TOI iterations this step,
- CCD sub-steps this step,
- earliest TOI fraction,
- CCD cap-hit count,
- continuous sensor crossings.

Debug drawing must optionally show:

- swept AABB,
- start pose outline,
- predicted end pose outline,
- impact pose outline,
- hit point,
- hit normal,
- numeric TOI fraction.

## 16. Solver Inspector integration

A contact created at TOI must be inspectable through the Solver Inspector.

Its capture must identify:

- contact origin = `TOI`,
- impact fraction,
- impact sub-step index,
- normal/tangent impulse,
- restitution contribution,
- remaining time fraction.

Inspector instrumentation must retain the non-interference guarantee.

## 17. Shape Cast product feature

The Sandbox must provide a Shape Cast / Sweep Query tool.

The query sweeps one query shape through a translation vector without modifying the world.

Supported query shapes:

- circle,
- rectangle,
- arbitrary convex polygon.

Shape Cast rotation during the query is out of scope for the interactive query tool; the starting orientation remains fixed during the sweep. This restriction does not weaken rigid-body CCD, which must account for body rotation.

## 18. Shape Cast inputs

A Shape Cast has:

- query shape geometry,
- start position,
- start angle,
- translation vector,
- collision category/mask/group filter,
- include/exclude sensors option,
- optional ignored body ID.

The interactive tool must support selecting an existing body as the query-shape source without moving that body.

## 19. Shape Cast result

Closest-hit Shape Cast is mandatory.

The result includes at least:

- hit / no hit,
- body ID,
- fixture/shape ID if separately modeled,
- hit fraction in `[0,1]`,
- world hit point,
- world hit normal,
- traveled distance,
- sensor flag.

All-hits Shape Cast is optional and may not substitute for closest-hit correctness.

## 20. Shape Cast visualization

The viewport must display:

- start shape outline,
- end ghost outline,
- sweep direction,
- hit shape pose at earliest impact,
- hit point and normal,
- numeric fraction and target body ID.

No-hit results must be visibly distinguishable from hit results.

## 21. Query non-interference

Running Shape Cast must not mutate:

- body transforms,
- velocities,
- sleep state,
- contacts,
- sensor lifecycle,
- broad-phase persistent proxies,
- replay state.

Repeated identical queries at the same canonical world state must return identical result fields.

## 22. Persistence and replay

Scene persistence stores body CCD mode.

Replay commands must cover:

- toggling body CCD mode,
- interactive Shape Cast configuration when it is part of a recorded verification workflow only if the query result affects recorded UI evidence; query-only commands need not alter physics state digest.

The physics state digest must include the CCD mode because it changes future simulation behavior.

## 23. Mandatory CCD automated cases

The following cases are release-blocking.

- **CCD-01** high-speed circle vs thin static wall; no tunneling.
- **CCD-02** high-speed rectangle vs thin static wall; no tunneling.
- **CCD-03** high-speed convex polygon vs thin static wall; no tunneling.
- **CCD-04** high-speed circle vs moving kinematic wall.
- **CCD-05** BULLET dynamic vs dynamic head-on impact.
- **CCD-06** off-center polygon impact produces finite angular response.
- **CCD-07** rotating thin rectangle sweep catches corner-first impact.
- **CCD-08** rotating convex polygon catches impact missed by center-only ray logic.
- **CCD-09** sensor thin-strip crossing emits deterministic BEGIN/END without impulse.
- **CCD-10** high restitution continuous impact rebounds with expected velocity envelope.
- **CCD-11** high friction continuous impact modifies tangential velocity within Coulomb bound.
- **CCD-12** BULLET wakes sleeping impacted body/island.
- **CCD-13** initially overlapping BULLET body uses finite penetration recovery.
- **CCD-14** multiple walls choose earliest TOI independent of insertion order.
- **CCD-15** two equal-fraction candidate impacts use documented deterministic tie-break.
- **CCD-16** toggling DISCRETE -> BULLET at runtime changes the next-step tunneling result.
- **CCD-17** toggling BULLET -> DISCRETE restores ordinary discrete semantics.
- **CCD-18** render cadence 30/60/jitter does not change checked CCD state digests.
- **CCD-19** same CCD fixture repeated five times produces identical digests and TOI ordering.
- **CCD-20** checkpoint before impact + restore reproduces identical TOI and post-impact digest.
- **CCD-21** replay containing CCD-mode change reproduces the original impact.
- **CCD-22** CCD solver instrumentation on/off has identical canonical physics digest.
- **CCD-23** 100 deterministic randomized high-speed seeds produce no NaN/Inf or safety-cap violation.
- **CCD-24** 10,000 continuous impacts complete without unbounded allocation growth.
- **CCD-25** CCD sub-step cap is exercised by an adversarial fixture and reported rather than hidden.
- **CCD-26** body connected by distance joint remains bounded through BULLET impact.
- **CCD-27** body connected by revolute joint remains bounded through BULLET impact.
- **CCD-28** collision filtering prevents CCD contact exactly as it prevents discrete contact.
- **CCD-29** group override semantics are honored by CCD candidate filtering.
- **CCD-30** continuous sensor crossing respects collision mask/group filtering.

## 24. Mandatory quantitative thin-wall fixture

`CCD-THIN-WALL-01` is a fixed release fixture:

- fixed timestep: `1/60 s`,
- circle radius: `0.10`,
- circle mass: `1.0`,
- initial x: `-2.0`,
- initial linear velocity: `+240.0` x,
- wall center x: `0.0`,
- wall thickness: `0.05`,
- wall height: `4.0`,
- gravity: zero,
- restitution: zero,
- friction: zero,
- body mode: BULLET.

Release acceptance requires:

- at least one valid TOI in `(0,1)`,
- body center may not appear on the far side of the wall without a resolved contact,
- no NaN/Inf,
- final speed <= initial speed + `1e-6`,
- maximum penetration after resolution <= `0.01` world units,
- repeated runs produce identical canonical state digest checkpoints.

A corresponding DISCRETE control fixture may tunnel and is used only to demonstrate that CCD mode is behaviorally meaningful; the control is not itself a release failure.

## 25. Mandatory rotating-body fixture

`CCD-ROT-01` uses a long thin rectangle with simultaneous translation and high angular velocity aimed so that an end-cap reaches a static obstacle before the center trajectory does.

Acceptance requires:

- collision is detected before end-of-step overlap,
- TOI is strictly less than 1,
- contact point lies on the swept body boundary within geometric tolerance,
- center-only ray oracle would be insufficient and therefore may not be the implementation path,
- no solver explosion after 300 subsequent steps.

## 26. Shape Cast automated cases

- **CAST-01** circle cast direct hit.
- **CAST-02** rectangle cast direct hit.
- **CAST-03** convex polygon cast direct hit.
- **CAST-04** no-hit sweep.
- **CAST-05** starts touching target and reports fraction 0 according to documented touching epsilon.
- **CAST-06** starts overlapping target and returns deterministic overlap result without NaN.
- **CAST-07** two targets returns nearest target.
- **CAST-08** nearest target is invariant to insertion order.
- **CAST-09** sensor excluded option skips sensor.
- **CAST-10** sensor included option may return sensor.
- **CAST-11** category/mask filter excludes target.
- **CAST-12** group filter override obeyed.
- **CAST-13** ignored body is not returned.
- **CAST-14** world translation metamorphic test preserves fraction/normal after translation adjustment.
- **CAST-15** 90-degree rotated scene/query produces correspondingly rotated hit normal.
- **CAST-16** 1,000 identical queries do not mutate state digest.
- **CAST-17** query at zoom levels 0.25x/1x/4x returns identical world-space result.
- **CAST-18** 100 deterministic randomized casts agree with a slower verification sweep oracle within tolerance.

## 27. CCD performance cases

`perfbench` must include:

### PERF-CCD-1000

- 1,000 bodies,
- at least 250 BULLET bodies,
- bounded world,
- at least 30 seconds simulated time,
- report swept candidates, TOI calls, mean/p95 iterations, cap hits, simulation time per fixed step.

### PERF-CCD-THIN-200

- 200 BULLET circles,
- array of thin static obstacles,
- deterministic seed,
- at least 20 seconds simulated time,
- no missed mandatory wall crossings.

Performance gates do not permit dropping collisions to meet timing.

## 28. E2E acceptance

Mandatory E2E workflows:

- **E2E-CCD-01** create a circle, enable BULLET, launch it at a thin wall, observe TOI debug overlay and non-tunneling result;
- **E2E-CCD-02** edit an existing body from DISCRETE to BULLET and verify persisted scene reload keeps the mode;
- **E2E-CCD-03** use Shape Cast tool from an existing body and inspect target ID/fraction/normal;
- **E2E-CCD-04** toggle sensor inclusion and collision filtering and observe result change;
- **E2E-CCD-05** capture a TOI contact in Solver Inspector;
- **E2E-CCD-06** record/replay a CCD impact and verify final digest equality.

## 29. Acceptance evidence

Required artifacts include:

- screenshot or frame sequence of `CCD-THIN-WALL-01`,
- screenshot of rotating-body TOI debug visualization,
- Shape Cast hit/no-hit screenshots,
- Solver Inspector capture of TOI contact,
- machine-readable CCD test report,
- `PERF-CCD-1000` report,
- deterministic replay report for CCD case,
- anomaly-monitor report showing no unexplained safety-cap violations.

## 30. Release blocking conditions

The release is BLOCKED if any of the following occur:

- a mandatory BULLET fixture tunnels,
- a required rotating-shape impact is missed,
- a sensor continuous crossing is silently lost,
- a Shape Cast mutates world state,
- TOI ordering is nondeterministic,
- sub-step cap hits are hidden,
- any CCD test contains NaN/Inf,
- a required CCD report/evidence artifact is missing,
- any mandatory `CCD-*` or `CAST-*` test fails or is skipped.

## 31. Definition of complete

CCD/TOI/Shape Cast is complete only when the production engine, UI, persistence, replay, diagnostics, tests, performance measurements, and evidence all satisfy this document and the global v1.0 release gates.
</file>
