# 14 — v1.0 Requirement Traceability Matrix

This matrix assigns stable IDs to mandatory requirement families. It is normative for verification/evidence coverage; detailed subsystem documents remain authoritative for behavior.

Every ID below SHALL appear exactly once in the implementation's `RELEASE_EVIDENCE.json`.

## A. Platform, dependency, and architecture

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-PLAT-01 | C17 implementation | README/00 | build audit | Build |
| R-PLAT-02 | Linux/X11 target | README/00 | launch/manual/build | Build |
| R-PLAT-03 | Only permitted low-level platform APIs | README/18 | link/source audit | Dependency |
| R-PLAT-04 | No prohibited GUI/physics/render/parser framework substitution | 18 | audit/tests | Dependency/Anti-placeholder |
| R-PLAT-05 | GUI and headless share production core | 07/18 | equivalence tests | Headless/Anti-placeholder |
| R-PLAT-06 | Software-rendered required UI | 02/18 | visual/source audit | Main UI |

## B. Core UI and custom rendering

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-UI-01 | Main custom desktop layout and wired controls | 01/02 | UI E2E + V01 | Main UI |
| R-UI-02 | Hover elevation | 02 | state test + A01 | Main UI |
| R-UI-03 | Click-point ripple | 02 | math/state + A02 | Main UI |
| R-UI-04 | Border glow | 02 | state + A03 | Main UI |
| R-UI-05 | Sliding capsule indicator | 02 | state + A04/A21 | Main UI |
| R-UI-06 | Animated collapsible panels | 02 | layout + A05/A21 | Main UI |
| R-UI-07 | Modal scale+opacity transition | 02 | numeric state + A06/A07 | Main UI |
| R-UI-08 | Real app-content backdrop blur/dim | 02 | pixel/visual + V12 | Main UI |
| R-UI-09 | Dynamic frosted toolbar | 02 | pixel/state + A09 | Main UI |
| R-UI-10 | Resize-safe minimum/large layouts | 02 | layout + V14/V15/A10 | Main UI |
| R-UI-11 | Correct clipping/damage/expose repaint | 02/23 | UI tests + A25 | Desktop Interaction |
| R-UI-12 | Animation interruption from current interpolated state | 02/23 | state + A21 | Desktop Interaction |
| R-UI-13 | Modal/popup input capture and dismissal | 23 | UI tests + A24 | Desktop Interaction |
| R-UI-14 | Keyboard focus ring and deterministic focus model | 23 | UI tests + V29/A23 | Desktop Interaction |
| R-UI-15 | Tab/Shift+Tab and keyboard activation | 23 | UI tests + A23 | Desktop Interaction |
| R-UI-16 | Reduced Motion behavior | 23 | state/visual | Desktop Interaction |
| R-UI-17 | 100/125/150/200% rerasterized UI scale | 23 | hit/layout + V31–V33/A22 | Desktop Interaction |
| R-UI-18 | UTF-8-safe custom text field editing | 23 | unit/UI tests | Desktop Interaction |
| R-UI-19 | Chinese text preserve/render/search/save/load/copy/paste | 23 | E2E + V29 | Desktop Interaction |
| R-UI-20 | Command Palette | 29 | state/E2E + V30 | Desktop Interaction |

