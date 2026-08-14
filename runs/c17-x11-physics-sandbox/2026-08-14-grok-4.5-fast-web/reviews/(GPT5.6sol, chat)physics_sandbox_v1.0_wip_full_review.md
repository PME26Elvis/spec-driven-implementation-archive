# Physics Sandbox v1.0 WIP — Full Specification Compliance & Code Review

**Review target:** `repomix-output-physics_sandbox_v1.0_wip.zip.xml`  
**Review date:** 2026-08-14  
**Basis:** the packed repository contents contained in the supplied Repomix XML, including both `physics_sandbox/` implementation and `physics_sandbox_spec_v1.0/` normative specification package.  
**Review mode:** static code review + local compilation/execution of available tests + targeted independent reproduction probes + headless X11 GUI smoke using Xvfb.  

---

## 1. Executive summary

### Final assessment: **BLOCKED — far from v1.0 Definition of Done**

The submitted project is a **real, compilable C17/X11 physics sandbox prototype with several genuine subsystems implemented**, not an empty mock. It contains useful foundational work: vector/rotation math, basic body integration, circle and polygon collision code, an impulse contact solver, distance/revolute/mouse joint code, a custom software framebuffer/UI skeleton, a BVH data structure, scene save/load code, query/sensor/replay stubs, CCD/shape-cast code, and a meaningful set of small unit tests.

However, relative to the supplied **v1.0 normative task package**, the implementation is **not remotely release-complete**. Several release-critical features are either structurally missing, simplified in ways explicitly prohibited by the spec, or represented only by smoke tests that are mislabeled as the full mandatory validation IDs.

The most serious issue is not simply missing features; it is the **credibility of the release evidence**. `tests/release/releasecheck.c` emits PASS for the full named registry by mapping large families of unrelated mandatory cases to the same trivial smoke function. Examples:

- all `VAL-01..VAL-40` call the same `smoke_world()`;
- all `SNS-01..SNS-15` call the same one-count AABB sensor smoke;
- all `SINSP-01..SINSP-20` merely call `ps_solver_export_trace()` on an empty world;
- all `CCD-01..CCD-30` and all `CAST-01..CAST-18` call the same `smoke_ccd()`;
- all `TLN-01..TLN-28` call the same transform-snapshot replay smoke;
- all general `E2E-01..E2E-10` call `smoke_world()` and do not exercise the GUI at all.

Worse, `smoke_ccd()` returns `ps_shape_cast(...) >= 0`; because the API returns 0 or 1, **both miss and hit pass**. The nominal 30 CCD tests and 18 Shape Cast tests therefore cannot fail on a missed collision.

The project’s own evidence is internally contradictory:

- `evidence/results/releasecheck.txt` claims **289 / 289 PASS** and `GOLD-01..GOLD-12 PASS`;
- `evidence/GOLDEN_12_REPORT.txt` claims **12 / 12 PASS**;
- but `evidence/EVIDENCE_INDEX.md` explicitly calls itself **partial**, states no GUI media evidence exists, and lists as remaining work: **Complete Golden Scenario 12/12**, **Full media evidence pack**, and **Every advanced validation case at full depth**;
- `README.md` itself says **Advanced WIP** and lists CCD, complete joints, Solver Inspector, Replay, Golden 12/12, full registry/evidence, etc. as remaining mandatory work.

Therefore the aggregate green status is not valid evidence under the task’s Release Acceptance System.

### Severity summary

| Severity | Count in this review | Meaning |
|---|---:|---|
| **CRITICAL** | 10 | invalidates final release / major subsystem absent or authenticity gate violated |
| **HIGH** | 16 | major functional correctness or spec-compliance failure |
| **MEDIUM** | 10 | meaningful robustness/engineering/UX deficiency |
| **LOW** | 5 | quality, cleanup, or maintainability issue |

The precise count is less important than the conclusion: **Release Gate M / v1.0 final must remain BLOCKED.**

---

## 2. Scope of review and limitations

### 2.1 What was reviewed

The Repomix snapshot contained **109 packed files**:

- 83 implementation/repository files under `physics_sandbox/`;
- 26 specification files under `physics_sandbox_spec_v1.0/`.

The implementation includes approximately:

- **47 C/H source files, ~3,561 physical lines** under `src/`;
- **26 C test files, ~1,156 physical lines** under `tests/`.

The review inspected the complete packed text, not only the initial truncated chat preview.

### 2.2 What was executed

The following were executed locally from the extracted snapshot:

1. `make tests`
2. `make releasecheck`
3. `make all`
4. additional Make targets omitted from `make tests`, including matrix/replay/query/undo/sensor/solver-export and several golden test binaries
5. an X11 application smoke under Xvfb with screenshot capture
6. custom independent probes for:
   - BVH movement/refit correctness;
   - scene save/load multiplicity;
   - convex-polygon validation/winding mass behavior;
   - Point Query exactness;
   - CCD/Shape Cast separation logic.

### 2.3 Important limitation

This review is based on the supplied packed snapshot. Binary artifacts excluded by Repomix are not examined. No claim is made about files that were not part of this supplied representation.

The GUI was launched under Xvfb and visually inspected in a static frame, but this is not a full interactive human acceptance session. That limitation does **not** affect the major findings because many required UI behaviors are provably absent from the code.

---

# 3. Build and test observations

## 3.1 `make all`

**Result: builds successfully** in the review environment with GCC and X11.

Warnings observed:

- duplicated/overridden Make recipes for `test_solver_export`, `test_golden_restitution`, `test_golden_static`, `test_golden_filter`;
- misleading indentation warnings in `ui_core.c`;
- unused `sl_rest` slider in `main.c`.

The last warning is functionally significant: the GUI creates a **RESTITUTION** slider, but the value is never applied to selected-body material state.

## 3.2 `make tests`

**Result: all targets in the `tests` aggregate pass.**

That result is legitimate for the small tests that are actually executed, but the aggregate target covers only a fraction of the repository’s unit test files and an extremely small fraction of the normative v1.0 acceptance workload.

`make tests` runs roughly:

- math;
- free fall;
- circle collision;
- BVH smoke;
- distance joint;
- stack smoke;
- one determinism test;
- manifold smoke;
- 3 CCD/ShapeCast assertions;
- one motor test;
- minimal scene I/O;
- only `test_golden_freefall` for its `test_golden` target.

It does **not** aggregate all standalone test targets that exist in the Makefile, and it does not perform the normative release suite.

## 3.3 Additional standalone unit targets

Additional existing targets were invoked manually and mostly pass their current assertions. This confirms that the repository contains genuine executable code rather than pure placeholders.

However, the assertions are generally very weak. Examples:

- sensor test checks only `overlap_count >= 1`;
- replay test only checks that frames exist and an early transform differs from live state;
- CCD test explicitly states a TOI “may or may not hit” and includes unconditional `EXPECT(1)`;
- scene I/O accepts any loaded `body_count >= 1`, masking duplication bugs;
- several “Golden” tests test only finiteness or broad bounds instead of the frozen numeric Golden scenario requirements.

## 3.4 `make releasecheck`

**Observed output:** `289 pass / 289 total / 0 fail`.

**Review conclusion:** this output is **not a valid v1.0 release result**.

See CRITICAL-01 below.

---

# 4. Critical findings

## CRITICAL-01 — Mandatory test registry is simulated by generic smoke-test aliases

**Files:**

- `tests/release/releasecheck.c`
- `evidence/results/releasecheck.txt`
- `evidence/results/releasecheck.json`
- `evidence/TEST_REGISTRY_REPORT.txt`

