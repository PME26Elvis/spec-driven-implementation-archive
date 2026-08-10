# 03 — Edit Mode and Scene Authoring Specification

## 1. Editor tools

Required tools:

- Select;
- Move;
- Rotate;
- Resize;
- Pan;
- placement tools for every required object type.

Select and Move MAY be combined if all behavior remains available.

## 2. Selection

- click selectable object: select it;
- click empty canvas: clear selection;
- modifier-click: toggle object in multiselection;
- drag marquee on empty canvas: select objects whose editor bounds intersect marquee;
- disabled authored objects remain selectable/editable;
- sensors remain selectable in Edit Mode even if transparent in Play.

The Inspector shows common editable properties for multiselected objects where meaningful.

## 3. Primary selection

One object in a multiselection is primary. It determines single-object Inspector sections, ID-edit focus, and optional alignment reference.

## 4. Move

Dragging selected objects updates world position continuously.

Requirements:

- pointer/world transform remains correct at every zoom;
- snapping applies if enabled;
- multiselection preserves relative offsets;
- one drag is one Undo command, not one command per pointer sample;
- Escape during active drag cancels and restores pre-drag state.

## 5. Rotate

Rotatable objects include at least Ramp/Slope, Flipper authored orientation, One-Way Gate, Slingshot, and Launcher direction.

- direct handle or equivalent manipulation;
- angle displayed in degrees in Inspector;
- internal radians are allowed;
- angle snapping applies when enabled;
- default angle snap: 15 degrees.

## 6. Resize

Resizable properties include:

- Wall/Ramp length and thickness;
- Bumper radius;
- Flipper length/thickness;
- Sensor/Drain width and height;
- Launcher interaction region where applicable;
- Gate/Slingshot length and thickness.

Interactive resize clamps at legal limits. Typed invalid values are visibly rejected rather than silently rewritten.

## 7. Grid and snapping

Defaults:

- major grid: 50 logical units;
- minor grid: 10 logical units;
- snap increment: 10 logical units.

User can toggle grid visibility and snapping independently and choose snap increment from at least 5, 10, 25, 50.

View-only grid state does not dirty the document.

## 8. Object placement workflow

1. Choose object from palette.
2. Pointer enters placement state.
3. Preview geometry follows pointer.
4. Click/drag defines object.
5. Object receives unique default ID.
6. Object becomes selected.
7. Inspector shows properties.
8. Creation enters Undo history as one command.

Escape cancels placement without creating an object.

## 9. Default IDs

Default IDs SHALL be deterministic within a document and human-readable, e.g. `wall_001`, `bumper_001`, `flipper_002`.

Deletion need not reuse suffixes. Duplicate/Copy/Paste MUST assign new unique IDs.

## 10. ID editing

Allowed characters: ASCII letters, digits, `_`, `-`. Length 1–63 bytes. First character must be alphanumeric or underscore.

ID must be unique. Renaming an object updates all authored references atomically. Invalid rename leaves old ID and references unchanged.

## 11. Duplicate

Duplicate creates an offset copy of selected object(s), recommended +20,+20 logical units.

- new IDs;
- copied properties;
- references between objects duplicated together SHOULD remap to duplicated targets;
- references to external objects remain external;
- one Undo reverses whole duplicate command.

## 12. Copy/Paste

Copy stores selected authored objects in application memory. Paste creates new objects with new IDs. Repeated paste creates distinct objects.

System clipboard integration is optional.

## 13. Delete and references

Deleting selected objects must not leave silent dangling references.

The editor MUST consistently either:

- remove dependent actions/references with explicit warning; or
- block deletion and explain dependency.

Chosen policy is documented by implementation.

## 14. Undo/Redo

At least 100 reversible persistent editing commands are required.

Undoable:

- create;
- delete;
- move;
- rotate;
- resize;
- property edit;
- ID rename;
- duplicate;
- paste;
- event/action create/delete/change;
- global table physics changes.

View-only zoom/pan need not be undoable. A new edit after Undo clears the Redo branch.

## 15. History granularity

Continuous pointer drag produces one command. Continuous slider drag SHOULD commit as one command. Typing into one property field SHOULD become one logical property command after commit, not one per character.

## 16. History integrity

Undo/Redo MUST restore object values, IDs, references, and coherent selection. It must not falsely claim saved state if content differs from disk.

If saved-history-position tracking is implemented, undoing exactly back to saved content SHOULD clear dirty.

## 17. Inspector commit behavior

Committed property change:

- validates type/range;
- mutates authored scene;
- updates canvas;
- enters history;
- marks document dirty.

Invalid text remains visibly invalid until resolved and cannot corrupt runtime structures.

## 18. Table properties

Expose:

- table name;
- world width/height;
- gravity X/Y;
- default ball radius/mass/restitution/friction/damping/max speed;
- maximum active balls;
- starting turns.

Changing world size MUST NOT silently move objects. Objects outside new bounds become validation issues.

## 19. Simulation Preview

Edit Mode MUST support non-destructive Preview.

- launches one temporary test ball from chosen/default spawn;
- uses authored geometry read-only;
- runtime state is separate;
- authored editing is disabled during active preview;
- ending preview discards all runtime changes;
- score/event runtime changes are discarded;
- runtime enable/disable/spawn changes are discarded;
- Undo history remains unchanged.

A visible Preview control is required even if a shortcut also exists.

## 20. Preview determinism

Same authored scene and same Preview start must produce same initial runtime state. Default Preview uses no uncontrolled randomness.

## 21. Validate Scene

Validation categories:

- **Error** — table is unplayable or violates required invariant;
- **Warning** — valid but likely problematic;
- **Info** — advisory.

Errors block normal Play Mode. Warnings do not.

## 22. Required validation errors

At minimum:

- no enabled Ball Spawn;
- no enabled Drain;
- duplicate object ID;
- malformed geometry;
- non-positive required dimension;
- parameter outside supported range;
- dangling object reference;
- event source/target incompatible with action;
- spawn center inside solid collider;
- invalid flipper angle range or identical endpoints when motion required;
- invalid flipper length/thickness;
- invalid gate direction;
- zero launcher direction;
- Launcher target is missing or is not a Ball Spawn;
- more than one enabled Launcher targets the same Ball Spawn;
- launcher maximum launch speed is lower than minimum launch speed;
- active-ball limit outside range;
- non-finite numeric value;
- unsupported scene version.

## 23. Required validation warnings

At minimum:

- object partially outside table bounds;
- sensor with no actions;
- scoring object with zero score;
- bumper impulse zero;
- restitution >1.0;
- overlapping enabled Ball Spawns;
- Drain entirely outside table bounds.

A logically impossible condition may be promoted to Error.

## 24. Validation UI

Validation presentation provides:

- count by severity;
- object ID where relevant;
- readable explanation;
- ability to select/focus implicated object;
- stable deterministic ordering.

## 25. Object overlap policy

Overlapping solid authored objects are allowed because intentional tables can overlap. Validator MAY warn about suspicious overlap but cannot categorically reject all overlap.

## 26. Editing during Play

Persistent editing is prohibited while Play simulation is running or paused. Return to Edit to mutate scene. Runtime is discarded when returning.

## 27. Replay recording and mode switch

If replay recording is active, switching to Edit must end/cancel recording with a clear user-visible result. Authored dirty state is unaffected by Play runtime.

## 28. Transient UI state is not scene data

The `.pbt` file need not persist:

- panel collapse state;
- canvas zoom/pan;
- current selection;
- open modal;
- hover state;
- runtime score;
- runtime ball positions.

## 29. Editor stress baseline

A table containing 500 authored objects MUST load, validate, remain selectable/pannable/zoomable, save, and reload without seconds-long UI freezes or pathological memory explosion.

## 30. v1.0 advanced editor requirements

All selection details, groups, layers, locking, exact transform entry, alignment/distribution, structured clipboard remapping, semantic dirty-state checkpoint behavior, zoom/pan, Edit/Play isolation, and Measurement tool requirements are normative in `19_advanced_editor_contracts.md`.

Where this older baseline section says a behavior MAY/SHOULD be chosen by implementation but document 19 fixes a v1.0 behavior, document 19 controls.

## 31. All 15 object types

Placement, selection, applicable transforms, Inspector editing, Undo/Redo, copy/paste, layer/group/lock behavior, serialization, and validation apply to all 15 required object types, including Drop Target, Stand-up Target, Rollover, Spinner, and Kickout.

## 32. History capacity and memory

The editor SHALL retain at least 100 normal Undo transactions and obey the history memory-cap semantics in document 25. One pointer drag, one paste, one multi-edit, one alignment/distribution command, and one group/layer operation are atomic transactions.

## 33. Dirty state

Undoing exactly to the saved semantic checkpoint SHALL clear dirty state; this is mandatory in v1.0.0, not optional. The implementation must remain correct if old history entries are evicted.
