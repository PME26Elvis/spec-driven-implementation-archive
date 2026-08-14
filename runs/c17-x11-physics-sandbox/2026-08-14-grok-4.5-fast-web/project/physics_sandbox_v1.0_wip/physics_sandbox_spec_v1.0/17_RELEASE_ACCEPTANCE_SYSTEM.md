# 17 — Release Acceptance System, Self-Diagnostics, and Reproducible Failure Artifacts

## 1. Purpose

The project is complex enough that visual inspection alone is not an acceptable primary correctness mechanism.

The implementation must include a comprehensive release-acceptance system capable of detecting, localizing, and reproducing common physics, collision, constraint, lifecycle, persistence, and GUI-integration failures before the deliverable is declared complete.

This document defines required acceptance outputs and failure diagnostics. It does not prescribe how implementation work is organized.

## 2. Acceptance principle

A successful application launch, attractive UI, or plausible-looking simulation is insufficient evidence of correctness.

The release acceptance system must answer all of the following:

- Did every mandatory automated case execute?
- Did any case fail or skip?
- Did any runtime invariant fail?
- Did any simulation produce NaN/Inf or invalid references?
- Did any stable-scene envelope indicate explosive/implausible motion?
- Are deterministic replay and state digests reproducible?
- Can every randomized/stress failure be reproduced from retained inputs?
- Are performance and long-run stability gates satisfied?
- Are required UI/E2E behaviors connected to the real physics engine?
- Is the submitted evidence fresh and attributable to the same implementation revision?

If any mandatory answer is unknown, the acceptance status is failure/incomplete.

## 3. Required aggregate release checker

The submission must provide one top-level release-checking entry point, referred to normatively as `releasecheck`.

The name may differ only if the documentation makes the equivalent entry point unambiguous.

`releasecheck` must aggregate at least:

1. unit tests,
2. integration tests,
3. parser/serialization regressions,
4. base physics validations,
5. force/trajectory validations,
6. advanced physics validations,
7. spatial query validations,
8. sensor lifecycle validations,
9. replay/checkpoint validations,
10. E2E tests,
11. deterministic fuzz suites,
12. performance benchmarks required for release,
13. lifecycle stress,
14. long-run soak tests,
15. developer-tool self-tests,
16. evidence-manifest completeness checks,
17. prohibited-dependency/substitution checks that can be automated.

It must execute real test binaries/results rather than merely checking that report files already exist.

## 4. Aggregate exit semantics

`releasecheck` must:

- return exit status `0` only when every mandatory release-blocking group passes,
- return non-zero if any mandatory case fails,
- return non-zero if any mandatory case is skipped,
- return non-zero if a required report/metric is missing,
- return non-zero if a test executable crashes or times out,
- return non-zero if any report contains a non-finite required metric,
- clearly distinguish infrastructure/test-harness failure from product assertion failure in its report while both still block release.

No "mostly passed" status may map to process success.

## 5. Machine-readable release report

The checker must emit a deterministic machine-readable report in JSON using the project's own JSON implementation or a purpose-built C serializer.

Required top-level fields:

- report format version,
- implementation/build identifier,
- start/end metadata sufficient for evidence traceability,
- overall status,
- group summaries,
- case results,
- performance metrics,
- soak/lifecycle metrics,
- failure artifact references,
- evidence-manifest validation result.

Each case result must include at least:

- stable case ID,
- group,
- status (`PASS`, `FAIL`, `SKIP`, `HARNESS_ERROR`, `TIMEOUT`),
- duration,
- optional seed,
- optional fixed-step index,
- measured values,
- expected values/tolerances when applicable,
- failure artifact path when not passing.

## 6. Human-readable release summary

A human-readable summary must also be produced.

It must show at least:

- total mandatory cases,
- passed,
- failed,
- skipped,
- harness errors,
- timeouts,
- list of failing IDs,
- release gate status A onward,
- final `PASS` or `BLOCKED` status.

