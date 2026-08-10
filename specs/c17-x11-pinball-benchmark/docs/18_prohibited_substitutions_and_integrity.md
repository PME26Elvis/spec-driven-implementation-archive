# 18 — Prohibited Substitutions and Implementation Integrity

## 1. Purpose

This assignment deliberately requires difficult production subsystems. A substitute that produces a convincing screenshot or passes only a narrow demonstration is not equivalent to implementing the required behavior.

This document states explicit integrity boundaries. A violation is a Release Gate failure even if the visible application appears functional.

## 2. General rule

A required subsystem MUST exist as functional production code and MUST be exercised by the product path that claims to use it.

It is prohibited to replace required behavior with:

- mock behavior;
- placeholder behavior;
- hard-coded expected outputs;
- fixture-specific branches;
- pre-rendered imagery;
- disconnected controls;
- separate simplified logic used only for tests;
- hidden high-level libraries that perform the required subsystem;
- claims in documentation without corresponding executable behavior.

## 3. GUI framework substitution

Prohibited:

- GTK widgets;
- Qt widgets or Qt Quick;
- SDL UI/rendering as a replacement for required X11/software-rendered stack;
- GLFW;
- Cairo as the application renderer;
- WebView, Electron, browser DOM/CSS UI;
- ImGui or similar immediate-mode GUI frameworks;
- any third-party widget toolkit.

Allowed X11/Xlib/XCB primitives may create/manage windows, receive events, interact with low-level text/input facilities where specified, and present the application-owned pixel output. They do not waive the custom UI-engine requirement.

## 4. Rendering substitution

Prohibited:

- embedding screenshots of buttons/panels instead of rendering controls;
- using a prerecorded animation as UI feedback;
- using OpenGL/Vulkan solely to import an external rendering/UI engine;
- treating an opaque toolkit surface as the main application renderer;
- skipping clipping and drawing outside control bounds while hiding the defect under a static layout.

The product must own the render state and draw the required UI and table content from live state.

## 5. Animation substitution

Prohibited:

- sleeping for the animation duration and then jumping to final state;
- toggling only a color while claiming hover elevation;
- a ripple that always originates from button center regardless of click position;
- replacing blur with a flat translucent rectangle where blur is explicitly required;
- playing a fixed sequence that cannot be interrupted/reversed;
- making screenshots of intermediate states without a real time-varying transition.

## 6. Physics-engine substitution

Prohibited:

- Box2D, Chipmunk, Bullet, or another external physics engine;
- hard-coded trajectories;
- moving balls along authored splines;
- collision outcomes chosen by object name or acceptance-fixture identity;
- teleporting a tunneling ball to the safe side without performing the specified continuous collision behavior;
- using frame-rate-dependent variable-step physics as the production solver;
- using one solver in GUI and another simplified solver in headless/tests.

The production physics core must calculate state from the specified initial state, input sequence, scene properties, and deterministic rules.

## 7. Fake multiball

Prohibited:

- rendering clones around one authoritative physics ball;
- omitting ball-ball collisions while claiming complete multiball;
- sharing position/velocity state among visually distinct balls;
- serially replaying one ball several times and compositing the result;
- capping internally below the required stress count while displaying a larger count.

Each active ball requires independent runtime state and deterministic runtime ID.

## 8. Flipper substitution

Prohibited:

- teleporting the ball on flipper key press;
- adding a fixed upward velocity whenever the ball is near a flipper;
- ignoring moving-surface velocity in collision response;
- treating flippers as static colliders with only a visual rotation;
- using animation angle that differs from physics angle.

## 9. Sensor/event substitution

Prohibited:

- scoring based only on ball visual overlap without the production Sensor/event path;
- firing Sensor ENTER every frame while the ball remains inside;
- ignoring high-speed swept crossings;
- executing fixture-specific scores from scene filename;
- silently discarding event actions to avoid event-order complexity.

## 10. Launcher substitution

Prohibited:

- random launch velocity;
- fixed launch velocity regardless of held time;
- visually filling a power meter that does not control launch speed;
- repeatedly launching on key-repeat events without a press/release logical transition;
- applying gravity or score contacts to a launcher-held READY ball before release.

## 11. Scoring substitution

Prohibited:

- incrementing a visual score label without updating authoritative score state;
- using wall-clock time for combo/cooldown rules that are specified in simulation time;
- ignoring multiplier ordering;
- wrapping score on integer overflow;
- awarding an acceptance-scene expected score without processing its actual events.

