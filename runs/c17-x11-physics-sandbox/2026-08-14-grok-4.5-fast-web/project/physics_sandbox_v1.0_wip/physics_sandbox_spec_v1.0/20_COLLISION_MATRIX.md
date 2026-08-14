# Collision Layers, Masks, Groups, and Collision Matrix

Version: **1.0**
Status: **Normative**

## 1. Purpose

The collision filtering system is a first-class product feature. It must be editable, visible, persistent, deterministic, and testable. Filtering applies consistently to discrete contacts, CCD, sensors, spatial queries, Shape Cast, and replay.

## 2. Collision categories

The engine must provide at least 16 collision category bits.

A scene may assign human-readable names to all 16 categories. Default names are:

1. Default
2. World
3. Dynamic
4. Debris
5. Character
6. Sensor
7. Vehicle
8. Projectile
9. Category 9
10. Category 10
11. Category 11
12. Category 12
13. Category 13
14. Category 14
15. Category 15
16. Category 16

Names are scene metadata and do not change bit semantics.

## 3. Per-body filter fields

Each collidable body/fixture has:

- `category_bits` at least 16 bits,
- `mask_bits` at least 16 bits,
- signed `group_index` with zero meaning no group override.

A body may belong to multiple categories through multiple category bits.

## 4. Base mask rule

If neither pair member has a matching nonzero group override, A and B are eligible only when both are true:

- `(A.mask_bits & B.category_bits) != 0`
- `(B.mask_bits & A.category_bits) != 0`

The rule must be symmetric.

## 5. Group override rule

When two shapes have the same nonzero `group_index`:

- positive group index => they always pass collision filtering,
- negative group index => they never pass collision filtering.

The group rule overrides category/mask bits.

Different nonzero group indices do not override masks.

## 6. Scope of filtering

The same production filter function or semantically identical shared logic must govern:

- Dynamic AABB Tree candidate acceptance,
- discrete narrow-phase contacts,
- CCD/TOI candidates,
- sensors,
- Ray Cast,
- Point Query,
- AABB Query,
- Shape Cast.

A separate inconsistent UI-only or query-only filter implementation is prohibited.

## 7. Runtime editing

Changing filter fields at runtime must take effect before the next fixed physics step.

When an active physical contact becomes filtered out:

- it must be removed,
- cached impulses for that pair must not continue affecting the bodies,
- any relevant sensor lifecycle emits deterministic END semantics,
- broad-phase proxies remain valid without requiring body recreation.

When a previously filtered pair becomes eligible, normal broad-phase movement/query rules may create a new contact on the next relevant step.

## 8. Collision Matrix UI

The application must provide a Collision Matrix editor reachable from Sandbox or Diagnostics.

The matrix displays all named category bits as rows and columns.

Each unordered pair has one editable allow/deny cell representing the default category-level relationship.

The matrix must:

- support 16 categories without horizontal truncation of essential controls,
- keep row/column labels identifiable while scrolling,
- provide hover/focus state,
- visually distinguish allowed and denied pairs,
- update the underlying default mask configuration,
- support reset to default,
- participate in undo/redo.

## 9. Matrix symmetry

Editing cell `(A,B)` must produce the same category-pair relationship as `(B,A)`.

The UI must not permit an apparently asymmetric matrix while the runtime uses symmetric pair logic.

Per-body custom mask fields may still create asymmetric bit declarations internally, but final pair eligibility remains symmetric because both directional mask tests are required.

## 10. Per-body filter Inspector

The selected body Inspector must expose:

- category multi-select/bit view,
- mask multi-select/bit view,
- group index numeric field,
- derived human-readable summary of currently allowed category names.

Invalid numeric bit patterns beyond the supported width must be rejected rather than truncated silently.

## 11. Sensors

Sensors use the same filter rules.

Filtered sensor pairs must not emit BEGIN/STAY/END contacts for overlaps that are ineligible.

If a sensor pair becomes filtered out while active, exactly one deterministic END lifecycle transition must be produced.

## 12. CCD

CCD must apply filters before expensive TOI processing where possible.

A pair filtered out may not generate:

- TOI contact,
- physical impulse,
- sensor continuous-crossing event.

## 13. Query filtering

Ray/Point/AABB/Shape Cast queries have an explicit query filter:

- category bits,
- mask bits,
- group semantics where applicable,
- include sensors option,
- optional ignored body ID.

