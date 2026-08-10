# 11 — Acceptance Evidence Requirements

## 1. Purpose

The submission must include evidence sufficient for a human reviewer to verify visible behavior and automated engineering checks without inferring completion from source code alone.

This document defines **what evidence must exist**, not the mechanism used to capture it.

## 2. Evidence directory structure

A clear structure equivalent to the following is required:

- `evidence/ui/`
- `evidence/physics/`
- `evidence/e2e/`
- `evidence/tests/`
- `evidence/performance/`
- `evidence/dev_tools/`
- `evidence/INDEX.md`

Exact names may differ if equivalently organized.

## 3. Evidence index

`INDEX.md` must map every release-gate item to:

- evidence file(s).
- test/report case name where applicable.
- brief statement of what is shown.

A reviewer should not need to search an unstructured screenshot folder.

## 4. UI screenshot evidence

Required still images:

### UI-01 Default Sandbox

Show:

- top nav.
- tool rail.
- viewport.
- inspector.
- status strip.
- starter scene.

### UI-02 Hover elevation

Show a button/card in hover state with visible elevation compared to normal-state reference.

### UI-03 Border glow/focus

Show a focused editable control or selected item with glow.

### UI-04 Fully open modal

Show:

- modal.
- darkened backdrop.
- visibly blurred application content behind it.

### UI-05 Frosted nav at scroll zero

Scenes/Diagnostics/About view with top content position.

### UI-06 Frosted nav after scroll

Same view after sufficient scroll, demonstrating stronger blur/surface/shadow.

### UI-07 Responsive/collapsed inspector

Show narrow layout or manually collapsed inspector state.

### UI-08 Diagnostics

Show live diagnostic metrics and history graph.

## 5. Animation evidence

Still screenshots cannot prove animation. The following require short recording, GIF, or ordered frame sequence:

### ANIM-01 Hover transition

Normal → lifted → normal.

### ANIM-02 Click ripple

Pointer-down origin → expanding ripple → faded ripple.

### ANIM-03 Capsule slide

Active indicator transitions between two top-level destinations.

### ANIM-04 Panel collapse

Expanded → intermediate → collapsed.

### ANIM-05 Modal transition

Closed/background → scale+opacity opening → fully open blurred backdrop → closing.

### ANIM-06 Dynamic frosted nav

Scroll through threshold while blur/shadow increase smoothly.

The recording must show actual application UI, not a separately recreated animation.

## 6. Physics visual evidence

### PHY-01 Free fall/boundary bounce

Show multiple circles/rectangles under gravity and interacting with boundaries.

### PHY-02 Collision manifold

Show two colliding polygons with:

- collision-highlight outline.
- red contact crosshair(s).
- contact normal vector.

### PHY-03 Two-point manifold

Show a face-face polygon contact with two distinguishable contact points when geometry produces two points.

### PHY-04 Stable five-block tower

Show tower after settling interval.

### PHY-05 Tower disturbance

Recording or frames show applied external impulse/interaction and physically plausible collapse.

### PHY-06 Friction ramp

Show high-friction held body and low-friction sliding body in comparable setup.

### PHY-07 Restitution comparison

Show bodies with visibly different rebound behavior.

### PHY-08 Pendulum/distance joint

Show constrained oscillation with anchor visualization.

### PHY-09 Suspension bridge before impact

Show bridge topology and anchors.

### PHY-10 Suspension bridge during/after impact

Recording or frames show multi-segment response to collision.

### PHY-11 Ragdoll/linked system

Show multi-body constrained structure responding to gravity/contact.

### PHY-12 Sleeping overlay

Show a settled island with sleeping-state visualization and Diagnostics awake/sleep counts.

## 7. Debug overlay evidence

At least one image must show together:

- AABBs.
- fat AABBs.
- center of mass.
- velocity vector.
- contact point.
- contact normal.
- joint anchor/constraint visualization.

## 8. Save/load evidence

Provide report or E2E evidence proving:

- create/edit scene.
- save.
- load fresh.
- representative values preserved.
- invalid scene load rejected without replacing current valid scene.

## 9. Test evidence

Provide latest machine-readable and human-readable summary for:

- unit tests.
- integration tests.
- E2E tests.
- physics validation.
- parser/scene regression.
- dev-tool tests.

Each summary must include pass/fail counts.

## 10. Performance evidence

Provide benchmark report containing:

- environment-neutral test/fixture identifiers.
- body counts.
- step sample count.
- median and p95 physics time.
- subsystem timings.
- candidate/contact counts.

