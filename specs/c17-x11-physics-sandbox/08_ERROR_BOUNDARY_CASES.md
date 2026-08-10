# 08 — Error Handling, Numerical Boundaries, and Edge Cases

## 1. General error policy

Errors must be surfaced at the layer where the user can act on them.

The application must not crash, silently discard scene edits, or continue a corrupted simulation for ordinary invalid user/file input.

Errors fall into:

- validation errors.
- file I/O errors.
- numerical simulation failures.
- resource-limit errors.
- internal invariant failures.

## 2. User-visible error presentation

Recoverable errors must use one of:

- inline field validation.
- non-blocking banner/toast.
- blocking modal when user decision is required.

Error text must identify the failed operation and useful cause.

Examples:

- “Polygon is concave; only convex polygons are supported.”
- “Scene could not be opened: joint 17 refers to missing body 42.”
- “Save failed: target file could not be replaced. Current scene remains unsaved.”

Generic “Error” alone is insufficient.

## 3. Invalid numeric editor input

Properties must reject:

- empty committed value when a number is required.
- malformed number.
- NaN.
- infinity.
- out-of-range values.

The previous valid value remains active until valid edit commit.

## 4. Geometry edge cases

### Circle

Reject non-positive and non-finite radius.

### Rectangle

Reject non-positive/non-finite width or height.

### Convex polygon

Reject:

- fewer than 3 unique vertices.
- >64 vertices.
- repeated adjacent points within geometry epsilon.
- self-intersection.
- concavity.
- effectively zero area.
- non-finite coordinates.

Nearly collinear vertices may be normalized/removed only if the resulting polygon remains valid and the user is informed when an explicit edit was adjusted.

## 5. Extreme world values

The application must define documented safe editing ranges.

Recommended acceptance ranges:

- position magnitude ≤ 1e5 world units.
- linear speed magnitude ≤ 1e4 units/s.
- angular speed magnitude ≤ 1e4 rad/s.
- mass > 0 and ≤ 1e8 for dynamic bodies.
- density > 0 and ≤ 1e8.

Values outside implementation-safe bounds must be rejected visibly rather than allowed to destabilize the engine.

## 6. Zero and near-zero denominators

Physics code must guard effective-mass and normalization denominators.

Cases with zero total inverse mass/inertia must not divide by zero.

A joint between two static bodies should be rejected or represented as inert with a visible validation message; it must not create NaN.

## 7. Deep initial overlap

Deep overlap can occur from file load or paused editing.

Required behavior:

- simulation remains finite.
- correction impulses are clamped.
- diagnostics may mark severe penetration.
- solver attempts recovery over multiple steps.

The engine is not required to solve arbitrarily impossible packed configurations perfectly.

## 8. High-speed tunneling and CCD failure boundaries

Continuous collision detection is mandatory for bodies in BULLET mode.

DISCRETE bodies may still tunnel in deliberately adversarial control fixtures; that behavior must not be hidden or represented as BULLET correctness.

For BULLET bodies:

- mandatory CCD fixtures may not tunnel;
- TOI iteration/sub-step cap hits are diagnostic events and must not be hidden;
- reaching a cap must remain finite and deterministic;
- malformed/non-finite TOI state blocks the step and acceptance rather than committing corrupt transforms;
- a failed TOI search must be represented in diagnostics/reproduction artifacts.

See `19_CCD_TOI_SHAPE_CAST.md`.

## 9. Window and display edge cases

Handle:

- initial expose.
- resize during simulation.
- minimization/unmapping.
- restoration.
- close-window protocol.
- pointer leaving window during capture.

Minimized/unmapped windows may reduce or pause rendering work, but simulation behavior must be documented and must not accumulate unbounded catch-up time on restore.

## 10. Input edge cases

Handle:

- rapid repeated button clicks.
- drag release outside originating widget/window where events permit.
- Escape during body creation.
- deletion of selected body while inspector field is focused.
- body deletion while mouse joint active.
- scene change while a modal/popover is open.
- keyboard shortcut while numeric field is editing.

## 11. Scene file corruption

Malformed or partially written files must never partially replace the active valid scene.

Error output should include line/column or byte offset when practical.

## 12. Duplicate identifiers

Duplicate body or joint IDs are invalid.

The loader must not silently renumber ambiguous references.

New editor-created IDs must not collide with currently loaded IDs.

## 13. Missing resources

If an optional icon/glyph asset is missing:

- application may fall back to a built-in simple glyph or text label.
- core controls must remain identifiable.

Missing built-in scene data that is required by release gates is a release failure.

## 14. Allocation failures

Critical allocation failures must:

- avoid dereferencing null.
- preserve prior scene when load/import allocation fails.
- report failure if user-visible state can continue.
- exit non-zero for command-line verification utilities if they cannot continue.

## 15. Internal invariant failure

Debug/test builds should detect invariants such as:

- corrupt dynamic tree parent/child relation.
- invalid tree heights.
- dead body referenced by contact/joint.
- contact count outside valid bounds.
- non-finite impulse.

Release behavior must fail safely rather than continue silently when corruption is detected.

## 16. Performance overload

When a scene exceeds smooth real-time capability:

- input remains responsive where possible.
- physics-step catch-up remains capped.
- diagnostics indicates dropped/clamped simulation lag.
- application does not enter an unbounded loop trying to catch up.

## 17. Save path errors

Handle:

- permission denied.
- nonexistent parent directory.
- target is directory.
- disk/write failure.
- rename/replace failure.

A failed save cannot be reported as success.

## 18. Export cancellation/failure

Cancelled export must not produce a misleading success toast.

Partially written export should be removed when safe to do so or clearly marked as failed/temporary.

## 19. Test/verification utility failure semantics

Every required command-line utility must:

- return exit code 0 only when requested operation succeeds.
- return non-zero on validation/test failure.
- emit human-readable error.
- emit machine-readable report where required even when some cases fail, unless startup itself is impossible.

## 20. Force/impulse and recorder boundary cases

The application must handle without crash, invalid state, or silent corruption:

- force/impulse targeting static or kinematic bodies;
- zero vector;
- extremely small finite vector;
- rejected NaN/infinite numeric input;
- vector preview while paused;
- impulse commit while paused;
- body deletion during or immediately after interaction preview;
- scene Reset during active force gesture;
- active force target entering sleep logic;
- camera zoom/pan during vector preview;
- recorder with zero selected bodies;
- recorded body deleted during recording;
- recording paused for an arbitrary real-time interval;
- recording across scene Reset;
- full recorder capacity/ring-buffer eviction;
- constant-value graph channel;
- graph channel containing negative and positive values;
- export with no retained samples;
- export failure while recording remains active.

## 21. v1.0 query/filter/timeline failure boundaries

Additional required safe failure cases:

- Shape Cast starts from invalid/non-finite geometry;
- Shape Cast result target is deleted before UI presentation;
- collision category/mask numeric input is malformed;
- active contact becomes filtered out;
- sensor is filtered while overlapping;
- replay Timeline references missing/corrupt checkpoint;
- seek target exceeds replay duration;
- replay mismatch occurs during scrub;
- Fork From Here cannot persist provenance;
- Golden fixture digest differs from package manifest.

In all cases, the application must fail transactionally where world replacement is involved and preserve the last valid live scene when feasible. A verification failure remains BLOCKED even if the interactive application can recover and continue.