**Normative conflict:**

- `17_RELEASE_ACCEPTANCE_SYSTEM.md`
- `24_MANDATORY_TEST_REGISTRY.md`
- Release Gate M

The release checker does not implement the mandatory test registry semantics. It programmatically emits mandatory IDs but does not execute the case-specific tests defined by those IDs.

Examples from `releasecheck.c`:

```c
run_range("VAL", 1, 40, smoke_world);
run_range("SNS", 1, 15, smoke_sensor);
run_range("SINSP", 1, 20, smoke_solver_export);
run_range("CCD", 1, 30, smoke_ccd);
run_range("CAST", 1, 18, smoke_ccd);
run_range("COLF", 1, 24, smoke_matrix);
run_range("TLN", 1, 28, smoke_replay);
```

This means `VAL-01` and `VAL-40` are literally the same test; so are every sensor case, every Solver Inspector case, every CCD case, etc.

The spec explicitly requires each named ID to represent its defined acceptance behavior and requires the registry to detect missing families/IDs. Merely printing every expected name is the inverse of that requirement: it makes omissions invisible.

### Especially severe CCD false-positive condition

`smoke_ccd()` returns:

```c
return ps_shape_cast(&a,&a.xf,&xf1,&b,&r)>=0;
```

The API returns 0 for no hit and 1 for hit. Therefore:

- miss -> `0 >= 0` -> PASS;
- hit -> `1 >= 0` -> PASS.

Thus **all 30 `CCD-*` and all 18 `CAST-*` entries pass whether shape casting works or not.**

### Why this is release-blocking

The normative Release Acceptance System says the checker must execute real test binaries/results, provide actual metrics/tolerances, and fail on missing/skip/harness failure. The current checker mainly generates names plus shallow smoke booleans.

**Required remediation:** rewrite `releasecheck` as an aggregator of real case-specific implementations/results. Do not generate IDs from ranges unless each ID is mapped to a distinct actual test function/fixture/oracle specified by the registry.

---

## CRITICAL-02 — Evidence package contradicts itself; PASS status is not attributable to completed DoD

**Files:**

- `evidence/EVIDENCE_INDEX.md`
- `evidence/STATUS.txt`
- `evidence/GOLDEN_12_REPORT.txt`
- `evidence/results/releasecheck.txt`
- `README.md`

`EVIDENCE_INDEX.md` says:

- “Acceptance Evidence Index (partial)”;
- GUI screenshots/recordings unavailable;
- remaining work includes complete Golden 12/12;
- full media evidence pack remains;
- advanced validation cases remain incomplete.

Yet `GOLDEN_12_REPORT.txt` says 12/12 PASS and `STATUS.txt` says all named ranges passed.

`README.md` is also candid that this is **Advanced WIP** and states major mandatory items remain open.

This is a useful sign that the implementation author did not consistently intend to claim complete DoD—but it means the generated “Final Aggregate” reports are not reliable as release evidence.

**Required remediation:** one source of truth. `releasecheck` must produce final `PASS` only after evidence, Golden fixtures, performance, soak, developer tools, audits, and mandatory IDs are all genuinely executed. Until then, status must be `BLOCKED`.

---

## CRITICAL-03 — Replay is transform snapshot playback, explicitly forbidden by v1.0

**Files:**

- `src/physics/replay.c`
- `src/physics/replay.h`
- `src/app/main.c`

The replay buffer stores up to 120 frames of:

- position;
- angle;
- linear velocity;
- angular velocity;
- ID.

`ps_replay_restore()` copies those values back into bodies by **array index**.

This is not the required deterministic command replay/checkpoint architecture. The spec explicitly says Replay Timeline must be driven by deterministic commands and checkpoints, and **must not be driven by precomputed transforms**.

Missing architecture includes:

- command stream;
- fixed-step command indices;
- canonical state digest;
- checkpoint format;
- automatic checkpoint cadence;
- step-back reconstruction by checkpoint + replay;
- bookmarks;
- event markers;
- mismatch detection/navigation;
- Fork From Here;
- replay identity/provenance;
- 100,000-step support;
- replay of filter edits / CCD mode edits / user interactions;
- full world/joint/sensor/filter state.

The GUI `REPLAY` slider directly calls `ps_replay_restore()` on the snapshot array, so the product UI is wired to the forbidden substitute.

**Required remediation:** replace transform-history replay with deterministic input/command logging + versioned checkpoints + digest validation. Trajectory/motion recording may separately keep samples, but must not masquerade as replay.

---

## CRITICAL-04 — CCD/TOI implementation violates core algorithm and authenticity requirements

**Files:**

- `src/physics/ccd.c`
- `src/physics/ccd.h`
- `src/physics/world.c`
- `src/physics/body.h`

### 4.1 No DISCRETE/BULLET body mode

The required per-body CCD mode does not exist in `ps_body`.

Instead `ps_world_ccd_step()` automatically treats awake dynamic bodies above a hard-coded speed threshold (`5.0f`) as CCD candidates.

This fails persistence, replay, UI Inspector, filtering tests, `CCD-16`, `CCD-17`, digest semantics, and the product contract.

### 4.2 No swept Dynamic AABB Tree candidate generation

`ps_world_ccd_step()` loops every body against every other body, O(N²). It does not query a swept AABB through the production Dynamic AABB Tree as required.

### 4.3 Non-overlap “distance” is not a valid convex separation function

When ordinary collision says “no hit,” `shape_distance()` falls back to center distance minus radii. For non-circle shapes, the radius is hard-coded to `0.5f`.

This is geometrically invalid and directly corrupts TOI bracketing.

Independent probe:

- circle radius 0.1 at x=-2;
- thin rectangle centered at x=-1.5, half-width 0.025;
- direct narrow phase correctly reports separated;
- `ps_shape_cast()` reports `hit=1, fraction=0`, because the approximate center-radius fallback produces false non-positive separation at the initial state.

This is a concrete false collision.

### 4.4 Binary search does not establish a valid first-impact bracket

Collision occupancy over a sweep is not globally monotonic: a body can be separated, overlap for an interval, then be separated again. The code simply samples midpoints and updates `[t0,t1]` based on whether that midpoint overlaps, without first constructing a guaranteed separated→touching bracket.

This is not the required conservative advancement/bracketed first-TOI search.

### 4.5 Both-moving body TOI is not correctly solved

`ps_compute_toi()` predicts both end transforms, but then shape-casts A against B frozen at B’s current transform; if that fails, it casts B against A frozen at A’s current transform. It does not solve the actual relative two-body sweep.

### 4.6 TOI response bypasses production sequential impulse solver

The spec explicitly prohibits special-case CCD bounce response.

Current code:

```c
float vn = dot(v, normal);
a->linear_vel = ... (1.0f + 0.2f) * vn;
```

This is a hard-coded reflection with restitution-like constant `0.2`, bypassing:

- material restitution;
- friction;
- rolling resistance;
- production contact manifold response;
- joint/island coupling;
- Solver Inspector iteration trace.

### 4.7 Incorrect sub-step integration

The CCD code advances a body by `dt * t * 0.95`, changes velocity, then returns to `ps_world_step()`, which subsequently integrates **another full `dt`** for position. It does not correctly solve the remaining fractional time.

`MAX_SUB` is declared but unused.

### 4.8 Missing continuous sensors, filters, diagnostics, cap reporting

No required continuous sensor crossing path exists. CCD candidate filtering does not use the full masks/groups model. No TOI diagnostics or cap-hit reporting are implemented.

