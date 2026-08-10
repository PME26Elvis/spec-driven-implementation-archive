# 09 — Automated Testing, Physics Validation, E2E, Regression, and Performance

## 1. General requirement

Testing is a mandatory deliverable, not optional supporting work.

The submission must include executable automated tests that exercise the actual implementation.

Required test categories:

1. unit tests.
2. subsystem integration tests.
3. full-application E2E tests.
4. physics validation scenarios.
5. serialization/parser regression tests.
6. performance benchmarks.
7. developer-tool self-tests.

## 2. Test result contract

Each test executable or aggregate runner must provide:

- total tests/cases.
- passed count.
- failed count.
- skipped count.
- explicit failed case names.
- non-zero exit status if any required case fails.

Skipped mandatory cases count as not passing the release gate.

## 3. Unit test minimum areas

### Math

- vector arithmetic.
- dot/cross.
- normalization.
- rotation transforms.
- centroid and inertia math.

### Geometry

- AABB operations.
- convexity validation.
- winding normalization.
- self-intersection detection.

### Broad phase

- proxy lifecycle.
- tree query.
- balancing/invariants.
- candidate uniqueness.

### Narrow phase

- every shape pair class.
- SAT separation.
- manifold clipping.
- edge/vertex cases.

### Solver

- contact effective mass.
- normal impulse clamp.
- restitution.
- static/dynamic friction.
- rolling resistance.
- stabilization.
- warm starting.

### Joints

- distance joint.
- spring/damped distance joint.
- hinge anchors.
- hinge limits.
- hinge motor.
- mouse joint.

### Data

- JSON lexer/parser.
- string escape handling.
- number handling.
- serializer.
- scene validation.
- deterministic save ordering.
- CSV escaping.

### UI core

At minimum test pure/layout/state components where practical:

- hit testing.
- clipping intersection.
- focus routing.
- scroll coordinate mapping.
- animation interpolation end points.
- modal input gating.

## 4. Integration tests

Required integration cases:

- body creation updates broad-phase proxy.
- body deletion removes contacts and joints safely.
- geometry edit updates mass/inertia and proxy.
- collision filter edit removes/disallows contacts.
- scene load creates valid world.
- scene reset restores original runtime state.
- save/load round trip preserves scene definition.
- undo/redo restores physics editor state.
- sleeping island wakes from impact.
- debug overlay reads actual engine manifolds.

## 5. Full-application E2E requirements

E2E tests must exercise the actual application as a complete program, including real UI state transitions.

Required flows:

### E2E-01 Basic launch

- launch application.
- verify Sandbox is present.
- verify default starter scene contains required objects.
- Play then Pause.

### E2E-02 Create/edit/delete

- create circle.
- create rectangle.
- select body.
- edit material/property.
- verify engine/editor state changed.
- delete body.

### E2E-03 Convex polygon validation

- create valid convex polygon.
- attempt concave polygon.
- verify concave shape is rejected and no body is added.

### E2E-04 Save/reload

- modify scene.
- Save As.
- clear/new scene.
- reopen saved scene.
- verify body/joint/property counts and selected representative properties.

### E2E-05 Unsaved-change modal

- modify scene.
- initiate scene switch.
- verify modal opens with blur/dim.
- Cancel and verify scene remains.

### E2E-06 Debug contact

- load collision manifold scene.
- run until contact.
- verify debug state exposes actual manifold point and normal.

### E2E-07 Bridge interaction

- load suspension bridge.
- apply/drag/launch a dynamic body into bridge.
- verify multiple linked bodies move while constraints remain connected.

### E2E-08 UI navigation

- navigate Sandbox→Scenes→Diagnostics→About.
- scroll a long view.
- verify nav frosted-state metric/visual state changes.
- verify capsule active indicator changes through animation state.

### E2E-09 Undo/redo

- create body.
- change property.
- undo twice.
- redo twice.
- verify exact editor-state sequence.

### E2E-10 Close with unsaved changes

- modify scene.
- request window close.
- verify Save/Discard/Cancel options.
- Cancel keeps process open and scene intact.

The mechanism used to drive the E2E sequence is implementation-defined, but tests must be reproducible and part of the submitted project.

## 6. Physics validation suite

