# 16 — Spatial Queries, Sensors/Triggers, Deterministic Replay, and Checkpoints

## 1. Scope

This document defines three mandatory product/engine subsystems:

1. spatial query APIs and interactive probe tools,
2. non-solid sensor/trigger contacts and contact lifecycle events,
3. deterministic input replay and runtime checkpoints.

These are production features. They must operate on the same world, bodies, broad-phase proxies, narrow-phase geometry, fixed-step clock, and stable identifiers used by the normal simulation.

None of these features may be implemented as a visualization-only approximation or a separate simplified geometry model.

## 2. Spatial query API overview

The engine must expose reusable query operations independent of the GUI:

- closest-hit ray cast,
- all-hits ray cast,
- point query,
- axis-aligned bounding-box query.

Each query must support a filter containing at least:

- collision category mask,
- include/exclude sensors,
- optional body ID exclusion,
- optional static/dynamic/kinematic body-type mask.

The query API must not mutate world physics state.

Executing the same query against an unchanged world must return the same semantic result.

## 3. Ray cast definition

A ray cast is defined by:

- world-space origin `p0`,
- world-space destination `p1`,
- segment direction `d = p1 - p0`,
- normalized segment fraction `t` in `[0,1]`.

A required ray hit record contains:

- body stable ID,
- shape type,
- hit point in world coordinates,
- outward surface normal in world coordinates,
- fraction `t`,
- world-space distance from `p0`,
- sensor flag.

Degenerate rays with effectively zero segment length must be rejected as invalid input rather than producing a fabricated hit.

### 3.1 Closest-hit ray cast

Closest-hit mode must return:

- no hit, or
- the geometrically nearest valid hit along the segment.

The result must not depend on dynamic-tree traversal order.

If multiple hits are equal within the documented geometric epsilon, ties must be resolved deterministically by stable body ID and then stable shape/subfeature ordering where needed.

### 3.2 All-hits ray cast

All-hits mode must return every valid intersected body exactly once unless the shape semantics legitimately produce multiple sub-hits and the API explicitly documents them.

For this task, one hit record per body is sufficient and preferred.

Results must be sorted by:

1. ascending fraction `t`,
2. stable body ID for deterministic ties.

The returned count and ordering must not depend on broad-phase tree topology.

### 3.3 Circle ray cast

Circle intersection must be geometric, not based on the circle AABB.

Required cases include:

- segment misses circle,
- tangent segment,
- origin outside and segment enters circle,
- origin inside circle,
- segment endpoint on circle,
- segment ends before reaching circle.

For an origin inside the circle, the required hit is the first forward exit intersection on the segment if one exists.

### 3.4 Convex polygon and rectangle ray cast

Rectangle and convex polygon ray casts must test actual polygon half-spaces/edges.

Required cases include:

- face hit,
- vertex hit,
- grazing/tangent hit,
- origin inside polygon,
- parallel to an edge,
- segment stopping before the body,
- rotated shape.

For an origin inside a convex polygon, the required result is the first forward exit intersection.

The reported normal must correspond to the actual surface at the selected hit.

## 4. Point query

Point query receives one world-space point and returns all shapes that contain the point according to the engine's documented boundary convention.

Requirements:

- circle containment uses actual radius,
- polygon containment uses actual convex geometry,
- rectangle uses actual transformed rectangle geometry,
- query filters apply,
- sensors may optionally be included or excluded,
- results are ordered by stable body ID.

A point exactly on a boundary must follow one documented inclusive/exclusive convention consistently across all supported shapes.

Point query must not be implemented by testing only the body AABB.

## 5. AABB query

AABB query receives a world-space axis-aligned box and returns bodies whose **tight world AABB** overlaps the query box.

This operation is intentionally a bounding-box query, not an exact shape-overlap query.

Requirements:

- production implementation uses the dynamic AABB tree,
- the final semantic result is based on tight AABB overlap, not merely fat-proxy overlap,
- duplicate body IDs are removed,
- filters apply,
- results are ordered by stable body ID,
- invalid boxes with non-finite values or inverted bounds are rejected.

A separate exact arbitrary-shape overlap query is not required.

## 6. Query broad-phase authenticity

