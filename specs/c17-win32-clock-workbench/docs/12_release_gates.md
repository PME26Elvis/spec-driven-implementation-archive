# 12 — Definition of Done and Release Gates

## 1. Completion rule

The assignment may be declared complete only when every **Required Gate** below passes.

If any required gate fails, the delivery must be described as incomplete and the failed gate(s) identified.

Optional polish does not compensate for a failed required gate. Unless a gate explicitly tests an animation/effect-disabled setting, visual-effect gates are evaluated from the shipped default configuration (`animation.enabled=true` and the documented nonzero default effect strengths/radii), so user-configurable disabling does not excuse a missing default implementation.

## Gate G01 — Clean C17 build

**Required.**

Pass conditions:

- Project A and Project B build from submitted source;
- no mandatory generated source is missing;
- build uses C17 mode;
- no C++ compilation is required;
- build produces the documented executables.

## Gate G02 — Library/platform compliance

**Required.**

Pass conditions:

- no forbidden GUI/render/parser framework is linked or embedded;
- Project B uses custom application-owned framebuffer rendering;
- Win32/GDI are limited to the allowed substrate responsibilities; no native child-control or GDI/Direct2D text/shape rendering substitutes for the custom UI;
- final DIB presentation is 1:1 and does not use GDI stretching/interpolation as application UI scaling.

## Gate G03 — Project A complete

**Required.**

Pass conditions:

- `locscan` works and passes required fixtures;
- `cfgcheck` works for JSON and YAML and normalized equivalence;
- `stateprobe` validates the versioned fixture schema and the shipped good/bad fixtures;
- all are C17 implementations;
- final self-validation actually runs the three Project A tools against the submitted Project B/examples as required by the testing specification, so the utilities are not merely unused side deliverables.

## Gate G04 — Main window and custom UI

**Required.**

Pass conditions:

- app launches a usable Win32 main window;
- Clock and Settings sections exist;
- mandatory controls are custom-rendered;
- no placeholder/dead mandatory controls.

## Gate G05 — Canonical-time synchronization

**Required.**

Pass conditions:

- one canonical simulated time drives all displays;
- startup/reset never substitutes operating-system wall-clock time;
- analog and digital views update in same frame after committed edits;
- no reproducible divergence during playback/manipulation.

## Gate G06 — Analog clock rendering

**Required.**

Pass conditions:

- face, ticks, numerals, three hands, center hub rendered;
- continuous minute/hour positions correct;
- smooth/tick second-hand setting works;
- resize preserves circular geometry.

## Gate G07 — Direct hand manipulation

**Required.**

Pass conditions:

- all three hands can be grabbed and dragged;
- hit testing is practical;
- 12-o'clock crossing is continuous;
- full-revolution semantics correct;
- drag outside face remains coherent;
- Escape cancellation restores pre-drag time;
- focus loss cancels safely without a history entry;
- with stable second-hand style `tick`, active dragging adds no extra tick-flooring: `drag_snap=off/on_release` remains continuous and only explicit `drag_snap=live` may quantize during motion; stable tick rendering resumes after the gesture.

## Gate G08 — Digital time editing

**Required.**

Pass conditions:

- individual hour/minute/second fields are typable and scrub-draggable;
- scrub threshold/unit/wrapping behavior matches the fixed contract;
- one completed digital scrub produces one undo transaction;
- valid typed commit updates clock;
- invalid values do not enter canonical state;
- keyboard increment/decrement and field navigation work;
- 12/24-hour mode works.

## Gate G09 — Playback engine

**Required.**

Pass conditions:

- Play/Pause works;
- rate slider supports negative, zero, positive values using the required piecewise mapping;
- negative rates run backward;
- midnight wraps both directions;
- zero rate stops time without breaking UI animations;
- direct time edit temporarily suspends advancement.

## Gate G10 — Undo/Redo

**Required.**

Pass conditions:

- Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z work;
- UI buttons mirror behavior;
- required action classes are undoable;
- drag events are coalesced into one transaction;
- new edit after undo clears redo;
- automatic elapsed time is not continuously recorded.

## Gate G11 — JSON configuration

**Required.**

Pass conditions:

- valid JSON loads;
- required escape/unicode/number syntax supported;
- malformed JSON rejected with location diagnostic;
- duplicate/unknown keys rejected;
- serializer emits valid reloadable JSON.

## Gate G12 — YAML configuration

**Required.**

Pass conditions:

- specified block-mapping/sequence subset loads;
- quoting/comments/scalars work;
- unsupported advanced features are rejected clearly;
- duplicate keys rejected;
- YAML serializer output reloads;
- equivalent JSON/YAML normalize identically.

## Gate G13 — Settings GUI wiring

**Required.**

