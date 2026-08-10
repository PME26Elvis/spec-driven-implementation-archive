# Package Manifest

Version: **1.0**
Status: **Final / scope frozen**

This manifest lists every human-readable Markdown document in the v1.0 task package. Generated logs, reports, screenshots, recordings, binaries, caches, and other non-document artifacts are excluded from the package documentation line count.

| Document | Lines |
| --- | ---: |
| `01_PRODUCT_SCOPE.md` | 471 |
| `02_ENGINEERING_CONSTRAINTS.md` | 194 |
| `03_MATH_KINEMATICS.md` | 261 |
| `04_COLLISION_SYSTEM.md` | 290 |
| `05_SOLVER_CONSTRAINTS.md` | 305 |
| `06_UI_UX_RENDERING.md` | 363 |
| `07_SCENE_DATA_IO.md` | 304 |
| `08_ERROR_BOUNDARY_CASES.md` | 267 |
| `09_TEST_VERIFICATION.md` | 539 |
| `10_DEV_TOOLS.md` | 274 |
| `11_ACCEPTANCE_EVIDENCE.md` | 337 |
| `12_MILESTONES_RELEASE_GATES.md` | 370 |
| `13_DEFINITION_OF_DONE.md` | 531 |
| `14_FORCE_TRAJECTORY_TOOLS.md` | 394 |
| `15_ADVANCED_PHYSICS_VALIDATION.md` | 835 |
| `16_QUERY_SENSOR_REPLAY.md` | 679 |
| `17_RELEASE_ACCEPTANCE_SYSTEM.md` | 814 |
| `18_SOLVER_INSPECTOR.md` | 646 |
| `19_CCD_TOI_SHAPE_CAST.md` | 468 |
| `20_COLLISION_MATRIX.md` | 270 |
| `21_REPLAY_TIMELINE.md` | 288 |
| `22_GOLDEN_SCENARIO_ACCEPTANCE.md` | 486 |
| `23_SCOPE_FREEZE_TRACEABILITY_AUDIT.md` | 330 |
| `24_MANDATORY_TEST_REGISTRY.md` | 319 |
| `README.md` | 153 |
| `PACKAGE_MANIFEST.md` | 53 |

**Total human-readable documentation lines: 10241.**

Line counts are physical text lines in the Markdown source files. The manifest is included in the total.

## v1.0 package invariants

- 24 numbered normative specification documents (`01`–`24`).
- One top-level `README.md`.
- One `PACKAGE_MANIFEST.md`.
- Continuous collision detection / TOI is in scope.
- Concave polygon rigid bodies remain out of scope.
- Golden Scenario Acceptance requires 12/12 PASS.
- Mandatory named functional/validation/E2E registry contains 284 IDs.
- 11 mandatory named performance workloads are registered.
- 5 stable-envelope fixtures and 2 fixed CCD fixtures are registered.
- Minimum non-Golden deterministic regression corpus is 60 fixtures.
- Final release status is PASS or BLOCKED only.