Physics validation must use deterministic scene fixtures and generate a machine-readable report.

### VAL-01 Free fall

Configuration:

- single dynamic body.
- no collision during measurement.
- known gravity.
- zero damping.
- initial velocity zero.

Compare numerical position/velocity against analytic constant-acceleration reference over 1 second.

Acceptance:

- velocity relative/absolute error consistent with semi-implicit Euler and fixed step.
- position error ≤ **1.0% of total fall distance** at default 1/120 s step.

### VAL-02 Elastic head-on collision

- two dynamic bodies.
- no gravity.
- no damping.
- no friction.
- restitution 1.
- known masses and initial velocities.

Acceptance:

- total linear momentum relative error ≤ **0.5%**.
- total kinetic energy relative error ≤ **2.0%** after transient contact resolves.

### VAL-03 Off-center angular collision

Acceptance:

- total angular momentum about chosen fixed origin relative error ≤ **2.0%** in isolated configuration.

### VAL-04 Resting contact

- one box on static floor.
- run 10 simulated seconds.

Acceptance after settling:

- no penetration deeper than **0.02 world units** sustained for >0.5 s.
- vertical speed remains below sleep threshold when settled.
- body sleeps when sleeping enabled.

### VAL-05 Five-block tower

- five identical boxes vertically stacked.
- run 15 simulated seconds without external disturbance.

Acceptance:

- no box falls through floor.
- no NaN/infinity.
- no box center drifts horizontally more than **0.15 box widths** from its initial tower axis before disturbance.
- tower reaches sleeping/stable state.

Then apply a deterministic lateral impulse to top or middle block.

Acceptance:

- tower reacts and can topple naturally.
- contacts remain finite.

### VAL-06 Friction ramp

Use multiple bodies/material settings on an incline.

Acceptance must demonstrate:

- sufficiently high friction can hold a body at rest for the chosen slope.
- lower friction permits sliding.
- friction direction opposes relative tangent motion.

### VAL-07 Restitution comparison

Identical balls with different restitution dropped from same height.

Acceptance:

- higher-restitution body reaches a higher first rebound than lower-restitution body.
- restitution 0 body does not exhibit a large bounce.

### VAL-08 Distance joint

Run pendulum/linked pair for 20 simulated seconds.

Acceptance:

- anchor distance error RMS ≤ **1% of target length** under default solver settings.
- maximum error ≤ **5%** excluding first 0.25 s of severe initial correction.

### VAL-09 Hinge joint

Run a two-body hinge for 20 seconds.

Acceptance:

- anchor separation RMS ≤ **0.02 world units** for a scene with ~1-unit bodies.
- enabled angular limits are not persistently violated by > **0.05 rad**.

### VAL-10 Suspension bridge impact

A bridge of at least 12 dynamic segments is anchored at both ends and struck by a falling/propelled body.

Acceptance:

- all intended joint connections remain valid for 20 simulated seconds.
- no NaN/infinity.
- at least three bridge segments show measurable displacement in response to impact.
- bridge subsequently damps/settles rather than gaining energy without bound.

### VAL-11 Sleeping island

A mixed stack of at least 20 bodies settles.

Acceptance:

- majority of eligible bodies sleep within 20 simulated seconds.
- measured physics work/awake-body count drops after sleep.
- applying an impulse wakes the impacted connected region.

### VAL-12 Broad-phase oracle

Generate at least 100 deterministic randomized scenes across multiple seeds, each with 50–500 AABBs.

Acceptance:

- every true brute-force overlap pair is present in dynamic-tree candidate result.
- duplicate candidate pairs are absent after pair normalization.

## 7. Regression fixtures

At least 20 deterministic regression fixtures must be included across:

- collision geometry.
- scene parsing.
- solver stability.
- joints.
- UI state transitions.

Each fixture has a stable ID/name and documented expected invariant/result.

## 8. Performance benchmark

Benchmark mode must measure actual engine execution without replacing algorithms with mocks.

Required scenes:

### PERF-100

- 100 active mixed bodies.

### PERF-1000

- 1000 mixed bodies, with a meaningful fraction potentially interacting.

### PERF-BROAD-5000

- 5000 bodies distributed so broad phase is exercised without all-pairs overlap.