## C. Editor and authoring

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-ED-01 | All 15 required object types authorable | 01/03/21 | editor tests + V02 | Editor/Mechanisms |
| R-ED-02 | Deterministic single/multi/Shift selection | 19 | state tests | Advanced Editor |
| R-ED-03 | Marquee replace/union/subtract | 19 | state tests | Advanced Editor |
| R-ED-04 | Overlapping-object selection cycling | 19 | state + A16 | Advanced Editor |
| R-ED-05 | Move/rotate/resize with correct world transforms | 03 | command tests | Editor |
| R-ED-06 | Grid and snapping | 03 | numeric tests + V16 | Editor |
| R-ED-07 | Groups create/ungroup/transform | 19 | state/history + V21 | Advanced Editor |
| R-ED-08 | Layer create/rename/order/visibility | 19 | state tests + V21 | Advanced Editor |
| R-ED-09 | Layer/object locking | 19 | state + A17 | Advanced Editor |
| R-ED-10 | Exact Transform Inspector commit/cancel/reject | 19/23 | UI/model tests + V22 | Advanced Editor |
| R-ED-11 | Alignment six variants | 19 | numeric tests + V22 | Advanced Editor |
| R-ED-12 | Distribution four variants | 19 | numeric tests + V22 | Advanced Editor |
| R-ED-13 | Mixed multi-selection Inspector | 19 | model/UI tests | Advanced Editor |
| R-ED-14 | Structured clipboard payload | 19 | unit/E2E | Advanced Editor |
| R-ED-15 | Paste fresh IDs and internal reference remap | 19 | unit/E2E | Advanced Editor |
| R-ED-16 | Duplicate reference/ID semantics | 19 | unit/E2E | Advanced Editor |
| R-ED-17 | >=100-step transactional Undo/Redo | 03/19/25 | history tests | Editor/Advanced Editor |
| R-ED-18 | Drag/multi-action history granularity | 19 | history tests | Advanced Editor |
| R-ED-19 | Undo to saved semantics restores clean | 19 | history/persistence tests | Advanced Editor/Persistence |
| R-ED-20 | Undo memory cap without transaction corruption | 25 | resource/history test | Performance Resource |
| R-ED-21 | Pointer-centered zoom | 19 | numeric UI test | Advanced Editor |
| R-ED-22 | Fit Scene/Fit Selection/100% | 19 | layout tests | Advanced Editor |
| R-ED-23 | Pan behavior and clamp | 19 | input/layout tests | Advanced Editor |
| R-ED-24 | Measurement tool distance/angle | 19 | math/UI tests | Advanced Editor |
| R-ED-25 | Edit/runtime state isolation | 19 | integration | Advanced Editor |
| R-ED-26 | Non-destructive Simulation Preview | 03/19 | state/fingerprint | Editor |
| R-ED-27 | Scene validation blocks Errors | 03 | tests + V05 | Editor |
| R-ED-28 | Object/group/layer hard-limit atomic failure | 17/19 | limit tests | Advanced Editor/Error Handling |

## D. Persistence, migration, and reliability

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-IO-01 | Current `.pbt` format-2 parser/writer | 06 | parser/round-trip | Persistence |
| R-IO-02 | Legacy format-1 read and deterministic migration | 24 | migration tests + V36 | Reliability Recovery |
| R-IO-03 | Unknown newer version transactional reject | 06/24 | parser test | Persistence/Reliability |
| R-IO-04 | Stable canonical serialization | 06 | byte/semantic round-trip | Persistence |
| R-IO-05 | Transactional failed load preserves open document | 06/24 | failure test | Persistence |
| R-IO-06 | Atomic save | 06/24 | fault-injection | Persistence/Reliability |
| R-IO-07 | Save failure stages preserve prior bytes/dirty identity | 24 | fault-injection | Reliability Recovery |
| R-IO-08 | Disk-full/no-space safe behavior | 24 | injected integration | Reliability Recovery |
| R-IO-09 | External modification conflict detection | 24 | integration + V35 | Reliability Recovery |
| R-IO-10 | External deletion handling | 24 | integration | Reliability Recovery |
| R-IO-11 | Autosave inactivity/max interval | 24 | test-clock integration | Reliability Recovery |
| R-IO-12 | Autosave never replaces formal save/clears dirty | 24 | integration | Reliability Recovery |
| R-IO-13 | Crash recovery discovery and explicit Recover/Discard | 24 | restart integration + V34 | Reliability Recovery |
| R-IO-14 | Recovery opens dirty without overwriting original | 24 | integration | Reliability Recovery |
| R-IO-15 | Corrupt recovery skipped safely | 24 | integration | Reliability Recovery |
| R-IO-16 | Safe Startup after repeated failed restore | 24 | restart integration | Reliability Recovery |
| R-IO-17 | Context-rich parser diagnostics | 24 | corpus tests | Error Handling |
| R-IO-18 | Mandatory malformed/fuzz corpus | 24/28 | data-driven tests | Error Handling |
| R-IO-19 | UTF-8 and NUL handling | 06/23/24 | parser/text tests | Persistence/Error Handling |
| R-IO-20 | Resource-size/count limits | 06/17/24 | boundary tests | Error Handling |