Production ray and AABB queries must traverse the real dynamic AABB tree.

A production query path that scans every body is prohibited except for:

- an independent brute-force validation oracle,
- very small internal unit fixtures explicitly testing reference behavior.

The validation oracle must not call the production query traversal and must independently enumerate bodies.

## 7. Interactive Probe Tool

The Sandbox must expose a **Probe** tool with three modes:

- Ray,
- Point,
- AABB.

The tool must be available without stopping the simulation.

### 7.1 Ray probe interaction

The user must be able to drag from ray origin to destination.

Visual feedback must show:

- ray segment,
- origin marker,
- destination marker,
- closest hit marker,
- hit surface normal,
- optional markers for all hits when all-hit mode is enabled.

The inspector/readout must show at least:

- body ID,
- hit point X/Y,
- normal X/Y,
- fraction,
- distance,
- sensor status.

### 7.2 Point probe interaction

The current world point under the probe must be visible.

The readout must list every matching body ID in deterministic order and indicate sensor status.

### 7.3 AABB probe interaction

The user must be able to drag a rectangular query region.

The query rectangle and matched bodies must be visibly distinguishable from ordinary selection.

The readout must show:

- query bounds,
- result count,
- ordered result IDs.

### 7.4 Query visualization separation

Probe visualization is diagnostic/editor state only.

Enabling, moving, or disabling the Probe tool must not:

- wake bodies,
- apply forces,
- create contacts,
- modify collision filters,
- modify scene dirty state unless a persistent editor preference is explicitly changed.

## 8. Sensor/trigger model

Every body shape must support a boolean `sensor` property.

A sensor:

- participates in broad-phase pair generation,
- participates in narrow-phase overlap/manifold detection sufficiently to know overlap state,
- participates in collision filtering,
- generates lifecycle events,
- does **not** generate normal collision impulses,
- does **not** generate friction impulses,
- does **not** generate rolling-resistance impulses,
- does **not** use Baumgarte correction to separate the pair.

A dynamic body must therefore pass through a sensor without being physically pushed by the sensor.

Sensors may be static, dynamic, or kinematic.

A sensor body's own normal dynamics against non-sensor bodies are also non-solid for any pair involving the sensor property.

## 9. Sensor contact identity

Sensor overlap identity must be based on stable body IDs and the real collision pair.

The implementation must maintain persistent overlap state between fixed steps so it can distinguish:

- Begin,
- Stay,
- End.

Transient pointer addresses, dynamic-tree node indices, or unordered hash iteration must not define externally visible event identity/order.

## 10. Contact lifecycle event stream

The engine must expose a deterministic per-fixed-step contact event stream for both solid contacts and sensors.

Minimum event types:

- `CONTACT_BEGIN`,
- `CONTACT_STAY`,
- `CONTACT_END`,
- `SENSOR_BEGIN`,
- `SENSOR_STAY`,
- `SENSOR_END`.

Each event must contain at least:

- fixed-step index,
- event type,
- body A stable ID,
- body B stable ID,
- sensor flags,
- representative world point when geometrically available,
- representative normal when geometrically available.

For deterministic ordering within one step, events must be sorted by a documented tuple beginning with event type order and canonical `(minBodyID,maxBodyID)` pair order.

A pair must not emit duplicate Begin or duplicate End events without a real intervening lifecycle transition.

## 11. Sensor lifecycle semantics

Required sequence for a body entering, remaining in, and leaving a sensor:

- first overlapping fixed step: exactly one `SENSOR_BEGIN`,
- each subsequent overlapping fixed step: exactly one `SENSOR_STAY`,
- first non-overlapping fixed step after overlap: exactly one `SENSOR_END`,
- later separated steps: no event for that pair.

The implementation may choose not to emit `STAY` for solid contacts only if the final API clearly documents that distinction; `SENSOR_STAY` is mandatory for this task.

## 12. Lifecycle changes caused by edits/deletion

The event/contact manager must handle:

- body deletion,
- sensor toggle,
- collision category/mask edit,
- shape geometry edit,
- scene reset,
- scene load,
- joint `collide-connected` change.

If an existing sensor overlap becomes invalid because of one of these committed world changes, stale overlap state must be removed.