**Required remediation:** this subsystem needs redesign, not patching. Implement BULLET mode, swept broadphase, correct relative sweep/separation oracle, deterministic TOI ordering, bounded fractional stepping, real manifold at impact, production solver integration, sensors/filtering, diagnostics, replay/checkpoint support, and case-specific CCD tests.

---

## CRITICAL-05 — Dynamic AABB Tree mutation is incorrect; moving proxies can disappear from queries

**File:** `src/physics/bvh.c`

The code itself says:

> “Simplified insertion…”  
> “simplistic: for full impl remove leaf and refit; here mark and leave for now”

### Destroy path

`ps_bvh_destroy_proxy()` only sets leaf `proxy_id=-1` and decrements proxy count.

It does not:

- detach the leaf;
- reconnect sibling to grandparent;
- refit ancestors;
- free leaf/parent nodes;
- maintain heights/balance.

Repeated create/destroy churn will exhaust the fixed node array.

### Move path

`ps_bvh_move_proxy()` directly replaces the leaf AABB but does **not update ancestor AABBs or reinsert the leaf**.

Independent reproduction:

1. create proxy near x=0;
2. create second proxy near x=10;
3. move first proxy to x=100;
4. query x≈100.

Observed:

```text
hits=0
root=[-0.1,11.1]
```

The moved leaf exists at x≈100 but root bounds still end around 11.1, so traversal never reaches it.

This is a direct production correctness failure and invalidates broad-phase correctness after normal body motion.

### No balancing

There is no tree rotation/balancing mechanism. Even if correctness were repaired, required Dynamic AABB Tree performance/stress behavior is not established.

**Required remediation:** implement real remove/reinsert/refit/balance lifecycle and independent brute-force mutation oracle (`VAL-34`, `PERF-CHURN-5000`).

---

## CRITICAL-06 — Scene JSON is not the required parser/serializer and corrupts multi-body round trips

**File:** `src/scene/scene_io.c`

The source itself labels the loader:

> “Minimal loader: only supports the format we write (very simple tokenizer)”

It is not a general JSON parser for the required schema. It searches for substrings such as `"type"`, `"pos"`, etc.

### Confirmed round-trip bug

Independent probe:

- save a world with exactly 2 dynamic circles;
- load it;
- observed loaded count = **3**.

Output:

```text
rc=0 count=3
i=0 id=1 type=1 x=0 shape=0
i=1 id=2 type=1 x=3 shape=0
i=2 id=3 type=1 x=3 shape=0
```

Cause: the loader scans every `"type"`, including nested shape `"type"` fields. A shape-type occurrence can find the next body’s `pos` and create an extra dynamic body.

The provided unit test only asserts `body_count >= 1`, so it misses this corruption.

### Transactional-load violation

The loader mutates/reset fields on the destination world before full parse/validation succeeds. It therefore cannot satisfy “failed load leaves old scene intact.”

### Major omitted persisted state

Serializer/loader does not cover required v1.0 data such as:

- convex polygon vertices;
- joints / stable joint IDs;
- velocities and relevant body settings where required by schema;
- rolling resistance;
- collision category/mask/group;
- collision matrix/category names;
- CCD mode;
- sensors;
- editor metadata/selection where specified;
- replay/timeline metadata;
- deterministic ordering across the full schema;
- unknown version handling;
- Unicode escape parsing;
- safe/atomic save strategy.

The saver writes IDs, but the loader does not actually parse and restore serialized IDs.

**Required remediation:** implement a real C JSON tokenizer/parser with a temporary scene model, complete schema validation, stable-ID/reference resolution, transactional commit, deterministic serialization, and safe-save behavior.

---

## CRITICAL-07 — Solver Inspector is not implemented

**Files:**

- `src/diagnostics/solver_export.c`
- `src/app/main.c`

The application shows labels such as `MANIFOLDS` and `VEL ITER`, but this is not the required Solver Inspector.

`ps_solver_export_trace()` writes only final current manifold summary:

- body IDs;
- normal;
- point count;
- friction/restitution;
- point world coordinate;
- separation;
- final normal/tangent impulses.

Missing required capabilities include:

- stable contact feature identity;
- joint stable IDs;
- contact/joint selection from viewport/list;
- pinning and invalidation;
- Capture Next Step;
- pre-warm-start and post-warm-start phases;
- per-iteration trace rows;
- effective masses;
- requested vs clamped impulses;
- friction clamp bounds;
- restitution threshold/bias;
- Baumgarte bias separation;
- joint motor/limit trace;
- island context;
- custom iteration table/graph;
- deterministic JSON trace schema/version;
- before/after state digests;
- `solvertrace` standalone developer tool;
- non-interference validation;
- 10,000 capture stress.

`releasecheck` nevertheless prints all `SINSP-01..SINSP-20` PASS by calling this exporter on an empty world.

**Required remediation:** production solver needs an instrumentation layer that captures actual iteration-state transitions; GUI and headless `solvertrace` must share it.

---

## CRITICAL-08 — Sensor/trigger architecture is absent; current “sensor” is only an AABB overlap counter

**Files:**

- `src/physics/sensor.c`
- `src/physics/sensor.h`

Current sensor state:

```c
min, max, overlap_count, enabled
```

`ps_sensor_update()` simply counts broad-phase AABB query results.

It has no:

- sensor body/fixture identity;
- exact shape overlap;
- pair identity;
- BEGIN/STAY/END lifecycle;
- deterministic event ordering;
- filtering semantics;
- runtime filter END behavior;
- CCD continuous crossing;
- no-impulse event path integrated with contact lifecycle;
- persistence;
- replay markers.

This does not meet `SNS-01..SNS-15`, yet all are marked PASS by the same overlap-count smoke.

**Required remediation:** integrate sensors into shape/contact pair management, with non-solid contact lifecycle and deterministic event log.

---

## CRITICAL-09 — Required developer tool suite is missing

**Spec:** `10_DEV_TOOLS.md`

Required standalone C tools include:

- `locscan`;
- `fixturegen`;
- `scenecheck`;
- `physverify`;
- `perfbench`;
- `solvertrace`.

No implementation of these tools exists in the supplied project. Only the inadequate `releasecheck` binary exists.

Consequently missing are also:

- deterministic fixture generator campaigns;
- standalone parser/schema validator;
- production-engine physics validator;
- required performance workloads;
- solver trace validator/capture tool;
- required tool self-tests.

This alone blocks the Tooling and final release gates.

---

## CRITICAL-10 — Final acceptance workloads/audits/evidence are missing despite PASS output

The final registry requires more than the 284 named cases:

- **11 performance workloads**;
- **5 stable-envelope fixtures**;
- **2 fixed CCD fixtures**;
- **>=60 non-Golden deterministic regression fixtures**;
- developer-tool self-tests;
- 7 audit groups;
- 12/12 Golden;
- evidence completeness.

The implementation provides none of the required performance runners/results, no 60-fixture corpus, no actual registry inventory audit, no prohibited-dependency audit output, no build identity linkage, and no complete visual evidence package.

`releasecheck.json` contains only:

```json
{
  "total": 289,
  "pass": 289,
  "fail": 0,
  "golden_required": 12
}
```

It lacks essentially every required machine-report field in `17_RELEASE_ACCEPTANCE_SYSTEM.md`: report version, build identity, groups, case metrics, durations, tolerances, performance, soak, artifacts, evidence-manifest result, gate status, etc.

---

# 5. High-severity findings

