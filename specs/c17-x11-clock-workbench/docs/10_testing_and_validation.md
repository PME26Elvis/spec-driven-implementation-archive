# 10 — Testing and Validation Requirements

## 1. Testing goal

Tests must demonstrate that the application is more than a visual prototype.

The suite must exercise pure logic, parsing, rendering primitives, history semantics, time synchronization, and selected end-to-end GUI flows.

## 2. Required test categories

The submission must contain:

- unit tests;
- parser/schema tests;
- state-model tests;
- renderer tests;
- history tests;
- Project A tests;
- integration tests;
- deterministic GUI/end-to-end validation tests or scripts.

## 3. Test executables

Tests must be runnable from a documented command-line target such as:

```text
make test
```

The target must return nonzero if a mandatory test fails.

## 4. Test isolation

Tests must not depend on network access.

Tests must not require user-specific home-directory content.

Temporary files must be created in a controlled temporary directory and removed when practical.

## 5. Unit-test harness

A small custom C test harness is acceptable and encouraged.

It must report failed assertions with file/test context.

A single failed test must cause nonzero overall status.

## 6. Time normalization tests

Mandatory cases:

- `0`;
- exact day boundary;
- positive overflow;
- negative one second;
- multiple positive days;
- multiple negative days;
- fractional seconds;
- near-boundary floating values;
- rejection of NaN/Infinity if representable in API.

## 7. Angle derivation tests

Verify known times:

- `00:00:00` all at 12;
- `03:00:00` hour at 3;
- `06:30:00` hour halfway between 6 and 7;
- `12:15:30` minute includes seconds;
- smooth second fraction produces fractional angle;
- in `tick` mode, stable rendering floors only the second-hand angle, while an active second-hand drag uses the continuous drag candidate and returns to tick rendering after release/cancel.

## 8. Reverse simulation tests

Advance known state with negative rates and measured deltas.

Include crossing midnight backward.

## 9. Gap-clamp tests

A supplied 10-second elapsed delta with default `large_gap_clamp_seconds = 5` must advance simulation by exactly 5 seconds times rate. A second case must change the config value (for example to 2 seconds) and verify that the configured clamp, not a hard-coded 5, is used.

UI animation delta policy should be tested separately where exposed.

## 10. Digital conversion tests

Test 24-hour and 12-hour formatting including:

- midnight;
- noon;
- 1 PM;
- 11:59:59 PM;
- leading zeros.

## 11. Digital edit tests

Test:

- valid commit;
- invalid hour;
- invalid minute;
- Escape cancel;
- focus-loss revert;
- increment wrap;
- decrement wrap;
- AM/PM toggle.

## 12. Drag-angle unwrap tests

Feed synthetic angle sequences crossing `359° -> 1°` and `1° -> 359°`.

Verify accumulated angular delta is small in correct direction.

Test multiple revolutions.

## 13. Hand mapping tests

For each hand, verify the normative drag-start-plus-unwrapped-delta formula with quarter turns, reverse quarter turns, and full turns:

- second hand full revolution = 60 sec;
- minute hand full revolution = 3600 sec;
- hour hand full revolution = 43200 sec;
- pointer-down at more than one valid offset within the hand hit tolerance produces zero initial time delta, and later motion is relative to that pointer-down angle;
- snap quantization uses the documented absolute unwrapped-time step and halfway tie direction.

## 14. Dead-radius tests

Pointer positions inside the fixed 12-logical-pixel dead radius must not update candidate time; leaving the radius must resume from the last valid angular sample without a discontinuity.

## 14A. Digital scrub tests

Mandatory cases:

- movement below 4 logical pixels remains click/focus and changes no time;
- 12 logical pixels upward on hour/minute/second fields adds exactly one corresponding unit;
- 24 logical pixels downward subtracts exactly two units;
- lower-order time and subsecond preservation follows the product contract;
- day wrapping works;
- one scrub with many move events creates exactly one history entry;
- Escape/focus-loss cancellation restores the starting time and creates no entry.

## 15. History tests

Mandatory sequences:

- one analog hand drag -> undo -> redo;
- one digital scrub -> undo -> redo;
- drag with many move events -> exactly one history entry;
- cancelled drag -> zero history entry;
- two-digit edit -> one entry;
- invalid edit -> zero entry;
- slider drag -> one entry;
- undo then new edit -> redo cleared;
- capacity overflow drops oldest safely;
- automatic time advancement creates no history entries.

## 16. State synchronization tests

Given one canonical time mutation, derive and compare:

- expected digital fields;
- expected three hand angles;
- expected 12/24 display conversion.

