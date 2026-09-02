# Pinball Sandbox v1.0.0 — Independent Static Code Review

**Reviewer:** GPT-5.6 Sol (Chat, high reasoning)  
**Review date:** 2026-09-02  
**Review target:** `runs/c17-win32-pinball-benchmark/2026-08-17-hy3-192k-workbuddy-default/project/pinball_sandbox_v1.0.0_without_evidence/`  
**Review mode:** independent source-centric static audit of the archived Windows implementation. I inspected the native editor, production physics core, scene parser/writer/validator, replay and deterministic-state machinery, software renderer/PNG path, automated tests, headless tools, build scripts, and release/evidence generators. I did not execute the Windows GUI binary in this environment.  
**Scope:** this is intentionally **not** a requirement-by-requirement spec checklist. The central questions are: what does the code really do, where are its invariants sound, where do they fail, what user/runtime effects follow, and how much confidence should be placed in the archived test/release evidence?

---

# 1. Executive assessment

## Overall judgment

**This is a large, genuinely implemented C17/Win32 project with a credible headless architecture, but its interactive editor, several advertised gameplay mechanisms, malformed-input safety, runtime/editor state separation, and release-evidence pipeline are substantially less complete than the archived PASS narrative suggests.**

The project is much more than a toy pinball renderer. It contains:

- a native Win32 editor/playable application;
- a custom software framebuffer renderer presented through GDI;
- a deterministic fixed-step physics core shared with headless tools;
- a typed scene language with 15 object types, layers, groups, events and actions;
- scene validation and canonical writing;
- wall/bumper/flipper/spinner/kickout/ball-ball collision code;
- scoring, turns, drains, nudging, tilt state, multiball actions and replay;
- deterministic state fingerprints/checkpoints;
- dependency-free PNG generation;
- multiple CLI verification tools;
- a broad fixture corpus and a unified regression harness;
- release, traceability and visual-evidence generators.

The core architectural direction is good. `src/core` is not a fake test double: the headless tools and GUI link the same simulator. A fixed `1/240 s` step, deterministic object iteration, explicit runtime structs, machine-readable headless output, scene round trips and repeated-run comparisons are all sensible foundations.

The problem is that several failures sit exactly at the places the headless-heavy test suite does not stress.

Normal editor operation has severe faults. On a fresh scene, palette object creation increments `obj_count` and writes `scene.objects[idx]` without allocating the array; the first created object can therefore dereference NULL. The unsaved-close dialog's **Yes** path contains no save at all before destroying the window. Preview mode advances `preview_sim` but routes keyboard input to `sim`. Narrow window resizing can make canvas-copy coordinates extend beyond the framebuffer.

The scene parser also has memory-safety defects reachable from malformed `.pbt` input: over-arity vectors/rectangles write past fixed local arrays before arity is checked; declared `action_count` and `member_count` values are not bounded before later loops index fixed arrays; and a 64-byte identifier can receive its NUL terminator one byte beyond a 64-byte destination.

The physics engine contains real mechanics, but several advertised semantics are disconnected. Spinner collisions change angular velocity but spinner angle is never integrated. Launcher input is explicitly discarded. Drop-target delayed/new-ball reset modes are stored but not implemented. Nudge cooldown and tilt decay are dead fields. Tilt does not suppress mechanisms. Several object types ignore `enabled`. Segment CCD uses the supporting infinite line rather than a finite capsule. Penetration correction receives the wrong sign. Exact zero-time contacts can consume the entire per-step impact loop without advancing time.

Most importantly, runtime gameplay mutates the **authored `Scene` itself**. Drop-target hits and event actions write object enabled/drop state directly into the same table model used by the editor. `sim_reset()` does not restore authored values. GUI preview/play both point at `a->scene`, so merely previewing a table can alter authoring state without an edit command, dirty flag or undo record.

Finally, the release/evidence pipeline cannot be treated as execution-derived ground truth. `tools/make_release.py` hard-codes `722/722`, per-category totals, stress values, fingerprints, release-gate PASS states and `gui_headless_match = True`; it marks entire requirement-prefix families PASS and synthesizes `T-...` IDs instead of ingesting the test runner's actual case-level records. `tools/generate_evidence.py` fills UI-oriented visual IDs with generic headless physics fixture frames, while `VISUAL_EVIDENCE.md` describes those IDs as hover/modal/DPI/focus/editor states. This is a benchmark-evidence integrity defect, not merely optimistic wording.

### Severity summary

| Severity | Count | Meaning |
|---|---:|---|
| **Critical** | **9** | Memory corruption, immediate normal-workflow crash/data loss, authored-state corruption, or evidence-integrity failures that invalidate core safety/verification claims |
| **High** | **28** | Major physics/gameplay/editor/replay correctness failures, unsafe persistence/failure behavior, or large schema/runtime disconnects |
| **Medium** | **24** | Robustness, validation, API-contract, determinism, portability, resource or UX defects |
| **Low / quality** | **12** | Maintainability, documentation drift, dead state, smaller architectural debt |