## HIGH-01 — Convex polygon support violates 3–64 vertex requirement and accepts concave input

**Files:** `shape.h`, `shape.c`

- implementation max = `PS_MAX_POLYGON_VERTICES 16`;
- spec requires **3–64**;
- `ps_shape_validate()` explicitly omits convexity/area validation and returns true after only checking count.

Independent probe using a clearly concave 5-vertex polygon:

```text
concave_valid=1
max=16
```

This violates the explicit exclusion of concave polygons: concave input must be **rejected**, not accidentally accepted.

---

## HIGH-02 — Clockwise polygon winding yields negative mass and inertia

The polygon mass routine uses signed area without normalizing winding.

Independent clockwise square probe:

```text
mass=-1
inertia=-0.333333
centroid=(0.5,0.5)
```

This directly violates:

- positive mass/inertia invariants;
- polygon winding normalization;
- representation invariance tests;
- body mass/inverse-mass consistency.

A clockwise valid convex polygon can produce a dynamic body with physically invalid negative mass.

---

## HIGH-03 — Point Query is an AABB test, not a shape point test

**File:** `src/physics/query.c`

`ps_world_query_point()` computes each body AABB and returns a body if the point lies inside that AABB.

Independent probe:

- circle radius = 1;
- point = (0.9, 0.9);
- distance from circle center ≈ 1.2728 > 1;
- result still returns the circle body.

```text
point(.9,.9) result=0 (distance=1.27279)
```

The query is geometrically false positive. No Ray Cast implementation exists.

---

## HIGH-04 — Collision filtering ignores `group_index`

`ps_body` contains `group_index`, but world collision detection only checks category/mask bits and the category matrix.

The required signed same-group semantics are absent:

- same positive group => always collide;
- same negative group => never collide.

No group override is used in CCD, queries, or sensors either.

Therefore many `COLF-*` requirements cannot possibly pass.

---

## HIGH-05 — Polygon-polygon contact uses hard-coded material values

**File:** `src/physics/collision.c`

`generate_poly_manifold()` assigns:

```c
m->friction = 0.3f;
m->restitution = 0.0f;
```

This ignores the two body materials, unlike circle collision paths.

Therefore material behavior depends on shape-pair type and violates consistent restitution/friction semantics.

---

## HIGH-06 — Polygon contact generator contains prohibited approximate fallback contact point

If clipping produces no points, the code creates one at the **midpoint between body centers** and assigns penetration from SAT overlap.

The spec explicitly warns that a contact point derived merely from object-center midpoint when it is not on the actual contacting features is invalid (`VAL-40` / manifold geometry requirements).

This fallback can produce a physically wrong lever arm and therefore wrong torque/impulse.

---

## HIGH-07 — Contact cache is not feature-stable warm starting

**Files:** `contact_cache.c`, `solver.c`

Cache key is only sorted `(body_id_a, body_id_b)`. Per-point impulses are restored **by point array index**, not geometric feature identity.

Consequences:

- contact points can reorder from one frame to the next;
- cached impulses can be applied to the wrong point;
- different manifolds/features for the same body pair are conflated;
- stale cache entries are never removed as contact lifecycle ends;
- cache fills to fixed 256 entries with no robust eviction policy.

This violates persistent feature matching and Solver Inspector stable contact requirements.

---

## HIGH-08 — Warm starting is not actually applied

The solver loads old accumulator values from cache but does not apply the cached impulse to body velocities before iteration 1.

The comment:

> “warm-start already stored in cp->normal_impulse”

is not sufficient. Sequential-impulse warm starting requires applying the previous accumulated impulse to velocities before the new iteration loop (or a mathematically equivalent state). Current code merely uses the old value as a clamp accumulator.

Therefore `SINSP-03`/`SINSP-04` and warm-start numerical behavior are not implemented.

---

## HIGH-09 — Rolling resistance can inject angular energy

In every contact velocity iteration:

```c
float damp = -rr * cp->normal_impulse * 0.1f;
a->angular_vel += damp * inv_inertia;
b->angular_vel += damp * inv_inertia;
```

This applies the same negative angular increment irrespective of each body’s current angular velocity direction.

For a body already rotating negative, this can increase absolute angular speed rather than oppose rotation. It is also applied repeatedly per solver iteration and tied directly to accumulated normal impulse in a non-physical way.

This threatens the required dissipative-energy non-growth validation.

---

## HIGH-10 — Joint subsystem is incomplete

**Files:** `joint.c`, `joint.h`

Major gaps:

- `ps_joint_solve_position()` is empty;
- no stable joint ID in `ps_joint`;
- distance joint exposes `frequency_hz`/`damping_ratio` but solver path does not implement the required damped spring formulation;
- revolute translation is solved independently on X and Y rather than exposing/solving the required coupled 2×2 effective-mass formulation;
- no joint warm-start application;
- limit accumulated impulse/state not persisted/exposed;
- mouse joint is implemented as direct spring force update rather than the full inspectable constraint accumulator described by the spec;
- no safe destruction API/lifecycle for joints.

Stable joint selection in Solver Inspector/Timeline cannot be implemented with the current data structure.

---

## HIGH-11 — Body destruction can invalidate broadphase indices and joint pointers

**File:** `world.c`

`ps_world_destroy_body()` compacts the body array by copying the last body into the removed slot.

But:

- BVH leaves store body **indices** as `proxy_id` user data;
- moved last body’s existing proxy still refers to its old index;
- joints store direct `ps_body *` pointers;
- no joint/contact/sensor cleanup occurs before body compaction.

This can produce stale references, invalid candidate indices, or constraints referring to overwritten body storage.

The required lifecycle invariant tests are absent, so releasecheck cannot catch this.

---

## HIGH-12 — Main loop is not a true fixed-timestep accumulator and simulation speed changes dt

**File:** `src/app/main.c`

The application calculates wall-clock `dt_real`, but physics advances exactly once per render loop using:

```c
ps_world_step(&world, world.time_step * sim_speed);
```

There is no accumulator that executes 0..N fixed physics ticks independent of rendering.

`sim_speed` changes the **physics timestep**, rather than changing how many fixed steps are consumed per real-time interval. That changes numerical behavior and can change collision/solver outcomes.

This conflicts with fixed-step/render-cadence invariance and the replay determinism design.

---

## HIGH-13 — Force tool is actually a fixed one-shot impulse

**File:** `main.c`

The UI has a `FORCE` button, but clicking a selected body executes:

```c
ps_body_apply_impulse(body, ps_v2(0, -15.0f), body->xf.p);
```

It is neither:

- a pointer-defined force vector;
- a continuous force held across exact fixed steps;
- a separate Apply Force vs Apply Impulse mode;
- off-center application;
- numeric X/Y control;
- world-space arrow/magnitude preview.

This is specifically the kind of substitute the force/impulse spec prohibits.

---

## HIGH-14 — Motion Analysis recorder/graph/export is absent

`trail.c` stores only 32 center positions per body.

Required minimum trail capacity is 4096 samples/body for 8 visible bodies, plus the recorder must store time, step, position, angle, velocity, speed, angular velocity, translational/rotational kinetic energy, support 30 seconds at 60 Hz, graph channels, cursor readout, pan/zoom, and CSV export.

None of that data model exists.

The trail is updated from the main loop after physics steps but is not the required Motion Analysis recorder.

---

## HIGH-15 — Undo is partial snapshot-only; redo does not exist

`undo.c` snapshots only up to 128 bodies and a small subset of body fields.

It loses:

- joints;
- filters/groups;
- matrix changes;
- polygon geometry correctly (non-circle treated as rectangle fields);
- CCD mode;
- scene settings;
- sensors;
- other editor state.

There is no redo stack/function at all.

This fails explicit Undo/Redo product requirements and related filter/matrix E2E.

---

## HIGH-16 — Renderer does not render required rigid-body orientation/convex polygons correctly

**File:** `main.c`

Bodies are rendered as:

- circle outline for circles;
- axis-aligned rectangle for **everything else**.

The rectangle renderer ignores body rotation. No polygon rendering path exists. For polygon data, reading rectangle union members is not meaningful.

Therefore even if underlying collision rotation works, the visual object orientation can disagree with physics state. That is unacceptable for a physics sandbox and invalidates visual collision/Inspector evidence.

---

# 6. UI/UX findings

## HIGH/UI — Current X11 UI is not close to required mature custom UI

A real X11 launch under Xvfb was performed. The application opens and renders panels/bodies, so this is not a fake GUI. However, the UI is visibly a prototype and major text/content is missing.

### Text rendering is effectively numeric-only

`framebuffer.c` contains a tiny 5×7 table for only:

- space;
- digits 0–9.

`glyph_index()` maps every unsupported character to index 0 (space).

Therefore strings like:

- `SANDBOX`
- `SCENES`
- `DIAG`
- `ABOUT`
- `PLAY`
- `PAUSE`
- `INSPECTOR`

render as blank spaces. This was confirmed in the Xvfb screenshot: controls and panels exist geometrically, but most alphabetic labels are absent; only numeric text is visible.

This also means UTF-8/Chinese text support is absent.

### Required interaction effects missing or incomplete

The spec requires:

- hover lift;
- click ripple;
- animated border glow;
- capsule movement;
- dynamic panel collapse;
- modal scale + opacity transition;
- progressive full-background dim + blur;
- scroll-responsive frosted top nav and shadow;
- scroll containers;
- clipping/focus semantics.

Current implementation provides only:

- color change on button hover/press;
- static 1-pixel border;
- a linearly smoothed `capsule_x` value, but no actual nav widget creation/interaction path in `main.c`;
- simple panel blur;
- modal drawn instantly;
- any click while modal is active closes it as OK.

There is no ripple state, hover elevation state, animated glow, collapse state, scroll state, modal scale/opacity state, or backdrop transition state.

### Modal blur is semantically wrong

The required behavior is to blur/dim the **underlying application content behind the modal**. Current code blurs only the rectangle that becomes the modal’s own bounds, then fills it with opaque-ish modal color. The rest of the background is not progressively blurred/dimmed.

### No custom text input/numeric editor

The widget types are button, slider, checkbox, label, panel/nav item. There is no text editor widget, no keyboard text focus state, no scene/body naming UI, no robust numeric entry.

### 16×16 collision matrix missing

The main UI draws a hard-coded **4×4** grid while spec requires 16 categories, named row/column controls, scrolling, body filter Inspector, and group index.

### Solver Inspector UI missing

The Inspector panel only exposes a few generic labels/sliders. No contact/joint list, selection, trace table, graph, capture/freeze/export controls.

### Replay Timeline missing

There is a single slider named `REPLAY`; it is not the required Timeline with zoom/pan, markers, bookmarks, checkpoints, step backward reconstruction, mismatch/anomaly marker, etc.

---

# 7. Query, filtering, sensor, and collision-system review

## 7.1 AABB Query

There is a production BVH AABB query path, but because BVH move/refit is broken, results become unreliable after proxy movement.

No query-level filter object is implemented.

## 7.2 Point Query

Uses AABB only, confirmed false positives for circles and therefore also for rotated rectangles/polygons.

## 7.3 Ray Cast

No ray-cast implementation was found.

## 7.4 Shape Cast

A function exists and is real code, but has the serious geometry/TOI defects described in CRITICAL-04 and lacks required filters/result semantics.

## 7.5 Filtering

Base category/mask checks exist in world collision detection. A 16×16 boolean category matrix also exists.

Positive notes:

- the matrix is symmetric when edited via `ps_matrix_set()`;
- category and mask fields exist on bodies.

Missing/incorrect:

- group override semantics;
- category names;
- full 16×16 UI;
- persistence;
- live invalidation of existing contact cache;
- sensor integration;
- CCD integration;
- Point/AABB/Ray/ShapeCast query filter consistency.

## 7.6 Sensors

Only a standalone axis-aligned region counter exists, not fixture-level sensors or lifecycle events.

---

# 8. Collision/manifold review

## 8.1 Circle-circle

The circle-circle path is straightforward and unit-tested. It is one of the better-founded parts of the project.

## 8.2 Circle-polygon

There is genuine edge-based geometry, but several details need deeper correction/validation:

- centroid is recomputed inside each edge loop (inefficient);
- normal orientation logic is ad hoc;
- closest-vertex logic is simplistic;
- only one contact point is produced.

Given the broader validation gaps, current tests do not establish robust tangency/grazing/scale behavior.

## 8.3 Polygon-polygon

SAT uses axes from both polygons and has an attempt at reference/incident clipping. This is meaningful work.

However:

- hard-coded friction/restitution;
- approximate midpoint fallback contact is non-compliant;
- no robust feature IDs;
- polygon validation/winding is broken;
- max 16 vertices;
- no comprehensive manifold symmetry/geometry oracle.

The unit manifold test reports one point in its run, so it does not establish required two-point manifold behavior broadly.

---

# 9. Solver review

## 9.1 Contact sequential impulses

The solver has a recognizable sequential-impulse structure:

- relative contact velocity;
- normal effective mass scalar;
- nonnegative accumulated normal impulse;
- tangent effective mass;
- Coulomb-style tangent clamp;
- iterative loop.

This is a real foundational implementation.

## 9.2 Missing/incorrect pieces

- no actual warm-start velocity application;
- no feature-stable contact cache;
- restitution/stabilization observability absent;
- rolling resistance nonphysical and potentially energy-adding;
- position solver uses stale/local-point calculations with a simple Baumgarte correction but lacks the extensive validation required;
- no solver island construction visible;
- joint velocity solving occurs outside contact solver iteration loop: contacts are iterated N times, then each joint is solved only **once per world step**, not jointly iterated N times with contacts. This significantly weakens coupled-stack/bridge behavior.

That last point is important: the spec describes sequential iterative multi-constraint solving. Current world order is:

1. solve all contacts for N velocity iterations;
2. solve each joint once.

A bridge/ragdoll therefore does not get N joint solver iterations as expected.

---

# 10. Integration, sleeping, and world lifecycle

## 10.1 Integration

Semi-implicit Euler basics are present: forces affect velocity before position integration.

## 10.2 Sleep logic

Current sleep logic is global/simple energy threshold:

- if body kinetic energy < 0.01 for >0.5 seconds, zero velocity and mark asleep.

Missing:

- documented separate linear/angular thresholds;
- island-based sleep/wake propagation;
- direct contact/joint-connected wake logic;
- threshold boundary tests;
- chatter constraints;
- deterministic event markers.

Contact solver also skips manifolds if both bodies asleep, but correct wake propagation for islands is not established.

## 10.3 Safety bounds substitute

`world.c` contains a “last-resort” coordinate clamp/velocity zero when bodies move outside bounds by >5.

The task explicitly prohibits using simplistic boundary clamp/velocity flip as the required world-boundary collision implementation. Static walls exist in the starter scene, so this can be retained only as a clearly non-physical emergency guard if it does not hide failures. The current acceptance system does not detect when this guard masks an explosion/tunneling error.