Report at least:

- median physics step time.
- p95 physics step time.
- broad-phase median.
- narrow-phase median.
- solver median.
- average candidate pairs.
- average active contacts.

## 9. Performance release thresholds

Performance is evaluated on the machine used for acceptance; thresholds emphasize gross algorithmic failures rather than hardware ranking.

Mandatory:

- `PERF-100`: median physics step < **4 ms**.
- `PERF-1000`: median physics step < **25 ms**.
- `PERF-BROAD-5000`: broad-phase candidate-generation median < **20 ms** when fixture is designed for sparse overlap.
- no benchmark may show production broad phase performing an unconditional N² body-pair narrow-phase loop.

If hardware is unusually constrained, timing may be recorded as conditional evidence, but algorithmic broad-phase oracle and candidate counts remain mandatory.

## 10. Long-run soak test

Run at least one mixed scene for **120 simulated seconds**.

Required result:

- no crash.
- no NaN/infinity.
- no invalid joint/contact references.
- no unbounded monotonic energy growth without external energy input.
- memory usage does not grow without bound due to leaked contacts/proxies/history.

## 11. Test evidence retention

The final submission must include latest generated summaries for:

- unit/integration tests.
- E2E.
- physics validation.
- performance benchmark.
- dev-tool self-tests.

Generated evidence is not a substitute for test source code; both are required.

## 20. External force and impulse verification

The automated suite must implement the numerical cases in `14_FORCE_TRAJECTORY_TOOLS.md` Section 17, including center impulse, off-center impulse, equal/opposite impulse, constant force acceleration, camera invariance, exact force-step count, exactly-once impulse behavior, and paused impulse semantics.

These tests must inspect engine state numerically. A screenshot or visually plausible movement is not a substitute.

## 21. Motion recorder and graph verification

The automated suite must implement `14_FORCE_TRAJECTORY_TOOLS.md` Section 18 and the E2E workflows in Section 19.

At minimum it must verify fixed-step sample counts, sample/state consistency, pause behavior, deterministic export ordering, discontinuity handling, graph auto-range edge cases, nearest-sample lookup, and camera-correct world-space trails.

The graph's data path must be tested against the recorder so a visually rendered line backed by synthetic/example data cannot pass.

## 22. Advanced numerical and metamorphic validation

The release-blocking advanced cases `VAL-13` through `VAL-40` are defined in `15_ADVANCED_PHYSICS_VALIDATION.md`.

They are mandatory and include:

- force-free translation/rotation.
- constant torque.
- projectile analytic trajectory.
- analytic mass/inertia checks.
- unequal-mass restitution reference.
- action/reaction consistency.
- numeric static/dynamic friction behavior.
- dissipative energy non-growth.
- high mass-ratio stability.
- deep-overlap recovery.
- tangency/grazing robustness.
- translation and rotation invariance.
- insertion-order/ID permutation.
- render-cadence independence of fixed-step physics.
- repeated-run determinism.
- time-step convergence.
- hinge motor torque limiting.
- damped spring response.
- long-chain constraint stress.
- sleep/wake threshold boundaries.
- dynamic-tree mutation oracle.
- collision-filter live updates.
- polygon representation invariance.
- thin/high-aspect-ratio geometry.
- moderate geometry-scale robustness.
- deterministic randomized finite-state fuzz.
- manifold geometric validity.

A final validation summary must report base, force/trajectory, and advanced groups separately.

## 23. Regression fixture count for v1.0

The minimum deterministic regression corpus is **60 fixtures**, superseding the earlier 20/40-fixture minima. The twelve Golden scenarios are additional and do not count toward these 60 regression fixtures.

The distribution and required families are defined in `15_ADVANCED_PHYSICS_VALIDATION.md` Section 36.

## 24. Additional performance and lifecycle validation

The final report must also include:

- `PERF-DENSE-500`.
- `PERF-JOINT-200`.
- `PERF-CHURN-5000`.
- repeated world/body/joint lifecycle stress across at least 250 cycles.

Their required workload semantics are defined in `15_ADVANCED_PHYSICS_VALIDATION.md` Sections 34–35.

