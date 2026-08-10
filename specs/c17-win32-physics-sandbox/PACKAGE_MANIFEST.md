# Package Manifest

Version: **1.0 Windows sibling**
Status: **Final / scope frozen**

This manifest lists every human-readable Markdown document in the Windows v1.0 task package. Generated logs, reports, screenshots, recordings, binaries, caches, and other non-document artifacts are excluded from the package documentation line count.

| Document | Lines |
| --- | ---: |
| `01_PRODUCT_SCOPE.md` | 479 |
| `02_ENGINEERING_CONSTRAINTS.md` | 214 |
| `03_MATH_KINEMATICS.md` | 261 |
| `04_COLLISION_SYSTEM.md` | 290 |
| `05_SOLVER_CONSTRAINTS.md` | 305 |
| `06_UI_UX_RENDERING.md` | 382 |
| `07_SCENE_DATA_IO.md` | 316 |
| `08_ERROR_BOUNDARY_CASES.md` | 290 |
| `09_TEST_VERIFICATION.md` | 564 |
| `10_DEV_TOOLS.md` | 289 |
| `11_ACCEPTANCE_EVIDENCE.md` | 345 |
| `12_MILESTONES_RELEASE_GATES.md` | 385 |
| `13_DEFINITION_OF_DONE.md` | 553 |
| `14_FORCE_TRAJECTORY_TOOLS.md` | 394 |
| `15_ADVANCED_PHYSICS_VALIDATION.md` | 835 |
| `16_QUERY_SENSOR_REPLAY.md` | 679 |
| `17_RELEASE_ACCEPTANCE_SYSTEM.md` | 834 |
| `18_SOLVER_INSPECTOR.md` | 646 |
| `19_CCD_TOI_SHAPE_CAST.md` | 468 |
| `20_COLLISION_MATRIX.md` | 270 |
| `21_REPLAY_TIMELINE.md` | 288 |
| `22_GOLDEN_SCENARIO_ACCEPTANCE.md` | 486 |
| `23_SCOPE_FREEZE_TRACEABILITY_AUDIT.md` | 353 |
| `24_MANDATORY_TEST_REGISTRY.md` | 337 |
| `25_WINDOWS_PLATFORM_ADAPTATION.md` | 312 |
| `26_WINDOWS_PLATFORM_VALIDATION.md` | 332 |
| `README.md` | 165 |
| `PACKAGE_MANIFEST.md` | 61 |

**Total human-readable documentation lines: 11133.**

Line counts are physical text lines in the Markdown source files. The manifest is included in the total.

## Windows v1.0 package invariants

- 26 numbered normative specification documents (`01`–`26`).
- One top-level `README.md`.
- One `PACKAGE_MANIFEST.md`.
- Core physics/product requirements remain equivalent to the Linux/X11 v1.0 sibling.
- Windows OS boundary is limited to low-level User32/GDI32/Kernel32/Imm32-class responsibilities defined by the package.
- Required client-area UI/rendering remains hand-built; ready-made Windows controls/dialogs/rendering substitutes are prohibited.
- Per-monitor DPI, UTF-8↔UTF-16, IME, Unicode paths, capture/focus, resize/paint, and native-close behavior are normative.
- Continuous collision detection / TOI is in scope.
- Concave polygon rigid bodies remain out of scope.
- Golden Scenario Acceptance requires 12/12 PASS with unchanged physics envelopes.
- Mandatory named functional/validation/E2E/platform registry contains 322 IDs (284 shared/core + 38 Windows-specific).
- 11 mandatory named performance workloads are registered.
- 5 stable-envelope fixtures and 2 fixed CCD fixtures are registered.
- Minimum non-Golden deterministic regression corpus is 60 fixtures.
- Windows platform validation adds 30 `WIN-*` and 8 `E2E-WIN-*` cases plus `WIN-EV-01`–`WIN-EV-12` evidence.
- Final Windows release requires Gate N and the `windows_platform` aggregate group.
- Final release status is PASS or BLOCKED only.