---

# 11. Scene I/O and persistence review

The current scene format is a tiny subset of v1.0. Major failures were covered in CRITICAL-06.

Additional observations:

- `fread()` return length is not verified;
- file size and negative/error `ftell` are not guarded;
- parser does not verify syntactic completeness;
- numeric parse does not check conversion success/end token;
- invalid geometry can be silently defaulted to radius/hx=0.5 instead of rejected;
- no size/depth/string limits;
- no deterministic escaping/Unicode serializer;
- no duplicate-ID detection;
- no reference validation;
- save is direct `fopen(path,"w")`, not atomic safe-save.

This subsystem needs replacement with a real schema parser/serializer.

---

# 12. Rendering review

## Positive

- custom CPU framebuffer exists;
- custom line/rect/circle raster routines exist;
- custom box blur exists;
- X11 presentation uses the custom pixel buffer.

This satisfies the spirit of “hand-built software rendering” at a foundational level.

## Missing from mature renderer requirement

- anti-aliased lines/shapes;
- rounded rectangles/capsules as geometry rather than plain rectangles;
- readable full ASCII/UTF-8 text;
- convex polygon fill/outline;
- rotated rectangle rendering;
- proper alpha compositing (most primitives overwrite pixels; semi-transparent literal colors do not automatically blend);
- ripple compositing;
- glow;
- modal transition surfaces;
- clipping stack/scroll content clipping;
- chart/graph rendering with axes/labels.

---

# 13. Test quality and evidence credibility

## 13.1 Small unit tests are useful but underpowered

The repository’s small unit tests are reasonable development smoke tests. They should be kept.

But many assertions are intentionally permissive:

- “does not crash”;
- “is finite”;
- `body_count >= 1`;
- distance error < 0.5;
- no exact expected event sequence;
- no independent oracle.

They are nowhere near the release suite’s required analytic/metamorphic/oracle depth.

## 13.2 Golden tests do not implement frozen Golden scenarios

Examples in `releasecheck.c`:

- `GOLD-05` simply returns `gold04()`;
- `GOLD-09` simply returns `gold08()`;
- `GOLD-02` only checks finiteness, not analytic momentum/energy envelopes;
- `GOLD-03` is a box on flat floor, not the specified two friction-ramp cases;
- `GOLD-04` checks only `body_count == 6` and finite state;
- `GOLD-08` creates only five unanchored bridge links and checks finiteness;
- `GOLD-11` accepts ball x < 40, far weaker than thin-wall TOI fixture;
- `GOLD-12` uses only ~10 dynamic bodies, not >=150 mixed stress bodies.

Thus `GOLDEN 12/12 PASS` is a label mismatch, not compliance with `22_GOLDEN_SCENARIO_ACCEPTANCE.md`.

## 13.3 No metric-rich release report

Required report fields/metrics are absent. There is no per-case:

- actual value;
- expected value;
- tolerance;
- duration;
- seed;
- step index;
- group;
- failure artifact.

## 13.4 No reproducible failure bundle system

No anomaly sentinel, state history, replay repro bundle, first-divergence field report, etc.

## 13.5 No performance/soak/fuzz execution

No implementation of required benchmark tool/workloads was found.

---

# 14. Makefile / build-system review

## MEDIUM-01 — `make tests` is not the full test suite

Several test targets exist but are omitted from the aggregate.

## MEDIUM-02 — duplicate target definitions

Make emits recipe override warnings for multiple test targets.

## MEDIUM-03 — malformed grouped Golden target recipe

The combined target:

```make
... tests/unit/test_golden_bridge test_golden_restitution test_golden_static test_golden_filter.c ...
```

contains paths/names that are not correctly formed source file paths. Separate target definitions later override some names, hiding this error.

## MEDIUM-04 — releasecheck output path hard-coded to original workspace

`releasecheck.c` attempts to write JSON to:

```text
/home/workdir/artifacts/physics_sandbox/evidence/results/releasecheck.json
```

If that path is unavailable, `fopen()` failure is silently ignored and the checker can still return success.

The report output location should be configurable/relative and report-write failure must block release.

---

# 15. Robustness / error-boundary findings

## HIGH — Fixed-size arrays are not systematically guarded against required scale

Examples:

- BVH max nodes 2048;
- candidates array in collision detection only 64;
- joints max 128;
- contact cache 256;
- replay only 64 bodies, 120 frames;
- UI widgets 128;
- undo only 128 bodies.

Some limits can be implementation choices, but they conflict with required workloads/features in multiple cases. Most importantly, silently truncating collision candidates at 64 can miss contacts in dense scenes.

`detect_collisions()` query callback simply stops adding candidates after 64. There is no overflow failure or secondary pass. A body overlapping >64 candidate proxies can therefore silently miss collisions.

## MEDIUM — Missing allocation/overflow discipline in UI blur

Blur allocates `w*h*sizeof(uint32_t)` each draw for every frosted region without overflow check or reusable scratch buffer. This is a performance concern and can be significant because panels blur every frame.

## MEDIUM — `ps_v2_div` has no zero guard

Callers usually guard normalization, but generic API itself permits division by zero. Not a release blocker alone.

## MEDIUM — Error messages and actionable failures are minimal

Many functions return only `-1` with no structured error detail. Required error-boundary UX is not present.

---

# 16. Requirement compliance matrix (high-level)

Legend:

- **PASS** — substantial evidence of required behavior;
- **PARTIAL** — real implementation exists but normative scope is materially incomplete;
- **FAIL** — core requirement absent, contradicted, or prohibited substitute used;
- **UNVERIFIED** — cannot be established from supplied snapshot.

