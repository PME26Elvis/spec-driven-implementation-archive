# 12 — Delivery, Definition of Done, and Release Gates

## 1. Completion language

The assignment may be described as **complete** only when every mandatory Release Gate is PASS.

If any mandatory gate is FAIL, BLOCKED, SKIPPED, NOT RUN, or UNKNOWN, the assignment is not complete.

## 2. Required final project classes

The delivered implementation MUST include:

- application source and headers;
- engineering utility source;
- automated tests;
- test fixtures;
- build definition;
- user-facing README;
- architecture/implementation notes;
- `TEST_REPORT.md`;
- `RELEASE_CHECKLIST.md`;
- `KNOWN_ISSUES.md`;
- `VISUAL_EVIDENCE.md` and evidence files;
- `RELEASE_RESULT.json` conforming to the task-package schema;
- `RELEASE_EVIDENCE.json` conforming to the task-package evidence schema;
- locscan configuration and generated locscan report.

Exact directory names are not prescribed.

## 3. Build Gate

PASS only if:

- clean build from delivered source succeeds;
- implementation is C17;
- required application/utilities/tests binaries are produced;
- no prohibited dependency is linked/embedded;
- warnings policy is documented;
- release does not rely on missing generated source outside delivery.

## 4. Dependency Gate

Automatic FAIL examples:

- native Win32 Common Controls/dialogs, GDI/Direct2D/DirectWrite/GDI+/DWM material, WinUI/WPF/WinForms, GTK/Qt/SDL/GLFW/Cairo, or another prohibited framework used to replace required custom UI/rendering;
- Box2D/Chipmunk/other physics engine;
- third-party JSON/YAML parser replacing required parser implementation;
- WebView/Electron UI;
- OpenGL/Vulkan used to bypass software-render requirement;
- external animation/UI framework replacing required hand-built systems.

## 5. Main UI Gate

PASS only if:

- real Win32 top-level desktop window opens on the supported Windows target;
- required layout exists;
- custom controls function;
- minimum/large window layouts usable;
- required hover/ripple/glow/capsule/panel/modal animations exist;
- modal blur/dim exists;
- frosted toolbar exists;
- every primary visible button is wired.

## 6. Editor Gate

PASS only if:

- all 15 required object types can be authored through UI;
- selection/multiselection/marquee work;
- move/rotate/resize work;
- grid/snap work;
- duplicate/copy/paste/delete work;
- ≥100-step Undo/Redo works;
- Inspector edits real authored data;
- Simulation Preview is non-destructive;
- Validate Scene generates required Error/Warning classes.

## 7. Persistence Gate

PASS only if:

- `.pbt` parser/writer implemented;
- full save/load round-trip passes;
- malformed file safe rejection passes;
- duplicate IDs/references detected;
- Save As works;
- dirty Save/Discard/Cancel works;
- atomic-save fault test/evidence demonstrates previous-file protection;
- failed load preserves currently open scene.

## 8. Physics Core Gate

PASS only if:

- fixed `1/240 s` timestep;
- semi-implicit Euler baseline;
- ball/static collision;
- ball/ball collision;
- moving-flipper contact response;
- friction/restitution;
- penetration correction;
- bounded collision iterations;
- CCD high-speed thin-wall acceptance passes;
- Sensor/Drain swept crossing passes;
- required stress tests have no NaN/inf;
- GUI/headless share production core.

## 9. Determinism Gate

PASS only if:

- one nontrivial replay repeated 10× has identical same-build checkpoints/final fingerprints;
- GUI/headless replay state matches;
- playback speed does not alter step-indexed result;
- resize/display timing does not alter replay result.

## 10. Gameplay Gate

PASS only if:

- launcher charge/launch works;
- left/right flipper controls work;
- score works;
- combo boundary rules pass;
- multiplier override works;
- actual multiball works;
- ball-ball collision remains enabled;
- Drain/turn/game-over semantics pass;
- event actions work;
- cyclic events are bounded;
- 16-ball stress passes.

## 11. Replay Gate

PASS only if:

- recording starts from deterministic fresh session;
- `.pbr` save/load works;
- scene fingerprint checked;
- playback applies logical fixed-step actions;
- replay validation capability exists;
- malformed replay handling passes;
- final recorded/replayed score and state match.

## 12. Physics Inspector Gate

PASS only if user can inspect required selected-ball fields and toggle collision shapes, contact point/normal, velocity vector, Sensor/Drain bounds, and relevant diagnostics. Values must be real runtime state.

## 13. Headless Gate

PASS only if:

- simulation/replay runs with no GUI window/interactive desktop dependency;
- JSON final/checkpoint state emitted;
- expected comparison returns correct status;
- parse/validation failures return non-zero;
- headless path uses production physics/event modules.

## 14. Engineering Utilities Gate

PASS only if:

- locscan works;
- both JSON and YAML locscan config work;
- binary/generated exclusions work;
- scenecheck works;
- simcheck works;
- replaycheck capability works;
- unified test runner works.

## 15. Automated Test Gate

PASS only if:

- at least 420 meaningful mandatory automated tests exist;
- all mandatory tests pass;
- none are skipped;
- machine-readable summary exists;
- every named scenario is covered;
- 1,000,000-step long run passes.

## 16. Visual Evidence Gate

PASS only if every required V01–V38 static screenshot and A01–A25 transition evidence item is present, indexed, truthful, and corresponds to delivered build.