Where an ordinary simulated separation would produce `END`, an edit/removal-induced termination must either:

- produce a deterministic `END` before destruction, or
- be explicitly classified as a lifecycle cancellation event in diagnostics.

The chosen policy must be documented and tested. Silent stale state is prohibited.

## 13. Sensor UI and diagnostics

The Body Inspector must include a Sensor toggle.

Sensor bodies must have a distinct visual treatment that remains legible without relying on color alone, such as:

- dashed outline,
- hatch/pattern,
- icon/badge.

Diagnostics must expose at least:

- active solid-contact pair count,
- active sensor-overlap pair count,
- Begin/Stay/End event counts for the current or most recent fixed step,
- event list containing pair IDs and event types.

A debug overlay must be able to display active sensor overlaps independently of solid manifolds.

## 14. Sensor persistence

Scene JSON body serialization must include the `sensor` boolean.

Rules:

- omitted `sensor` in version 1 legacy-compatible input defaults to `false`,
- serializer always writes the field for deterministic clarity,
- wrong type is a validation error.

Save/load round-trip must preserve sensor behavior.

## 15. Deterministic replay purpose

Replay exists to reproduce simulation behavior from a known initial state and a fixed sequence of physics-affecting inputs.

It is a verification/debugging feature, not a prerecorded animation.

Replay must execute the real engine and real product command paths.

A replay file containing precomputed body transforms that are simply displayed is prohibited.

## 16. Replay log contents

A replay file must be versioned and contain at least:

- replay format version,
- canonical initial-scene identifier/digest,
- fixed timestep,
- starting simulation step,
- ordered command records,
- optional named checkpoints/digest expectations,
- end step.

A command record must contain:

- fixed-step index at which it takes effect,
- deterministic intra-step sequence number,
- command type,
- command payload.

Wall-clock timestamps must not determine replay physics timing.

## 17. Replayable commands

At minimum, replay must support all physics-affecting interactive operations needed by mandatory E2E/validation scenarios:

- play/pause state changes where relevant to command scheduling,
- single-step request,
- create body,
- delete body,
- edit body physical properties,
- edit transform while permitted by editor semantics,
- edit world gravity,
- change solver/world settings that are user-editable,
- apply force,
- apply impulse,
- mouse-joint begin/update/end,
- create/delete/edit required joints,
- toggle sensor,
- edit collision filter,
- reset scene.

Camera movement, panel scrolling, hover effects, and other purely visual commands do not need to be recorded for physics replay.

## 18. Command scheduling semantics

A replayed physics command must execute at the same defined phase of the fixed-step loop as when it was originally recorded.

The application must document this phase, for example:

1. consume commands scheduled for step N,
2. update broad phase / contacts as required,
3. solve/integrate step N,
4. emit post-step events/metrics,
5. increment step index.

The exact internal decomposition may differ, but recorded and replayed commands must share one deterministic semantic point.

## 19. Replay recording

The application must provide controls to:

- start recording,
- stop recording,
- clear current replay recording,
- save replay,
- load replay,
- play replay from its required initial scene,
- stop replay.

Recording must store commands/inputs, not per-frame visual pixels and not precomputed trajectory samples as the source of truth.

During replay, conflicting physics-affecting live editing must be disabled or explicitly cancel replay.

## 20. State digest

The engine must provide a deterministic canonical **simulation state digest** for verification.

The digest is for reproducibility, not cryptographic security.

The canonical digest input must include authoritative state sufficient to detect divergent physics, including at least:

- current fixed-step index,
- world physics settings,
- every body sorted by stable ID,
- body type,
- transform,
- linear/angular velocity,
- awake/sleep state and sleep timer,
- mass/inertia-affecting properties,
- material/filter/sensor properties,
- every joint sorted by stable ID and its user-visible dynamic state,
- persistent solver/contact state required for deterministic continuation where applicable.

It must exclude:

- memory addresses,
- frame/render counters,
- UI hover animation state,
- dynamic-tree node allocation addresses,
- wall-clock time.

Floating-point values must use one canonical representation. A recommended acceptable representation is C hexadecimal floating-point text (`%a`) before hashing.

The digest algorithm must be documented. A simple project-owned deterministic hash such as 64-bit FNV-1a is acceptable.