Related symptoms are grouped by root cause rather than counted line-by-line.

---

# 2. Reconstructed architecture

```text
                     +----------------------+
                     | Win32 editor / play  |
                     | src/app/editor.c     |
                     +----------+-----------+
                                |
          +---------------------+----------------------+
          |                                            |
          v                                            v
 +--------------------+                       +--------------------+
 | Authored Scene     |<----------------------+ Sim / preview Sim  |
 | objects/events/... |     currently MUTATED | src/core/sim.c     |
 +---------+----------+                       +---------+----------+
           ^                                            |
           |                                            v
   +-------+--------+                           fingerprints / JSON
   | parser/writer  |
   | validator      |
   +-------+--------+
           ^
           |
         .pbt

 .pbr --> replay.c --> logical input edges --> sim_input --> sim_step

 headless: simcheck / scenecheck / replaycheck / framegen
                    \_______________________________/
                                  |
                            same core objects

 tests.c -----------------> assertion-oriented summary
 make_release.py ----------> hard-coded/constructed release JSON
 generate_evidence.py -----> headless physics PNGs assigned visual IDs
```

The strongest part is the real shared core. The most consequential architecture break is the arrow from `Sim` back into authored `Scene`: transient runtime state is not isolated from editor data.

---

# 3. What the implementation does well

## 3.1 One production simulation core

The GUI and headless tools genuinely consume the same simulator rather than maintaining separate product/test implementations. That is exactly the right direction for deterministic verification.

## 3.2 Fixed-timestep simulation is a sound basis

`PB_DT = 1.0 / 240.0`, explicit step counting, no wall-clock dependence inside `sim_step()`, deterministic object order and bounded inner iterations are appropriate choices for a replayable sandbox.

## 3.3 The table format is nontrivial and meaningfully typed

The parser handles distinct schemas, format migration, layers/groups/events/actions, booleans, vectors, numbers, references, duplicate keys/IDs and diagnostic codes. The writer emits stable order and uses `%.17g` for round-trippable doubles.

## 3.4 Parser and semantic validation are separated

Keeping syntax/reference parsing apart from geometry/gameplay validation is good design. The validator's point-to-segment distance helper correctly clamps the projection to `[0,1]`, which is particularly useful as a contrast with the simulator's faulty sweep implementation.

## 3.5 Collision response is not placeholder math

Flipper response accounts for local surface velocity from angular motion. Spinner response includes rotational effective mass and friction impulse terms. Ball-ball response uses inverse-mass impulses and positional correction. The implementation clearly attempted real mechanics.

## 3.6 Replay records logical edges

The recorder captures DOWN/UP transitions rather than merely dumping sampled keyboard state. That is the right abstraction for deterministic reapplication.

## 3.7 Dependency-light rendering is real

Framebuffer allocation, clipping, alpha blending, primitive rasterization, text and PNG output are handwritten. The PNG writer correctly splits stored DEFLATE data into blocks no larger than 65,535 bytes.

## 3.8 The test suite has broad *categories*

The harness covers parse/roundtrip, deterministic repetitions, a million-step run, semantic validation, all object types, replay, malformed fixtures and tool smoke. The problem is not lack of effort; it is that the checks are heavily weighted toward repeatability/roundtrip rather than the state transitions where the serious defects live.

---

# 4. Critical findings

## C-01 — `parse_vec2()` and `parse_tuple4()` write out of bounds before checking arity

**File:** `src/core/scene_parse.c`  
**Functions:** `parse_vec2`, `parse_tuple4`

`parse_vec2()` uses `double vals[2]`; `parse_tuple4()` uses `double vals[4]`. Both append parsed components with `vals[n++] = ...` on separators/closing parenthesis and only verify final `n` afterwards.

Malformed examples such as:

```text
world_size = (1, 2, 3)
rect = (1, 2, 3, 4, 5)
```

can therefore write beyond the local stack array before the parser returns an error.

**Impact:** external scene input can cause memory corruption in every consumer of `parse_scene()` — editor load, simcheck, scenecheck, framegen, tests, etc.

**Fix:** test `n < capacity` before every assignment and return a deterministic arity error immediately. Add sanitizer-backed malformed-input fuzz cases.

---

## C-02 — `member_count` / `action_count` are not bounded before fixed-array traversal

**File:** `src/core/scene_parse.c`

`Group.members` and `Event.actions` each have fixed capacity 256. The parser accepts a `long`, narrows it to `int`, rejects only negatives, and later loops to the declared count during reference/index validation.

A file with `member_count = 10000` or `action_count = 10000` causes out-of-bounds struct reads. The event path may also inspect `ev->actions[a].type` far past the object.

