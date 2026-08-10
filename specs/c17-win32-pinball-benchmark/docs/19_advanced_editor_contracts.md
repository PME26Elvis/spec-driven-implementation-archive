# 19 — Advanced Editor Contracts

This document is normative for mature scene-authoring behavior in Edit Mode. It closes ambiguities that are commonly hidden by visually convincing but shallow editors.

## 1. Scope

The editor SHALL support authored scenes large enough to require grouping, locking, layering, exact transforms, precise selection semantics, alignment tools, clipboard preservation, and transactionally correct history.

All operations in this document act on authored scene state unless explicitly marked view-only.

## 2. Object groups

A group is an editor-authored container of object references. Groups do not create a new physics body and do not alter collision behavior.

Required behavior:

- create a group from two or more selected objects;
- ungroup a selected group;
- select a group as one editor unit;
- enter group to select/edit individual members;
- move/rotate group members as one transform transaction around the group pivot;
- duplicate/copy/paste a group preserving internal relative transforms and valid internal references;
- delete a group with a choice between deleting only group metadata or deleting contained objects;
- nested groups are not required and SHALL be rejected if the implementation exposes a path that would create them.

Group IDs are unique ASCII identifiers subject to the same identifier collision rules as object IDs.

## 3. Group transform semantics

The group pivot defaults to the arithmetic center of selected members' editor bounds at group creation. The pivot may be moved explicitly without changing member geometry.

Group translation applies the same world-space delta to every member.

Group rotation by angle `Δθ` rotates each member position around the group pivot and adds `Δθ` to each rotatable member orientation. Objects that are axis-aligned by definition, such as rectangular Sensor/Drain volumes in v1, rotate by rotating their center and then expanding to the smallest axis-aligned rectangle containing the rotated original bounds. This conversion MUST be previewed before commit and forms one Undo transaction.

Group scaling is not required in v1.0.0.

## 4. Locking

Every authored object and group has an editor-only `locked` state.

A locked entity:

- can be selected;
- appears with a distinct locked indicator;
- cannot be moved, rotated, resized, deleted, duplicated, or modified through Inspector fields that alter authored data;
- can be unlocked through the Inspector, context action, or command palette;
- participates normally in simulation.

Lock state is persisted in `.pbt` scene data.

Attempting to transform a mixed selection containing locked and unlocked objects SHALL transform only the unlocked members and present a non-blocking explanation that locked members were skipped.

## 5. Layers

The scene SHALL support at least 16 named editor layers.

Each authored object belongs to exactly one layer. Required default layers are:

- `Gameplay`;
- `Guides`;
- `Decorative`.

Objects used by required physics/gameplay features default to `Gameplay`.

Layer properties:

- unique layer ID;
- UTF-8 display name;
- visible flag;
- locked flag;
- authored stable order.

Layer visibility is an editor view property that SHALL NOT alter physics or runtime enable state. Hidden objects continue to exist in simulation unless explicitly disabled by their own authored/runtime property.

Layer lock prevents authored modifications to members exactly as object lock does.

Layer `visible` and `locked` values are persisted editor metadata. Changing either is one Undoable authored command and marks the scene dirty. Visibility affects editor drawing/hit testing only; it never changes runtime `enabled`.

## 6. Layer panel behavior

The editor SHALL provide a discoverable layer-management surface supporting:

- create layer;
- rename layer;
- reorder layers;
- show/hide layer;
- lock/unlock layer;
- move selected objects to layer;
- delete empty layer;
- delete non-empty layer only after explicit choice to move members elsewhere or delete members.

At least one layer SHALL always exist.

## 7. Selection model

Selection contains zero or more selectable entities and one optional primary selection.

Pointer selection rules:

- plain click on an unselected object: replace selection with that object and make it primary;
- plain click on selected object: keep current selection and make clicked object primary;
- Shift-click on unselected object: add it and make it primary;
- Shift-click on selected object: remove it; if it was primary, choose the most recently selected remaining item as primary;
- click empty canvas: clear selection;
- Shift-click empty canvas: preserve selection;
- Escape: clear transient tool state first, then clear selection on a subsequent Escape if no transient tool state exists.

## 8. Overlapping-object selection

When multiple selectable entities overlap at pointer position:

- the first click selects the topmost eligible entity by editor hit-test order;
- repeated click at substantially the same pointer position within 900 ms cycles deterministically through overlapping candidates;
- cycling order is visual layer order, then authored stable order, topmost first;
- locked entities remain selectable;
- hidden-layer entities are not hit-test candidates.