Do not include only a single FPS number.

## 11. `locscan` evidence

Provide:

- human-readable line-count report.
- JSON report.
- test fixture showing logs/results/binaries were excluded.

## 12. Failure evidence

At least one screenshot/report for each:

- concave polygon rejection.
- malformed scene rejection.
- unsaved-change confirmation modal.
- invalid numeric inspector input.

## 13. Evidence freshness

Evidence must correspond to the final submitted implementation.

Evidence created before later functional changes must be regenerated if the affected behavior could have changed.

## 14. Evidence integrity

Evidence must not be manually edited to fabricate a passing state.

Cropping for readability is acceptable if it does not remove relevant context.

## 15. Reviewer checklist summary

A reviewer must be able to verify from evidence:

- modern custom UI exists.
- required animation families move over time.
- modal blur is real.
- dynamic frosted nav responds to scroll.
- supported shape types function.
- concave polygons are rejected.
- real contact points/normals are visualized.
- stable stacking exists.
- constraints/joints react physically.
- save/load works.
- tests and benchmarks execute and pass.
- required developer tools exist and execute.

## 16. Force, impulse, and live motion evidence

Required evidence additions:

- Apply Force preview showing application point, vector, and numeric magnitude/components.
- Apply Impulse at an off-center point, with subsequent visible angular response.
- short sequence showing sustained force while held and cessation after release.
- two or more simultaneously visible world-space trajectory trails.
- Motion Analysis graph showing at least two bodies, readable axes, legend, and cursor/sample value readout.
- numerical verification report for center/off-center impulse and constant-force cases.
- E2E result for pause → impulse → single-step behavior.
- representative Motion Analysis trajectory CSV cross-checked by automated verification.

Capture mechanism remains unspecified. Only evidence content and state are normative.

## 17. Advanced validation evidence for v1.0

Provide the final machine-readable and human-readable advanced validation report covering `VAL-13` through `VAL-40`.

The evidence index must make it easy to locate at least these representative numeric results:

- analytic mass/inertia comparison.
- unequal-mass restitution comparison.
- static/dynamic friction numeric comparison.
- high mass-ratio collision result.
- deep-overlap recovery metrics.
- translation-invariance comparison.
- rotation-invariance comparison.
- time-step convergence table.
- hinge motor torque-cap result.
- long-chain constraint-error report.
- dynamic-tree lifecycle oracle summary.
- deterministic fuzz seed summary.
- contact-manifold geometric-validity summary.

Also provide reports for:

- `PERF-DENSE-500`.
- `PERF-JOINT-200`.
- `PERF-CHURN-5000`.
- repeated lifecycle stress.

These are numeric/report evidence; screenshots are not required for every advanced validation case.

## 18. Solver Inspector evidence

The final evidence package must include the visual and machine-readable Solver Inspector artifacts required by `18_SOLVER_INSPECTOR.md`.

At minimum retain:

- `SI-01` selected contact with matching viewport marker and stable IDs;
- `SI-02` two-point manifold with separate point impulses;
- `SI-03` warm-start state and first solver iteration;
- `SI-04` friction-clamp diagnostic;
- `SI-05` revolute motor requested versus clamped impulse;
- `SI-06` per-iteration table and graph from one captured step;
- `SI-07` pinned contact after lifecycle invalidation;
- `SI-08` bridge joint Inspector;
- `SI-09` exported JSON trace plus successful validator report;
- `SI-10` Inspector-on versus Inspector-off state-digest comparison report.

The evidence manifest must map each `SINSP-*` and `E2E-SI-*` requirement to its corresponding test/report/evidence artifact.

## 19. v1.0 evidence additions

The evidence index must additionally contain:

- CCD thin-wall impact sequence and rotating-body impact visualization;
- Shape Cast hit and no-hit views;
- Collision Matrix and selected-body filter controls;
- runtime filter-change behavior report;
- Replay Timeline overview, zoomed markers, anomaly/mismatch navigation, and fork provenance;
- Timeline-to-Solver-Inspector diagnostic workflow;
- Golden Scenario summary with all twelve statuses;
- required named capture points for each Golden scenario;
- v1.0 traceability matrix;
- mandatory test-inventory/discovery report;
- prohibited-dependency/anti-placeholder audit result.

Evidence must identify the same build/source identity as the numeric release reports. Evidence generated from a different physics configuration cannot satisfy acceptance.