| Area | Status | Review summary |
|---|---|---|
| C17 + X11 low-level boundary | **PASS/PARTIAL** | C17/X11/software framebuffer genuine; product maturity incomplete |
| Math vectors/rotations/transforms | **PASS** | basic math solid; tests useful |
| Circle rigid bodies | **PASS/PARTIAL** | core behavior implemented |
| Rectangle rigid bodies | **PARTIAL** | physics exists; renderer ignores rotation |
| Convex polygon 3–64 | **FAIL** | max 16, concavity accepted, winding can make negative mass |
| Concave polygon rejection | **FAIL** | explicitly not validated |
| Semi-implicit integration | **PARTIAL** | core integrator exists; app timestep architecture wrong |
| Dynamic AABB Tree | **FAIL** | move/remove lifecycle incorrect; no balancing |
| Broad-phase oracle/stress | **FAIL** | absent |
| Narrow-phase circle-circle | **PASS/PARTIAL** | reasonable basic implementation |
| Circle-polygon | **PARTIAL** | real implementation; limited validation |
| Polygon-polygon SAT/clipping | **PARTIAL** | real SAT/clipping but fallback/material/feature issues |
| Contact manifold accuracy | **FAIL/PARTIAL** | approximate midpoint fallback violates geometry contract |
| Sequential impulse contacts | **PARTIAL** | real solver; warm start/cache/rolling issues |
| Static/dynamic friction model | **PARTIAL** | single coefficient path, no required full validation |
| Rolling resistance | **FAIL** | nonphysical signed damping can inject energy |
| Restitution | **PARTIAL** | basic contact threshold path |
| Baumgarte stabilization | **PARTIAL** | simple position correction present |
| Distance joint | **PARTIAL** | basic velocity constraint, no full spring/damping/position solve |
| Revolute joint | **PARTIAL** | motor/limit code present but incomplete architecture |
| Mouse joint | **PARTIAL** | basic drag force path |
| Stable joint IDs | **FAIL** | absent |
| Joint iterative solver | **FAIL/PARTIAL** | each joint solved once after contact iterations |
| Sleeping/wake islands | **FAIL/PARTIAL** | simple per-body KE sleep only |
| Apply Force / Impulse tools | **FAIL** | UI FORCE is fixed impulse; distinct full tools absent |
| Motion recorder | **FAIL** | 32-point trail only |
| Time-series graph | **FAIL** | absent |
| Trajectory CSV | **FAIL** | absent |
| Ray Cast | **FAIL** | absent |
| Point Query | **FAIL** | AABB-only false positives |
| AABB Query | **PARTIAL** | API exists; BVH correctness failure |
| Shape Cast | **FAIL/PARTIAL** | function exists; geometry/TOI/filter semantics non-compliant |
| Sensor/Trigger lifecycle | **FAIL** | overlap counter only |
| Collision masks | **PARTIAL** | base bits exist |
| Signed group filtering | **FAIL** | ignored |
| 16×16 Collision Matrix product UI | **FAIL** | only hard-coded 4×4 UI subset |
| Scene JSON parser | **FAIL** | substring parser; confirmed round-trip corruption |
| Transactional load | **FAIL** | mutates world before full success |
| Safe atomic save | **FAIL** | direct overwrite |
| Full scene persistence | **FAIL** | most v1 fields missing |
| Undo | **PARTIAL** | minimal body snapshot |
| Redo | **FAIL** | absent |
| Deterministic replay commands | **FAIL** | transform snapshots prohibited |
| Checkpoints | **FAIL** | absent |
| Timeline/Scrubber | **FAIL** | only snapshot slider |
| Bookmarks/Fork/mismatch markers | **FAIL** | absent |
| Solver Inspector | **FAIL** | final summary exporter only |
| CCD BULLET mode | **FAIL** | mode absent |
| CCD swept broadphase | **FAIL** | O(N²) scan |
| TOI production solver integration | **FAIL** | special hard-coded reflection |
| CCD sensors/filtering/replay | **FAIL** | absent |
| UI custom widgets | **PARTIAL** | genuine basic custom UI |
| Full text/UTF-8 rendering | **FAIL** | digits-only font |
| Hover lift/ripple/glow | **FAIL** | color hover only |
| Modal animation/backdrop | **FAIL** | instant modal, local blur only |
| Frosted scroll nav | **FAIL** | no scroll architecture |
| Responsive panels/collapse | **FAIL** | absent |
| Golden 12 scenarios | **FAIL** | named smokes do not match frozen fixtures/metrics |
| 284 mandatory named tests | **FAIL** | names emitted, cases not implemented |
| 11 performance workloads | **FAIL** | absent |
| 5 stable envelope fixtures | **FAIL** | absent as normative runners |
| >=60 regression corpus | **FAIL** | absent |
| Developer tools | **FAIL** | missing required tool suite |
| Evidence pack | **FAIL** | explicitly partial/no media |
| Final releasecheck | **FAIL** | invalid smoke aliasing, incomplete schema/groups |
| Release Gate M | **BLOCKED** | multiple earlier gates unsatisfied |

---

# 17. Independent reproduction probes performed during this review

These probes were written independently for review and are **not modifications to the submission**.

## Probe A — BVH move/refit

Setup: two proxies, move one far outside the original root AABB, query new location.

Observed:

```text
hits=0 root=[-0.1,11.1]
```

Expected: moved proxy must be queryable near x=100, and ancestors must contain it.

**Result: confirmed production BVH mutation bug.**

## Probe B — two-body JSON save/load

Setup: create 2 circles, save using submission serializer, load using submission loader.

Observed:

```text
rc=0 count=3
```

One duplicate body appears.

**Result: confirmed persistence round-trip corruption.**

## Probe C — concavity and winding

Concave 5-vertex polygon:

```text
concave_valid=1
```

Clockwise square mass properties:

```text
mass=-1
inertia=-0.333333
```

**Result: confirmed shape validation/mass invariant failures.**

## Probe D — Point Query

Circle radius 1, query point (0.9,0.9), whose radius distance ≈1.2728.

Observed:

```text
result=0  # body index 0 returned
```

**Result: confirmed AABB false positive.**

## Probe E — Shape Cast start separation

A small circle and a thin rectangle are clearly separated at the start. Direct narrow phase reports no collision. The CCD distance fallback nevertheless can classify the start as non-positive separation because the rectangle gets a fixed 0.5 approximate radius, yielding a Shape Cast hit at fraction 0.

**Result: confirmed invalid separation oracle in Shape Cast/TOI.**

## Probe F — X11 GUI launch

The built application was run inside Xvfb and captured successfully.

Observed:

- main window opens;
- panels and body graphics render;
- many UI text labels are visually blank;
- numeric glyphs appear;
- UI is visibly a prototype, consistent with the digits-only bitmap font implementation.

**Result: confirms GUI is real but evidence claim of readable complete UI is unsupported.**

---

# 18. Release-gate judgment

The following is a conservative gate-level judgment based on the normative package.

| Gate area | Judgment | Primary blockers |
|---|---|---|
| Functional completeness | **BLOCKED** | many product features absent |
| Algorithm authenticity | **BLOCKED** | CCD special reflection, snapshot replay, simplistic BVH lifecycle, Point Query AABB substitute |
| Automated correctness | **BLOCKED** | mandatory IDs aliased to smoke tests |
| Stability | **BLOCKED** | no required fuzz/soak/envelope system; confirmed lifecycle/BVH issues |
| Persistence safety | **BLOCKED** | non-transactional substring JSON loader; direct overwrite |
| UI/UX completeness | **BLOCKED** | text unreadable, required effects/workflows absent |
| Evidence completeness | **BLOCKED** | evidence explicitly partial, no media |
| Numerical quality/invariance | **BLOCKED** | advanced VAL cases not actually implemented |
| Solver Inspector | **BLOCKED** | absent |
| CCD/TOI/Shape Cast | **BLOCKED** | algorithm and product contract non-compliant |
| Filtering | **BLOCKED** | group semantics/persistence/query consistency absent |
| Timeline determinism | **BLOCKED** | prohibited snapshot replay substitute |
| Golden final | **BLOCKED** | “Golden” functions do not match fixed scenarios/thresholds |
| v1.0 final Gate M | **BLOCKED** | cannot pass while any above is blocked |

### Final DoD conclusion

The implementation may fairly be described as a **WIP prototype / partial implementation**.

It must **not** be described as a completed v1.0 implementation under this task package.

---

# 19. Recommended remediation order

The best strategy is not to patch UI polish first. The current architecture has several foundational correctness issues that invalidate higher-level tests.

## Phase 0 — Repair acceptance honesty first

1. Change final status to **BLOCKED**.
2. Remove generic range-to-smoke aliases from `releasecheck`.
3. Create a static mandatory-ID registry and require each ID to map to a distinct real case definition.
4. Make missing test/evidence/perf/tool outputs fail closed.
5. Make report write failures fatal.
6. Add build identity and full result schema.

This is essential because otherwise future work cannot be measured honestly.

## Phase 1 — Repair core shape/BVH/lifecycle correctness

