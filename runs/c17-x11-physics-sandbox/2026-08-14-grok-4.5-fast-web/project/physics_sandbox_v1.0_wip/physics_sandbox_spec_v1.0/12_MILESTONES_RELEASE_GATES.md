# 12 — Milestones and Release Gates

## 1. General rule

Milestones describe incremental acceptance boundaries. They do not reduce the final requirements.

A later milestone may replace an intentionally simple early implementation only when the final implementation satisfies all final engineering constraints.

## 2. Milestone 0 — Project skeleton and deterministic core tests

Required:

- source/module structure established.
- math library implemented.
- test runner implemented.
- fixed-step world loop implemented without final collisions.
- software framebuffer can open/present in X11 window.

Gate:

- math tests pass.
- deterministic fixed-step smoke test passes.
- application opens and closes cleanly.

## 3. Milestone 1 — Free fall and boundary interaction

Required:

- circles and rectangles visible in X11 window.
- dozens of bodies can fall under gravity.
- world boundaries prevent escape.
- final milestone state must route boundary response through real contact solver once available.

Visual acceptance:

- at least 20 mixed circles/rectangles move naturally under gravity.
- bodies interact with all four configured world boundaries.

## 4. Milestone 2 — Precise collision signal and contact visualization

Required:

- dynamic AABB-tree broad phase.
- circle-circle.
- circle-polygon.
- polygon-polygon SAT.
- reference/incident clipping manifold.
- red crosshair at each contact point.
- contact normal overlay.
- collision-highlight body outline.

Gate:

- broad-phase oracle passes.
- collision unit tests pass.
- visual evidence includes one-contact and two-contact manifold examples.

## 5. Milestone 3 — Stable rigid-body contacts and stack collapse

Required:

- sequential impulse contact solver.
- restitution.
- Coulomb friction.
- rolling resistance.
- warm starting.
- Baumgarte stabilization.
- sleeping/waking.

Acceptance scene:

- stable five-block tower.
- deterministic external lateral impulse causes plausible disturbance/topple.

Gate:

- `VAL-02` through `VAL-07` relevant cases pass.
- tower visual evidence exists.
- no persistent floor penetration beyond tolerance.

## 6. Milestone 4 — Joints, mouse dragging, bridge and ragdoll

Required:

- distance joint.
- hinge/revolute joint.
- hinge limits.
- hinge motor.
- mouse joint.
- joint debug overlays.
- constraint warm starting.

Acceptance:

- user can click/drag dynamic body physically while simulation runs.
- suspension bridge has at least 12 dynamic segments.
- impact causes realistic distributed response.
- linked-body/ragdoll scene remains connected and stable.

Gate:

- `VAL-08` through `VAL-10` pass.
- bridge and linked-body evidence exists.

## 7. Milestone 5 — Complete editor and persistence

Required:

- all creation tools.
- selection.
- inspector.
- geometry editing.
- collision filtering.
- undo/redo.
- camera pan/zoom/fit.
- save/load/revert/new scene.
- custom JSON parser/serializer.
- transactional load.
- safe save.
- trajectory/statistics export.

Gate:

- all scene-format tests pass.
- save/reload E2E passes.
- concave polygon rejection E2E passes.
- dirty-state modal E2E passes.

## 8. Milestone 6 — Modern custom UI polish

Required:

- reusable custom widgets.
- hover elevation.
- click ripple.
- border glow.
- capsule sliding indicator.
- animated panel collapse.
- modal scale+opacity transition.
- progressive backdrop dim+blur.
- dynamic frosted nav tied to scroll.
- responsive layout.

Gate:

- required UI screenshots exist.
- required animation recordings/frame sequences exist.
- E2E navigation/scroll/modal cases pass.

## 9. Milestone 7 — Force/impulse interaction and live motion analysis

Required completion:

- distinct Apply Force and Apply Impulse tools.
- center-of-mass and off-center application.
- numeric vector control and world-space gesture preview.
- correct one-shot impulse and per-fixed-step sustained force semantics.
- transient vector indicators/debug history.
- fixed-step Motion Analysis recorder.
- world-space trajectory trails.
- custom-rendered readable time-series graph.
- multi-body recording and graph legend.
- history navigation and sample cursor/readout.
- recorder-backed trajectory CSV export.
- force/impulse numerical verification and recorder/E2E tests.

Milestone gate:

- center/off-center impulse tests pass.
- constant force test passes.
- paused impulse does not advance simulation.
- camera zoom does not change interaction physics.
- recorder sample count/state consistency tests pass.
- screenshot/recording evidence demonstrates the interactive tools, trails, and graph.

## 10. Milestone 8 — Diagnostics, developer tools, performance, evidence

Required:

- Diagnostics view.
- debug overlays.
- `locscan`.
- `fixturegen`.
- `scenecheck`.
- `physverify`.
- `perfbench`.
- complete automated suite.
- complete evidence index.

Gate:

- dev-tool self-tests pass.
- base, force/trajectory, and advanced physics validation groups pass.
- deterministic fuzz and metamorphic validation groups pass.
- repeated lifecycle stress passes.
- E2E suite passes.
- performance thresholds pass or are explicitly recorded under allowed hardware exception while algorithmic gates pass.
- soak test passes.

## 11. Release Gate A — Functional completeness

Every mandatory product control must be implemented and connected to real state.

Failure examples:

- button has no behavior.
- property field does not alter body.
- scene card cannot open scene.
- export button creates no correct file.

Any such failure blocks release.

## 12. Release Gate B — Algorithm authenticity

Blocks release if any prohibited substitute is detected, including:

- O(N²) production broad phase replacing dynamic tree.
- AABB-only polygon collision.
- fake contact points.
- teleporting mouse drag during running simulation.
- direct-transform “joint”.
- external physics/UI library.
- placeholder test reports.

## 13. Release Gate C — Automated correctness

Release requires:

- zero failing mandatory unit tests.
- zero failing mandatory integration tests.
- zero failing mandatory E2E tests.
- zero failing mandatory physics validations.
- zero failing parser/scene regressions.
- zero failing dev-tool self-tests.

Skipped required tests count as failures for this gate.

## 14. Release Gate D — Stability

Release requires:

- no crash in required E2E suite.
- no NaN/infinity in validation/soak scenes.
- no invalid persistent contact/joint references.
- five-block stack stable before disturbance.
- bridge remains constrained during required interval.
- sleeping validation passes.

## 15. Release Gate E — Persistence safety

Release requires:

- deterministic valid scene round trip.
- invalid load is transactional.
- failed save does not claim success.
- unsaved changes confirmation behavior works.

## 16. Release Gate F — UI/UX completeness

Release requires visual/animation evidence for every mandatory custom effect.

A visually plain X11 debug window with correct physics does not pass this gate.

## 17. Release Gate G — Evidence completeness

Release requires `evidence/INDEX.md` mapping every gate to evidence.

Missing required evidence blocks release even when source appears to contain the feature.


## 18. Release Gate H — Numerical quality and invariance

Release requires all mandatory requirements in `15_ADVANCED_PHYSICS_VALIDATION.md`.

In particular, release is blocked by any of the following:

- any failing or skipped `VAL-13` through `VAL-40` case.
- non-finite metric accepted as passing.
- failed translation/rotation metamorphic comparison.
- failed same-input determinism check.
- time-step convergence that becomes worse as `dt` is halved in the required analytic fixture.
- dynamic AABB-tree mutation oracle mismatch.
- deterministic fuzz seed that produces non-finite or invalid live references.
- missing advanced performance/lifecycle reports.

Continuous collision detection is mandatory for BULLET bodies under Release Gate J. DISCRETE control fixtures may exhibit tunneling, but mandatory BULLET fixtures may not.

## 19. Milestone 9 — Solver Inspector and solver observability

Completion requires:

- contact and joint selection from viewport and deterministic lists;
- production-backed live constraint fields;
- stable pin/invalidation lifecycle;
- one-fixed-step capture;
- warm-start visibility;
- per-iteration contact/joint trace;
- friction/restitution/stabilization clamp diagnostics;
- joint motor/limit diagnostics;
- iteration table and graph;
- deterministic JSON trace export;
- `solvertrace` developer tool;
- all `SINSP-*` and `E2E-SI-*` cases passing.

## 20. Release Gate I — Solver observability and non-interference

Release is blocked unless:

- the Solver Inspector group is PASS;
- trace capture is deterministic;
- Inspector instrumentation does not alter canonical physics state digests;
- no required trace value is fabricated/recomputed from a separate simplified solver;
- contact/joint lifecycle invalidation is safe;
- trace export schema validation passes;
- capture stress passes;
- required Solver Inspector acceptance evidence exists.

## 21. Milestone 10 — Continuous collision and swept queries

Completion requires:

- DISCRETE/BULLET body mode;
- swept Dynamic AABB Tree candidate generation;
- rotationally aware TOI search;
- bounded deterministic CCD sub-stepping;
- sensor continuous crossing;
- Shape Cast for circle/rectangle/convex polygon;
- CCD/Shape Cast diagnostics, persistence, replay, Solver Inspector integration;
- all mandatory `CCD-*` and `CAST-*` tests and evidence.

## 22. Release Gate J — CCD / TOI / Shape Cast

BLOCKED if any mandatory `CCD-*` or `CAST-*` case fails/skips, required BULLET tunneling occurs, TOI ordering is nondeterministic, cap hits are hidden, or required evidence/performance output is missing.

## 23. Milestone 11 — Collision Matrix and filtering

Completion requires all engine/UI/persistence/replay behavior in `20_COLLISION_MATRIX.md` and `COLF-01` through `COLF-24` PASS.

## 24. Release Gate K — Filtering consistency

BLOCKED if filtering differs across discrete contacts, CCD, sensors, or queries; if stale contacts/impulses survive filtering; if group semantics fail; or if the Matrix/Inspector is disconnected from production state.

## 25. Milestone 12 — Replay Timeline and time-travel diagnostics

Completion requires deterministic seeking, step backward reconstruction, checkpoints, markers, bookmarks, anomaly/mismatch navigation, Fork From Here, Solver Inspector/trajectory integration, long-replay behavior, all `TLN-*` tests, and evidence.

## 26. Release Gate L — Timeline determinism

BLOCKED on any seek digest mismatch, marker/event mismatch, checkpoint transactional failure, original replay overwrite, mandatory `TLN-*` failure/skip, or missing evidence.

## 27. Milestone 13 — Golden acceptance and scope freeze

Completion requires:

- immutable Golden fixture integrity;
- 12/12 Golden PASS;
- complete requirement/test/evidence traceability;
- mandatory test-ID discovery audit;
- anti-placeholder/prohibited-dependency audit;
- no flaky mandatory test;
- no mandatory skip/timeout;
- final `releasecheck` PASS.

## 28. Release Gate M — v1.0 final

This is the terminal release gate.

PASS requires every earlier release gate plus all requirements in `22_GOLDEN_SCENARIO_ACCEPTANCE.md`, `23_SCOPE_FREEZE_TRACEABILITY_AUDIT.md`, and `24_MANDATORY_TEST_REGISTRY.md`.

There is no PARTIAL PASS. Final status is PASS or BLOCKED.
</file>