The editor SHALL expose enough visual feedback to identify the selected entity unambiguously.

## 9. Marquee selection

Drag on empty canvas with Select tool starts marquee selection after pointer displacement exceeds 4 device-independent pixels.

Default marquee semantics use intersection: any selectable visible entity whose editor bounds intersect the marquee is included.

Modifier behavior:

- no modifier: replace selection;
- Shift: union with prior selection;
- Ctrl: subtract intersecting entities from prior selection.

Marquee selection MUST use world geometry transformed through current zoom/pan and MUST NOT be dependent on framebuffer pixel colors.

## 10. Multi-selection Inspector

When multiple objects are selected:

- common editable fields are shown;
- fields with identical values display that value;
- fields with different values display a mixed-state indicator;
- editing a common field applies to all eligible unlocked selected objects in one history transaction;
- fields that are not meaningful for all selected object types are hidden or disabled rather than silently applied to a subset.

## 11. Exact transform Inspector

For a single selected entity, numeric transform fields SHALL allow exact authored values.

At minimum, where meaningful:

- X;
- Y;
- width/length;
- height/thickness;
- angle in degrees;
- pivot X/Y for flippers/groups where exposed.

Numeric entry follows the desktop numeric-field rules in `16_desktop_interaction_spec.md`.

Commit behavior:

- Enter commits;
- focus loss commits if value is valid;
- Escape restores the pre-edit value;
- invalid input remains visibly invalid and SHALL NOT mutate authored state;
- clamping without warning is prohibited for out-of-range authored values; the editor must reject or explicitly offer a corrected value.

## 12. Coordinate display

During move/rotate/resize, the editor SHALL show live exact values with sufficient precision to distinguish one logical unit and 0.1 degree at normal zoom.

Pointer status SHALL expose world coordinates independent of UI scale and zoom.

## 13. Alignment commands

For two or more eligible selected objects, required alignment commands are:

- Align Left;
- Align Horizontal Centers;
- Align Right;
- Align Top;
- Align Vertical Centers;
- Align Bottom.

The primary selection is the anchor when an anchor is required. If no primary exists, use the object with the earliest authored stable order.

Alignment acts on editor bounds, not collision-contact points.

## 14. Distribution commands

For three or more eligible selected objects:

- Distribute Horizontal Centers;
- Distribute Vertical Centers;
- Distribute Horizontal Gaps;
- Distribute Vertical Gaps.

The two outermost eligible objects remain fixed. Interior objects are repositioned deterministically by authored stable order after sorting by the relevant coordinate.

If geometry overlaps such that equal positive gaps are impossible, equal signed gaps are still used; the command is valid and reversible.

## 15. Transactional history for transforms

A pointer drag from press to release is exactly one Undo command regardless of the number of intermediate motion events.

A committed Inspector edit is one Undo command.

A multi-object alignment/distribution action is one Undo command.

Create Group/Ungroup, layer reassignment, lock changes, paste, duplicate, and delete each form one atomic Undo command per user invocation.

An operation that fails validation before commit SHALL add no history entry.

## 16. Dirty-state equivalence

Dirty state is semantic, not monotonic.

Required behavior:

1. load/save scene state `S0` -> clean;
2. edit to `S1` -> dirty;
3. Undo exactly back to `S0` -> clean;
4. Redo to `S1` -> dirty.

The implementation MAY use a history checkpoint marker or semantic fingerprint. Merely keeping a boolean that is set forever after first edit is prohibited.

View-only operations such as zoom, pan, temporary selection, panel width, debug visibility, and measurement tool SHALL NOT dirty the scene unless a subsystem specification explicitly marks them authored.

## 17. Clipboard model

The application SHALL implement an internal clipboard payload for authored objects. It may also mirror text to the platform clipboard, but platform text is not a substitute for internal structured copy/paste.

A clipboard payload contains:

- copied objects and supported groups;
- stable copy ordering;
- relative transforms;
- authored properties;
- layer relationships;
- event links whose source and all referenced targets are included in the copied set.

## 18. Clipboard reference remapping

On paste:

- every pasted object/group/event receives a fresh unique ID;
- references between copied entities are remapped to the corresponding new IDs;
- references from copied entities to entities outside the copied set remain external references only when the referenced entity still exists in the destination scene and the reference type permits it;
- otherwise the paste is rejected or the affected reference is removed with explicit warning; silent dangling references are prohibited.

