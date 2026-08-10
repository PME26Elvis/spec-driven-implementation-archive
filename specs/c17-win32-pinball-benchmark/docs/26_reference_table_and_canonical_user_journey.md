# 26 — Official Reference Table and Canonical End-to-End User Journey

## 1. Purpose

Unit and physics fixtures can pass while the product remains disconnected. v1.0.0 therefore requires one official playable reference table and one end-to-end user journey that crosses editor, persistence, validation, gameplay, replay, diagnostics, and Undo/Redo.

## 2. Official table fixture

Task package SHALL include `acceptance/fixtures/reference_full_game_v2.pbt`.

It is normative acceptance input and SHALL exercise all 15 mandatory object types at least once, except duplicate variants may be omitted where one object is sufficient.

## 3. Reference table required composition

At minimum:

- 1 main Ball Spawn;
- 1 launcher lane with Launcher;
- left/right Flippers;
- perimeter/guide Walls;
- at least 3 Bumpers;
- at least 2 Slingshots;
- at least 3 Sensors/Rollovers combined;
- 1 Drain;
- 1 One-Way Gate;
- 1 Ramp/Slope;
- 3 Drop Targets forming a bank;
- 2 Stand-up Targets;
- 1 Rollover lane;
- 1 Spinner;
- 1 Kickout;
- at least one multiball event chain;
- scoring events;
- gate/open or target-reset event;
- at least three layers;
- at least one locked decorative/guide object;
- at least one authored group.

## 4. Playability

The table SHALL be playable through documented controls without requiring debug manipulation.

Launching a ball must lead into the active playfield. Flippers can prevent immediate drain under reasonable player timing. The scene must not begin with invalid overlap or unavoidable runtime error.

## 5. Reference objectives

The table SHALL provide observable routes for:

- normal bumper scoring;
- rollover scoring;
- target-bank completion;
- spinner ticks;
- Kickout capture/eject;
- opening a gate;
- triggering multiball;
- draining balls and advancing turn;
- causing Tilt through repeated nudge.

## 6. Reference event chain

At least one deterministic chain SHALL include three or more causal actions, for example:

`third drop target -> target-bank Sensor/event -> ADD_SCORE -> OPEN_GATE -> START_MULTIBALL`.

Exact IDs are defined by the fixture and trace acceptance manifest.

## 7. Reference-table validation

`scenecheck` SHALL report zero Errors for the official table. Warnings are permitted only if explicitly listed in acceptance manifest; v1.0.0 target is zero warnings.

## 8. Canonical E2E journey overview

The implementer SHALL provide an automated/reproducible E2E workflow covering phases J01–J24 below. Automation method is implementation-defined; observable outcomes are normative.

## 9. J01 New

Create a new scene. Verify:

- clean document;
- default world/table values;
- default layer set;
- no authored objects;
- Edit mode active.

## 10. J02 Create boundaries

Create at least three Walls through UI. Verify palette/tool behavior and Inspector values.

## 11. J03 Create core gameplay objects

Add:

- Ball Spawn;
- Launcher;
- left/right Flippers;
- Bumper;
- Drain;
- Sensor.

## 12. J04 Exact edit

Use Inspector numeric entry to assign at least one exact coordinate, length, angle, and gameplay property. Verify committed authored values.

## 13. J05 Group/layer/lock

- multi-select two objects;
- group them;
- move group to a non-default layer;
- lock one member or layer;
- demonstrate locked transform skip semantics.

## 14. J06 Align/distribute

Create/select at least three objects and invoke one alignment and one distribution command. Verify exact resulting coordinates and one-command Undo granularity.

## 15. J07 Event authoring

Create a Sensor event with at least two actions including score and state mutation/spawn action. Verify references shown correctly.

## 16. J08 Validate

Invoke Validate Scene. Deliberately create one invalid reference or missing required object property first, confirm Error appears and Play is blocked; repair it and confirm zero Errors.

## 17. J09 Save

Save As to a new `.pbt` path. Verify:

- file exists;
- current format version 2;
- dirty clears;
- backing identity established.

## 18. J10 Dirty checkpoint

Edit one authored property -> dirty. Undo exactly to saved semantic state -> clean. Redo -> dirty. Undo again -> clean.

## 19. J11 Simulation Preview

Run non-destructive preview and manipulate flipper. Stop preview. Verify authored scene fingerprint/dirty/history unchanged.

## 20. J12 Play and launch

Enter Play, charge Launcher for a known duration, release, verify active ball moves into playfield and Event/Collision traces populate.

## 21. J13 Score

Cause at least one qualifying Bumper/Sensor/target score. Verify score, combo, trace record, statistics update.

## 22. J14 Multiball

Trigger authored multiball event. Verify >=2 actual independently simulated balls and ball-ball collisions remain enabled.

## 23. J15 Pause/Step/Resume

Pause. Record fingerprint. Wait wall time: fingerprint unchanged. Single Step: step increments exactly one. Resume.

## 24. J16 Nudge/Tilt

Perform nudges sufficient to trigger Tilt. Verify Tilt indication, score suppression, flipper/launcher suppression, continuing ball physics.

## 25. J17 Drain/new turn

Drain all active balls. Verify next turn semantics and Tilt clear.

## 26. J18 Replay record

Start a fresh deterministic session, record launcher/flipper/nudge actions, stop/save `.pbr`.

## 27. J19 Replay playback

Reload reference scene/fingerprint and replay. Verify final/checkpoint fingerprints and score match recording.

## 28. J20 Diagnostics

Inspect selected ball Collision Trace and Event Trace; export both. Verify trace schema metadata and real contact/event values.

## 29. J21 Headless replay

Run same replay headlessly to final step. Verify GUI/headless final fingerprint matches.

## 30. J22 External modification conflict

With scene open, modify backing file externally using a controlled valid edit. Attempt Save and verify conflict path; Cancel preserves dirty scene; Save As preserves external original.

## 31. J23 Recovery

Make dirty edit, produce recovery snapshot using test-controlled logical timing, simulate abnormal termination, restart and recover. Verify recovered state opens dirty without overwriting original.

## 32. J24 Final Save/Reload

Save recovered/edited scene to final path, close, reopen, validate zero Errors, compare semantic model to saved state.

## 33. Journey evidence

Evidence SHALL map each J01–J24 step to:

- automated test/log or manual action;
- expected observable state;
- actual status;
- screenshot/trace/report reference where applicable.

No single final screenshot may stand in for the sequence.
