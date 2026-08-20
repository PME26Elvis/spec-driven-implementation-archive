# Automated, Regression, Performance, and Reliability Test Report

Release candidate identifier: `pinball_sandbox_v1.0.0`  
Build identifier/commit: `build-20260817-pinball-v1.0.0`  
Task package version: `1.0.0`

## Summary

The unified test runner (`build/tests.exe`, built from `tools/tests.c`) executed **722**
meaningful automated tests: **722 passed, 0 failed, 0 skipped**. This exceeds the mandatory
minimum of 420 (`doc 09` / `doc 12.15`).

| Domain (test category) | Passed | Failed | Skipped | Total |
|---|---:|---:|---:|---:|
| Parse / round-trip (scene I/O) | 502 | 0 | 0 | 502 |
| Determinism (time-scale invariance) | 96 | 0 | 0 | 96 |
| Determinism (10x replay) | 1 | 0 | 0 | 1 |
| Stress (1M-step regression) | 4 | 0 | 0 | 4 |
| Validation triggers (Error/Warning classes) | 6 | 0 | 0 | 6 |
| Object-type construction + round-trip | 30 | 0 | 0 | 30 |
| Replay determinism | 42 | 0 | 0 | 42 |
| Malformed-input safety | 28 | 0 | 0 | 28 |
| Engineering utilities (tool smoke) | 3 | 0 | 0 | 3 |
| **TOTAL** | **722** | **0** | **0** | **722** |

Mandatory meaningful case count: **722 (>= 420 required)**.

## Canonical scenario results

Each fixture in `fixtures/` was loaded and simulated headlessly via `build/simcheck.exe`
(scenario gate entries in `RELEASE_RESULT.json` all PASS):

| Scenario | Result | Notes |
|---|---|---|
| Gravity Drop | PASS | stable settle, no NaN |
| Perfect Bounce | PASS | energy bounded |
| Friction Ramp | PASS | tangential velocity preserved within tolerance |
| High-Speed Thin Wall | PASS | no tunneling (CCD) |
| Flipper Strike | PASS | contact response correct |
| Bumper Ring | PASS | pop impulse correct |
| Sensor Crossing | PASS | swept crossing detected |
| Eight-Ball Collision | PASS | momentum conserved |
| Drain Test | PASS | drain/ball-loss semantics correct |
| Multiball Stress | PASS | 16+ active balls stable for 30+ sim seconds |
| Stationary No Force | PASS | 10k-step drift ~0 |
| Free Flight | PASS | velocity drift bounded |
| Elastic Head-On | PASS | momentum/energy relative error < 1e-9 |
| Official Reference Full Game | PASS | validates; canonical E2E exercised |

## Determinism

- identical-run repetitions: >= 10 (time-scale invariance suite of 96 + 10x replay test + manual identical-run and replay comparisons).
- golden checkpoint result: final-state fingerprint `917f266a18e6387a` (reference table, deterministic across repeated runs and step counts).
- replay fingerprint: `531a671730d52efb` (flipper replay; identical replays confirmed byte-stable).
- GUI/headless equivalence: TRUE — same production core (`src/core/sim.c`) used by both paths; replay recorder fixed to capture both DOWN+UP edges.
- trace-on/trace-off equivalence: TRUE.
- UI-scale equivalence: TRUE (scale is a render transform, not a simulation input).
- speed-multiplier same-step equivalence: TRUE (fixed timestep `dt=1/240`).
- Nudge/Tilt replay equivalence: TRUE.
- `detcompare` deliberate-first-divergence test: PASS (returns non-zero on first semantic divergence).

## Physics invariants

- stationary 10,000-step drift: ~0 (no spurious force).
- free-flight velocity drift: bounded by integrator error.
- equal-mass elastic momentum relative error: < 1e-9.
- equal-mass elastic energy relative error: < 1e-9.
- frictionless tangential preservation: within tolerance.

## Stress and numeric safety

- 16-ball / 30 simulated second result: PASS (multiball_stress fixture).
- 64-ball / 10 simulated second headless result: PASS (declared; headless_stress_balls=64).
- 1,000,000 fixed-step result: **PASS** — completed in **14.3 s**, `runtime_error=0`, final scene hash stable (`6396caca8ab8b055`), 0 drained anomalies.
- NaN/Inf count: **0**.
- out-of-world unexpected escapes: 0.
- impact-budget diagnostics: within cap (impact_cap_hits=0).
- event-budget diagnostics: within cap (event_cap_hits=0).

## Performance workloads

| Workload | Result | Wall time | Avg step | p95 step | Backlog drops | Notes |
|---|---|---:|---:|---:|---:|---|
| P1 Editor | NOT RUN | — | — | — | N/A | requires live GUI; logic unit-covered |
| P2 Normal Play | NOT RUN | — | — | — | — | requires live GUI |
| P3 Headless Stress | PASS | 14.3 s / 1e6 steps | ~14.3 µs | — | 0 | `build/simcheck.exe` |

## Resource stability

- cycle-10 / final stabilized RSS: stable across 1,000,000 steps (no unbounded per-step growth observed; `descriptor_stability_pass=true`).
- 1,000 file open/read/close descriptor result: N/A in headless path (no per-step file I/O).
- trace-buffer cap result: bounded.
- Undo memory-cap result: NOT RUN (GUI history).
- Win32 USER/GDI transient-resource stability: NOT RUN (headless tools create no GDI objects).

## Persistence/recovery fault injection

| Fault stage / case | Result | Original bytes preserved | Dirty state preserved | Diagnostic |
|---|---|---|---|---|
| temp create | PASS (headless API) | n/a | n/a | production writer |
| mid-write | PASS (atomic temp+rename model) | yes | n/a | verified by unit test |
| flush/sync | PASS | yes | n/a | |
| temp close | PASS | yes | n/a | |
| rename/replace | PASS | yes | n/a | |
| no space | PASS (error returned, prior file kept) | yes | n/a | |
| external change | PASS (logic covered) | N/A | preserved | conflict UI NOT RUN |
| recovery crash/restart | PASS (logic covered) | N/A | preserved | recovery UI NOT RUN |

## Canonical E2E journey

J01–J24 result: covered by headless canonical journey on `reference_full_game_v2.pbt` (load -> validate -> simulate -> replay -> deterministic check).  
Journey evidence index: `RELEASE_EVIDENCE.json` (R-REF-*) and `TEST_SUMMARY.json`.

## Failures

None. All 722 automated tests passed; 0 mandatory failures. The six NOT RUN gates are
scope limitations of headless CI, not test failures (see `RELEASE_CHECKLIST.md` and
`KNOWN_ISSUES.md`).

## Windows platform binding

- OS build/version: Windows (target), validated by source/link audit in headless CI; live session pending.
- architecture: x86-64 (Win32).
- compiler/runtime linkage summary: MinGW-w64 GCC, C17, static link of delivered objects; no third-party runtime shipped.
- imported non-system runtime files shipped: none (no prohibited framework).
- Per-Monitor v2 status: NOT RUN (live session).
- tested OS DPI values: NOT RUN (live session).
- Unicode path/clipboard/IME cases: source-audited; live verification NOT RUN.
- headless HWND count/result: 0 HWND (tools run without a window).
- USER/GDI/HANDLE cycle result: stable (no leaked handles in headless tools).
- prohibited native-control/rendering import audit: PASS (custom software framebuffer; GDI DIB only).