A reviewer must be able to identify why release is blocked without opening raw logs first.

## 7. Runtime invariant monitor

Verification and debug-capable builds must provide a reusable invariant-monitoring facility that can be enabled during validations, fuzz, E2E, replay, and soak tests.

At minimum, after every fixed physics step in monitored runs it must be able to assert:

### 7.1 Body invariants

- all position components finite,
- angle finite,
- linear/angular velocities finite,
- accumulated force/torque finite,
- mass/inverse mass consistency,
- inertia/inverse inertia consistency,
- static body inverse mass/inertia remain zero,
- non-negative sleep timer,
- valid body type,
- valid live shape reference/data.

### 7.2 Shape/AABB invariants

- shape dimensions finite and within accepted positive ranges,
- convex polygons remain valid convex shapes,
- tight AABB finite and ordered,
- fat AABB finite and ordered,
- fat proxy contains required tight AABB margin according to implementation policy,
- live broad-phase proxy references a live body exactly once.

### 7.3 Contact/manifold invariants

- referenced bodies are live,
- pair IDs are canonical/valid,
- contact count within allowed shape-pair range,
- contact points finite,
- normals finite and approximately unit length,
- penetration/separation values finite,
- accumulated impulses finite,
- no sensor pair contributes normal/friction/rolling impulses.

### 7.4 Joint invariants

- referenced bodies are live,
- anchors/parameters finite,
- effective mass denominators guarded,
- accumulated impulses finite,
- motor impulse respects configured cap,
- deleted joints have no live solver references.

### 7.5 Lifecycle invariants

- body IDs unique,
- joint IDs unique,
- no stale contact pair after destruction/filter invalidation,
- no stale sensor pair after destruction/filter invalidation,
- Begin/Stay/End state machine cannot enter an impossible transition,
- event stream contains no duplicate event for one canonical pair/type/step.

Any enabled invariant violation must immediately fail the current case and produce a reproducible failure artifact.

## 8. Numerical anomaly sentinel

The validation system must include scenario-aware anomaly checks intended to detect visually obvious "objects exploding/flying away" failures automatically.

Global finite checks alone are insufficient.

Required anomaly metrics include:

- maximum linear speed per step and per fixture,
- maximum angular speed,
- maximum position magnitude from fixture origin,
- maximum contact penetration,
- maximum joint positional error,
- maximum constraint impulse magnitude,
- total kinetic energy,
- total mechanical energy where a fixture has a meaningful reference,
- body count/contact count/event count discontinuities.

Each deterministic fixture must define either:

- an analytic/reference tolerance, or
- a finite expected envelope appropriate to the fixture.

The acceptance system must detect and fail unbounded/explosive behavior in fixtures that are expected to settle or remain bounded.

## 9. Stable-scene envelope checks

At minimum, the following mandatory visual-style scenes must have quantitative envelopes in addition to screenshots/recordings:

### 9.1 Five-block tower

After its specified settling interval and before disturbance:

- no body is below the ground beyond allowed penetration tolerance,
- maximum body speed is below a declared settling threshold,
- maximum angular speed is below a declared settling threshold,
- top body remains within a bounded horizontal region relative to tower base,
- total kinetic energy remains below the fixture threshold,
- no body AABB leaves the defined safety region.

### 9.2 Suspension bridge

During the no-impact settling phase:

- link separation error remains bounded,
- no link/body becomes non-finite,
- no endpoint anchor detaches,
- kinetic energy settles below the fixture threshold.

During and after the defined impact:

- joint error remains under a larger bounded threshold,
- no link exceeds the safety-region bound,
- bridge remains connected for the required duration.

### 9.3 Ragdoll/linked-body scene

- every required joint remains live,
- maximum anchor error stays bounded,
- no body obtains non-finite/explosive speed,
- connected components remain connected for the required duration.

### 9.4 Resting/sleeping pile