Pass conditions:

- all mandatory GUI-editable settings have real controls;
- changes affect actual behavior according to immediate/deferred effect timing;
- `active_config`/`saved_config` Apply/Revert semantics work;
- dirty Settings exit uses Save/Discard/Cancel;
- config Reload and Save work.

## Gate G14 — Custom software rendering primitives

**Required.**

Pass conditions:

- app owns framebuffer;
- rounded shapes, lines, circles, clipping, alpha, text, shadows, blur are custom implemented;
- renderer tests pass;
- no obvious out-of-bounds artifacts under tested sizes.

## Gate G15 — Modern button interactions

**Required.**

Pass conditions:

- hover elevation;
- click ripple;
- hover/focus border glow;
- pressed feedback;
- keyboard focus state;
- disabled state.

## Gate G16 — Capsule navigation and transitions

**Required.**

Pass conditions:

- active capsule slides between Clock/Settings;
- page content transition is animated when enabled;
- input/focus does not leak to inactive page during transition.

## Gate G17 — Modal transition and background blur

**Required.**

Pass conditions:

- top-level app scene has required launch/graceful-close scale + opacity transition;
- modal opens/closes with scale + opacity animation;
- easing is time-based and uses custom interpolation;
- background progressively darkens and spatially blurs;
- background input blocked;
- interrupted modal transition remains coherent.

## Gate G18 — Dynamic frosted navigation

**Required.**

Pass conditions:

- Settings content can scroll;
- nav remains anchored;
- nav blur grows smoothly with scroll distance;
- bottom shadow grows from absent/minimal to visible;
- collapse behavior is smooth and reversible;
- effect is based on application content beneath nav.

## Gate G19 — Resize and UI scaling

**Required.**

Pass conditions:

- reference/minimum window sizes and the `3840×2160` physical maximum behave safely;
- minimize/restore never persists or reallocates from a zero/minimized client geometry;
- repeated resize produces no rendering corruption;
- UI scales 1.0/1.25/1.5 work;
- hit testing remains aligned;
- Per-Monitor V2 DPI awareness is active and a DPI-change path preserves alignment without changing configured `ui_scale`.

## Gate G20 — Error recovery

**Required.**

Pass conditions:

- runtime malformed config reload preserves current valid config;
- config save failure is surfaced;
- invalid digital input is recoverable;
- corrupted optional state falls back safely;
- focus loss/drag edge cases do not leave stuck state.

## Gate G21 — Persistence safety

**Required.**

Pass conditions:

- config save uses temp-write/replace or equivalent failure-safe strategy;
- saved JSON/YAML reloads successfully;
- configured runtime state persistence behaves as specified, including last valid non-minimized geometry and adequate numeric precision;
- config/state target collision is rejected before runtime-state saving can overwrite configuration;
- failed write does not knowingly destroy previous valid config;
- exit-time runtime-state save failure presents Retry / Exit without saving state / Cancel rather than crashing.

## Gate G22 — Mandatory tests

**Required.**

Pass conditions:

- root `build.bat test` runs the mandatory suite;
- test result is nonzero on failure;
- model/parser/history/renderer/Project A tests exist;
- integration and deterministic GUI validation exist;
- `validation/test-summary.txt` is generated from actual results;
- no unconditional mandatory test is skipped while the suite reports success.

## Gate G23 — Deterministic validation mode

**Required.**

Pass conditions:

- fixed initial time possible;
- logical time advancement can be frozen or deterministically injected;
- known window size/scale possible;
- same production UI/render/state paths are used.

## Gate G24 — Manual acceptance checklist

**Required.**

Pass conditions:

- every required item in `13_manual_acceptance_checklist.md` is marked pass with no known contradiction;
- any failing item blocks completion.

## Gate G25 — Clean delivery

**Required.**

Pass conditions:

- source, docs, tests, examples and required reference fixtures included;
- no essential dependency hidden outside package;
- no large irrelevant caches/build garbage;
- instructions identify build/test/run commands.

## 2. Stopping condition

Development stops only when all G01–G25 pass or when the implementation is explicitly delivered as incomplete with failed gates enumerated.

A partial prototype must not be labeled `complete`.

## 3. Severity guidance

The following are automatically release-blocking even if a checklist item seems ambiguous:

- crash during ordinary mandatory flow;
- memory corruption observed in mandatory tests;
- analog/digital desynchronization;
- negative playback not functioning;
- any required hand not draggable;
- any digital HH/MM/SS field missing mandatory scrub-drag;
- Undo/Redo corrupting state;
- JSON or YAML support delegated externally;
- fake/non-spatial blur;
- settings controls not connected;
- parser accepting malformed config and applying partial state;
- build not reproducible from submitted files.