## E. Physics, timing, determinism, and numeric safety

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-PHY-01 | Fixed `1/240 s` timestep | 04/15 | unit/scenario | Physics Core |
| R-PHY-02 | Semi-implicit Euler baseline | 04/15 | math tests | Physics Core |
| R-PHY-03 | Ball/static capsule collision | 04/15 | scenarios | Physics Core |
| R-PHY-04 | Ball/ball collision | 04/15 | scenarios | Physics Core |
| R-PHY-05 | Moving-flipper collision/energy transfer | 04/15 | scenario + A11 | Physics Core |
| R-PHY-06 | Restitution/friction/penetration correction | 04/15 | invariant/scenarios | Physics Core |
| R-PHY-07 | CCD high-speed no tunneling | 04/15 | scenario + A13 | Physics Core |
| R-PHY-08 | Swept Sensor/Drain crossings | 04/20 | scenario | Physics Core |
| R-PHY-09 | Deterministic simultaneous-contact ordering | 20 | named scenarios | Determinism |
| R-PHY-10 | Same-TOI multi-contact solver | 20 | corner/wedge tests | Physics Core/Determinism |
| R-PHY-11 | Separate render/simulation clocks | 20 | scheduler tests | Determinism |
| R-PHY-12 | Frame-stall 60-step catch-up cap/no giant dt | 20 | scheduler test | Determinism |
| R-PHY-13 | Pause and exact Single Step | 20 | state/replay | Physics Core |
| R-PHY-14 | Numeric finite-value barrier | 20 | fault/extreme tests | Physics Core/Error Handling |
| R-PHY-15 | NON_FINITE_STATE inspectable failure | 20 | fault test | Error Handling |
| R-PHY-16 | BALL_OUT_OF_WORLD inspectable failure | 20 | scenario | Error Handling |
| R-PHY-17 | Deterministic PRNG/seed policy if randomness exists | 20 | replay tests | Determinism |
| R-PHY-18 | Deterministic runtime fingerprint complete state | 20/07 | checkpoint tests | Determinism |
| R-PHY-19 | Golden intermediate checkpoints | 20/28 | manifest/scenarios | Determinism |
| R-PHY-20 | Momentum/energy/stationary/free-flight invariants | 20/28 | invariant suite | Physics Core |
| R-PHY-21 | 1,000,000-step finite stable soak | 20/28 | headless soak | Stress |
| R-PHY-22 | 64-ball headless stress | 20/28 | headless soak | Stress |
| R-PHY-23 | Simulation-speed state equivalence | 20 | replay/checkpoint | Determinism |
| R-PHY-24 | Resize/UI scale/debug/trace cannot alter replay | 20 | equivalence suite | Determinism |

