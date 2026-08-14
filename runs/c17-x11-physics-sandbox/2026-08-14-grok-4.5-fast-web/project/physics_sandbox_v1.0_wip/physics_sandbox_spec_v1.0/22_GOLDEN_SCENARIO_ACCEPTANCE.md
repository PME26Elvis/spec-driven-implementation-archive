# Golden Scenario Acceptance Suite

Version: **1.0**
Status: **Normative**

## 1. Purpose

The Golden Scenario Acceptance Suite is the final integrated defense against a physics engine that passes isolated unit tests but behaves visibly incorrectly in realistic scenes.

Exactly twelve mandatory reference scenarios combine engine, solver, UI-facing state, replay, diagnostics, anomaly detection, and acceptance evidence.

A v1.0 release requires **12 / 12 GOLDEN PASS**.

## 2. General golden rules

Every golden scenario has:

- immutable fixture ID and version,
- deterministic initial scene,
- fixed timestep,
- deterministic seed where randomness is used,
- fixed run duration or step count,
- required digest checkpoints,
- numeric acceptance metrics,
- anomaly/safety caps,
- required evidence frames,
- replay artifact,
- machine-readable result record.

Implementations may not edit a golden fixture to make their engine pass.

## 3. Golden fixture integrity

Each golden scene file must have a content digest recorded in the acceptance manifest.

`releasecheck` verifies the digest before running it.

A changed fixture is a release failure until the task package itself is versioned; a submission cannot self-authorize new thresholds or fixture values.

## 4. Common execution settings

Unless a scenario overrides them:

- fixed timestep = `1/120 s`,
- solver velocity iterations = 12,
- solver position/stabilization iterations or equivalent = documented production default,
- gravity = `(0, -9.81)`,
- simulation runs headless for numeric acceptance and may also be rendered for evidence,
- debug rendering must not alter physics digest.

## 5. Common hard failures

Any scenario immediately fails on:

- NaN or Infinity,
- state outside global safety caps,
- unresolved engine assertion,
- missing required contact/joint/sensor event,
- digest nondeterminism across repeated identical runs,
- crash/hang/timeout,
- missing mandatory report field.

## 6. GOLD-01 Free Fall Analytic

Scene:

- one dynamic circle, radius `0.25`, mass `1`,
- start `(0, 10)`, velocity `(0,0)`,
- no colliders,
- gravity `(0,-9.81)`,
- run 120 fixed steps at `dt=1/120`.

Acceptance:

- x drift absolute <= `1e-9`,
- final vertical velocity within integrator-specific semi-implicit Euler reference tolerance `1e-9`,
- final y within `1e-8` of the project's discrete analytic reference for semi-implicit Euler,
- zero collision contacts,
- no angular velocity generated,
- digest exact across five repeats.

Evidence:

- trajectory plot y(t), vy(t),
- machine comparison against analytic/discrete reference.

## 7. GOLD-02 Elastic Momentum Collision

Scene:

- two circles on x-axis,
- masses 1 and 2,
- zero gravity/friction/rolling resistance,
- restitution 1,
- initial velocities chosen for one clean isolated collision.

Acceptance:

- linear momentum relative error <= `1e-6`,
- kinetic energy relative error <= `5e-5`,
- y velocity and angular velocity remain <= `1e-8`,
- exactly one physical collision episode,
- no persistent penetration > `0.002`.

Evidence:

- before/after momentum table,
- Solver Inspector contact trace.

## 8. GOLD-03 Friction Ramp

Scene:

- rectangular dynamic block on a 25-degree static ramp,
- deterministic static/dynamic friction coefficients selected so one reference block remains at rest and a second otherwise-identical block with lower friction slides,
- run 10 seconds.

Acceptance stationary block:

- center displacement along ramp <= `0.02`,
- speed during final 2 seconds <= `0.02`.

Acceptance sliding block:

- moves downslope by >= `1.0`,
- no upslope energy injection,
- measured tangential acceleration agrees with friction reference within 5% after transient.

Evidence:

- side-by-side trajectories,
- friction impulse Solver Inspector capture.

## 9. GOLD-04 Five-Block Tower

Scene:

- five equal axis-aligned boxes stacked vertically on static floor,
- no initial velocity,
- ordinary gravity,
- moderate friction, zero restitution,
- run 20 seconds.

Acceptance:

- all five remain in correct vertical order,
- no box center leaves horizontal interval `[-0.20, 0.20]`,
- maximum penetration after first 3 seconds <= `0.015`,
- maximum linear speed in final 2 seconds <= `0.05`,
- maximum angular speed in final 2 seconds <= `0.08 rad/s`,
- total kinetic energy in final 2 seconds <= `0.02`,
- at least four boxes are sleeping by final step,
- no anomaly sentinel event.