**Fix:** reject values greater than the exact fixed capacity before assignment and reject values not representable as `int`.

---

## C-03 — 64-byte field identifiers overflow 64-byte destinations by the terminator

**File:** `src/core/scene_parse.c`  
**Function:** `parse_id`; `PSET_ID`

Destinations such as `layer`, launcher `spawn_id` and event `source` are `char[PB_ID_MAX]` with `PB_ID_MAX == 64`. `PSET_ID` passes `PB_ID_MAX`, while `parse_id()` rejects only `strlen(t) > maxlen`, then performs `strcpy()`.

A 64-byte identifier needs 65 bytes including NUL.

**Impact:** one-byte struct-adjacent overwrite from scene input.

**Fix:** define the parameter as destination *capacity* and reject `strlen >= capacity`.

---

## C-04 — First palette object creation in the native editor can dereference NULL

**File:** `src/app/editor.c`  
**Function:** `create_obj_at`

A fresh `Scene` has `objects == NULL`, `obj_count == 0`, `obj_cap == 0`. Object creation bypasses `scene_add_object()`:

```c
int idx = a->scene.obj_count++;
a->scene.objects[idx] = o;
```

No allocation occurs first. `duplicate_selection()` uses the same direct append model and can overrun capacity later.

**Impact:** a primary editor workflow — choose a tool and click the canvas on a new table — can immediately crash.

**Fix:** all structural changes must use a single checked insertion API; increment counts only after successful allocation/copy.

---

## C-05 — Narrow resizing can make the canvas copy write beyond the framebuffer

**File:** `src/app/editor.c`  
**Functions:** `calc_layout`, `app_render`

`canvas_w` is forcibly raised to 100 even if the client is narrower than `canvas_x + 100`. The copy loop then writes destination coordinates `canvas_x + cx` without clipping them to `W`.

**Impact:** sufficiently narrow native-window resize can cause heap out-of-bounds writes through the software framebuffer.

**Fix:** clip copy coordinates against actual framebuffer bounds and set a sensible minimum window size with `WM_GETMINMAXINFO`; never rely on layout minimums as memory bounds.

---

## C-06 — Runtime simulation mutates authored editor data and reset does not restore it

**Files:** `src/core/sim.c`, `src/app/editor.c`

`Sim` stores `Scene *scene`. Runtime behavior directly changes `Obj` fields in that `Scene`:

- drop-target hit: `o->u.cap.enabled = 0`;
- ENABLE/DISABLE/OPEN_GATE/RESET_TARGET/SET_TARGET_DROPPED actions modify table objects.

The GUI initializes play/preview sims with `&a->scene`. `sim_reset()` clears runtime structs but does not restore mutated authored object fields.

**Impact:**

- preview can change the editor's table;
- a dropped target may stay dropped across reset/new game;
- event actions can alter authoring data without dirty state or undo;
- saving after play/preview can persist transient game state;
- repeated simulations can begin from different authored states without reparse.

**Fix:** authored `Scene` must be immutable to simulation. Put all mutable object state in Sim-owned runtime arrays or construct a complete runtime snapshot from the authored scene.

---

## C-07 — ENABLE/DISABLE writes the wrong union member for most object types

**File:** `src/core/sim.c`  
**Function:** `execute_action`

Both generic actions write `o->u.cap.enabled` regardless of the active `Obj.u` member. That field is not the same storage location as `u.bumper.enabled`, `u.flipper.enabled`, `u.sensor.enabled`, `u.launcher.enabled`, `u.spinner.enabled` or `u.kickout.enabled`.

**Impact:** an event targeting a non-capsule can alter unrelated union bytes while failing to enable/disable the intended object. This is state corruption from otherwise valid table content.

**Fix:** type-dispatched enabled handling, preferably in runtime state rather than authored union storage.

---

## C-08 — Exact zero-time contacts can exhaust the impact loop without consuming motion; triggers can multiply

**File:** `src/core/sim.c`

Sweep routines accept `t == 0`. The collision loop then does:

```c
remain *= (1.0 - best_t);
```

so zero-time contacts leave `remain` unchanged. A separating contact may receive no response, and penetration correction is also ineffective (H-03). The same contact can be selected repeatedly up to `PB_MAX_IMPACT`.

Sensor/rollover/drain responses do not alter velocity or maintain crossing state, so one physical crossing can trigger repeatedly within the same step.

**Impact:** pinned balls, silently truncated movement, duplicated score/actions/counters, and invisible impact-cap exhaustion. `impact_budget_used` is not maintained and `RT_E_IMPACT_BUDGET` is not surfaced here.

**Fix:** approach-aware collision acceptance, epsilon advancement, explicit trigger occupancy/crossing state, and fail/report on exhausted impact budget.

---

## C-09 — Release/evidence artifacts are constructed from hard-coded PASS state rather than raw executed results