Repeated paste from the same clipboard SHALL generate a new non-colliding ID set each time.

## 19. Paste placement

Default paste offsets copied geometry by `(20, 20)` logical units from its source position.

A second consecutive paste offsets by another `(20, 20)` from the previously pasted set, unless the user moves the pasted set or changes the clipboard.

Paste remains one Undo transaction.

## 20. Duplicate

Duplicate uses the same ID/reference semantics as copy+paste but does not modify the clipboard. Its initial offset is `(20, 20)` logical units.

## 21. Zoom behavior

Required zoom range is 25%–400%.

Required commands:

- Zoom In;
- Zoom Out;
- 100%;
- Fit Scene;
- Fit Selection when selection is non-empty.

Mouse-wheel/gesture zoom SHALL be centered on the world coordinate currently under the pointer: that coordinate remains within 1 device-independent pixel of the same screen point after zoom, absent clamping at world viewport limits.

Zoom does not modify authored geometry.

## 22. Pan behavior

Pan is available using at least:

- middle-button drag; and
- Space + primary-button drag while no text field owns Space.

Panning may extend beyond world bounds enough to inspect edges, but at least 10% of the logical table bounds SHALL remain visible after pan clamping at normal zoom.

## 23. Fit Scene

Fit Scene computes the largest zoom at which the complete logical table bounds fit within the usable canvas with at least 24 device-independent pixels of padding on each side, clamped to supported zoom limits.

## 24. World coordinate contract

Normative world coordinates:

- origin `(0,0)` is the table's top-left logical corner;
- +X points right;
- +Y points down;
- distances use logical units;
- velocity uses logical units/s;
- acceleration uses logical units/s²;
- angles are degrees in authored UI/file data;
- physics trigonometry may internally use radians;
- positive authored rotation is clockwise because +Y points downward.

Framebuffer/device pixels SHALL NOT be used as physics units.

## 25. UI scale independence

Changing UI scale changes control sizes and device mapping but SHALL NOT change:

- world coordinates;
- scene geometry;
- physics results;
- replay fixed-step actions;
- measurement results;
- serialized authored values.

## 26. Edit/runtime isolation

Entering Play creates runtime state from the authored scene.

Runtime mutations such as:

- ball positions;
- enabled/disabled event targets;
- opened gates;
- drop-target state;
- lights;
- score/combo;
- sensor occupancy;
- flipper angle;
- nudge/tilt state;

MUST NOT mutate authored scene properties.

Stop/Restart/return-to-Edit discards runtime mutations unless a feature explicitly provides an authored Apply operation; v1.0.0 provides no such Apply operation.

## 27. Simulation Preview isolation

Simulation Preview has stronger isolation:

- no authored mutation;
- no persistent history entry;
- no dirty-state change;
- no high-score/session persistence;
- no autosave trigger caused solely by runtime preview state.

## 28. Measurement tool

The editor SHALL include a view-only Measurement tool.

User can place two endpoints and see:

- endpoint world coordinates;
- ΔX;
- ΔY;
- Euclidean distance;
- angle in authored clockwise degrees normalized to `[0,360)`.

Measurement overlays are transient and are not serialized, copied, or included in simulation.

Snapping MAY be used while placing measurement endpoints when snapping is enabled.

## 29. Measurement accuracy

For endpoints `(x1,y1)` and `(x2,y2)`, distance SHALL equal `hypot(x2-x1, y2-y1)` within `1e-9` logical units for representable finite inputs prior to display rounding.

Displayed distance SHALL show at least two decimal places when non-integral.

## 30. Editor hard limits

The editor SHALL remain correct at the normative hard limits defined in `17_reference_defaults_and_limits.md`.

When a create/paste/duplicate action would exceed a hard limit, the operation is rejected atomically: no partial object subset may be inserted.

## 31. Acceptance minimums

The v1.0.0 Editor Gate SHALL include automated or reproducible checks for:

- overlap-selection cycling;
- Shift add/remove selection;
- marquee replace/union/subtract;
- locked member skip semantics;
- layer visibility not changing simulation;
- group move/rotate Undo;
- alignment/distribution exact coordinates;
- mixed Inspector values;
- paste ID remapping with internal event references;
- repeated paste uniqueness;
- dirty-state returning to clean after Undo to saved checkpoint;
- pointer-centered zoom invariance;
- Fit Scene padding;
- Edit/Play authored-state isolation;
- measurement numeric correctness.
