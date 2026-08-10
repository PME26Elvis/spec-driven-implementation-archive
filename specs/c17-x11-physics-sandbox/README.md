# C17 + X11 2D Physics Sandbox — Implementation Task Package

Version: **1.0**
Status: **Final / scope frozen**
Target: **Linux desktop application implemented in C17 with X11/Xlib/XCB-class system APIs only**

## 1. Purpose

This package defines the complete product and engineering requirements for a desktop 2D rigid-body and constraint physics sandbox.

The deliverable is not a visual prototype. The final application must contain a real-time physics engine, collision system, iterative constraint solver, custom-rendered user interface, interactive scene editor, scene persistence, diagnostics, developer verification utilities, automated tests, and acceptance evidence.

Every requirement marked **MUST**, **REQUIRED**, **SHALL**, or listed under a release gate is mandatory unless a later version of this package explicitly changes it.

## 2. Normative document map

| File | Purpose |
| --- | --- |
| `01_PRODUCT_SCOPE.md` | User-visible product, pages, modes, tools, controls, workflows |
| `02_ENGINEERING_CONSTRAINTS.md` | Language/API restrictions, prohibited substitutes, architectural boundaries |
| `03_MATH_KINEMATICS.md` | Math primitives, transforms, rigid bodies, integration, timing |
| `04_COLLISION_SYSTEM.md` | Shapes, broad phase, SAT, manifolds, filtering, edge cases |
| `05_SOLVER_CONSTRAINTS.md` | Sequential impulses, friction, stabilization, sleeping, joints |
| `06_UI_UX_RENDERING.md` | Custom UI engine, visual states, animations, blur, input, accessibility |
| `07_SCENE_DATA_IO.md` | Scene schema, save/load, deterministic serialization, exports |
| `08_ERROR_BOUNDARY_CASES.md` | Invalid data, numerical limits, corruption, extreme input, failure behavior |
| `09_TEST_VERIFICATION.md` | Unit/integration/E2E/physics validation/performance requirements |
| `10_DEV_TOOLS.md` | Required C17 developer and verification utilities |
| `11_ACCEPTANCE_EVIDENCE.md` | Required screenshots, recordings, reports, evidence index |
| `12_MILESTONES_RELEASE_GATES.md` | Milestones and release-blocking gates |
| `13_DEFINITION_OF_DONE.md` | Final stopping condition and complete acceptance checklist |
| `14_FORCE_TRAJECTORY_TOOLS.md` | Force/impulse interaction, motion recording, trails, live graphs, related verification |
| `15_ADVANCED_PHYSICS_VALIDATION.md` | Analytic, invariant, metamorphic, fuzz, stress, and numerical-quality validation |
| `16_QUERY_SENSOR_REPLAY.md` | Spatial queries, sensors, deterministic replay, checkpoints |
| `17_RELEASE_ACCEPTANCE_SYSTEM.md` | Aggregate releasecheck, anomaly sentinel, repro bundles, acceptance envelopes |
| `18_SOLVER_INSPECTOR.md` | Production solver observability and deterministic per-iteration tracing |
| `19_CCD_TOI_SHAPE_CAST.md` | Continuous collision detection, TOI, Shape Cast and related acceptance |
| `20_COLLISION_MATRIX.md` | Collision categories, masks, groups, Matrix UI and filtering acceptance |
| `21_REPLAY_TIMELINE.md` | Replay Timeline, scrubber, checkpoint seeking and time-travel diagnostics |
| `22_GOLDEN_SCENARIO_ACCEPTANCE.md` | Twelve integrated Golden scenarios and final 12/12 gate |
| `23_SCOPE_FREEZE_TRACEABILITY_AUDIT.md` | v1.0 frozen scope, traceability and anti-placeholder/spec audits |
| `24_MANDATORY_TEST_REGISTRY.md` | Canonical mandatory test/performance/fixture ID registry and expected counts |

## 3. Product identity

The application is a **2D Physics Sandbox** with four main top-level views:

1. **Sandbox** — interactive simulation and scene editing.
2. **Scenes** — scrollable built-in scene gallery and user scene browser.
3. **Diagnostics** — physics, performance, contact, solver, and body diagnostics.
4. **About** — concise project information and keyboard/mouse control reference.

The application must launch into the Sandbox view with a valid default scene.

## 4. Required deliverable groups

The submission must contain all of the following groups:

- Main desktop application source.
- Physics engine source.
- Custom UI/rendering engine source.
- Scene data and built-in scene fixtures.
- Automated unit tests.
- Automated integration tests.
- Full-application E2E tests.
- Physics validation programs and reference scenarios.
- Performance benchmark program.
- Required developer tools.
- Acceptance evidence.
- Human-readable build/use documentation sufficient to identify the produced binaries and controls.

## 5. Core non-negotiable scope

The implementation must provide:

- C17 implementation.
- Linux desktop target.
- X11/Xlib and/or XCB-class low-level APIs for window/input/presentation.
- Custom 2D software rendering and UI components.
- Circle, rectangle, and arbitrary convex polygon rigid-body shapes.
- Explicit rejection of concave polygons.
- Static, dynamic, and kinematic body types.
- Fixed-step simulation.
- Dynamic AABB-tree broad phase.
- SAT-based convex narrow phase plus dedicated circle paths.
- Contact manifold generation with geometric contact points.
- Sequential impulse solver with warm starting.
- Restitution, Coulomb friction, and rolling resistance.
- Baumgarte-style contact stabilization.
- Sleeping and waking.
- Distance, revolute/hinge, and mouse constraints.
- Scene creation/editing, save/load, import validation, export, undo/redo.
- Debug visualization layers.
- Built-in validation scenes including free fall, collision, stack, bridge, and ragdoll/linked-body examples.
- Automated evidence and test reports.
- Advanced physics validation using analytic references, invariants, metamorphic transformations, deterministic fuzz, and stress fixtures.
- Production-backed Solver Inspector with contact/joint selection, one-step iteration tracing, warm-start/clamp diagnostics, deterministic trace export, and non-interference validation.
- BULLET continuous collision detection with rotationally aware TOI and bounded sub-stepping.
- Circle/rectangle/convex-polygon Shape Cast queries.
- 16-category collision layers/masks/groups and functional Collision Matrix editor.
- Replay Timeline/Scrubber with checkpoint reconstruction, bookmarks, anomaly markers, and Fork From Here.
- Twelve-scenario Golden Acceptance Suite with numeric envelopes, replays, evidence, and mandatory 12/12 PASS.
- v1.0 traceability, acceptance-coverage, anti-placeholder, and prohibited-dependency audits.

## 6. Explicitly out of scope for v1.0

The following are not required and must not be used to claim completion of a mandatory feature:

- Concave polygon rigid bodies or automatic concave decomposition.
- Soft-body, cloth, fluid, or dedicated particle-system physics.
- 3D physics.
- GPU/OpenGL/Vulkan rendering.
- Multithreaded physics solver.
- Networked simulation/multiplayer.
- Embedded scripting language or arbitrary plugin system.
- Gear, pulley, or dedicated wheel/suspension joint primitives.
- Dedicated deformable rope solver beyond the required joint-chain behavior.
- Rotating interactive Shape Cast query (production CCD still accounts for body rotation).
- General-purpose SQL/database subsystem.

Continuous collision detection / Time of Impact is explicitly **in scope** for v1.0.

## 7. Reading rules

If requirements appear to conflict:

1. `13_DEFINITION_OF_DONE.md` determines whether the task may be declared complete.
2. `12_MILESTONES_RELEASE_GATES.md` determines whether a release is acceptable.
3. Engineering restrictions in `02_ENGINEERING_CONSTRAINTS.md` override convenience choices elsewhere.
4. More specific subsystem requirements override more general product wording.

## 8. Acceptance philosophy

A feature is complete only when all three are true:

- The user-visible behavior exists and is connected to real state.
- The underlying implementation satisfies the engineering restrictions.
- The required automated tests and acceptance evidence demonstrate the behavior.

Mock data, hard-coded success states, decorative controls, placeholder panels, pre-rendered videos, static screenshots, or disconnected UI do not satisfy functional requirements.

## 9. Solver observability requirement

The project must include the Solver Inspector defined in `18_SOLVER_INSPECTOR.md`.

This is a mandatory product and verification feature, not optional debug logging. It exists so contact instability, friction errors, warm-start mistakes, motor/limit clamping errors, and joint drift can be diagnosed from real production solver state before final delivery.


## 10. v1.0 scope freeze

Version 1.0 is the frozen assignment. No additional feature is needed to claim conformance beyond the normative package, and no mandatory feature may be omitted in exchange for an extra feature.

The final release is acceptable only when the aggregate `releasecheck` result is PASS, the Golden Scenario Suite reports 12/12 PASS, all mandatory test IDs are discovered/executed/passing, all required evidence is indexed, and the traceability/prohibition audits have no gap.