**Files:** `tools/make_release.py`, `tools/generate_evidence.py`

`make_release.py` hard-codes:

- `722/722` and every category count;
- final/replay fingerprints;
- stress metrics;
- broad gate PASS values;
- every delivered fixture scenario as PASS;
- `gui_headless_match = True`.

It defines families such as `PHY`, `GAME`, `EVT`, `RES`, etc. in a `PASS_CODES` set and synthesizes one ID per requirement:

```python
status = "PASS"
tids = [f"T-{code}-{idx:03d}"]
```

This is not ingestion of case-level output from `tests.exe`.

The visual generator similarly maps `Vxx/Axx` UI descriptions to generic headless fixture frames. The generated index describes hover, modal blur, DPI, focus traversal, command palette, autosave recovery and other UI states that those physics frames do not demonstrate.

**Impact:** in a benchmark archive, generated evidence can be internally self-consistent yet not execution-derived. Downstream readers can mistake declarative constants for measured verification.

**Fix:** release generation must consume immutable raw test/session records, fail closed when evidence is missing, reference real emitted test IDs, and generate visual IDs from the exact UI state they name with build/command/input provenance.

---

# 5. High-severity findings

## H-01 — Spinner angular velocity is never integrated into angle

Spinner collision response changes `sr->ang_vel`; mechanism update damps it. No `angle += ...` occurs. Collision geometry and rendering read `sr->angle`, so the spinner never physically rotates after impact.

`SpinnerRT` also documents angle in degrees and angular velocity in rad/s, so the eventual integrator must explicitly convert units.

## H-02 — Spinner tick semantics are not implemented

`tick_angle_deg` and `score_per_tick` are unused. `ev_spinner_tick++` and `SPINNER_TICK` fire once per collision impulse instead of per angular threshold crossed.

## H-03 — Penetration correction has the sign reversed at static/mechanism call sites

`correct_pen()` expects positive penetration depth, but callers pass `distance - radius`, which is negative during overlap. Ball-ball correction uses the correct `Rsum - dist` convention.

## H-04 — Segment CCD is against an infinite supporting line, not a finite capsule

`sweep_point_segment()` removes the longitudinal component and solves perpendicular distance, but never constrains the projected contact to the segment extent and does not test endpoint circles. Balls can hit invisible wall/flipper/target extensions.

## H-05 — Launcher is schema/editor/validation data with no simulation behavior

`sim_input()` explicitly `(void)launch`s the value. No charge timer, curve, ownership, release impulse or launcher/spawn interaction exists in the main simulator.

## H-06 — Drop-target reset modes are stored but not executed

`AFTER_DELAY`, `ON_NEW_BALL`, `reset_delay` and `initially_raised` exist in parse/write/validation. Runtime only disables a target on hit; no delayed/new-ball reset path exists.

## H-07 — Nudge cooldown and tilt decay are dead runtime fields

`nudge_cooldown`, `tilt_decay_per_second` and per-ball cooldown state are never applied/ticked.

## H-08 — Tilt does not suppress normal gameplay

Crossing threshold sets `tilted` and fires an event, but flippers, nudges, scoring and mechanisms continue unless some authored event happens to change them.

## H-09 — Authored `max_active_balls` is ignored

Spawn capacity is checked only against compile-time `PB_MAX_BALLS == 256`.

## H-10 — `enabled` is not respected by multiple runtime object types

The collision loop omits enabled checks for bumpers, flippers, spinners, sensors and drains. Mechanism updates similarly process disabled flippers/spinners.

## H-11 — Sensor ENTER/STAY/EXIT model is not implemented

The enum/schema includes ENTER/STAY/EXIT, but simulation only emits ENTER-like behavior on swept contact and tracks no inside-set. Rollover has the same crossing-state problem.

## H-12 — Ball-ball collision lacks CCD

Balls are integrated first and only overlapping endpoints are resolved. High-speed multiball pairs can tunnel through one another.

## H-13 — Core simulation allocations are unchecked

The bumper cooldown allocation is roughly 8 MiB (`256 * 4096 * sizeof(double)`), recorder arrays are also heap allocated, and no result is checked. `sim_init/reset` cannot report allocation failure.

## H-14 — `scene_fingerprint()` omits behavior-relevant authored fields

Examples include one-way allowed direction; slingshot impulse/base score/cooldown; rollover width/base score; drop/standup hit speed, score and cooldown; drop reset configuration. Semantically different scenes can therefore have identical fingerprint input bytes.

## H-15 — `sim_fingerprint()` omits future-relevant runtime state

Cooldowns, kickout hold timing, some mechanism state, override duration, event/runtime mutations and other transient values are omitted. Equal fingerprint does not necessarily mean equal future evolution.

## H-16 — Replay grammar is permissive in ways that weaken artifact integrity