These additional workloads primarily detect pathological scaling, tree-update cost, joint-solver cost, stale references, and lifecycle leaks. They do not replace the absolute timing gates already defined for `PERF-100`, `PERF-1000`, and `PERF-BROAD-5000`.

## 25. Test-oracle integrity

The verifier must satisfy `15_ADVANCED_PHYSICS_VALIDATION.md` Section 37.

In particular:

- expected analytic values must not come from the production update routine being tested.
- the broad-phase oracle must remain independent and brute-force.
- every threshold comparison must explicitly reject non-finite metrics.
- deterministic randomized failures must report their seed and failing step.
- a missing required metric is a test failure, not a skipped comparison.

## 26. Solver Inspector verification

All `SINSP-01` through `SINSP-20` cases and `E2E-SI-01` through `E2E-SI-06` defined in `18_SOLVER_INSPECTOR.md` are mandatory release-blocking tests.

The most important cross-cutting acceptance properties are:

- trace values are the real production solver values;
- per-iteration accumulation/clamping is correct;
- warm-start continuity is observable;
- contact/joint stable identity is safe across lifecycle changes;
- tracing is deterministic;
- tracing and Inspector visibility do not alter physics state digests;
- paused Single Step captures exactly one fixed step;
- trace export parses and validates;
- repeated capture remains bounded and safe.

A Solver Inspector test group that omits instrumentation non-interference or deterministic trace validation is incomplete.

## 27. v1.0 continuous collision and Shape Cast verification

All `CCD-01` through `CCD-30` and `CAST-01` through `CAST-18` cases in `19_CCD_TOI_SHAPE_CAST.md` are mandatory.

The suite must include quantitative thin-wall and rotating-body fixtures, deterministic repeat/replay checks, CCD non-interference instrumentation, adversarial cap handling, sensor crossing, jointed-body impact, and randomized high-speed seeds.

## 28. v1.0 filtering verification

All `COLF-01` through `COLF-24` cases in `20_COLLISION_MATRIX.md` are mandatory, including runtime contact invalidation, sensor lifecycle, group override, query/CCD semantic consistency, persistence, Matrix E2E, undo/redo, and stress edits.

## 29. v1.0 Replay Timeline verification

All `TLN-01` through `TLN-28` cases in `21_REPLAY_TIMELINE.md` are mandatory.

Seeking and stepping backward must compare reconstructed canonical state digests against uninterrupted forward references. Timeline marker correctness must compare against production event logs rather than UI state.

## 30. Golden Scenario integrated verification

All twelve scenarios in `22_GOLDEN_SCENARIO_ACCEPTANCE.md` are mandatory and release-blocking.

They supplement rather than replace unit/integration/validation/E2E tests.

The aggregate result must be exactly 12/12 PASS before the release can pass.

## 31. Mandatory test inventory audit

The release runner must hold a registry/manifest of all mandatory test IDs and compare expected IDs with discovered/executed result IDs.

Missing, duplicate, skipped, flaky, timeout, malformed-result, or unknown required-case states block release as defined by `23_SCOPE_FREEZE_TRACEABILITY_AUDIT.md`.

## 32. Windows-native platform verification

The Windows sibling additionally requires every `WIN-01` through `WIN-30` and `E2E-WIN-01` through `E2E-WIN-08` case in `26_WINDOWS_PLATFORM_VALIDATION.md`.

These cases are release blocking and are additional to the existing 284 non-platform named cases.

The Windows platform suite must specifically catch:

- message/presentation cadence accidentally driving variable-step physics;
- DIB orientation/stride/channel errors;
- stale paint content after invalidation/restore;
- unsafe repeated framebuffer resize;
- DPI double-scaling or missing-scaling in hit testing;
- DPI-dependent world-force/impulse mapping;
- stuck mouse capture or held keys after focus/capture loss;
- UTF-8/UTF-16 and surrogate-pair corruption;
- IME duplicate commit/cancellation/focus-transfer errors;
- Unicode path loss through ANSI APIs;
- safe-save replacement/locking failure misreporting;
- native-close bypass of the custom unsaved-change modal;
- prohibited native/high-level UI/rendering substitutions;
- Windows platform state changing canonical physics/replay behavior.

The final Windows named-case total is governed by `24_MANDATORY_TEST_REGISTRY.md`.