## F. Pinball mechanisms, gameplay, scoring, and events

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-GAME-01 | Launcher charge/launch curve | 01/05 | tests + A12 | Gameplay |
| R-GAME-02 | Bumper/slingshot scoring cooldown | 05 | scenarios | Gameplay |
| R-GAME-03 | Combo/multiball/override score order | 05 | gameplay tests | Gameplay |
| R-GAME-04 | Genuine multiball independent balls/ball-ball collision | 05 | scenario + V07/A14 | Gameplay |
| R-GAME-05 | Drain/turn/game-over lifecycle | 05 | scenarios + V19 | Gameplay |
| R-GAME-06 | Drop Target states/reset/scoring | 21 | mechanism tests + V23 | Mechanisms Tilt |
| R-GAME-07 | Stand-up Target qualified hits | 21 | mechanism tests | Mechanisms Tilt |
| R-GAME-08 | Rollover swept activation | 21 | mechanism tests + V24 | Mechanisms Tilt |
| R-GAME-09 | Spinner angular response/ticks | 21 | physics/game tests + A18 | Mechanisms Tilt |
| R-GAME-10 | Kickout capture/hold/eject | 21 | mechanism tests + A19 | Mechanisms Tilt |
| R-GAME-11 | Blocked Kickout ejection failure | 21 | scenario | Mechanisms Tilt/Error Handling |
| R-GAME-12 | Nudge left/right/up impulses | 21 | deterministic tests | Mechanisms Tilt |
| R-GAME-13 | Nudge cooldown and Tilt meter decay | 21 | boundary tests | Mechanisms Tilt |
| R-GAME-14 | Tilt trigger/state/visual | 21 | state + V25/A20 | Mechanisms Tilt |
| R-GAME-15 | Tilt suppresses score/flipper/launcher but not physics | 21 | integration | Mechanisms Tilt |
| R-GAME-16 | Tilt clears under turn lifecycle | 21 | integration | Mechanisms Tilt |
| R-GAME-17 | Nudge/Tilt replay determinism | 21/07 | replay | Determinism/Mechanisms Tilt |
| R-EVT-01 | Sensor ENTER/STAY/EXIT semantics | 20/05 | edge tests | Gameplay |
| R-EVT-02 | Same-step high-speed ENTER+EXIT ordering | 20 | scenario | Gameplay/Determinism |
| R-EVT-03 | Authored action order preserved | 05 | tests | Gameplay |
| R-EVT-04 | Deterministic cross-source event ordering | 20 | scenario | Determinism |
| R-EVT-05 | Required baseline event actions | 05 | integration | Gameplay |
| R-EVT-06 | Target/Kickout/Tilt new triggers/actions | 21 | integration | Mechanisms Tilt |
| R-EVT-07 | Event cycle/action budget protection | 20 | cycle/budget tests | Gameplay/Error Handling |
| R-EVT-08 | Event budget failure pauses/detectable headless | 20 | integration | Error Handling |

## G. Replay, headless, diagnostics, and utilities

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-RPL-01 | Replay record/save/load logical fixed-step inputs | 07 | replay tests | Replay |
| R-RPL-02 | Scene semantic fingerprint checked | 07 | replay tests | Replay |
| R-RPL-03 | Replay nudge inputs; Tilt derived | 07/21 | replay tests | Replay |
| R-RPL-04 | 10x same-build deterministic playback | 07/20 | repeated run | Determinism |
| R-HDL-01 | Headless no X11/display dependency | 07 | process test | Headless |
| R-HDL-02 | Headless final/checkpoint JSON | 07 | schema/check | Headless |
| R-HDL-03 | GUI/headless state equivalence | 07/20 | comparison | Headless/Determinism |
| R-DBG-01 | Physics Inspector real runtime fields | 22 | state + V09 | Physics Inspector |
| R-DBG-02 | Debug collision shapes/normals/vectors | 02/22 | visual + V08 | Physics Inspector |
| R-DBG-03 | Event Trace exact execution order/fields | 22 | trace tests + V26 | Diagnostics Trace |
| R-DBG-04 | Event Trace filtering/cap/export | 22 | unit/integration | Diagnostics Trace |
| R-DBG-05 | Collision Trace real contact/impulse fields | 22 | trace tests + V27 | Diagnostics Trace |
| R-DBG-06 | Headless trace export does not affect state | 22 | equivalence | Diagnostics Trace |
| R-DBG-07 | Scene Statistics edit/play correctness | 22 | state + V28 | Diagnostics Trace |
| R-UTIL-01 | locscan JSON/YAML categorized counting/exclusions | 11 | utility tests | Engineering Utilities |
| R-UTIL-02 | scenecheck shared parser/validator | 11 | utility tests | Engineering Utilities |
| R-UTIL-03 | simcheck shared physics/events | 11 | utility tests | Engineering Utilities |
| R-UTIL-04 | replaycheck capability | 11 | utility tests | Engineering Utilities |
| R-UTIL-05 | detcompare first divergence | 22/11 | equal/diverge tests | Diagnostics Trace/Utilities |
| R-UTIL-06 | releasecheck evidence/gate/version validation | 27/11 | schema/consistency tests | Release Evidence/Utilities |
| R-UTIL-07 | unified tests runner with stable test IDs | 11/28 | runner report | Automated Tests |