The test must ensure UI adapters are not storing independent source-of-truth time values.

## 17. JSON lexer/parser tests

Mandatory valid cases:

- all value types;
- nested objects/arrays;
- escapes;
- Unicode BMP escape;
- valid surrogate pair;
- exponent numbers;
- UTF-8 direct strings.

Mandatory invalid cases:

- trailing comma;
- comments;
- bad escape;
- lone high surrogate;
- lone low surrogate;
- duplicate key;
- invalid number;
- trailing garbage;
- excessive nesting.

## 18. YAML parser tests

Mandatory valid cases:

- nested mapping;
- sequence;
- sequence of mappings;
- exact empty collection literals `[]` and `{}`;
- comments;
- quoted strings;
- `10:10:30` preserved as string;
- booleans/null/numbers;
- UTF-8 string.

Mandatory invalid/unsupported cases:

- tab indentation;
- bad indentation;
- anchor/alias;
- tag;
- block scalar;
- non-empty flow mapping such as `{a: 1}`;
- non-empty flow sequence such as `[1, 2]`;
- duplicate key;
- excessive nesting.

## 19. Cross-format tests

Every sample config pair must parse to equivalent internal values and identical normalized JSON.

Serializer round-trip tests are mandatory.

## 20. Schema tests

Test every bounded setting at:

- valid minimum;
- valid maximum;
- below minimum;
- above maximum;
- wrong type;
- unknown key.

Test `playback.default_rate` at `-100`, `0`, `+100`, below `-100`, above `+100`, and with non-finite values supplied through internal validation APIs where representable.

## 21. Atomic-save integration test

Use a temporary directory to verify:

- initial config exists;
- successful save replaces it with valid new config;
- simulated write failure before replace leaves original intact when failure injection is available.

At minimum source code must separate serialization/temp-write/replace enough to test failure paths.

## 22. `locscan` tests

All cases listed in Project A specification are mandatory.

The test suite must verify its own known fixture tree expected totals.

## 23. `cfgcheck` tests

Validate example JSON/YAML and malformed fixtures.

`--dump-normalized` equivalence is mandatory.

## 24. `stateprobe` tests

Mandatory `stateprobe` coverage includes:

- accept `examples/stateprobe_good.json`;
- reject `examples/stateprobe_bad_angle.json`;
- reject `examples/stateprobe_bad_history.json`;
- cover every history kind defined by the fixture schema, including a `config_batch` containing at least one valid non-GUI-editable schema leaf to prove Reload history can be represented;
- reject extra/missing fields, wrong types, out-of-range values, no-op entries, and unknown history kinds;
- verify `normalize` is byte-for-byte idempotent and respects its required key ordering.

## 25. Renderer primitive tests

At minimum:

- alpha blend;
- clipping;
- circle/rounded geometry bounds;
- blur radius 0;
- blur symmetry/basic expected spread;
- line/polygon rasterization staying inside allocated buffer;
- glyph drawing bounds.

## 26. Framebuffer guard tests

Renderer tests must allocate guard bytes/canaries around small buffers and verify drawing clipped shapes does not overwrite guards.

## 27. Animation tests

Using synthetic monotonic timestamps, test:

- start value;
- intermediate progress;
- exact/after completion;
- interruption/retargeting;
- disabled-animation immediate completion;
- cubic Bézier endpoint behavior;
- modal scale/opacity targets `0.96/0` and `1/1`;
- at injected eased progress `q = 0, 0.5, 1`, modal dim alpha is exactly `0, 0.21, 0.42` and modal blur factor is exactly `0, 0.5, 1` times the configured radius.

## 28. Scroll/frost tests

Given synthetic scroll offsets `0`, `effects.frost_scroll_range/2`, and at/beyond the range, verify the exact required formulas:

- `p = 0, 0.5, 1` respectively;
- nav height = `72, 64, 56` logical pixels;
- title translation = `0, -2, -4` logical pixels;
- title scale = `1, 0.96, 0.92`;
- blur strength/radius factor = `0, 0.5, 1` times `effects.nav_blur_radius`;
- shadow strength = `0, 0.5, 1` times `effects.shadow_strength`;
- values clamp at the maximum beyond the range.

## 29. Layout tests

Pure layout calculations must be testable at least for:

- reference size;
- minimum size;
- wide size;
- supported UI scale factors;
- physical client size smaller than the scaled logical minimum, verifying the requested/runtime minimum calculation.

Assert mandatory control rectangles are positive and within intended content bounds.

## 30. Hit-testing tests

Test points:

- center of button;
- just outside button;
- rounded-corner exclusion where applicable;
- hand shaft near tolerance;
- overlapping hand priority;
- slider thumb.

## 31. Deterministic GUI mode

Project B must support a validation-oriented deterministic mode that permits repeatable screenshots/input scenarios.

At minimum it must be possible to:

- start at a specified canonical time;
- freeze automatic time advancement or inject a fixed logical delta;
- disable nonessential randomness;
- use a known window size and UI scale.

This may be implemented by documented command-line flags reserved for validation.

## 32. Deterministic mode restrictions

Deterministic mode must use the same real UI/rendering/state code as normal execution.

It must not swap in mock pages, hard-coded screenshots, or simplified controls. For automated pointer/keyboard scenarios, a project-owned test harness may inject normalized input events at the same internal dispatch boundary used after X11 event translation, but it must not bypass hit testing or call time/config/history mutation functions directly in place of the UI gesture. A manually executed deterministic scenario is also acceptable where the checklist explicitly calls for human interaction, but the submission must still include repeatable logic assertions and deterministic visual evidence for the mandatory GUI scenarios. External desktop-automation software is not a required dependency.

## 33. Mandatory GUI scenario A — synchronization

Automated or scripted deterministic validation must cover:

1. launch at `10:10:30` paused;
2. verify digital display state via exported test state or deterministic probe;
3. drag minute hand to change time;
4. verify canonical time changed;
5. verify digital time matches;
6. undo;
7. verify exact prior canonical time;
8. redo;
9. verify changed time restored.

## 34. Mandatory GUI scenario B — reverse playback

1. start `00:00:01`;
2. set rate `-1×`;
3. play with injected two-second logical elapsed interval;
4. expected resulting canonical time exactly `23:59:59` before any subsequent injected advancement;
5. analog/digital derived outputs must match.

## 35. Mandatory GUI scenario C — settings modal/error

1. open Settings;
2. scroll enough to activate frosted nav;
3. open a confirmation or error modal;
4. verify background input blocked;
5. close modal;
6. verify navigation/settings interaction returns to normal.

The scenario must also render at least one deterministic frame that can be inspected manually, either as a captured X11 screenshot or as a project-generated lossless framebuffer dump.

## 36. Mandatory GUI scenario D — config reload

1. launch with valid config;
2. replace test config with invalid fixture;
3. trigger Reload config;
4. verify error surfaced;
5. verify active configuration unchanged;
6. restore valid file;
7. reload;
8. verify new values applied.

## 37. Mandatory GUI scenario E — resize

1. launch reference size;
2. resize to minimum;
3. interact with digital editor and slider;
4. resize wide;
5. drag clock hand;
6. verify no hit-test/layout desynchronization.

## 38. Screenshot evidence

The final validation evidence must include a small set of deterministic visual artifacts generated by the real renderer; screenshots are not substitutes for executable tests.

Recommended views:

- main Clock view;
- active hand drag/hover;
- Settings with frosted nav after scroll;
- modal with blurred background;
- negative playback rate.

## 39. Sanitizer/static diagnostics

If the chosen build environment supports compiler sanitizers or static warning flags, a documented validation build is strongly recommended. This recommendation is not itself a release gate; any memory-safety failure it reveals is release-blocking.

The task pack does not require a particular external sanitizer tool, but memory-safety failures discovered during tests are release-blocking.

## 40. Test data ownership

Fixtures and expected files used by tests must be included in the source tree unless generated deterministically by a checked-in generator.

## 41. Test result artifact

`make test` must produce `validation/test-summary.txt` as a concise text summary recording:

- number of tests run;
- number passed;
- number failed;
- skipped tests with reasons;
- overall result.

A hard-coded always-pass summary is prohibited.

## 42. Final Project-A-on-Project-B self-validation

Project A must be used as part of final validation rather than merely built and unit-tested. After the submitted tree is in its clean validation layout:

- run `locscan` on the submission root with its built-in defaults and emit JSON evidence under a generated-results directory excluded by the default scanner rules;
- run `cfgcheck --dump-normalized` on the shipped `examples/config.json` and `examples/config.yaml` and verify their normalized JSON is byte-for-byte identical;
- run `stateprobe validate` on all three shipped stateprobe fixtures, requiring the good fixture to pass and both bad fixtures to fail;
- run `stateprobe normalize` on the good fixture twice and verify byte-for-byte idempotence.

These checks must use the built C17 Project A executables from the submission. A script may orchestrate the commands, but it must not replace their implementation. The evidence is generated validation output, not authored-source LOC.
