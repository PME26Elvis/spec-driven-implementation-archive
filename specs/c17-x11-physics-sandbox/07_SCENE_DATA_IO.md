# 07 — Scene Data, Persistence, Import Validation, and Export

## 1. Scene format

User scenes must use a human-readable **JSON** format.

The submission must implement its own JSON parser and serializer in C17.

The format must have a top-level schema/version field.

Required top-level fields:

- `format_version`
- `scene_name`
- `world`
- `bodies`
- `joints`
- optional `editor`

## 2. Versioning

Initial format version: integer `1`.

Loader behavior:

- missing version: error.
- version 1: load if otherwise valid.
- unknown higher version: reject with explicit unsupported-version error.
- malformed version type: reject.

The loader must never silently reinterpret unknown future data as version 1.

## 3. World object

Must store at least:

- gravity x/y.
- fixed timestep.
- sleep enabled.
- default solver iterations or explicit per-scene solver settings.

## 4. Body serialization

Each body must serialize:

- stable ID.
- optional name.
- body type.
- transform.
- initial velocity.
- shape definition.
- density or mass mode.
- material properties.
- damping.
- rotation lock.
- collision category/mask.

Transient values such as broad-phase node indices and accumulated contact impulses must not be required in saved scene files.

## 5. Shape serialization

### Circle

- type.
- radius.
- optional local center if non-origin centers are supported.

### Rectangle

- type.
- width.
- height.

### Convex polygon

- type.
- ordered vertex array.

The loader must revalidate convexity and geometry even if the file claims the polygon is valid.

## 6. Joint serialization

All joint files must refer to bodies by stable ID, not pointer-like transient values.

Distance joint stores:

- joint ID.
- body A/B IDs.
- local anchors.
- target length.
- spring parameters.
- collide-connected.

Revolute joint stores:

- joint ID.
- body A/B IDs.
- local anchors/reference angle.
- limit settings.
- motor settings.
- collide-connected.

## 7. Editor state

Optional editor state may store:

- camera position.
- zoom.
- selected body/joint ID.
- debug overlay toggles.
- panel collapse state.

Physics correctness must not depend on editor-only state.

## 8. JSON parser requirements

The custom parser must correctly support JSON primitives needed by scene/config files:

- object.
- array.
- string.
- number.
- `true`.
- `false`.
- `null`.

Strings must correctly handle at least:

- escaped quote.
- escaped backslash.
- `\n`, `\r`, `\t`.
- Unicode `\uXXXX` escape parsing sufficient to preserve valid JSON text.

Malformed escape sequences must be rejected.

Trailing garbage after the root value must be rejected except whitespace.

## 9. Parser limits

To avoid resource exhaustion, loader must impose documented finite limits, including at least:

- maximum file size: at least 8 MiB, may be higher.
- maximum nesting depth: at least 32.
- maximum bodies: at least 10,000.
- maximum joints: at least 20,000.
- polygon vertices: max 64 per required scope.

Files exceeding limits must produce a clear validation error rather than crash or allocate without bound.

## 10. Validation transaction

Loading must be transactional.

Required behavior:

1. parse entire candidate file into temporary representation.
2. validate all required fields and ranges.
3. validate unique IDs.
4. validate joint references.
5. validate shape geometry.
6. validate numerical finiteness.
7. only then replace the active scene.

A failed load must leave the previously active scene intact.

## 11. Required validation errors

Loader must detect at least:

- invalid JSON syntax.
- missing required field.
- wrong field type.
- duplicate body ID.
- duplicate joint ID.
- joint referencing nonexistent body.
- invalid body type.
- invalid shape type.
- radius ≤ 0.
- rectangle width/height ≤ 0.
- polygon fewer than 3 vertices.
- polygon more than 64 vertices.
- concave polygon.
- self-intersecting polygon.
- zero/near-zero-area polygon.
- negative density/mass where forbidden.
- restitution outside `[0,1]`.
- negative friction/damping/rolling resistance.
- NaN/infinite numeric values if textual parser can encounter them through non-standard number forms; they must be rejected.

## 12. Deterministic serialization

Saving the same editor scene without changes should produce semantically identical and, where practical, byte-stable JSON.

Required stability rules:

- deterministic body ordering by stable ID.
- deterministic joint ordering by stable ID.
- deterministic key ordering within objects produced by the serializer.
- locale-independent number formatting using `.` decimal separator.
- no timestamps embedded into the scene JSON unless explicitly stored as optional metadata outside deterministic snapshot comparisons.

## 13. Safe save behavior

A save operation must not leave a truncated valid-path scene if writing fails part way.

The implementation must use a safe-save strategy equivalent to:

- write to sibling temporary file.
- flush/close successfully.
- replace/rename target only after successful write.

If replace fails, the original file should remain available when possible.

## 14. Dirty state

Editor dirty state becomes true after a committed scene-definition edit.

Successful Save resets dirty state.

Simulation-only body motion must not automatically make the scene dirty unless the user explicitly commits current simulated transforms back into the scene definition.

## 15. Scene reset distinction

The implementation must distinguish:

- editor scene definition.
- current simulation runtime state.

Reset restores runtime state from editor scene definition.

Revert reloads editor scene definition from the last saved file/built-in source.

## 16. Built-in scenes

Built-in scenes may be compiled data or shipped JSON files, but they must pass the same validation rules as user scenes.

A validation utility must check every built-in scene.

## 17. Trajectory CSV and Motion Analysis recorder

Trajectory export must use the actual retained Motion Analysis samples defined in `14_FORCE_TRAJECTORY_TOOLS.md`.

CSV export must:

- include a header.
- use one row per sampled body per simulation sample.
- quote fields when required.
- use locale-independent numeric formatting.
- identify body by stable ID.
- include simulation time and fixed-step index.
- include position X/Y, angle, velocity X/Y, speed, angular velocity, translational kinetic energy, and rotational kinetic energy.
- preserve deterministic ordering by sample step and stable body ID.

Sampling interval may equal fixed physics step or an integer multiple selected by user.

Recording history is runtime diagnostic data and is not required to be serialized into the scene JSON. Save/load therefore must not fabricate or restore trajectory samples unless a future scene-format version explicitly adds that feature.

## 18. Statistics export

Statistics export must include at least:

- simulation time.
- step index.
- candidate pairs.
- active manifolds.
- contact points.
- awake bodies.
- physics step duration.
- broad-phase duration.
- narrow-phase duration.
- solver duration.

## 19. Import/export tests

Required automated cases:

- minimal valid scene.
- all body types.
- all shape types.
- all joint types.
- save→load round trip.
- load→save deterministic ordering.
- malformed syntax.
- concave polygon rejection.
- duplicate ID rejection.
- bad joint reference rejection.
- unknown version rejection.
- safe-load leaves old scene intact.
- safe-save failure leaves dirty state.
- CSV quoting.

## 20. v1.0 persistence additions

Scene persistence must include:

- per-body CCD mode;
- per-body category bits, mask bits, group index;
- scene category display names and Collision Matrix/default policy.

Replay/timeline persistence must include the versioned command stream/checkpoint metadata defined in `16_QUERY_SENSOR_REPLAY.md` and `21_REPLAY_TIMELINE.md`.

Golden fixtures are immutable normative inputs and must be integrity-checked before execution.

Required round-trip tests must now cover all v1.0 fields. Loading an older scene without new fields may apply documented safe defaults, but saving it produces the current format and must not silently lose old valid data.