- awake body count trends to the required settled range,
- sleeping bodies do not spontaneously gain energy,
- no repeated wake/sleep chatter above the fixture threshold without external cause.

These checks are release blocking.

## 10. Independent reference/oracle policy

A validation is meaningful only if its expected answer is independent enough to detect production bugs.

Required policies:

- analytic free-fall/projectile/constant-force/torque references use direct closed-form equations,
- broad-phase and query oracles use brute-force independent enumeration,
- collision geometric oracle cases must not call the same production narrow-phase function for expected results,
- replay equality uses canonical state digest from independently recorded expected run/checkpoint comparisons, not a hard-coded success flag,
- parser invalid-input expectations are fixture metadata, not production parser output echoed back as expected output.

A test whose expected value is produced by the same routine under test is invalid for the corresponding release gate.

## 11. Cross-check and metamorphic acceptance

The release system must execute the mandatory metamorphic comparisons defined elsewhere and aggregate them as first-class release gates.

At minimum:

- scene translation invariance,
- 90-degree scene rotation invariance,
- body creation-order/stable-ID permutation,
- render-cadence independence,
- timestep convergence,
- polygon vertex starting-index/winding normalization invariance,
- query translation/rotation consistency,
- sensor event-stream determinism,
- replay repeated-run determinism.

A metamorphic case must compare actual numeric state/events, not screenshots alone.

## 12. Differential fixed-step schedule checks

For selected deterministic fixtures, release acceptance must compare runs using:

- baseline fixed step,
- half-sized fixed step,
- where practical, quarter-sized fixed step.

The comparison must verify convergence/expected qualitative improvement rather than requiring byte-identical states across different `dt` values.

Any fixture where halving `dt` causes grossly larger error or instability must fail unless the fixture explicitly lies outside the documented stable operating envelope.

## 13. Deterministic replay as a failure reproducer

Every validation path capable of generating a sequence of world-mutating commands must be able to preserve enough information to reproduce the failure.

For command-driven fuzz/E2E/stress failures, the preferred failure artifact is a replay file compliant with `16_QUERY_SENSOR_REPLAY.md`.

For fixture-only failures with no user command stream, the failure artifact must at least contain:

- exact scene fixture,
- seed if any,
- world settings,
- failing fixed-step index,
- state digest before/at failure.

## 14. Failure artifact bundle

Every mandatory automated failure must create or reference a **failure artifact bundle**.

Minimum contents where applicable:

- case ID,
- implementation/build identifier,
- deterministic seed,
- fixture/scene copy or stable fixture identifier,
- replay file if commands are involved,
- nearest prior checkpoint when available,
- failing fixed-step index,
- last N fixed-step state summaries (N at least 16),
- actual metric,
- expected metric,
- tolerance,
- involved body/joint/contact IDs,
- current and previous state digests,
- invariant/anomaly name,
- concise diagnostic message.

For crash/timeout cases, retain the last successfully emitted step/heartbeat and relevant input identity.

## 15. Field-level divergence report

Replay/checkpoint/determinism comparisons must provide more than two unequal hash values when practical.

On a digest mismatch, the verifier must perform or retain a canonical field-wise comparison that identifies the first differing entity/field among at least:

- body existence/ID,
- position,
- angle,
- linear velocity,
- angular velocity,
- sleep state,
- joint state,
- sensor/contact lifecycle state.

The report must include actual and expected values in canonical numeric form.

## 16. Seed retention and randomized tests

Every randomized test must be deterministic from an explicit seed.

Required behavior:

- seed is printed before/with the case result,
- failure bundle stores the seed,
- rerunning the same seed reproduces the same generated inputs,
- failed seeds are never silently discarded,
- mandatory seed range/count is fixed by the relevant specification.

"Random test passed once" without seed traceability is not acceptable evidence.

## 17. Fuzz assertions

The deterministic physics/query/command fuzz suites must assert at least:

- no crash,
- no timeout beyond case threshold,
- no NaN/Inf,
- no invalid live reference,
- no impossible lifecycle transition,
- no invalid AABB/tree invariant,
- no invalid manifold invariant,
- no sensor impulse leakage,
- periodic state digest production succeeds,
- final world can be cleanly destroyed.

Where a fuzz scenario has bounded world construction parameters, it must also assert the configured safety envelopes for speed/position/energy.

## 18. Soak acceptance

The long-run soak test must combine multiple subsystems rather than simulate an empty or trivial world.

A mandatory soak workload must include during its run:

- dynamic bodies,
- contacts,
- at least one distance/revolute joint family,
- sensors,
- periodic spatial queries,
- sleeping/waking,
- periodic force/impulse commands,
- periodic create/delete churn,
- periodic state digest,
- at least one checkpoint save/restore cycle in a dedicated deterministic variant.

Release is blocked by:

- crash,
- NaN/Inf,
- stale reference,
- tree corruption,
- unbounded memory/resource growth beyond documented tolerance,
- impossible sensor/contact lifecycle,
- checkpoint/replay mismatch,
- fixture safety-envelope violation.

## 19. Performance-with-correctness rule

Performance benchmarks do not pass if the workload silently skips required physics work.

Each performance report must include workload integrity counters such as:

- active body count,
- candidate pair count,
- manifold/contact count where expected,
- joint count,
- query count for query benchmark,
- sensor overlap count for sensor benchmark.

A suspicious zero count where the fixture expects nonzero work is a benchmark failure.

## 20. Added mandatory performance workloads

In addition to existing performance cases, v1.0 requires:

### PERF-QUERY-5000

A world with at least 5,000 broad-phase proxies executes a deterministic mixture of:

- ray casts,
- point queries,
- AABB queries.

Report:

- queries/second by query type,
- median and p95 query time,
- average candidates visited if instrumented,
- correctness cross-check sample count against brute-force oracle.

### PERF-SENSOR-1000

A deterministic scene with at least 1,000 active sensor/solid candidate interactions measures:

- broad-phase time,
- narrow-phase time,
- sensor lifecycle processing time,
- emitted event count,
- total physics step time.

### PERF-REPLAY-100K

Replay at least 100,000 fixed steps from a nontrivial command log and report:

- total replay duration,
- steps/second,
- periodic digest count,
- digest mismatches (must be zero).

Performance thresholds may be hardware-sensitive where already allowed, but correctness counters and zero-mismatch rules are not waivable.

## 21. E2E acceptance philosophy

E2E tests must verify observable state after real user workflows, not only that a button can be clicked.

For physics-affecting workflows, E2E must verify at least one of:

- authoritative engine state exposed by diagnostics,
- saved/reloaded scene state,
- state digest,
- emitted event stream,
- exported data whose values are cross-checked.

A screenshot-only E2E assertion is insufficient for physics correctness.

## 22. Visual evidence and quantitative pairing

For visually important physics scenes, acceptance evidence must pair visual output with quantitative report identifiers.

Examples:

- five-block tower recording + tower envelope report,
- collision manifold screenshot + manifold validation case ID,
- bridge recording + joint-error/energy report,
- sensor recording + event-sequence report,
- replay UI screenshot + digest-equivalence report.

This prevents attractive but numerically incorrect demo scenes from serving as sole evidence.

## 23. Evidence manifest integrity

`evidence/INDEX.md` must identify the implementation/build revision used for the evidence set.

The release checker must verify that every required evidence ID has an entry and referenced file exists.

Evidence freshness rules must prevent knowingly mixing old evidence from a materially different implementation with a new release claim.

The exact revision-identification mechanism is project-defined; it must be deterministic and human-readable.

## 24. Required acceptance matrix

The package must maintain a machine- or human-readable matrix mapping every mandatory feature to:

- normative requirement/document section,
- automated case IDs,
- E2E case ID if user-visible,
- acceptance evidence ID if visually relevant,
- release gate.

A mandatory feature with no verification mapping is itself a release-gate failure.

The matrix may be generated, but the generated output must be retained as acceptance evidence.

## 25. No silent degradation

The following behaviors block release:

- disabling a failing mandatory test,
- lowering a required test count without updating the normative package,
- converting a failure to warning,
- silently clamping invalid physics state and continuing as if healthy,
- replacing a difficult workload with a smaller one while keeping the same case ID,
- omitting failing seeds from summary,
- producing a release report before mandatory long-running groups finish,
- declaring a group passed because its executable was not found,
- accepting stale prerecorded report JSON as proof of current execution.

## 26. Harness self-tests

The acceptance system itself must have self-tests proving at minimum that it:

- returns non-zero for an injected failing child case,
- returns non-zero for a skipped mandatory case,
- returns non-zero for a missing executable/report,
- rejects NaN/non-finite mandatory metric,
- records and propagates deterministic seed,
- records timeout status,
- detects malformed child result data,
- detects missing evidence entry,
- can create a failure bundle,
- does not report overall PASS when any gate is blocked.

## 27. Release-blocking result groups

The final release report must expose separate status for at least:

- Core Math/Kinematics,
- Collision/Broad Phase,
- Solver/Contacts,
- Joints,
- UI/Rendering,
- Persistence,
- Force/Trajectory,
- Advanced Physics,
- Spatial Queries,
- Sensors/Lifecycle,
- Replay/Checkpoint,
- E2E,
- Deterministic Fuzz,
- Performance,
- Soak/Lifecycle Stress,
- Dev Tools/Harness,
- Acceptance Evidence,
- Prohibited Substitution Check.

Every group is mandatory.

## 28. Final release status

There are only two normative final states:

- `PASS`
- `BLOCKED`

`PASS` requires:

- all mandatory cases executed,
- zero mandatory failures,
- zero mandatory skips,
- zero harness errors/timeouts,
- all required metrics finite/present,
- all required evidence mapped/present,
- all release gates satisfied,
- no known prohibited substitution.

Any other condition is `BLOCKED`.

## 29. Normative anti-explosion fixture envelopes

The following values are mandatory defaults for the corresponding release fixtures. They are not implementation-selected thresholds.

A test may use stricter tolerances. It may not weaken these values while retaining the same normative case ID.

### 29.1 Tower envelope `ENV-TOWER-01`

Fixture:

- five identical dynamic rectangles,
- each rectangle width `1.0`, height `1.0`, mass `1.0`,
- static horizontal ground,
- gravity `(0,+9.81)`,
- fixed step `1/120 s`,
- restitution `0.0`,
- static friction at least `0.6`, dynamic friction at least `0.4`,
- no external force/impulse during the 15-second pre-disturbance interval.

From simulated time 10 s through 15 s, acceptance requires:

- maximum center horizontal drift from initial tower axis ≤ `0.15` world units,
- maximum linear speed of any tower body ≤ `0.05` world units/s for at least the final continuous 2 s,
- maximum absolute angular speed ≤ `0.05` rad/s for at least the final continuous 2 s,
- maximum sustained penetration ≤ `0.02` world units,
- total tower kinetic energy ≤ `0.01` for at least the final continuous 2 s,
- no body center leaves a safety box extending 3 world units horizontally and 8 world units vertically around the initial tower footprint.

If sleeping is enabled in the normative fixture, all five dynamic boxes must be asleep by 15 s.

After the specified disturbance impulse, the scene is allowed to topple, but all body state must remain finite and no body may exceed the global sanity caps in Section 30.

### 29.2 Bridge envelope `ENV-BRIDGE-01`

Fixture:

- 12–20 dynamic bridge links,
- characteristic link length `1.0`,
- fixed anchors at both ends,
- gravity `(0,+9.81)`,
- fixed step `1/120 s`,
- no motor injecting energy,
- deterministic impact body/impulse defined by the fixture.

