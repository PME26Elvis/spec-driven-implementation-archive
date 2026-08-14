# v1.0 Scope Freeze, Traceability, and Specification Audit

Version: **1.0**
Status: **Normative final**

## 1. Purpose

This document freezes the v1.0 assignment scope and defines how completeness of the specification itself is checked. It prevents a submission from declaring completion by implementing a visually convincing subset while omitting difficult engine, UI, persistence, testing, diagnostic, or evidence requirements.

## 2. Scope-freeze rule

The mandatory v1.0 scope is the union of all normative requirements in documents listed by `PACKAGE_MANIFEST.md`.

A submission may implement extra features, but extras:

- do not compensate for missing mandatory requirements,
- must not violate engineering constraints,
- must not weaken deterministic behavior or tests,
- are not required for acceptance.

## 3. Final v1.0 explicit out-of-scope list

The following are intentionally not required:

- concave rigid-body polygons,
- automatic concave decomposition,
- soft bodies,
- cloth simulation,
- fluid simulation,
- particle-system physics as a product feature,
- 3D physics,
- GPU rendering,
- OpenGL/Vulkan rendering path,
- multithreaded physics solver,
- networked simulation/multiplayer,
- embedded scripting language,
- arbitrary plugin system,
- gear joint,
- pulley joint,
- wheel/suspension joint as a dedicated primitive,
- deformable rope solver distinct from the required joint chain,
- arbitrary rotational Shape Cast query,
- general SQL/database subsystem.

Continuous collision detection is **in scope** for v1.0.

## 4. Normative terminology

- MUST / SHALL / REQUIRED: release-blocking.
- SHOULD: expected quality requirement; omission requires documented rationale but is not automatically a release failure unless referenced by a gate.
- MAY / OPTIONAL: non-blocking.
- PROHIBITED / MUST NOT: violation is release-blocking.

## 5. Requirement identity

Major automated requirements have stable prefix IDs. The mandatory prefix families are:

- `MATH-*`
- `COL-*`
- `SOL-*`
- `UI-*`
- `IO-*`
- `ERR-*`
- `E2E-*`
- `VAL-*`
- `FORCE-*` / `IMP-*` / motion-analysis families
- `QRY-*`
- `SNS-*`
- `RPL-*`
- `SINSP-*`
- `CCD-*`
- `CAST-*`
- `COLF-*`
- `TLN-*`
- `GOLD-*`
- `PERF-*`

Exact legacy naming inside earlier subsystem documents remains normative even if a family is not repeated here.

## 6. Required submission traceability matrix

The implementation deliverable must include a machine-readable or Markdown traceability matrix with one row per mandatory feature/requirement group containing:

- requirement ID/group,
- implementation module/file references,
- automated test IDs,
- acceptance evidence references where required,
- release gate that consumes the result,
- status.

A row may reference multiple implementation files/tests.

Status values are only:

- PASS,
- BLOCKED,
- NOT_APPLICABLE only for explicitly optional requirements.

Mandatory requirements may not be marked NOT_APPLICABLE.

## 7. Minimum subsystem traceability map