## 12. Edit-mode substitution

Prohibited:

- an Object Palette whose items do nothing;
- property fields that change only their displayed text;
- a Save file that omits properties visible in Inspector;
- editing runtime objects while pretending authored scene changed;
- implementing Undo by reloading a fixed starter scene;
- limiting Undo/Redo to one operation while the UI suggests history support;
- changing only visuals for Move/Rotate/Resize while collision geometry remains stale.

## 13. Copy/Paste and duplicate substitution

Clipboard operations must create real authored objects with new unique IDs and preserve all applicable properties/references according to editor rules.

Prohibited:

- copying only a display label;
- duplicating an object at render time without adding it to authored scene;
- producing duplicate IDs;
- leaving copied events silently pointing to impossible targets without validation.

## 14. Preview substitution

Simulation Preview is non-destructive.

Prohibited:

- modifying authored object positions as Preview physics runs;
- polluting Undo history with runtime motion;
- retaining temporary balls after Preview ends;
- saving Preview runtime state into `.pbt` unless a separate explicitly allowed authoring action exists.

## 15. Persistence substitution

Prohibited:

- Save button that only clears dirty flag;
- storing scene solely in process memory;
- writing a screenshot instead of scene data;
- serializing only object position while dropping behavior/properties/events;
- destructive load that clears current document before candidate parse/validation succeeds;
- truncating the destination file before a failed Save leaves a valid replacement;
- silently accepting unsupported scene version as current format.

## 16. File-picker substitution

Because high-level toolkit dialogs are unavailable by design, Open and Save As require a real in-application workflow.

Prohibited:

- hard-coding one input/output path as the only normal workflow;
- requiring source-code edits to change path;
- making Open/Save As buttons nonfunctional and claiming CLI-only file selection;
- using an external high-level GUI toolkit solely for the file dialog.

## 17. Replay substitution

Prohibited:

- video capture presented as replay;
- recording only final coordinates;
- recording wall-clock timestamps without deterministic fixed-step inputs;
- replay that reads stored ball trajectories instead of re-running production simulation from initial state and logical inputs;
- accepting mismatched scene fingerprint without explicit failure/warning behavior required by the replay spec;
- omitting launcher/flipper logical transitions.

## 18. Headless substitution

Prohibited:

- invoking the GUI and merely hiding its window while still requiring DISPLAY for simulation;
- a second approximate physics implementation;
- different event ordering than GUI mode;
- skipping unsupported objects in headless mode;
- emitting fabricated checkpoint JSON without actually advancing simulation.

Headless operation must execute the same core state-transition logic as GUI operation.

## 19. Physics Inspector substitution

Prohibited:

- placeholder `0,0` vectors;
- stale values updated only when simulation stops;
- contact normals unrelated to active production contacts;
- hard-coded diagnostic counters;
- object IDs that do not identify the corresponding runtime/authored entity.

## 20. Scene validation substitution

Prohibited:

- validator that always returns success;
- parser-only validation while semantic invariants are omitted;
- GUI validator and `scenecheck` disagreeing because they use unrelated rule implementations;
- silently correcting invalid authored values on load where the spec requires an Error;
- suppressing validation Errors to allow Play.

## 21. Automated-test substitution

Prohibited:

- tests that only assert literal constants copied from the test body;
- tests that never invoke production modules for the behavior they claim to verify;
- counting the same parameterized case hundreds of times without meaningful distinct coverage solely to reach the required count;
- marking skipped/not-run tests as PASS;
- rewriting expected values at test runtime from actual output;
- swallowing assertion failures and returning exit code 0;
- excluding failing required categories from the default `--all` run.

## 22. Acceptance-fixture special casing

The package fixture names, paths, IDs, expected values, and hashes are public test data. Production code MUST NOT recognize them as special cases.

Examples of prohibited code patterns include logic semantically equivalent to:

- `if filename == "gravity_drop.pbt" then return expected_state;`
- `if object id == fixture_known_id then bypass collision;`
- `if step_count == manifest_checkpoint then overwrite state;`
- matching an acceptance scene hash to a predetermined result.

The same algorithms must operate on arbitrary equivalent user-authored scenes.

## 23. locscan substitution

Prohibited:

- shelling out to a preexisting line-counter and presenting its output as the required utility;
- always reporting hard-coded repository totals;
- ignoring the requested JSON/YAML config;
- counting binary/generated/result files despite exclusions;
- counting only `.c` files while claiming configured categories;
- treating malformed config as an empty valid config without error.