Before impact, after an initial 8-second settling interval:

- RMS joint anchor/separation error ≤ `0.02` world units,
- maximum joint error ≤ `0.05` world units,
- maximum link linear speed ≤ `0.10` world units/s over the final continuous 1 s before impact,
- maximum absolute angular speed ≤ `0.20` rad/s over the final continuous 1 s,
- all intended bridge joints remain live.

For 20 simulated seconds after the deterministic impact:

- RMS joint error ≤ `0.03` world units,
- no sustained joint error > `0.10` world units for longer than `0.25 s`,
- all intended bridge joints remain live,
- no bridge link center leaves a safety region expanded by 10 link lengths around the initial bridge AABB,
- no global sanity cap in Section 30 is exceeded,
- after the first 2 post-impact seconds, total kinetic energy must not show a monotonic/exponential growth trend over any subsequent continuous 3-second window.

### 29.3 Linked/ragdoll envelope `ENV-LINKED-01`

Fixture:

- at least 8 dynamic bodies,
- at least 7 required joints,
- characteristic body/link length approximately `1.0`,
- gravity `(0,+9.81)`,
- fixed step `1/120 s`.

During a 30-second deterministic run:

- every required joint remains live,
- RMS anchor error ≤ `0.03` world units,
- no sustained anchor error > `0.10` world units for longer than `0.25 s`,
- no body exceeds `30` world units/s linear speed,
- no body exceeds `80` rad/s absolute angular speed,
- no body leaves the fixture-defined safety region expanded by 15 characteristic lengths,
- all state remains finite.

### 29.4 Sleeping-pile envelope `ENV-SLEEP-01`

Fixture:

- at least 20 eligible dynamic bodies,
- static floor,
- gravity `(0,+9.81)`,
- fixed step `1/120 s`,
- sleeping enabled,
- no external input during settling.

Acceptance:

- at least 80% of eligible bodies are asleep by 20 simulated seconds,
- at least 95% are asleep by 30 simulated seconds unless fixture geometry explicitly creates a permanently unstable body and that exception is named in the fixture,
- no sleeping body changes position by > `1e-5` world units per fixed step while remaining asleep,
- no sleeping body changes angle by > `1e-5` rad per fixed step while remaining asleep,
- total kinetic energy after the 20-second mark must not increase above `0.05` without a wake-triggering event,
- wake/sleep transitions after 20 s must not exceed 10 transitions per body during the next 10 s without external input.

### 29.5 Sensor corridor envelope `ENV-SENSOR-01`

A dynamic test body traverses a non-solid static sensor corridor with identical initial state in two worlds:

- world A contains the sensor,
- world B contains no sensor geometry.

With all other collision interactions absent, after the body has fully exited the sensor region:

- position difference between A and B ≤ `1e-8` world units,
- velocity difference ≤ `1e-8` world units/s,
- angle difference ≤ `1e-8` rad,
- angular velocity difference ≤ `1e-8` rad/s,
- A emits exactly one Begin, the exact required Stay count for overlapped fixed steps, and one End,
- B emits no sensor event.

This directly detects hidden sensor impulse leakage.

## 30. Global sanity caps for mandatory bounded fixtures

Unless a specific analytic/performance fixture explicitly requires larger values and documents why, every mandatory bounded validation, deterministic fuzz world, and nontrivial soak workload must fail immediately if any live dynamic body exceeds:

- absolute position magnitude `10,000` world units from fixture origin,
- linear speed `100` world units/s,
- absolute angular speed `250` rad/s,
- contact penetration `2.0` characteristic world units,
- joint anchor/separation error `5.0` characteristic lengths.

These are emergency sanity caps, not substitutes for the tighter fixture-specific thresholds.

A case that legitimately applies inputs capable of exceeding a global cap must define a stricter physically derived expected envelope before execution; it may not simply disable anomaly monitoring.