| Subsystem | Primary spec | Required automated evidence | Final gate |
| --- | --- | --- | --- |
| Math/kinematics | `03_MATH_KINEMATICS.md` | unit + analytic validation | Core Physics |
| Collision shapes/broad phase/manifolds | `04_COLLISION_SYSTEM.md` | unit + oracle + fuzz | Collision |
| Sequential solver/joints | `05_SOLVER_CONSTRAINTS.md` | solver validation + Golden | Solver |
| Custom UI/rendering | `06_UI_UX_RENDERING.md` | E2E + visual evidence | UI |
| Scene persistence/export | `07_SCENE_DATA_IO.md` | round-trip/corruption tests | Data |
| Errors/boundaries | `08_ERROR_BOUNDARY_CASES.md` | negative tests | Reliability |
| General verification | `09_TEST_VERIFICATION.md` | all mandatory suites | Verification |
| Dev tools | `10_DEV_TOOLS.md` | tool self-tests | Tooling |
| Acceptance evidence | `11_ACCEPTANCE_EVIDENCE.md` | evidence index | Evidence |
| Release gates | `12_MILESTONES_RELEASE_GATES.md` | aggregate status | Release |
| DoD | `13_DEFINITION_OF_DONE.md` | completion checklist | Final |
| Force/trajectory | `14_FORCE_TRAJECTORY_TOOLS.md` | physics + E2E | Interaction |
| Advanced physics quality | `15_ADVANCED_PHYSICS_VALIDATION.md` | VAL/fuzz/metamorphic | Physics Quality |
| Queries/sensors/replay | `16_QUERY_SENSOR_REPLAY.md` | QRY/SNS/RPL | Query/Replay |
| Acceptance system | `17_RELEASE_ACCEPTANCE_SYSTEM.md` | releasecheck self-tests | Acceptance |
| Solver Inspector | `18_SOLVER_INSPECTOR.md` | SINSP | Diagnostics |
| CCD/TOI/Shape Cast | `19_CCD_TOI_SHAPE_CAST.md` | CCD/CAST | Continuous Collision |
| Collision Matrix | `20_COLLISION_MATRIX.md` | COLF | Filtering |
| Replay Timeline | `21_REPLAY_TIMELINE.md` | TLN | Time Travel |
| Golden Suite | `22_GOLDEN_SCENARIO_ACCEPTANCE.md` | GOLD 12/12 | Golden Final |
| Mandatory test registry | `24_MANDATORY_TEST_REGISTRY.md` | expected-ID discovery/execution audit | Final Registry |

## 8. Feature-to-test rule

Every mandatory user-visible feature must have at least one automated test that proves it is connected to production state.

Purely visual qualities that cannot be completely machine-scored still require:

- automated state/interaction verification,
- named visual evidence capture point.

A screenshot alone cannot prove a functional control is wired.

## 9. Test-to-oracle rule

Every mandatory automated physics test must identify its oracle class:

- analytic/reference equation,
- exact deterministic digest,
- slower independent algorithm/oracle,
- invariant/conservation law,
- metamorphic relation,
- bounded numerical envelope,
- lifecycle/event sequence,
- explicit expected error behavior.

A test that only checks `program exited 0` is insufficient for physics correctness.

## 10. No self-fulfilling oracle

A test must not compute its expected result using exactly the same production function path under test unless the purpose is only serialization/identity plumbing.

Examples:

- broad-phase validation compares production tree results against brute-force overlap oracle,
- Shape Cast randomized verification compares against a slower verification sweep/reference,
- mass/inertia compares against analytic formulas,
- replay compares independent recorded/reconstructed digests,
- Golden metrics compare against frozen thresholds.

## 11. Acceptance coverage audit

Before final release, `releasecheck` must generate a coverage inventory proving that every mandatory test ID known to the project is:

- discovered,
- executed,
- PASS.

Unknown missing IDs or duplicate IDs are BLOCKED.

A static registry/manifest of expected mandatory IDs is required so test discovery cannot silently omit an entire suite.

## 12. Skip policy

Mandatory tests may not be skipped because:

- environment is inconvenient,
- fixture is slow,
- visual subsystem is headless,
- a feature is unfinished.

A mandatory skip is BLOCKED.

A truly unsupported execution prerequisite produces BLOCKED, not PASS.

## 13. Timeout policy

Timeout is not equivalent to PASS or skip.

The report must identify:

- suite/test ID,
- configured timeout,
- elapsed time,
- reproduction command/fixture reference where applicable.

## 14. Flaky-test policy

Automatic retry may be used diagnostically, but a mandatory test that fails then passes on retry is classified as FLAKY and blocks v1.0 until the cause is resolved.

Determinism is a product requirement, so 'usually passes' is insufficient.

## 15. Threshold ownership

Normative thresholds in this package may not be automatically relaxed by the implementation.