The query filter does not permanently edit the scene.

## 14. Persistence

Scene data must persist:

- category names,
- category matrix/default mask policy,
- per-body category bits,
- per-body mask bits,
- per-body group index.

Round-trip save/load must preserve all values exactly.

Older/invalid schema handling follows `07_SCENE_DATA_IO.md` and `08_ERROR_BOUNDARY_CASES.md`.

## 15. Replay and checkpoints

Replay must record physics-affecting filter edits at the fixed-step command index.

Checkpoint state must include all active per-body filter data and scene matrix metadata required to continue deterministically.

Canonical physics digest includes per-body filter bits/group index.

## 16. Diagnostics

Diagnostics must display at least:

- number of broad-phase candidate pairs before filtering,
- pairs rejected by masks,
- pairs accepted by positive group override,
- pairs rejected by negative group override,
- active sensor pairs,
- active physical contacts.

## 17. Error handling

The application must reject or clearly handle:

- group index outside the chosen serialized integer width,
- malformed hexadecimal/decimal mask input,
- duplicate category display names if the UI claims uniqueness,
- scene files with unsupported category bit width,
- attempts to remove a built-in category slot while bodies still reference the bit.

Renaming a category is allowed and must not change its bit position.

## 18. Mandatory filtering tests

- **COLF-01** mutually enabled masks collide.
- **COLF-02** A mask excludes B category => no contact.
- **COLF-03** B mask excludes A category => no contact.
- **COLF-04** both masks exclude => no contact.
- **COLF-05** same positive group overrides masks and collides.
- **COLF-06** same negative group overrides masks and does not collide.
- **COLF-07** different nonzero groups fall back to masks.
- **COLF-08** runtime disable removes existing physical contact and stale impulses.
- **COLF-09** runtime enable allows subsequent contact creation.
- **COLF-10** runtime sensor disable emits one END and no later STAY.
- **COLF-11** CCD respects mask exclusion.
- **COLF-12** CCD respects positive group override.
- **COLF-13** CCD respects negative group override.
- **COLF-14** Ray Cast filter matches physical filter semantics for target eligibility.
- **COLF-15** Shape Cast filter matches target eligibility.
- **COLF-16** Point/AABB query filtering is deterministic.
- **COLF-17** save/load preserves all 16 category names and filter fields.
- **COLF-18** replay of filter changes reproduces contact/event stream and digests.
- **COLF-19** category rename does not alter bit identity or physics result.
- **COLF-20** insertion/body-ID permutation does not alter filter outcome.
- **COLF-21** Collision Matrix cell toggle updates default policy symmetrically.
- **COLF-22** undo/redo restores Matrix edits and body filter edits.
- **COLF-23** invalid mask input is rejected transactionally.
- **COLF-24** 10,000 runtime filter changes do not corrupt broad-phase/contact state.

## 19. E2E workflows

Mandatory E2E:

- **E2E-COLF-01** open Collision Matrix, disable Dynamic vs Debris, spawn overlapping representatives, verify no contact;
- **E2E-COLF-02** re-enable pair and verify ordinary contact appears;
- **E2E-COLF-03** assign same negative group to two otherwise colliding bodies and verify separation impulses stop applying;
- **E2E-COLF-04** create sensor pair, change filter while overlapping, verify lifecycle END;
- **E2E-COLF-05** save scene, reload, verify matrix and body filters visually and numerically;
- **E2E-COLF-06** undo and redo Matrix change;
- **E2E-COLF-07** replay a recorded filter change and compare digest/event log.

## 20. Acceptance evidence

Required evidence:

- Collision Matrix screenshot with multiple categories,
- body Inspector filter screenshot,
- runtime filter E2E report,
- sensor END lifecycle report,
- persistence round-trip report,
- replay determinism report,
- machine-readable `COLF-*` summary.

## 21. Release blocking conditions

The release is BLOCKED if:

- filtering semantics differ among discrete/CCD/sensor/query paths,
- group override is absent or incorrect,
- active contacts continue applying stale impulse after being filtered out,
- matrix UI is decorative/disconnected,
- filter state is not persisted,
- mandatory `COLF-*` case fails/skips,
- required filtering evidence is missing.

## 22. Complete condition

Collision filtering is complete only when the engine, Matrix UI, Inspector, persistence, replay, diagnostics, tests, and evidence agree on one deterministic filtering model.
</file>