## H. Performance, resource stability, E2E, visual evidence, and release honesty

| ID | Requirement family | Primary spec | Verification | Gate |
|---|---|---|---|---|
| R-PERF-01 | P1 editor response/validate/save floors | 25 | performance run | Performance Resource |
| R-PERF-02 | P2 1x real-time with zero persistent backlog drop | 25 | performance run | Performance Resource |
| R-PERF-03 | P2 Pause/input responsiveness | 25 | integration timing | Performance Resource |
| R-PERF-04 | P3 headless stress completes correctly | 25 | performance run | Performance Resource |
| R-RES-01 | Repeated-cycle process memory bound | 25 | 100-cycle test | Performance Resource |
| R-RES-02 | Trace buffers bounded | 22/25 | resource test | Performance Resource |
| R-RES-03 | File descriptor stability | 25 | 1000-cycle test | Performance Resource |
| R-RES-04 | X11 transient resource stability | 25 | resource test | Performance Resource |
| R-REF-01 | Official full current-format reference table | 26 | scenecheck + V37/V38 | Canonical E2E |
| R-REF-02 | Reference table exercises all 15 types/routes | 26 | fixture audit/E2E | Canonical E2E |
| R-E2E-01 | Canonical J01–J24 end-to-end journey | 26 | E2E/evidence | Canonical E2E |
| R-TST-01 | >=420 meaningful automated tests | 28 | machine summary | Automated Tests |
| R-TST-02 | All named mandatory catalog cases | 28 | stable test IDs | Automated Tests |
| R-VIS-01 | Static evidence V01–V38 | 10 | evidence index | Visual Evidence |
| R-VIS-02 | Transition evidence A01–A25 | 10 | evidence index | Visual Evidence |
| R-REL-01 | RELEASE_RESULT schema/status honesty | 12/27 | releasecheck | Release Evidence |
| R-REL-02 | RELEASE_EVIDENCE complete requirement map | 27 | releasecheck | Release Evidence |
| R-REL-03 | RELEASE_CHECKLIST/Test Report/Known Issues/version coherence | 12/27 | releasecheck/manual | Release Evidence |
| R-REL-04 | No mandatory known issue hidden as limitation | 12/18 | checklist/review | Anti-placeholder |
| R-REL-05 | No fixture-specific hard-coding/fake evidence | 18 | audit/adversarial tests | Anti-placeholder |

## Coverage and completion rules

1. Every ID above is mandatory for v1.0.0 unless the detailed specification explicitly says the requirement is conditional (for example, PRNG behavior if optional randomness exists).
2. Conditional requirements still appear in `RELEASE_EVIDENCE.json`; if the condition is not exercised, evidence SHALL prove the condition is absent and status remains PASS only when the specified non-random baseline is satisfied.
3. A requirement marked PASS must cite an appropriate verification reference under document 27.
4. A visual proof cannot replace a numeric/functional proof when the matrix lists an automated or headless channel.
5. If a future task-package revision adds a mandatory feature, it SHALL receive a new stable ID rather than silently changing an unrelated old ID.