Implementation-specific additional stricter thresholds are allowed.

If a test requires a tolerance not fixed by the package, the implementation must document it and justify it using numeric precision/scale; such tolerance may not be chosen after observing a failure solely to make the case pass.

## 16. Fixture ownership

Normative fixtures are immutable input.

Submission-specific supplemental fixtures are encouraged but do not replace mandatory fixtures.

## 17. Anti-placeholder audit

Final audit searches for and rejects mandatory functionality implemented as:

- TODO-only code path,
- placeholder button/panel,
- hard-coded PASS,
- hard-coded precomputed trajectory,
- static screenshot/video presented as live evidence,
- fake performance numbers,
- fake solver trace disconnected from solver state,
- replay of transforms instead of replay of commands,
- JSON parsing delegated to prohibited external library,
- UI controls that do not mutate real product state,
- dev tool that only shells out to a prohibited external equivalent.

## 18. Source/build artifact audit

The final package delivered by an implementation must separate:

- authored source,
- tests,
- fixtures,
- docs,
- generated reports/evidence,
- build/cache/temp outputs.

Required source or fixture files may not be generated only at runtime from opaque binaries.

## 19. Static engineering constraint audit

The submission must provide an auditable dependency/source inventory demonstrating:

- C17 source for production/test/dev tools as required,
- permitted X11/Xlib/XCB-class system APIs,
- no GTK/Qt/SDL/Cairo/OpenGL/Vulkan substitution,
- no external physics engine,
- no external GUI framework,
- no external JSON parser for mandatory hand-written parser scope,
- no external plotting library for mandatory graph rendering.

The exact method used to inspect this is not prescribed; the result is required.

## 20. Specification consistency audit

The task package itself establishes these v1.0 precedence rules:

1. `13_DEFINITION_OF_DONE.md` is the stopping condition.
2. `12_MILESTONES_RELEASE_GATES.md` and `17_RELEASE_ACCEPTANCE_SYSTEM.md` define aggregate blockers.
3. `22_GOLDEN_SCENARIO_ACCEPTANCE.md` defines final integrated 12/12 gate.
4. subsystem-specific documents define detailed behavior.
5. `02_ENGINEERING_CONSTRAINTS.md` overrides convenience implementations.
6. this Scope Freeze document determines whether something is in/out of v1.0 scope.

## 21. Contradiction handling

If an implementation finds a genuine contradiction that makes simultaneous compliance impossible, it must:

- identify exact file/section references,
- treat release as BLOCKED rather than silently selecting the easier requirement.

The task package is intended to be internally consistent; implementation convenience is not a contradiction.

## 22. Release profiles

Two execution profiles are allowed for development convenience:

- `quick`: fast local subset; never sufficient for release.
- `release`: every mandatory suite, Golden 12/12, evidence inventory, audits.

Only `release` may produce top-level PASS.

## 23. Required top-level release report

Final report includes at least:

- package version = 1.0,
- source/build identity,
- mandatory test discovered/executed/pass counts,
- suite statuses,
- Golden 12/12 table,
- performance case statuses and measurements,
- soak/fuzz summary,
- anomaly summary,
- evidence completeness,
- traceability completeness,
- dependency/prohibition audit,
- unresolved failures/flakes/skips/timeouts,
- final status PASS or BLOCKED.

## 24. PASS semantics

Top-level PASS means all of the following:

- all mandatory product behavior implemented,
- all engineering constraints respected,
- all mandatory tests executed and passed,
- no flaky mandatory tests,
- no mandatory skips/timeouts,
- all Golden scenarios pass,
- required evidence exists,
- traceability has no mandatory gaps,
- no prohibited substitute detected,
- release audits pass.

Anything else is BLOCKED.

## 25. Scope freeze and stopping condition

Once the v1.0 package is issued, feature scope is frozen. A conforming implementation stops feature work when the complete Definition of Done passes. Further optional enhancement is outside the assignment and must not delay or replace the required v1.0 completion evidence.
</file>
