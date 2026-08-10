# 01 — Product Requirements

## 1. Application identity

Product name: Pinball Sandbox  
Major modes: Edit, Play  
Default table format extension: `.pbt`  
Default replay format extension: `.pbr`

The title bar or top application chrome MUST visibly communicate application name, current table name, unsaved/dirty state, and current major mode.

## 2. Logical table world

The table uses a 2D logical coordinate system independent of window pixels.

Required conventions:

- origin: top-left of table world;
- +X: right;
- +Y: down;
- default world size: 1600 × 1000 logical units;
- minimum supported world size: 640 × 480;
- maximum supported world size: 8192 × 8192;
- all physics positions and velocities use double precision;
- rendering converts world coordinates to viewport coordinates.

The default canvas view SHALL fit the entire table inside the central workspace when a new table is created.

## 3. Required editable object types

The object palette MUST provide at least:

1. Ball Spawn
2. Wall
3. Ramp/Slope
4. Bumper
5. Flipper
6. Sensor
7. Drain
8. Launcher
9. One-Way Gate
10. Slingshot
11. Drop Target
12. Stand-up Target
13. Rollover
14. Spinner
15. Kickout

Each object MUST possess a unique stable ASCII identifier, object type, enabled state, editor transform/geometry, type-specific properties, and selectable Edit Mode representation.

Object identifiers MUST survive save/load. Duplicate identifiers MUST be rejected by validation and load handling.

## 4. Ball Spawn

Defines where a normal ball or explicitly targeted spawn action creates a ball.

Properties:

- position;
- radius override or default ball radius;
- optional initial velocity;
- enabled state.

At least one enabled Ball Spawn is required for a playable table.

## 5. Wall

A static collision segment with thickness.

Properties:

- start/end positions;
- thickness;
- restitution;
- friction;
- enabled state.

The physical shape SHALL be treated as a capsule/rounded segment so endpoint collision is defined.

## 6. Ramp/Slope

A semantic table surface represented by an oriented segment/capsule. It participates in collision as a static surface but is visually distinguished from a generic wall.

Properties include Wall properties plus a visual orientation indicator.

## 7. Bumper

A circular static collider that applies an additional outward impulse on qualified impact.

Properties:

- center;
- radius;
- restitution;
- friction;
- impulse strength;
- base score;
- retrigger cooldown;
- enabled state.

A bumper MUST NOT award score every physics step while one ball remains numerically overlapping.

## 8. Flipper

A motor-driven rotating capsule anchored to a fixed pivot.

Properties:

- pivot position;
- length;
- thickness;
- rest angle;
- active angle;
- angular speed while engaging;
- angular return speed;
- restitution;
- friction;
- logical input binding: left or right;
- enabled state.

Flipper motion MUST interact physically with balls and MUST NOT be simulated only by teleporting a static collider between two angles.

## 9. Sensor

A non-solid axis-aligned rectangular region that emits enter, stay, and exit events.

Properties:

- position;
- width;
- height;
- event hooks;
- debug-visible flag;
- enabled state.

A ball inside a sensor remains physically unaffected unless an attached action changes other runtime state.

## 10. Drain

A non-solid rectangular trigger region that removes balls from active simulation when entered. Drain behavior participates in turn and game-over transitions.

## 11. Launcher

A launch-lane mechanism converting press duration into ball launch speed.

Properties:

- position/interaction region;
- target Ball Spawn ID;
- launch direction unit vector;
- minimum launch speed;
- maximum launch speed;
- time to full charge;
- charge curve;
- enabled state.

The default charge curve is linear and clamps at full charge.

## 12. One-Way Gate

A segment collider with a permitted traversal direction. A ball crossing from the allowed side passes through; a ball approaching from the blocked side collides using gate material properties.

## 13. Slingshot

A segment/capsule collider with an active impulse response. It behaves similarly to a wall for base contact, then adds configured normal impulse and awards configured score subject to retrigger cooldown.

## 14. Ball defaults

Default ball:

- radius: 12 logical units;
- mass: 1.0 simulation mass unit;
- restitution: 0.78;
- friction: 0.08;
- linear damping: 0.03 s^-1;
- maximum linear speed: 3000 logical units/s.

Required validation ranges:

- radius: 4–64;
- mass: 0.05–100;
- restitution: 0.0–1.25;
- friction: 0.0–2.0;
- damping: 0.0–10.0;
- maximum speed: 100–10000.

Values outside supported ranges SHALL produce validation errors, not silent clamping on load. Interactive sliders MAY clamp user drag input while editing.

## 15. Global table physics settings

Every table stores:

- gravity X/Y;
- simulation-speed default;
- fixed-timestep physics version;
- default ball configuration;
- maximum active ball count;
- starting turns/balls per game.

Default gravity is `(0, 980)` logical units/s². Default maximum active balls is 16. Supported configurable range is 1–64. Default starting turns is 3.

## 16. Main application layout

The normal desktop layout contains:

- top command/navigation bar;
- left object palette/editor tools;
- center table canvas;
- right Properties/Physics Inspector;
- bottom status bar.

The central table canvas MUST remain the largest single region at the default window size.

## 17. Primary actions

Required top-level actions:

- New;
- Open;
- Save;
- Save As;
- Undo;
- Redo;
- Edit Mode;
- Play Mode;
- Play/Resume;
- Pause;
- Single Step;
- Restart;
- Validate Scene;
- toggle Physics Debug Overlay;
- toggle Inspector visibility;
- start/stop replay recording;
- load replay.

Disabled actions MUST visibly appear disabled and MUST ignore activation.

## 18. Edit/Play mode contract

### Edit Mode

- table objects may be created, selected, edited, transformed, duplicated, or deleted;
- no persistent gameplay state is active;
- persistent mutations mark the document dirty;
- persistent editor operations participate in Undo/Redo unless explicitly view-only.

### Play Mode

- table geometry and authored configuration are immutable;
- edit controls are disabled or unavailable;
- a fresh runtime state is created when entering normal Play Mode;
- score, balls, contacts, sensor state, and replay state are separate from authored editor state.

Returning to Edit MUST restore the authored table exactly as it was before runtime simulation. Runtime ball positions MUST NOT overwrite authored object positions.

## 19. Play controls

Simulation runtime SHALL support:

- Play/Resume;
- Pause;
- Single Step;
- Restart;
- 0.25×, 0.5×, 1×, 2×, 4× simulation-speed multipliers.

Single Step advances exactly one physics fixed step while remaining paused and SHALL NOT depend on display refresh timing.

Restart resets ball runtime states, sensor occupancy, scores, combo state, multipliers, turn state, replay cursor, runtime object enable changes, and transient runtime effects. It does not modify authored table data.

## 20. Input model

Logical gameplay actions MUST exist independently from raw key codes:

- `LEFT_FLIPPER`;
- `RIGHT_FLIPPER`;
- `LAUNCH`;
- `PAUSE_TOGGLE`.

Default bindings SHOULD include:

- left flipper: Left Shift or `Z`;
- right flipper: Right Shift or `/`;
- launcher: Space;
- pause toggle: `P`.

Replay MUST record logical actions rather than hardware scan codes.

## 21. Zoom and pan

Canvas supports zoom in/out/reset/fit, pointer-centered wheel zoom, and pan by middle-button drag or explicit Pan tool.

Required zoom range: 25%–400%.

Zoom and pan are view state and MUST NOT modify authored table coordinates.

## 22. Window sizing

Minimum application window: 1024 × 700 pixels. Recommended default: 1440 × 900.

At minimum size:

- no required primary control may become permanently inaccessible;
- sidebars MAY collapse to compact form;
- canvas MAY become smaller but remains operable;
- text must not render outside control bounds;
- no panel may incorrectly overlap modal input targets.

At larger sizes the canvas expands preferentially.

## 23. Unsaved document behavior

The document becomes dirty after persistent scene mutation. Dirty clears only after successful save or replacement by a confirmed New/Open workflow.

Attempting New, Open, window close, or Quit while dirty MUST display Save / Discard / Cancel.

If Save fails, the current document remains open and dirty.

## 24. Error visibility

Non-fatal runtime errors MUST be shown through a toast, notification, validation panel, or modal appropriate to severity.

Examples:

- unable to save file;
- invalid scene field;
- duplicate object ID;
- replay/scene mismatch;
- maximum active ball count reached.

## 25. File-selection baseline

Because high-level GUI toolkits are prohibited, Open and Save As MUST still be reachable through a real in-application file-selection workflow. Detailed behavior is defined in `16_desktop_interaction_spec.md`.

## 26. Interaction baseline

Required baseline:

- all primary controls have visible text or icon plus tooltip;
- keyboard focus is visible;
- modal focus is trapped inside the modal;
- Escape closes appropriate non-destructive modals;
- Enter activates unambiguous default modal action;
- controls are not distinguished only by color;
- disabled, hover, active, and selected states remain legible.

Complete screen-reader integration is not required for v1.

## 27. Additional pinball mechanisms

Drop Target, Stand-up Target, Rollover, Spinner, and Kickout are mandatory first-class editable objects. Their exact authored/runtime behavior is normative in `21_pinball_mechanisms_nudge_tilt_and_targets.md`.

## 28. Mature authoring

The main product SHALL expose groups, layers, lock state, exact transforms, deterministic overlap selection, alignment/distribution, structured copy/paste reference remapping, zoom/pan/Fit commands, and the Measurement tool defined in `19_advanced_editor_contracts.md`.

## 29. Nudge and Tilt

Play Mode SHALL implement deterministic left/right/up nudge actions, Tilt meter/decay, Tilt state, control/scoring suppression, and replay-compatible inputs as defined in document 21.

## 30. Reliability features

Autosave recovery, external modification protection, scene migration, atomic-save fault behavior, and Safe Startup are mandatory product capabilities under `24_reliability_autosave_recovery_and_external_changes.md`.

## 31. Diagnostics and statistics

Physics Inspector, Event Trace, Collision Trace, Scene Statistics, and deterministic trace comparison SHALL use real production runtime state according to `22_diagnostics_tracing_and_comparison.md`.

## 32. Command Palette

A searchable keyboard-accessible Command Palette is mandatory and SHALL invoke the same production command paths as corresponding visible UI controls. See document 29.

## 33. Current scene format

The canonical writer for v1.0.0 emits `PINBALL_TABLE 2`. Valid `PINBALL_TABLE 1` scenes are accepted and deterministically migrated as specified in document 24.