1. Implement convex polygon normalization and concavity/self-intersection rejection for 3–64 vertices.
2. Fix polygon mass/inertia for either winding.
3. Replace BVH destroy/move with real removal/reinsertion/refit/balance.
4. Fix body destruction so body-index proxies and joint/contact references cannot go stale; preferably move broadphase user data away from mutable body array index to stable ID/handle.
5. Add lifecycle invariant monitor.
6. Implement dense candidate handling without silent 64-pair truncation.

Run brute-force/metamorphic/fuzz oracles before proceeding.

## Phase 2 — Correct collision/solver/joint architecture

1. Remove midpoint fallback contact; correct clipping/feature IDs.
2. Use proper material mixing for polygon pairs.
3. Implement feature-stable contact cache.
4. Apply warm start impulses before iterations.
5. Correct rolling resistance to oppose relative rolling motion without energy injection.
6. Iterate joints within the sequential solver loop.
7. implement joint position correction/softness/spring damping as specified.
8. assign stable joint IDs.
9. add island/wake semantics.
10. build Solver Inspector instrumentation while solver structure is being corrected.

## Phase 3 — Replace CCD/Shape Cast

Do not incrementally polish current `ccd.c`; replace the core algorithm with a design matching the spec:

1. per-body DISCRETE/BULLET mode;
2. swept AABB via production BVH;
3. geometrically valid separation oracle;
4. relative transform sweep including rotation;
5. first-impact bracketing/conservative advancement;
6. deterministic TOI ordering;
7. bounded substeps;
8. production manifold + sequential solver at TOI;
9. solve only remaining time;
10. filtering/sensors/joints/replay/Inspector integration.

Then implement actual `CCD-01..30` and `CAST-01..18` individually.

## Phase 4 — Replace scene parser and editor transaction model

1. real JSON parser/tokenizer;
2. complete v1 schema;
3. temporary parse model;
4. validation;
5. stable-reference resolution;
6. transactional commit;
7. safe atomic save;
8. exact round-trip tests for every field;
9. malformed-input corpus.

## Phase 5 — Implement sensors/filtering/queries

1. exact point queries;
2. ray cast;
3. query filters;
4. signed group overrides shared across all paths;
5. fixture-level sensor state;
6. BEGIN/STAY/END pair lifecycle;
7. continuous sensor crossing;
8. event replay/timeline.

## Phase 6 — Implement deterministic replay/checkpoints/timeline

Retire transform snapshot replay as replay architecture. Keep it only as optional trajectory/history data if useful.

Implement:

- commands;
- step IDs;
- canonical state digest;
- checkpoints;
- seek;
- step backward by reconstruction;
- bookmarks;
- event markers;
- mismatch detection;
- forking;
- 100k-step tests.

## Phase 7 — Complete product UI and renderer

1. proper glyph/text system including required UTF-8 behavior;
2. rotated body and convex polygon rendering;
3. actual tool state model;
4. force and impulse interactions;
5. inspector editing;
6. 16×16 matrix;
7. Solver Inspector;
8. Motion Analysis graph;
9. Timeline;
10. UI effects/animation/focus/scroll/modal behavior;
11. responsive window handling;
12. visual E2E evidence.

## Phase 8 — Implement required dev tools and full release suite

Deliver and self-test:

- locscan;
- fixturegen;
- scenecheck;
- physverify;
- perfbench;
- solvertrace.

Then implement:

- all 284 cases with real semantics;
- 60+ regression fixtures;
- 11 perf workloads;
- 5 stability envelopes;
- 2 fixed CCD fixtures;
- fuzz;
- lifecycle stress;
- soak;
- all 12 exact Golden scenarios;
- evidence pack;
- traceability/audits.

Only then re-enable final Gate M PASS.

---

# 20. What should be preserved

Despite the severe compliance result, several pieces are worth retaining/refactoring rather than throwing away:

- `vec2.h`, `rot2.h`, `xform.h` basic math organization;
- basic body force/impulse APIs;
- circle-circle collision foundation;
- SAT/clipping code as a starting point after correctness repairs;
- sequential-impulse solver structure as a starting skeleton;
- custom framebuffer concept;
- custom UI state/widget skeleton;
- initial tests as quick smoke/unit layer;
- matrix data structure as a starting category matrix;
- the idea of a replay history structure, repurposed as Motion Analysis/history rather than deterministic replay;
- existing source modularization (`physics`, `scene`, `ui`, `render`, `diagnostics`).

The project does not need a total rewrite, but several subsystems—especially BVH mutation, JSON persistence, CCD, replay, sensors, Solver Inspector/acceptance—need architecture-level replacement.

---

# 21. Detailed finding index

## Critical

1. CRITICAL-01 Mandatory registry IDs are generic smoke aliases.
2. CRITICAL-02 Evidence/status is internally contradictory.
3. CRITICAL-03 Replay is forbidden transform snapshot playback.
4. CRITICAL-04 CCD/TOI/Shape Cast architecture non-compliant.
5. CRITICAL-05 BVH move/remove lifecycle incorrect; confirmed false-negative query.
6. CRITICAL-06 Scene I/O corrupts multi-body round trips and is not transactional JSON.
7. CRITICAL-07 Solver Inspector absent.
8. CRITICAL-08 Sensor lifecycle absent.
9. CRITICAL-09 Required dev tools absent.
10. CRITICAL-10 Performance/regression/audit/evidence release workloads absent.

## High

1. HIGH-01 Polygon max 16 / concavity accepted.
2. HIGH-02 Clockwise polygon creates negative mass/inertia.
3. HIGH-03 Point Query is AABB-only.
4. HIGH-04 Group filtering ignored.
5. HIGH-05 Polygon contact materials hard-coded.
6. HIGH-06 Approximate midpoint manifold fallback.
7. HIGH-07 Contact cache lacks feature identity/lifecycle.
8. HIGH-08 Warm start not actually applied.
9. HIGH-09 Rolling resistance may inject energy.
10. HIGH-10 Joint system incomplete.
11. HIGH-11 Body deletion can stale proxies/pointers.
12. HIGH-12 Main loop not proper fixed timestep.
13. HIGH-13 Force tool is hard-coded impulse substitute.
14. HIGH-14 Motion Analysis absent.
15. HIGH-15 Undo incomplete / Redo absent.
16. HIGH-16 Rigid-body renderer ignores rectangle rotation/polygons.

## Medium / Low highlights

- aggregate Make target incomplete;
- duplicate Make recipes;
- hard-coded evidence output path;
- fixed candidate limit can silently discard overlaps;
- repeated per-frame blur allocations;
- primitive error reporting;
- code contains explicit `WIP`, `simplified`, `for full impl`, and `full validation later` comments in release-critical code;
- UI labels only support digits;
- unused Restitution slider demonstrates disconnected UI state;
- project documentation/evidence generated at different completion states without freshness/build identity.

---

# 22. Final verdict

## Engineering quality of the WIP foundation

**There is meaningful implementation work here.** The project compiles, runs, renders a real X11 application, and has multiple genuine physics routines and unit tests. It is more than a mock or static demo.

## Compliance with the supplied v1.0 assignment

**Not compliant.** The gap is large and structural.

## Reliability of reported “289/289 PASS” and “Golden 12/12 PASS”

**Not reliable as release evidence.** Those reports are primarily generated by aliasing mandatory IDs to generic smoke functions, and the project’s own evidence files state that major work remains.

## Appropriate current status

> **BLOCKED / WIP — foundational prototype, not v1.0 complete.**

The most important corrective action is to make the acceptance system honest and fail-closed before continuing implementation. Once the test registry genuinely maps each ID to its specified fixture/oracle, the remaining implementation gap will become measurable rather than hidden behind an all-green report.