Any header beginning `PINBALL_REPLAY` is accepted; key checks use broad prefixes; numeric conversions are weakly validated; malformed `STEP` lines can be silently ignored; seed/physics metadata are not universally required by verification.

## H-17 — Replay allocation failure handling can crash

Initial malloc is unchecked. `realloc` is assigned directly to `r->ev`; failure loses the old pointer and later writes through NULL.

## H-18 — `scene_clone_into()` violates its deep-clone contract

After `*dst = *src`, pointers/capacities are zeroed but counts remain copied from the source. Subsequent `scene_add_*()` appends starting at a nonzero count into newly allocated storage, producing gaps/count inflation and potential capacity mismatch. It appears unused in the inspected main flow, but the public helper is materially broken.

## H-19 — Preview mode sends input to the wrong simulator

`sim_tick()` advances `preview_sim` when `in_preview`. Keyboard handlers send PLAY **and PREVIEW** input to `&a->sim`. The preview shown to the user therefore does not receive the intended flipper/launch state.

## H-20 — Native input model cannot maintain independent simultaneous flipper states

Each key event passes a complete state tuple. Pressing one key zeros the other; releasing either sends both zero. SPACE sends both flippers plus launch together.

## H-21 — Undo/redo ring loses coherent chronology after 256 snapshots

The history code overwrites modulo slots without maintaining a logical tail/base index. `head`/`pos` semantics become inconsistent once full, so later undo/redo can read the wrong snapshot chronology.

## H-22 — Multi-selection deletion can remove wrong objects

The comment says removal is descending, but code merely traverses the selected-index array backward. Selection order is not guaranteed sorted. Removing lower indices shifts higher ones and invalidates subsequent stored indices.

## H-23 — WM_CLOSE → Yes destroys the window without saving

The `IDYES` branch contains only a comment and then falls through to `DestroyWindow(hwnd)`. The user can explicitly choose “Save changes before closing?” and still lose the changes.

## H-24 — New/Open paths can discard dirty work without confirmation

Dirty-state prompting is limited to close. Ctrl+N/Ctrl+O replace state directly. Ctrl+N also calls `scene_init()` over a potentially allocated scene without first freeing it.

## H-25 — GUI save is non-atomic and truncates the prior file first

`save_scene()` uses `fopen(path, "wb")` and `fwrite`. Short write/crash/disk error can destroy the previous valid table. It completely bypasses the separately implemented `scene_write_file()`.

## H-26 — `scene_write_file()` is not a robust durable Windows atomic replacement path either

It uses fixed `path.tmp`, stdio flush/close and C `rename()`. It has no `FlushFileBuffers`/directory durability semantics, no unique temp, no external-change protection and no robust Win32 replacement policy. Several failure paths leave temp state.

## H-27 — Scene serialization can silently truncate on OOM

The internal writer buffer has no sticky failure flag. If grow fails, append helpers simply return and later serialization continues. `scene_write()` can return a partial buffer as though it were valid. Layer-sort index allocation is also unchecked.

## H-28 — Framebuffer allocation/size arithmetic is unchecked

`fb_init()` multiplies dimensions, mallocs, and immediately clears without null/overflow checks. `resize_framebuffer()` does the same for `client_rgb`. Authored world size × zoom can create large temporary buffers every timer frame.

---

# 6. Medium-severity findings

## M-01 — UTF-8 validation checks byte shape but not Unicode scalar validity

Overlong encodings, surrogate code points and values above U+10FFFF are not rejected.

## M-02 — `long` to `int` field assignment is not range-checked

`PSET_INT` can truncate/wrap values before later semantic checks.

## M-03 — Line-list growth overwrites the sole pointer with unchecked `realloc`

`push_line()` can dereference NULL after allocation failure.

## M-04 — Parser first-error state is global

`g_first` makes parsing non-reentrant and unsafe for concurrent parser use.

## M-05 — Multiple parser section commits ignore `scene_add_*()` failure

Under OOM, elements can disappear while the parser continues rather than producing a clean allocation error.

## M-06 — Validator can return `PBT_OK` while diagnostics contain severity ERROR

The missing-event-source path pushes `VAL_E_EVENT_CYCLE_STATIC` (classified as error) but does not update `first_err/have_err`. Callers trusting return code and callers scanning diagnostics can disagree.

## M-07 — Physical semantic validation is incomplete

Sensor/drain dimensions, launcher direction/speed/charge consistency, spinner thickness/tick angle, spawn radius and numerous coefficient ranges are not comprehensively checked.

## M-08 — Launcher ownership duplicate tracking stops after 256 entries

Object capacity is much larger, so duplicate ownership beyond the helper array can escape this validator.

## M-09 — First bumper hit can be suppressed by zero-initialized cooldown time

Cooldown timestamps begin at 0. A first hit before one cooldown interval has elapsed can fail the `now - prior >= cooldown` condition.