## 24. Parser substitution

Required `.pbt`, `.pbr`, JSON-config, and YAML-subset handling must be implemented within the dependency policy.

Prohibited:

- bundling a third-party parser implementation;
- invoking Python/Node/system tools as the normal parser path;
- assuming trusted input and using unchecked parsing that crashes on malformed fixtures.

## 25. Build substitution

Prohibited:

- checking in only a prebuilt executable;
- requiring source files not included in delivery;
- downloading source-generation blobs at build time to reconstruct required implementation;
- fetching forbidden dependencies during build;
- generating large required source modules from hidden/precompiled artifacts.

A clean source delivery must be sufficient to build the product under its documented Linux/X11 prerequisites.

## 26. Evidence substitution

Prohibited:

- screenshots from a design mockup rather than the delivered executable;
- images edited to hide clipping/overlap defects;
- evidence from a different build than the release candidate without disclosure;
- reusing one image under several evidence IDs when the required states differ;
- claiming an animation passed based only on start/end screenshots where transition evidence is required.

The method used to capture evidence is intentionally not prescribed.

## 27. Release-report substitution

A gate is PASS only when its required evidence actually succeeded.

Prohibited:

- PASS for a test that was not run;
- PASS because implementation code exists but acceptance failed;
- hiding failures under Known Issues while declaring all gates passed;
- changing mandatory to optional in delivery notes;
- deleting or weakening task-package tests/specification to obtain green status.

Use FAIL, BLOCKED, or NOT RUN truthfully where appropriate.

## 28. Permitted optimizations

Optimization is allowed when externally observable behavior remains conformant. Examples include:

- spatial broad phase;
- cached raster assets generated at runtime;
- dirty rectangles;
- SIMD written as optional platform-specific acceleration if a conforming baseline remains and dependency rules are respected;
- object pools;
- optimized parser buffers;
- compact Undo command storage.

Optimization must not introduce nondeterministic outcomes or remove required diagnostics.

## 29. Permitted extra features

Additional features are allowed only when they do not weaken required behavior, destabilize the release, or replace mandatory functionality.

Examples can include extra themes, additional editor helpers, extra benchmark scenes, extra diagnostics, or optional object properties.

Required acceptance evaluates the normative feature set, not the quantity of extras.

## 30. Integrity declaration

The final release checklist must include an explicit declaration that:

- no prohibited dependency or substitute implementation is knowingly present;
- acceptance fixtures are not special-cased;
- GUI and headless simulation share production physics/event logic;
- visual evidence originates from the delivered release candidate;
- required failures, if any, are reported rather than hidden.

A false declaration does not convert a nonconforming implementation into a conforming one.

## 30. Advanced editor substitutions

Prohibited:

- groups implemented only as visual selection with no persisted membership;
- layer visibility implemented by deleting/disabling physics objects;
- lock implemented only by hiding handles while Inspector still mutates data;
- alignment/distribution that changes only render position but not authored transforms;
- paste that preserves duplicate IDs or silently drops required internal references;
- dirty state that stays permanently dirty after Undo to exact saved semantics.

## 31. Reliability substitutions

Prohibited:

- autosave writing directly over the formal scene file;
- recovery that silently overwrites original on startup;
- checking only modification timestamp and overwriting known changed content;
- fake atomic save that truncates destination before complete temp write;
- treating unsupported new scene version as if it were current;
- Safe Startup that deletes recovery/problem scenes.

## 32. Determinism/physics substitutions

Prohibited:

- resolving simultaneous collisions in pointer/hash iteration order;
- using wall clock/random global state to affect canonical gameplay;
- changing `dt` to catch up after frame stalls;
- silently zeroing NaN/Inf and continuing;
- disabling ball-ball collisions/per-object mechanisms during stress runs;
- hard-coding golden checkpoint states by fixture name.

## 33. Diagnostics substitutions

Event/Collision Trace and Scene Statistics must read production runtime state. Prewritten log text, guessed impulse values, or a second simplified diagnostic simulation automatically fail Diagnostics/Anti-placeholder Gates.

## 34. HiDPI substitution

Rendering a complete 100% UI to a low-resolution framebuffer and uniformly scaling that bitmap to 125/150/200% is prohibited as the required HiDPI implementation.

## 35. Evidence substitution

A `RELEASE_EVIDENCE.json` entry marked PASS without a valid referenced proof is not completion. Creating placeholder evidence paths or test IDs is an integrity failure.
