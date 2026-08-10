# 09 — Automated Testing and Validation Requirements

## 1. Testing is mandatory

The assignee MUST implement automated tests and validation utilities sufficient to demonstrate required behavior. Tests are required product work, not optional evidence.

No third-party test framework is assumed. A native C harness or equivalent custom implementation is expected.

## 2. Required test layers

1. math/vector/geometry unit tests;
2. collision/physics response tests;
3. CCD/trigger tests;
4. scene parser/writer tests;
5. editor command/history tests;
6. gameplay/event/scoring tests;
7. replay/headless determinism tests;
8. CLI/engineering utility tests;
9. long-run/stress tests;
10. manual/visual acceptance for motion and appearance.

## 3. Unified test runner

A command such as `tests.exe --all` or a documented equivalent SHALL run all non-visual mandatory automated tests.

It MUST:

- return 0 only when all mandatory automated tests pass;
- return non-zero on any mandatory failure;
- print category totals;
- print failing test IDs;
- produce machine-readable JSON summary.

## 4. Minimum meaningful test counts

Final implementation SHALL contain at least:

- 40 math/vector/geometry tests;
- 60 collision/physics response tests;
- 30 CCD/trigger tests;
- 30 scene parser/writer tests;
- 25 editor/Undo-Redo tests;
- 20 event/scoring/multiball tests;
- 15 replay/headless determinism tests;
- 15 error/resource-limit tests;
- 10 CLI/engineering utility tests.

Minimum: **420 meaningful mandatory automated test cases**. Domain minimums and named v1 cases are normative in `28_v1_mandatory_test_catalog.md`.

Generated parameter subcases count only when each represents a materially distinct input and reports independently. Cosmetic duplication does not count.

## 5. Math coverage

At minimum:

- vector add/subtract/scalar multiply;
- dot product;
- length/length-squared;
- normalization;
- zero-vector normalization handling;
- closest point on finite segment;
- point/segment distance;
- circle/circle overlap;
- circle/AABB overlap;
- segment sidedness;
- clamp/epsilon helpers;
- degree/radian conversion;
- deterministic fallback normal;
- checked size arithmetic helpers where applicable.

## 6. Collision response coverage

At minimum:

- elastic head-on equal-mass ball collision;
- inelastic equal-mass collision;
- unequal mass collision;
- separating overlap receives no false closing impulse;
- normal wall reflection;
- angled wall reflection;
- endpoint collision;
- friction reduces tangential velocity magnitude;
- bumper adds outward impulse;
- slingshot adds impulse;
- one-way gate allowed direction;
- one-way gate blocked direction;
- stationary flipper behaves as static surface;
- moving flipper transfers energy;
- bounded penetration correction;
- passive energy does not increase systematically.

## 7. CCD coverage

At minimum:

- fast ball vs thin horizontal wall;
- fast ball vs thin vertical wall;
- fast ball vs segment endpoint;
- fast ball vs Bumper;
- fast ball crossing Sensor;
- fast ball crossing Drain;
- multiple impacts inside one step;
- near-tangent swept contact;
- trajectory miss does not false-positive;
- moving-flipper high-speed contact or equivalent required stress.

## 8. Named deterministic scenarios

The suite SHALL include these named scenarios:

- `gravity_drop`
- `perfect_bounce`
- `friction_ramp`
- `high_speed_thin_wall`
- `flipper_strike`
- `bumper_ring`
- `sensor_crossing`
- `eight_ball_collision`
- `drain_test`
- `multiball_stress`

Task-package fixture intent cannot be weakened.

## 9. Scenario assertions

Each scenario asserts one or more of:

- exact event count;
- no tunneling;
- ball side of barrier;
- position tolerance;
- velocity tolerance;
- energy invariant;
- active-ball count;
- score;
- game state;
- no non-finite value;
- deterministic checkpoint fingerprint;
- replay equivalence.

## 10. Gravity Drop

Fixture: `acceptance/fixtures/gravity_drop.pbt`

Purpose: integration/gravity.

Fixture principle:

- gravity `(0,980)`;
- initial position `(500,100)`;
- velocity zero;
- damping zero;
- no nearby collision;
- fixed known step count.

Expected value uses mandated semi-implicit Euler recurrence, not continuous analytic equation.

## 11. Perfect Bounce

Fixture: `acceptance/fixtures/perfect_bounce.pbt`

Purpose: TOI/reflection.

- gravity 0;
- restitution 1;
- friction 0;
- ball approaches a wall normally;
- post-collision normal velocity reverses at equal magnitude within tolerance;
- tangential component preserved.

## 12. Friction Ramp

Fixture: `acceptance/fixtures/friction_ramp.pbt`

Purpose: tangential response.

Ball strikes angled surface with tangential component. Post-contact tangential magnitude is reduced or equal according to friction policy; state remains finite.