Evidence:

- start/mid/final screenshots,
- velocity/energy envelope report.

## 10. GOLD-05 Pyramid Stack Perturbation

Scene:

- 15 boxes arranged 5-4-3-2-1 on static floor,
- settle for 8 seconds,
- apply defined horizontal impulse to top box,
- run to 20 seconds.

Acceptance before impulse:

- no box speed > `0.08` during final pre-impulse second.

After impulse:

- motion propagates physically through contact network,
- no body exceeds global anomaly caps,
- no body penetrates floor > `0.02`,
- final state is deterministic across five repeats,
- total mechanical energy after impulse may dissipate but must not exceed immediate post-impulse energy by > 2% once external impulse is complete.

Evidence:

- frame sequence around impulse,
- energy plot,
- replay artifact.

## 11. GOLD-06 Pendulum Constraint

Scene:

- one dynamic circle/box connected by revolute joint to static anchor,
- arm length `2.0`,
- initial angle 30 degrees,
- gravity on,
- low friction/damping,
- run 20 seconds.

Acceptance:

- anchor distance error RMS <= `0.005`,
- maximum distance error <= `0.02`,
- no secular energy growth > 2% over the initial mechanical energy envelope,
- oscillation remains finite and periodic-looking,
- no joint break/detach.

Evidence:

- angle(t) graph,
- joint-error graph,
- Solver Inspector joint trace.

## 12. GOLD-07 Motorized Revolute Joint

Scene:

- dynamic rectangular arm on revolute joint,
- gravity zero,
- motor target `2 rad/s`,
- finite torque cap,
- run 8 seconds.

Acceptance:

- reaches within 5% of target speed after startup where torque cap permits,
- reported motor impulse never exceeds per-step cap + `1e-8`,
- joint anchor drift max <= `0.01`,
- motor does not inject NaN or unbounded angular speed,
- Solver Inspector requested/capped/applied motor values agree with solver state.

Evidence:

- angular velocity plot,
- motor-clamp Inspector capture.

## 13. GOLD-08 Suspension Bridge Impact

Scene:

- at least 12 dynamic bridge deck segments,
- linked by revolute and/or distance constraints according to built-in bridge fixture,
- anchored at both ends,
- dynamic impactor block falls onto bridge at defined position,
- run 25 seconds.

Acceptance:

- RMS joint positional error <= `0.02`,
- maximum joint error <= `0.08`,
- no segment exceeds global velocity/angular safety caps,
- no joint spontaneously disappears,
- bridge oscillation energy after impact does not grow monotonically for more than 3 seconds without external work,
- all bodies remain inside fixture safety region,
- replay across five runs is digest exact at required checkpoints.

Evidence:

- before/impact/oscillation/final frames,
- joint-error graph,
- Timeline bookmark at peak impact.

## 14. GOLD-09 Ragdoll / Linked Body Drop

Scene:

- linked multi-body ragdoll-like structure using circles/rectangles/convex shapes and revolute constraints,
- dropped onto static ground and one obstacle,
- run 20 seconds.

Acceptance:

- no joint max error > `0.10`,
- no body separation inconsistent with joint limits,
- maximum penetration after settling transient <= `0.03`,
- no energy explosion/anomaly,
- final 3-second average kinetic energy lower than first 3-second post-impact average,
- at least half of eligible bodies sleep by final step.

Evidence:

- drop/contact/final frames,
- anomaly report,
- representative joint Solver Inspector trace.

## 15. GOLD-10 Sensor Course and Filtering

Scene:

- one moving body passes through three sensor regions,
- one allowed by masks,
- one excluded by masks,
- one initially allowed then disabled while overlapping,
- no physical obstacles except boundaries.

Acceptance:

- expected sensor 1 emits deterministic BEGIN/STAY/END sequence,
- filtered sensor 2 emits no lifecycle events,
- sensor 3 emits BEGIN then one END at filter change and no later STAY,
- body trajectory/velocity is identical within `1e-9` to a control scene with sensors removed,
- Timeline markers exactly match event log.

Evidence:

- event table,
- Timeline screenshot,
- control-vs-sensor digest comparison.

## 16. GOLD-11 High-Speed CCD Thin Wall

Uses `CCD-THIN-WALL-01` from `19_CCD_TOI_SHAPE_CAST.md`.

Additional golden acceptance:

- no tunneling,
- deterministic TOI fraction across five runs,
- TOI contact visible in Solver Inspector,
- replay seek to step immediately before impact then Step Forward reproduces same TOI/contact digest,
- Shape Cast of same circle path identifies the wall at a compatible earliest fraction within `5e-4` after accounting for query-vs-dynamic semantics.

Evidence:

- TOI debug view,
- Solver Inspector trace,
- Timeline impact marker,
- Shape Cast result panel.