## 31. Built-in scene validation profiles

Every built-in scene must have a corresponding validation profile, stored as fixture metadata or an adjacent verification definition, containing at least:

- scene/fixture stable ID,
- deterministic initial state reference,
- simulation duration,
- expected interaction schedule if any,
- safety region,
- maximum allowed speed/angular-speed envelope,
- required contact/joint/sensor count expectations where meaningful,
- settling expectation where meaningful,
- associated automated case IDs.

Built-in scenes intended only as performance stress may use workload-specific envelopes instead of a settling envelope, but they must still define finite-state and safety requirements.

A built-in scene with no automated validation profile is incomplete.

## 32. Solver Inspector release group

The aggregate `releasecheck` report must contain a release-blocking `solver_inspector` group.

It must aggregate:

- `SINSP-01` through `SINSP-20`;
- `E2E-SI-01` through `E2E-SI-06`;
- deterministic trace canonical-hash comparison;
- Inspector instrumentation non-interference digest comparison;
- trace export/schema validation;
- 10,000-capture stress result;
- mandatory Solver Inspector evidence-manifest presence.

The group must report actual measured values for numeric/clamp comparisons rather than only PASS/FAIL.

## 33. Solver anomaly diagnostic linkage

When a release-blocking physics validation, fuzz, stable-scene envelope, or soak case fails because of contact/joint instability, the failure bundle should include a Solver Inspector trace when a deterministic inspected constraint can be identified.

For failures that cannot identify a single responsible constraint, the bundle must still include:

- failing fixed-step index;
- island/body/joint/contact IDs active near the first anomaly;
- state digest before/after divergence;
- replay or seed required to reproduce the failure.

The absence of an automatically selected single constraint is not itself a failure, but manually fabricated solver traces are prohibited.

## 34. Inspector acceptance invariants

Mandatory Inspector runs fail immediately if:

- any recorded required scalar/vector component is non-finite;
- final traced accumulated impulse disagrees with the production final constraint state beyond the defined numeric tolerance;
- reported iteration count disagrees with the configured solver count;
- a friction accumulator exceeds its reported clamp bound beyond tolerance;
- a motor impulse exceeds its configured per-step torque-derived cap beyond tolerance;
- a pinned identity silently resolves to a different contact/joint;
- tracing changes canonical physics state digest;
- identical deterministic captures produce different canonical trace digests.

## 35. v1.0 releasecheck groups

The final aggregate report must additionally contain release-blocking groups:

- `ccd_toi_shape_cast` — all `CCD-*`, `CAST-*`, CCD performance and evidence;
- `collision_filtering` — all `COLF-*`, Matrix E2E/persistence/replay evidence;
- `replay_timeline` — all `TLN-*`, seek performance, marker consistency and evidence;
- `golden_suite` — GOLD-01..GOLD-12 and exactly 12/12 PASS;
- `scope_traceability_audit` — test inventory, traceability, anti-placeholder, dependency/prohibition, evidence completeness.

## 36. Mandatory test-ID inventory

`releasecheck` must compare an expected mandatory test registry with actual discovered and executed results.

Top-level PASS is impossible when any expected ID is absent, duplicate, skipped, flaky, timed out, malformed, or non-PASS.

## 37. v1.0 final aggregate condition

The release report may print `PASS` only when:

- all legacy groups are PASS;
- all groups in Section 35 are PASS;
- no anomaly/fuzz/soak blocker remains;
- required evidence is complete and associated with the tested build;
- all twelve Golden scenarios are PASS;
- final traceability and engineering/prohibition audits pass.

Any other outcome is `BLOCKED`.

## 38. Canonical mandatory ID registry

Expected mandatory case discovery and counting must follow `24_MANDATORY_TEST_REGISTRY.md`.

The aggregate runner must fail closed when a mandatory expected ID is missing or has no unique PASS result.
</file>