## 13. High-Speed Thin Wall

Fixture: `acceptance/fixtures/high_speed_thin_wall.pbt`

Ball displacement during one step exceeds wall thickness. It MUST collide rather than appear on forbidden side with unchanged velocity. This is a Release Gate case.

## 14. Flipper Strike

Fixture: `acceptance/fixtures/flipper_strike.pbt`

Moving flipper contacts slow/stationary ball. Ball leaves with increased kinetic energy in expected outward direction. A stationary-flipper comparison establishes moving surface as energy source.

## 15. Bumper Ring

Fixture: `acceptance/fixtures/bumper_ring.pbt`

Multiple bumpers around a path verify:

- qualified hit count;
- no every-step overlap score spam;
- score/combos;
- finite state.

## 16. Sensor Crossing

Fixture: `acceptance/fixtures/sensor_crossing.pbt`

Normal and high-speed variants verify ENTER/LEAVE order and count, including complete traversal within one fixed step.

## 17. Eight-Ball Collision

Fixture: `acceptance/fixtures/eight_ball_collision.pbt`

Eight simultaneously active balls in constrained region verify:

- unique runtime IDs;
- ball-ball collision;
- finite state;
- deterministic final/checkpoint state;
- safe add/remove container behavior.

## 18. Drain Test

Fixture: `acceptance/fixtures/drain_test.pbt`

Normal/high-speed Drain entry verifies exactly-once draining and correct turn accounting.

## 19. Multiball Stress

Fixture: `acceptance/fixtures/multiball_stress.pbt`

At least 16 active balls with static surfaces, sensors, and bumpers for at least 30 simulated seconds.

Required:

- no NaN/inf;
- no crash;
- no active-ball-cap violation;
- deterministic repeated checkpoint hashes;
- bounded physics;
- valid fixture does not hit event cap.

## 20. Long-run stability

Headless stress simulation runs at least 1,000,000 fixed steps.

Must complete without:

- crash;
- NaN/inf;
- transient memory leak proportional to step count;
- unbounded event/replay queue growth when recording disabled.

## 21. Repeated-run determinism

At least one nontrivial multiball replay runs 10 times in the same build. Every checkpoint and final deterministic fingerprint must match exactly.

## 22. GUI/headless equivalence

At least three replay traces execute through actual GUI replay runtime path and headless path. Final state summaries and deterministic checkpoints must match within same-build tolerance.

The GUI test may instrument state directly rather than automate pixels, but it must invoke the production GUI application/runtime path and shared physics/event modules.

## 23. Scene parser coverage

At minimum:

- valid minimal file;
- valid full file;
- comments;
- CRLF;
- UTF-8 name;
- escaped quote/backslash;
- duplicate field;
- duplicate ID;
- missing header;
- unsupported version;
- unknown object type;
- unknown field Warning;
- malformed float;
- overflow number;
- NaN/inf rejection;
- missing required field;
- malformed event action;
- missing action index;
- dangling reference;
- resource-limit boundary.

## 24. Round-trip coverage

At least 10 diverse valid scenes perform load → serialize → reload → authored semantic field-by-field comparison.

## 25. Atomic-save tests

Use injectable filesystem operations or equivalent controlled fault mechanism to fail stages of save. Verify previous destination remains intact when replacement does not complete. Tests must exercise production serialization/save logic.

## 26. Editor tests

At minimum:

- create/undo/redo;
- delete/undo/redo;
- move drag as one command;
- rotate;
- resize;
- property edit;
- duplicate unique IDs;
- copy/paste unique IDs;
- rename updates references;
- rename Undo restores references;
- Redo branch clears after new edit;
- 100-command history;
- multiselection move preserves offsets;
- canceled drag changes nothing;
- Preview leaves authored scene and history unchanged.

## 27. Event/gameplay tests

At minimum:

- Sensor ENTER once;
- Sensor LEAVE once;
- high-speed ENTER+LEAVE;
- action order;
- ADD_SCORE;
- combo progression;
- combo exact timeout boundary;
- Bumper cooldown boundary;
- multiball 2× bonus;
- multiplier override timing;
- spawn capacity;
- blocked spawn;
- enable/disable;
- OPEN_GATE duration;
- event-cycle cap;
- individual multiball drains;
- simultaneous drains consume one turn;
- drain-event respawn prevents turn end.

## 28. Replay tests

At minimum:

- valid replay parse;
- invalid header;
- malformed/decreasing step;
- unknown action;
- scene hash mismatch;
- record then replay final equivalence;
- pause/Single Step during playback does not change final outcome;
- playback-speed multiplier does not change final outcome;
- same replay 10× deterministic;
- GUI/headless equivalence.

## 29. UI state-model tests

Even when pixels are manually judged, internal UI logic tests cover:

- hover enter/leave;
- pressed cancellation;
- focus movement;
- disabled activation ignored;
- modal input capture;
- animation interruption/reversal;
- sidebar collapse hit-test geometry;
- resize layout bounds;
- dirty-document modal branch behavior.

## 30. Animation numeric tests

At representative normalized times:

- t=0 equals start;
- t=duration equals target;
- values finite;
- reverse begins from current value;
- opacity stays [0,1];
- scale remains safe;
- interrupted modal/panel/hover reaches correct eventual endpoint.

## 31. Blur tests

Software blur has deterministic pixel-buffer tests:

- constant-color input remains constant;
- impulse spreads symmetrically per implemented kernel/method;
- edge policy deterministic;
- alpha/channel values stay valid;
- modal blur excludes modal itself in composition-level test where feasible.

## 32. Memory/error instrumentation

The project SHOULD use available memory/error instrumentation during development, but this task package does not prescribe a particular external tool. Delivered code still requires bounds-aware allocation and error handling.

## 33. Test report

Final `TEST_REPORT.md` includes:

- application version/build tested;
- totals by test category;
- failing tests if any;
- named scenario results;
- long-run step count/result;
- multiball stress result;
- deterministic repeated-run result;
- visual evidence references;
- known limitations.

## 34. Machine-readable test result

Final delivery includes `RELEASE_RESULT.json` conforming to `schemas/release_result_schema.json`. `schemas/release_result_example.json` demonstrates the required shape.

Required information includes:

- `format_version` and task-package version;
- release/build identifier;
- total/pass/fail/skip counts;
- per-category counts;
- every Release Gate using status `PASS`, `FAIL`, `BLOCKED`, or `NOT_RUN`;
- failing test IDs;
- named scenario statuses;
- stress metrics;
- deterministic/replay fingerprints;
- known mandatory failures.

A skipped mandatory test means its related gate cannot be `PASS`. Boolean-only gate reporting is insufficient because it cannot distinguish FAIL, BLOCKED, and NOT RUN.

## 35. No self-fulfilling tests

Expected values must not be derived by simply calling the exact production function under test in a way that makes defects cancel. Use independent known geometry, explicit fixture values, analytic/simple recurrence calculations, or separate reference constants.

## 36. Mutation-resistance principle

The suite SHOULD catch obvious injected defects such as:

- gravity disabled;
- restitution forced to 1;
- ball-ball collision disabled;
- Sensor LEAVE removed;
- Save turned into no-op;
- Undo clearing entire scene;
- replay events ignored;
- CCD replaced by discrete overlap;
- flipper contact surface velocity omitted.

Formal mutation-testing tooling is not required, but delivery SHOULD list several injected faults used during development to demonstrate test sensitivity.

## 35. v1.0 expanded verification

The mandatory catalog in document 28 supplements every baseline named test. Requirements include invariant tests, simultaneous-contact tests, new pinball mechanisms, Nudge/Tilt, advanced editor transactions, UTF-8/focus/HiDPI, autosave/recovery/external modification, parser robustness corpus, fault-injected atomic saves, performance/resource stability, traces/determinism comparison, and release-evidence validation.

## 36. Golden checkpoints

At least five canonical simulation scenarios must validate intermediate checkpoints, not final state alone. Golden checkpoint data lives in acceptance manifests or checked-in expected data and is compared with declared normative tolerances.

## 37. Reference E2E

J01–J24 from document 26 are mandatory cross-subsystem acceptance. Their implementation method is not prescribed, but every phase must have reproducible evidence.

## 38. Performance and resource gates

P1/P2/P3 workloads, repeated-cycle memory test, descriptor stability, trace/history caps, and relevant timing measurements are mandatory under document 25.

## 39. Release evidence validation

`releasecheck` must validate test/evidence IDs and Gate consistency. A suite that passes but is not represented truthfully in required release reports does not satisfy the final Automated Test/Release Evidence Gates.


## 43. Windows platform-binding tests

The Windows variant SHALL include automated or deterministic integration coverage, as technically applicable, for:

- UTF-16/surrogate to UTF-8 conversion boundaries;
- Unicode file paths and spaces;
- custom picker path normalization;
- `WM_CLOSE` dirty-document routing;
- pointer-capture loss cleanup;
- Per-Monitor DPI state transition math and framebuffer reallocation;
- user-scale × OS-DPI composition;
- repeated GDI/USER/HANDLE resource cycles;
- headless startup proving no HWND creation;
- save replacement failure on access/sharing denial through fault injection;
- clipboard-unavailable failure path;
- IME committed-result conversion at the platform adapter boundary (the test may inject a captured UTF-16 result instead of requiring an interactive IME).

These cases are part of the existing >=420 meaningful-test floor and SHALL be represented under the most appropriate UI/I/O/resource categories. They do not reduce any physics/editor/gameplay domain minimum.