## 17. GOLD-12 Mixed Stress Playground

Scene contains at minimum:

- >= 150 dynamic bodies,
- circles, rectangles, convex polygons,
- >= 8 joints,
- >= 5 sensors,
- >= 10 BULLET bodies,
- collision categories/masks/groups,
- one kinematic platform,
- sleeping/waking,
- periodic deterministic impulses from fixture script/input stream,
- motion recording for selected bodies.

Run:

- 60 seconds simulated time,
- deterministic seed fixed by task package.

Acceptance:

- no NaN/Inf,
- no global safety-cap violation,
- no missed mandatory invariant monitor assertion,
- no active joint error > global mixed-stress cap `0.15`,
- maximum unresolved penetration <= `0.05`,
- body count and joint count remain expected unless fixture command explicitly changes them,
- final and periodic digests identical across three full repeats,
- replay of captured command stream matches original digests,
- diagnostics counters remain internally consistent,
- no memory growth classified as leak by project soak policy.

Evidence:

- overview screenshot,
- 60-second numeric report,
- digest table,
- performance summary,
- anomaly sentinel zero-failure report.

## 18. Golden digest checkpoints

Each scenario defines at least:

- initial digest,
- 25% duration digest,
- 50% duration digest,
- 75% duration digest,
- final digest.

Event-focused scenarios additionally record digests immediately before and after the defining event where deterministic command timing permits.

The submission generates its own reference digest on the first canonical run and immediately verifies repeated canonical runs against it in the same acceptance execution. Hard-coded digest constants from another implementation are not required because floating-point implementation details may differ while still meeting the normative numerical behavior.

## 19. Cross-build determinism expectation

Exact digest equality is mandatory across repeated runs of the same produced build/configuration.

Bit-identical digests across different compiler versions or machines are not required unless the implementation claims that stronger property.

Numerical metric envelopes are the cross-environment acceptance mechanism.

## 20. Golden evidence capture points

Each golden scenario declares named capture points. Evidence tooling must produce an index mapping:

- scenario ID,
- capture point name,
- fixed-step index,
- screenshot/frame artifact,
- associated numeric report,
- replay artifact.

The method used to capture the desktop image/video is not prescribed by this package.

## 21. Golden automation

`releasecheck` must run all twelve golden scenarios automatically for numeric acceptance.

Human-readable visual evidence may be captured separately, but absence of the required evidence blocks release.

No golden test may require a human to manually decide whether physics 'looks right' as the primary pass/fail mechanism.

## 22. Golden result schema

Each result contains at least:

- `scenario_id`,
- `fixture_digest`,
- `status`,
- `steps_run`,
- `seed`,
- `metrics`,
- `thresholds`,
- `digest_checkpoints`,
- `anomalies`,
- `reproduction_replay`,
- `evidence_refs`,
- failure reason if not PASS.

## 23. Failure reproducibility

On any golden failure, the acceptance system must retain:

- exact fixture,
- seed,
- replay/command stream,
- first failing step if known,
- nearest checkpoint,
- actual vs threshold metrics,
- relevant body/joint/contact IDs,
- recent fixed-step state history,
- anomaly data,
- Solver Inspector trace when solver-related and capturable.

## 24. Golden suite anti-cheat requirements

Prohibited:

- detecting a golden fixture ID and changing solver parameters only for that fixture,
- suppressing anomaly checks in golden mode,
- using relaxed physics settings for evidence screenshots that differ from numeric run,
- editing fixture geometry after integrity check,
- replacing engine output with prerecorded trajectories,
- skipping a scenario and reporting aggregate PASS.

## 25. Golden suite report

The final human-readable section must include exactly one summary table with all 12 IDs and PASS/BLOCKED status.

Top-level release status may be PASS only if all 12 are PASS.

## 26. Required repeated-run policy

Scenarios 01-11 run at least 5 deterministic repeats for digest stability where their individual section requires it or when selected by deterministic acceptance profile.

GOLD-12 runs at least 3 full repeats because of duration.

A nondeterministic digest is a release blocker even when scalar metric thresholds pass.

## 27. Visual sanity relationship

The numeric suite is designed to catch common visible failures:

- exploding stacks,
- jittering sleepers,
- rapidly growing joint error,
- tunneling,
- energy creation,
- filtered contacts still acting,
- sensors affecting motion,
- solver motor overshoot,
- replay state drift.

Visual evidence remains required because UI/rendering correctness cannot be inferred from physics metrics alone.

## 28. Complete condition

The Golden Scenario Acceptance Suite is complete only when fixture integrity, automated metrics, deterministic replays, evidence index, reproduction artifacts, and the 12/12 aggregate gate are operational through the normal release acceptance system.
</file>