## M-10 — Bumper cooldown slot aliases every 256 ball IDs

The index uses `ball_id % PB_MAX_BALLS`; a later ball can inherit an earlier ball's cooldown history.

## M-11 — Slingshot/target cooldown fields are not enforced

Only bumper cooldown has runtime storage.

## M-12 — Spawn action failures are ignored by event execution

SPAWN_BALL/START_MULTIBALL loop over `sim_spawn_ball()` without checking returned failure. Runtime error state can be set while the step continues.

## M-13 — Replay recorder silently drops events after 65,536 edges

No growth and no overflow diagnostic.

## M-14 — Editor's recording path would crash if enabled

`sim_tick()` calls `sim_recorder_take(sim, NULL, NULL, NULL)` when `recording` is true, while `sim_recorder_take()` unconditionally dereferences all output pointers.

## M-15 — `sim_spawn_ball()` API comment says “ball index”; implementation returns ball ID

A caller following the public contract could use the return as an array index incorrectly.

## M-16 — `sim_checkpoint_json()` becomes unsafe once its output buffer truncates

It adds `snprintf()`'s would-have-written length to `len`. After truncation, later `out + len` can point beyond the buffer and `outsz - len` can wrap.

## M-17 — Replay parser has no practical event/file-size cap

Even with safe realloc, an input replay can drive unbounded memory growth.

## M-18 — Scene fingerprint does not length-frame variable strings

Sequential member IDs such as different string partitions can feed identical concatenated bytes into FNV even before ordinary hash collision considerations.

## M-19 — FNV-1a is diagnostic, not strong integrity

Fine for repeatability checks, but it should not be described as a security-strength binding for untrusted replay/table artifacts.

## M-20 — TOOL_MOVE falls into object-creation behavior

Only TOOL_SELECT is handled specially. Other tools call `create_obj_at()`. `make_default_obj()` has no TOOL_MOVE case, leaving a zeroed object whose enum value becomes BALL_SPAWN-like invalid data.

## M-21 — Palette-created object defaults are incomplete/unsafe

Many created objects retain zero enabled/material values. A created spinner has zero inertia; if simulated, its collision math divides by inertia. Different object types become inert or inconsistent because simulator enabled handling itself is inconsistent.

## M-22 — Mouse-wheel client-coordinate conversion is discarded

`ScreenToClient()` is called on a temporary compound literal; the converted coordinates are not copied back into `x/y`. Zoom-about-pointer uses screen coordinates.

## M-23 — Selection geometry ignores actual/rest mechanism angle

Flipper and spinner hit tests assume horizontal geometry rather than authored/current orientation.

## M-24 — Inspector reads incompatible union members for some types

The wall/ramp/flipper branch prints capsule thickness plus `u.flipper.length`; for wall/ramp the latter is unrelated union storage.

---

# 7. Low / quality findings

## L-01 — Core build explicitly leaves warnings-as-errors off

`build_core.sh` comments that strict warnings-as-errors are disabled “to keep build green”. For this much fixed-array/union code, warning-clean should be a meaningful quality gate.

## L-02 — Build/test scripts hard-code a developer-specific `D:/0814/...` layout

The source archive is not directly reproducible without editing paths or recreating that filesystem layout.

## L-03 — README architecture path drift

Documentation refers to `src/platform`; the actual platform file is under `src/app/platform.c`.

## L-04 — `impact_budget_used` exists but is not maintained

The release report's “impact cap hits = 0” is not backed by meaningful runtime accounting in the inspected loop.

## L-05 — Several runtime error enums are aspirational

Event-budget, impact-budget, out-of-world and eject-blocked error codes exist without complete main-loop enforcement/reporting.

## L-06 — `balls_lost_this_turn` is dead state

It adds apparent feature surface without runtime semantics.

## L-07 — `combo_multiplier` lacks a substantive combo lifecycle

It is initialized and consulted for scoring, but no clear combo progression/expiration system is present in the inspected simulator.

## L-08 — Renderer ignores layer visibility

Objects are drawn regardless of authored layer visibility.

## L-09 — Editor transform/delete paths ignore object/layer lock state

The schema stores locks, but primary edit operations do not enforce them.

## L-10 — Native file UI is ANSI/MAX_PATH based

The app uses `WinMain`, `GetOpenFileNameA`/`GetSaveFileNameA`, `char[MAX_PATH]` and `fopen`, weakening Unicode-path support despite UTF-8 content and IME work.

## L-11 — Per-Monitor-V2 DPI support is not present in the inspected platform code

No DPI-awareness setup or `WM_DPICHANGED` handling is visible. The release gate correctly says live DPI NOT_RUN, but source support is also incomplete.

## L-12 — Renderer allocates/frees large framebuffers every timer frame

The 60 Hz app rebuilds both the client framebuffer and a full scaled world framebuffer instead of retaining reusable buffers.

---

