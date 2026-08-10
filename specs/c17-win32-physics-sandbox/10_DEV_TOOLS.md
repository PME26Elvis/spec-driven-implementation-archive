# 10 — Required Developer and Verification Tools

## 1. General rule

The project must include self-authored developer/verification utilities implemented in C17.

These tools are part of the assignment deliverable and must have their own tests.

They are not user-facing Sandbox features.

## 2. Required tool set

At minimum provide functional equivalents of:

1. `locscan` — source/document line statistics with ignore/config support.
2. `fixturegen` — deterministic physics/scene fixture generator.
3. `scenecheck` — standalone scene parser/schema/geometry validator.
4. `physverify` — physics validation suite runner.
5. `perfbench` — physics/broad-phase benchmark runner.

Exact binary names may differ if documented clearly.

## 3. `locscan`

### 3.1 Purpose

Count human-authored project content while excluding generated/log/result/build artifacts.

### 3.2 Required categories

Report line totals separately for at least:

- production C source/header.
- test/verification C source/header.
- developer-tool C source/header.
- human-readable Markdown documentation.
- build/config/data files as optional additional categories.

### 3.3 Human-readable document count

The tool must provide a dedicated **human-readable documentation total** that counts Markdown text lines and does not count binary assets as lines.

### 3.4 Ignore behavior

Support:

- built-in exclusions for common build/output directories.
- project ignore file, e.g. `.locignore`.
- optional JSON config, e.g. `locscan.json`.

Config may define:

- include extensions.
- ignore path patterns.
- category mapping.

At minimum patterns must support exact path and prefix/glob-like wildcard behavior sufficient to exclude directories and extensions.

### 3.5 Required default exclusions

Exclude at least:

- object files.
- binaries.
- build directories.
- cache directories.
- test result output.
- benchmark result output.
- screenshots/video/GIF evidence.
- logs.
- temporary files.
- generated fixture output when clearly marked generated.

### 3.6 Output

Provide:

- human-readable table.
- JSON output mode.
- total counted files.
- total lines by category.
- grand total.
- human-readable-doc total.

### 3.7 Tests

Fixture directory tests must prove ignored generated/log/binary files do not inflate counted source/document lines.

## 4. `fixturegen`

### 4.1 Purpose

Generate deterministic valid and invalid test scenes.

### 4.2 Seed

Must accept an explicit unsigned seed and emit the seed in generated metadata/report.

### 4.3 Required fixture families

- random circles.
- random rectangles.
- random convex polygons.
- sparse broad-phase world.
- dense contact cluster.
- randomized stack.
- chain/bridge.
- valid scene schema variants.
- malformed/invalid scene variants.

### 4.4 Convex polygon generation

Generated “valid convex” fixtures must actually be convex under the same validator used by production.

Invalid set must include concave polygons intentionally.

## 5. `scenecheck`

Standalone validator must:

- parse a scene JSON.
- run full schema validation.
- run geometry validation.
- run ID/reference validation.
- report all discoverable validation errors when practical.
- exit non-zero on invalid scene.

Output modes:

- human-readable.
- JSON report.

It must use shared parser/validator implementation or equivalent production logic; it must not be a disconnected weaker validator.

## 6. `physverify`

Runs the validation cases defined in `09_TEST_VERIFICATION.md`.

Required command capabilities:

- run all.
- run one named case.
- output JSON report.
- select/print deterministic seed for randomized cases.

Report per case:

- pass/fail.
- measured metrics.
- acceptance threshold.
- diagnostic message on fail.

`physverify` must run the actual physics engine library used by the GUI application.

## 7. `perfbench`

Runs benchmark scenes from `09_TEST_VERIFICATION.md`.

Must:

- execute warm-up iterations.
- execute measured iterations.
- use the production Windows monotonic/high-resolution timing layer, equivalent to QueryPerformanceCounter/QueryPerformanceFrequency capability.
- report sample count.
- report median.
- report p95.
- report subsystem timings when instrumentation supports them.
- output JSON and human-readable report.

It may not disable collision/solver paths that the benchmark is intended to exercise.

## 8. Tool self-test requirement

Each tool must have automated tests covering successful and failing cases.

Examples:

- `locscan` ignore patterns.
- `fixturegen` same seed → same output.
- `fixturegen` different seed → meaningfully different randomized scene.
- `scenecheck` valid/invalid exit status.
- `physverify` report schema.
- `perfbench` percentile calculation.

## 9. Shared-code requirement

Tools should reuse production modules where appropriate:

- scene parser.
- geometry validator.
- physics engine.
- timing utilities.

Copying a simplified second physics implementation into verification tools is not acceptable as primary verification of production behavior.

## 10. No generated-result substitution

The submission must contain source for every required tool.

Pre-generated JSON reports without the executable source that creates them do not satisfy this section.

## 11. Force/trajectory validation coverage

`physverify` or an equivalently named required verification executable must include deterministic machine-checkable cases for external forces, one-shot impulses, off-center torque response, recorder sample timing, and exported trajectory consistency.

The verification executable must operate on engine data numerically and must not depend on visual judgment.

Developer-tool self-tests must include malformed/edge inputs for any trajectory comparison or report parser introduced for these checks.

## 12. Advanced validation support for v1.0

`physverify` must execute all mandatory `VAL-13` through `VAL-40` cases defined in `15_ADVANCED_PHYSICS_VALIDATION.md`, in addition to the existing validation groups.

Required selection capabilities must permit:

- run the complete advanced group.
- run one advanced case by stable ID.
- run the deterministic randomized/fuzz campaign with the mandatory seed set.
- rerun one reported failing seed.
- emit all measured metrics and tolerances in JSON.

`fixturegen` must support deterministic fixture families needed for:

- translated/rotated metamorphic pairs.
- cyclic/winding-equivalent convex polygons.
- high mass-ratio collisions.
- thin/high-aspect-ratio geometry.
- dynamic AABB-tree mutation sequences.
- bounded randomized finite-state fuzz worlds.

`perfbench` must include the mandatory workload families `PERF-DENSE-500`, `PERF-JOINT-200`, and `PERF-CHURN-5000`.

Developer-tool self-tests must verify that a NaN or missing required metric cannot be reported as a passing validation result.

## 13. `solvertrace`

A dedicated C solver-trace verification executable is required by `18_SOLVER_INSPECTOR.md`.

It must share the production constraint instrumentation used by the GUI Solver Inspector and must support deterministic headless capture of contact/joint solver iterations.

Required outputs:

- human-readable summary;
- canonical JSON trace;
- nonzero exit status for invalid selection, invalid fixture, non-finite trace state, schema failure, or internal trace inconsistency.

Required self-tests include:

- deterministic identical capture;
- invalid contact/joint identity rejection;
- schema version rejection;
- final accumulator versus production solver consistency;
- no-physics-interference digest comparison;
- 10,000-capture stress behavior.

## 14. v1.0 developer-tool additions

Required verification tooling must support the new v1.0 scope. This may extend existing executables or add focused C17 executables, but the following capabilities are mandatory:

### `ccdsweepcheck`

Must run CCD/TOI and Shape Cast reference cases, report TOI fractions/iterations/cap hits, and support deterministic randomized high-speed fixtures.

### `timelinecheck`

Must validate replay seek, checkpoint reconstruction, step backward, marker/event consistency, and 100,000-step timeline behavior without requiring interactive GUI judgment.

### `goldencheck`

Must execute the twelve Golden scenarios, verify fixture digests, collect numeric metrics/digest checkpoints/reproduction artifacts, and return non-zero unless 12/12 pass.

These names are normative identifiers for capabilities; an implementation may integrate them as subcommands of a larger required C17 verification executable if the same independent invocation/reporting behavior exists.

`releasecheck` must consume their machine-readable outputs and independently reject missing/malformed mandatory results.

## 15. Windows platform validation support

The verification/dev-tool set must support the Windows-specific cases in `26_WINDOWS_PLATFORM_VALIDATION.md`.

At minimum it must provide machine-readable checks for:

- UTF-8↔UTF-16 conversion and invalid-surrogate handling;
- DIB/framebuffer stride/orientation/channel fixture validation;
- DPI logical/device/world coordinate conversion;
- platform-state non-interference digest comparison;
- Windows expected-ID discovery for `WIN-*` and `E2E-WIN-*`;
- prohibited native/high-level UI/rendering dependency audit.

Where native message/window behavior is required, the full-application E2E harness may own that portion rather than duplicating it in a command-line tool.