## 17. Stress Gate

PASS only if all succeed:

- 500-authored-object table load/validate/edit/save/reload;
- 16 active balls for at least 30 simulated seconds;
- 1,000,000 fixed-step headless run;
- 10× deterministic replay;
- no non-finite physics state;
- no crash;
- no unbounded transient memory growth attributable to per-step leak.

## 18. Error Handling Gate

PASS only if mandatory edge cases have automated or documented manual acceptance coverage. Malformed external input must not crash the product.

## 19. Anti-placeholder Gate

PASS only if reviewer can verify:

- every primary control executes specified behavior;
- no placeholder primary controls remain;
- no hard-coded PASS test output;
- no fake runtime Inspector data;
- no prewritten predetermined trajectory masquerades as physics;
- no separate toy solver exists only for tests;
- visual evidence is from actual delivered build.

Any deliberate substitution is an automatic FAIL.

## 20. Release checklist

Final `RELEASE_CHECKLIST.md` MUST list every Gate in this document and status:

- PASS;
- FAIL;
- BLOCKED;
- NOT RUN.

Each PASS cites evidence: test category, report, screenshot ID, transition ID, fixture, or output.

## 21. Known issues

`KNOWN_ISSUES.md` is mandatory. If none known, explicitly state none known after listed validation.

A known issue violating a mandatory Gate forces that Gate to FAIL. It cannot be renamed a harmless limitation.

## 22. Version coherence

Application exposes a version such as `1.0.0`. Test report, machine-readable results, locscan report, and visual evidence index must refer to the same release build/version.

## 23. Definition of Done

All must be true:

- complete buildable source delivered;
- dependency restrictions satisfied;
- custom C/Win32 software-rendered client UI complete;
- editor complete;
- deterministic physics complete;
- persistence robust;
- gameplay/events/multiball complete;
- replay complete;
- headless verification complete;
- Physics Inspector/debug complete;
- engineering utilities complete;
- automated tests complete/passing;
- stress gates pass;
- visual evidence complete;
- release reports complete;
- no mandatory known issue remains.

## 24. Stop condition

The implementer SHALL continue debugging rather than declare completion when a mandatory automated test or Release Gate is failing. If work ends with unresolved failures, final delivery MUST clearly enumerate each failure and corresponding Gate.

## 25. Advanced Editor Gate

PASS only if document-19 requirements are complete: groups/layers/locking, deterministic selection/marquee/overlap cycle, exact transforms, alignment/distribution, transactional history, semantic dirty-state restoration, structured clipboard reference remapping, zoom/pan/Fit, Edit/runtime isolation, and Measurement.

## 26. Pinball Mechanisms and Tilt Gate

PASS only if all five additional object mechanisms, Nudge, Tilt, new triggers/actions, replay semantics, scoring/physical behavior, validation, and canonical tests in document 21 pass.

## 27. Desktop Interaction/HiDPI Gate

PASS only if focus traversal/scopes, keyboard activation, UTF-8/Chinese preservation, required text editing, 100/125/150/200% UI scales, Reduced Motion, popup/modal capture, animation interruption, and damage repaint tests pass.

## 28. Reliability/Recovery Gate

PASS only if autosave/recovery, external modification conflict protection, legacy format migration, Safe Startup, parser robustness corpus, atomic-save fault injection, disk-full behavior, and transactional load invariants pass.

## 29. Diagnostics/Trace Gate

PASS only if Event Trace, Collision Trace, Scene Statistics, trace export/headless equivalence, and `detcompare` first-divergence behavior use real production state and pass acceptance.

## 30. Performance/Resource Gate

PASS only if P1/P2/P3 workloads, no-backlog P2 requirement, UI responsiveness floors, repeated-cycle memory bound, file-descriptor stability, trace/history memory bounds, and reported metrics meet document 25.

## 31. Canonical E2E Gate

PASS only if official `reference_full_game_v2.pbt` validates and J01–J24 are completed with mapped observable evidence.

## 32. Release Evidence Gate

PASS only if `RELEASE_EVIDENCE.json` is complete, every stable mandatory requirement is PASS with appropriate proof, all references resolve, Gate aggregation is consistent, and delivered `releasecheck` returns success.

## 33. Updated Definition of Done

Sections 23–24 remain applicable and are extended: all Gates in sections 3–19 and 25–32 are mandatory. Any Gate not PASS means the v1.0.0 assignment is incomplete.


## Windows Platform Binding Gate

PASS only if all of the following are true:

- build targets native Win32 desktop and C17;
- normal GUI launch creates a real top-level HWND and does not leave an unwanted console window;
- no required client-area control is implemented with a native child control/common control;
- required file picker and modals are custom surfaces;
- application-owned framebuffer is the source of required UI/canvas pixels;
- GDI use is within the document-32 presentation/glyph boundary;
- DWM/Direct2D/DirectWrite/GDI+ is not substituting for required software effects/rendering;
- Unicode Windows paths, clipboard text, and committed IME text satisfy the platform tests;
- Per-Monitor DPI behavior and runtime DPI change satisfy document 23/32;
- headless path creates no HWND and does not depend on interactive desktop state;
- Win32 USER/GDI/HANDLE resource stability passes;
- executable/runtime delivery is sufficient for launch on the documented Windows target without fetching a prohibited application framework.

Failure of this gate is a mandatory Release failure even when physics tests pass.