# 8. Test/evidence interpretation

## 8.1 “722 tests” is an assertion count, not 722 independent scenarios

`tools/tests.c` increments test totals for every `CHECKF` / `CHECKC`. The archived report says 502 of 722 belong to parse/roundtrip. This is useful coverage, but the headline count sounds broader than the behavior matrix really is.

## 8.2 Deterministic repeatability does not establish correct physics

A deterministic bug produces the same fingerprint every run. The repeat suite can pass while spinner motion, launcher behavior, finite-segment collision, target reset and tilt semantics are wrong.

## 8.3 The 1,000,000-step test is deliberately narrow

The long run uses `free_flight_v2.pbt`. It is valuable for gross non-finite/lifetime regression, but does not exercise most mechanisms implicated by this review.

## 8.4 “Malformed-input safety” uses 28 curated fixtures, not structural fuzzing

The suite only asks each delivered malformed file to return non-OK. It does not generate over-arity tuples, count-capacity boundaries, exact ID-length boundaries, per-allocation failure or mutation fuzzing under sanitizers. That explains C-01 through C-03.

## 8.5 The most dangerous desktop paths are outside the automated suite

First object creation, dirty close, preview input, history capacity, narrow-window resize and native save behavior sit in GUI code whose gates are marked NOT_RUN.

That NOT_RUN status should materially limit confidence in the *product*, not merely the presentation layer.

## 8.6 `gui_headless_match = true` is not the result of a GUI execution

The value is hard-coded by the release generator while `main_ui`, `editor` and `desktop_interaction` are NOT_RUN. Shared core code is evidence of reuse, but H-19 shows GUI routing can still differ from headless behavior.

## 8.7 Scenario PASS values are generated from fixture presence

`make_release.py` enumerates every `fixtures/*.pbt` and sets the corresponding scenario to `PASS`; it is not consuming a per-scenario execution result at generation time.

## 8.8 Visual IDs are semantically mismatched

The index names UI states such as hover, ripple, modal blur, sidebar motion, DPI 125/150/200, command palette, focus traversal, autosave recovery and external-change UI. `generate_evidence.py` fills the same ID ranges with headless `framegen.exe` renders of physics fixtures at different steps/resolutions.

Those images may be valid simulation frames, but they do not prove the labeled UI behavior.

---

# 9. Cross-cutting engineering diagnosis

## 9.1 Two different maturity levels exist in the same submission

The **headless/data layer** is relatively mature:

- parse;
- canonical write;
- basic validation;
- fixed stepping;
- repeat runs;
- framebuffer output;
- CLI tools.

The **stateful interactive layer** is much less mature:

- authoring vs runtime state;
- transactions and undo;
- dirty/save lifecycle;
- input state routing;
- layer/lock behavior;
- DPI/Unicode path integration;
- advanced mechanism semantics.

This explains how a headless-oriented result can look excellent while ordinary editor workflows remain unsafe.

## 9.2 Schema presence is repeatedly mistaken for feature completion

Fields exist for launcher charge, target reset, spinner ticks, nudge cooldown, tilt decay, locks, layers and diagnostics before end-to-end semantics exist.

A better completion rule is:

> parse → validate → runtime behavior → reset → replay/fingerprint → editor visualization → focused test must all agree before a feature is called supported.

## 9.3 Runtime state needs a hard ownership boundary

A robust model should be:

```text
Authored Scene
   |
   +-- editor commands / undo / save
   |
   +-- build runtime snapshot
             |
             v
       Runtime Object State + Sim
       enabled/drop/timers/holds/cooldowns/angles
             |
             +-- NEVER writes Authored Scene
```

That redesign would resolve an entire cluster of current bugs.

## 9.4 Evidence tooling is production code in a benchmark repository

Release/evidence scripts determine what later reviewers believe happened. They therefore require the same fail-closed, provenance-preserving treatment as persistence or replay code. Hard-coded PASS states are not harmless convenience values.

---

# 10. Remediation roadmap

## P0 — Memory safety and direct data-loss paths

1. Bound vector/tuple arity before writes.
2. Bound group/event counts before any traversal.
3. Fix destination-capacity handling for IDs/strings.
4. Route every editor insertion/duplication through checked allocation APIs.
5. Clip small-window rendering to actual framebuffer bounds.
6. Check all framebuffer/simulation allocations and arithmetic overflow.
7. Implement real WM_CLOSE → Yes save semantics.
8. Add ASan/UBSan parser/render fuzz runs if the Windows toolchain permits.

## P1 — Separate authoring from runtime

1. Make authored Scene immutable during play/preview.
2. Store enabled/drop/hold/reset/cooldown state in Sim-owned runtime tables.
3. Instantiate a clean runtime snapshot for every Play/Preview.
4. Make reset rebuild canonical transient state.
5. Guarantee preview/play cannot affect editor dirty state or saved table bytes.