## 21. Checkpoint definition

A runtime checkpoint captures enough simulation state to continue execution as though the simulation had not been interrupted.

Checkpoint restore must preserve or deterministically reconstruct all continuation-relevant state, including:

- body transforms and velocities,
- sleep state/timers,
- joints and their dynamic solver state where needed,
- active/persistent contacts needed for warm starting where needed,
- sensor overlap lifecycle state,
- replay command cursor when checkpointed during replay,
- fixed-step index,
- world settings.

It is acceptable to deterministically rebuild broad-phase tree topology from authoritative body state rather than serialize pointer/tree internals, provided continuation equivalence tests pass.

## 22. Checkpoint format and integrity

Checkpoint data must be versioned.

It may use a dedicated project-owned format rather than the human-editable scene JSON.

It must include an integrity checksum sufficient to detect accidental corruption.

Checkpoint load must be transactional:

- malformed/incompatible/corrupt checkpoint must not partially overwrite the active runtime world,
- successful restore replaces the runtime state atomically from the product perspective.

Checkpoint format must not serialize raw process pointers.

## 23. Replay and checkpoint UI

Diagnostics or Sandbox must expose a Replay/Checkpoint panel containing at least:

- recording state,
- replay state,
- current replay command index,
- current fixed-step index,
- current state digest,
- create checkpoint,
- restore checkpoint,
- replay controls,
- last replay mismatch status.

A digest mismatch must be clearly visible and identify the first checked step where mismatch was detected.

## 24. Replay mismatch diagnostics

When replay validation finds a digest mismatch, the produced report must include at least:

- replay file/fixture identifier,
- first mismatching fixed-step index,
- expected digest,
- actual digest,
- most recent command records before mismatch,
- first differing body or joint field if a field-wise reference snapshot is available,
- reproducible replay artifact path.

A mismatch must fail the corresponding verification case.

## 25. Query verification requirements

Mandatory automated query cases include at least:

- QRY-01 circle ray hit analytic point/normal/fraction,
- QRY-02 rotated rectangle face hit,
- QRY-03 convex polygon vertex/grazing hit,
- QRY-04 ray starts inside circle,
- QRY-05 ray starts inside polygon,
- QRY-06 closest-hit ordering independent of insertion order,
- QRY-07 all-hits ordered list versus brute-force oracle,
- QRY-08 point query circle/polygon boundary convention,
- QRY-09 point query versus independent brute-force geometry oracle,
- QRY-10 AABB query versus tight-AABB brute-force oracle,
- QRY-11 category/body-type/sensor filters,
- QRY-12 randomized tree-query oracle across at least 100 deterministic seeds,
- QRY-13 query has no physics side effects,
- QRY-14 query results invariant under scene translation,
- QRY-15 query results rotate consistently under a 90-degree scene transform where applicable.

Every randomized query failure must report seed, query parameters, and returned/reference body ID lists.

## 26. Sensor verification requirements

Mandatory automated sensor cases include at least:

- SNS-01 dynamic body passes through static sensor without impulse response,
- SNS-02 exactly one Begin/Stay*/End lifecycle sequence,
- SNS-03 sensor-sensor overlap lifecycle,
- SNS-04 dynamic sensor retains its own motion without sensor impulse,
- SNS-05 collision-filter suppression produces no sensor events,
- SNS-06 live filter edit terminates/restarts lifecycle according to policy,
- SNS-07 body deletion clears overlap state without stale references,
- SNS-08 sensor toggle solid→sensor changes response correctly,
- SNS-09 sensor toggle sensor→solid changes response correctly,
- SNS-10 scene reset/load clears or rebuilds lifecycle deterministically,
- SNS-11 event ordering is deterministic under body creation-order permutation,
- SNS-12 save/load preserves sensor flag and event behavior,
- SNS-13 100 simultaneous overlaps produce exact expected pair/event counts,
- SNS-14 sensor events are independent of render cadence,
- SNS-15 repeated same-input run produces identical event stream digest.

`Stay*` means one Stay event for every required overlapping fixed step after Begin and before End.

## 27. Replay/checkpoint verification requirements

Mandatory automated replay cases include at least:

- RPL-01 empty/no-input scene replay digest equality,
- RPL-02 force/impulse command replay equality,
- RPL-03 body create/edit/delete replay equality,
- RPL-04 mouse-joint drag replay equality,
- RPL-05 joint create/edit/delete replay equality,
- RPL-06 sensor/filter command replay equality including event stream,
- RPL-07 same replay executed at different render cadences yields same checked digests,
- RPL-08 replay run repeated five times yields identical digests,
- RPL-09 checkpoint restore immediately reproduces saved digest,
- RPL-10 checkpoint continuation equals uninterrupted continuation,
- RPL-11 checkpoint during active contacts preserves continuation equivalence,
- RPL-12 checkpoint during sleeping/wake boundary preserves continuation equivalence,
- RPL-13 checkpoint during sensor overlap preserves Begin/Stay/End continuation semantics,
- RPL-14 corrupted checkpoint rejected transactionally,
- RPL-15 wrong initial-scene digest rejects replay rather than silently running,
- RPL-16 edited replay command produces a detected divergence,
- RPL-17 long replay of at least 100,000 fixed steps remains deterministic,
- RPL-18 randomized command replay across at least 50 deterministic seeds reproduces final and periodic digests.

## 28. Required E2E workflows

The full-application E2E suite must include at least:

### E2E-Q01 Probe workflow

- open a known scene,
- use Ray probe,
- verify displayed hit body ID and hit marker,
- switch to Point probe,
- verify contained-body list,
- switch to AABB probe,
- verify matched-body count/list,
- verify world state digest is unchanged by query-only interaction.

### E2E-S01 Sensor workflow

- create or select a body,
- enable Sensor,
- move/launch another body through it,
- verify Begin/Stay/End diagnostics,
- verify the traversing body is not physically deflected by the sensor.

### E2E-R01 Replay workflow

- start replay recording,
- apply at least one impulse and one drag interaction,
- stop/save recording,
- reset to required initial state,
- replay recording,
- verify final digest equals original recorded-run digest.

### E2E-C01 Checkpoint workflow

- run a scene until active contacts exist,
- create checkpoint,
- continue for a fixed number of steps and record digest,
- restore checkpoint,
- run the same number of steps,
- verify final digest equality.

## 29. Required visual acceptance evidence

Evidence must include:

- ray probe with hit point and normal,
- point probe result display,
- AABB probe result highlighting,
- sensor body visual style,
- sensor event diagnostics showing Begin/Stay/End,
- replay recording state,
- replay playback state and digest,
- checkpoint restore success,
- an intentionally induced replay mismatch diagnostic captured from a verification fixture.

The evidence requirement specifies the result to capture, not the mechanism used to capture it.

## 30. Prohibited substitutions

The following do not satisfy this document:

- ray query against AABBs only,
- point query against AABBs only,
- linear production scan replacing dynamic-tree query traversal,
- sensor implemented by making collision restitution/friction zero while still applying normal impulses,
- sensor overlap determined only from AABBs,
- Begin/End generated from GUI hover/selection state,
- prerecorded transform sequence called a replay,
- replay driven by wall-clock/render frames instead of fixed-step command indices,
- checkpoint that stores only editor scene definition but loses runtime velocities/contact/sleep state,
- state digest containing pointer values or wall-clock data,
- swallowing replay mismatch and continuing with a success status.

## 31. Completion rule

Spatial queries, sensors, replay, and checkpoints are complete only when:

- user-facing workflows are connected to real engine state,
- save/load and diagnostics behavior is complete,
- all QRY/SNS/RPL mandatory cases pass,
- corresponding E2E workflows pass,
- required evidence exists,
- no prohibited substitute is present.

## 32. v1.0 integration

Spatial-query filtering follows the category/mask/group semantics in `20_COLLISION_MATRIX.md`.

Shape Cast is defined separately in `19_CCD_TOI_SHAPE_CAST.md` and uses the same query-filter model.

The deterministic replay/checkpoint primitives in this document are the state-reconstruction foundation for the user-facing Timeline in `21_REPLAY_TIMELINE.md`.

Replay implementations that pass base `RPL-*` cases but cannot satisfy `TLN-*` exact seek/step-back reconstruction are incomplete for v1.0.