## P2 — Correct contact/physics semantics

1. Implement true swept circle-vs-capsule (body + both endpoints).
2. Fix penetration-depth convention.
3. Handle `t=0` contacts and impact-budget exhaustion explicitly.
4. Track trigger occupancy/enter/stay/exit.
5. Integrate spinner angle and angular tick thresholds.
6. Add ball-ball CCD or enforce/document an invariant that prevents tunneling.

## P3 — Finish gameplay mechanisms

1. Implement launcher charge/release.
2. Implement target reset modes.
3. Implement nudge cooldown, tilt decay and tilt suppression.
4. Honor max_active_balls.
5. Honor enabled for every object type.
6. Implement declared slingshot/target cooldowns.

## P4 — Repair editor transaction model

1. Route preview input to preview_sim.
2. Maintain independent keyboard state.
3. Replace history ring with explicit tail/sequence/saved-index semantics.
4. Sort selected object indices before destructive deletion.
5. Prompt dirty state consistently on New/Open/Close.
6. Use Unicode Win32 file APIs and a real atomic replacement sequence.
7. Enforce locks/layers and remove/finish dead tools.

## P5 — Replay/evidence integrity

1. Canonically length-frame every scene/runtime field that affects behavior.
2. Make replay grammar exact, bounded and fully validated.
3. Make release generation ingest raw result records rather than constants.
4. Reference real emitted test IDs.
5. Generate visual evidence only from the named UI state and record provenance.
6. Measure GUI/headless equivalence through GUI-driven logical input + checkpoint comparison before claiming it.

---

# 11. Regression tests that should be added

1. Parse Vec2 `(1,2,3)` under ASan: deterministic reject, no memory error.
2. Parse five-value rect under ASan.
3. `member_count=257` and `action_count=257`: exact bounded error.
4. 63-byte ID accepted / 64-byte destination-overflow case rejected.
5. Fresh GUI → choose Wall → click canvas: object appears, no crash.
6. Resize GUI below palette+inspector width under guard heap/ASan.
7. Dirty table → Close → Yes → relaunch: bytes persisted.
8. Preview drop-target hit → exit preview: authored scene fingerprint unchanged.
9. Reset same runtime twice without reparsing: identical initial runtime state.
10. ENABLE/DISABLE every object type: only intended runtime enabled bit changes.
11. Ball passes beyond wall endpoint: no invisible infinite-line collision.
12. Ball rebounds and leaves contact next substep: no zero-time pin.
13. Sensor crossing emits one ENTER, expected STAYs, one EXIT.
14. Spinner impact changes angle and generates ticks only at tick-angle thresholds.
15. Launcher hold/release produces charge-dependent velocity.
16. Drop target AFTER_DELAY and ON_NEW_BALL each reset at the correct transition.
17. Nudge faster than cooldown is suppressed; tilt decays; tilt suppresses configured controls.
18. Table `max_active_balls=2`, action requests 10: active count never exceeds 2.
19. Change only slingshot impulse/one-way direction: scene fingerprint must change.
20. Replay malformed header/version/numeric/STEP lines: strict rejection.
21. 257+ edits: retained history undoes/redoes in exact chronological order.
22. Ctrl-select unsorted indices then Delete: exactly selected IDs disappear.
23. Preview keyboard input actuates preview flippers, not hidden play sim.
24. Release generation with missing/raw-failing tests must refuse to emit PASS.
25. Every V/A visual ID should contain an assertion/provenance record proving the named visual state.

---

# 12. Final judgment

This submission deserves substantial credit. It contains real handwritten systems code, a legitimate shared headless core, nontrivial format/parser work, deterministic simulation infrastructure, mechanism impulse math, a custom renderer/PNG path and a broad fixture/test/tool ecosystem.

But the archived `722/722` plus broad PASS narrative should **not** be read as proof that the desktop pinball sandbox is complete, safe or failure-resistant.

The central distinction is:

> **The project is strong at producing deterministic headless artifacts, but weak at preserving invariants across malformed input, runtime/editor state boundaries, native editor transactions, and evidence provenance.**

The editor has normal-use crash/data-loss paths. Several headline mechanisms exist in the schema but not in runtime. Collision/contact lifetime has serious correctness defects. Gameplay mutates authored table state. Replay fingerprints omit important semantics. Release/evidence scripts manufacture substantial portions of their PASS/traceability story from constants rather than executed case records.

**Benchmark-worthy implementation effort:** yes.  
**Credible evidence of a fully working v1.0 desktop editor/game:** no.  
**Safe to trust authored tables through normal editor/failure paths:** no.  
**Strong foundation after P0–P3 repairs:** yes.

The highest-value next work is not more feature surface or a larger assertion count. It is to repair state ownership, parser memory safety, editor transactions, finite-contact physics, and make every verification claim trace back to an actual executed artifact.
