# 04 — Shape, Broad-Phase, Narrow-Phase, and Contact Manifold Specification

## 1. Supported shape types

Mandatory v1.0 shape types:

- Circle.
- Rectangle.
- Arbitrary convex polygon with 3–64 vertices.

Rectangle may internally use the convex polygon collision path but must remain a distinct editor shape type.

Concave polygons are explicitly unsupported.

## 2. Shape-local representation

Each shape must have local-space geometry independent of body transform.

### Circle

- local center, normally origin.
- positive finite radius.

### Rectangle

- positive finite width and height.
- local-space vertices derived consistently.

### Convex polygon

- normalized winding.
- local-space vertices.
- outward normals for each edge or equivalent computable representation.
- validated convexity.
- non-zero area.

## 3. AABB

Every shape must provide a world-space axis-aligned bounding box.

Required operations:

- compute from transformed shape.
- merge two AABBs.
- overlap test.
- containment test.
- perimeter/cost measure used by tree insertion.
- expansion by fat margin.

## 4. Broad phase: dynamic AABB tree

The final implementation must use a **dynamic AABB tree** for broad-phase candidate generation.

A flat O(N²) pair scan is permitted only as a test oracle and must not be the production broad phase.

### 4.1 Tree node requirements

Nodes must track sufficient data for:

- parent.
- children.
- height.
- AABB.
- leaf/proxy association.

### 4.2 Fat AABB

Dynamic proxies must use fat AABBs to avoid reinsertion on every tiny movement.

The fattening scheme must include:

- a minimum margin.
- displacement prediction or additional extension for movement.

Exact constants may be tuned but must be documented in diagnostics or source constants.

### 4.3 Insertion heuristic

Insertion must use a perimeter/cost-based sibling search or equivalent tree-quality heuristic.

Arbitrary append-only tree growth without cost evaluation is insufficient.

### 4.4 Balancing

The tree must contain an explicit balancing/rotation mechanism or equivalent maintenance that prevents severe unbounded degeneration under ordinary moving-body workloads.

### 4.5 Proxy lifecycle

Required operations:

- create proxy.
- destroy proxy.
- move/reinsert proxy when it exits fat AABB.
- query overlap.
- generate candidate pairs.

### 4.6 Duplicate pair handling

Candidate pair generation must not process the same unordered body/shape pair multiple times in one physics step.

### 4.7 Broad-phase correctness oracle

A developer verification program must compare dynamic-tree candidate coverage against a brute-force AABB overlap oracle over deterministic randomized scenes.

The tree may generate false positives, but it must not omit true AABB-overlap pairs.

## 5. Collision filtering

Each body/shape must support:

- 16-bit or wider category bits.
- 16-bit or wider collision mask bits.

A pair may enter narrow phase only if both masks/categories permit it.

Bodies connected by a joint may optionally disable mutual collision per-joint.

Static-static pairs must not enter the contact solver.

## 6. Narrow-phase dispatch

Mandatory pair handlers:

- circle-circle.
- circle-rectangle.
- rectangle-circle.
- circle-convex polygon.
- convex polygon-circle.
- rectangle-rectangle.
- rectangle-convex polygon.
- convex polygon-rectangle.
- convex polygon-convex polygon.

Rectangle paths may be normalized to polygon-polygon after dispatch.

## 7. Circle-circle collision

Must compute:

- overlap/no-overlap.
- collision normal.
- penetration depth.
- contact point.

Coincident centers must use a deterministic fallback normal rather than divide by zero.

## 8. Circle-polygon collision

Must correctly handle the circle center nearest to:

- polygon face interior.
- polygon vertex.
- inside polygon.

The chosen normal and contact point must be geometrically consistent.

A pure polygon-edge SAT implementation that misses vertex axes is invalid.

## 9. Polygon-polygon SAT

Separating Axis Theorem must evaluate edge normals from both convex polygons.

Requirements:

- early separation on any separating axis.
- track minimum penetration axis.
- orient final normal consistently from body A toward body B.
- stable handling of nearly parallel faces.

Using only AABB overlap as “SAT” is invalid.

## 10. Contact manifold generation

SAT overlap alone does not satisfy polygon collision requirements.

The implementation must generate a contact manifold using a method equivalent to:

1. choose reference polygon/face.
2. choose incident face on the other polygon.
3. clip the incident segment against reference side planes.
4. retain points behind/on the reference face within tolerance.
5. produce one or two contact points.

Each contact point must store enough data for solving and debugging, including:

- world point.
- separation or penetration.
- local anchors or equivalent stable representation.
- accumulated normal impulse.
- accumulated tangent impulse.
- stable feature/contact identifier sufficient for warm starting.

## 11. Manifold persistence

Across adjacent physics steps, contacts that represent the same geometric feature should preserve accumulated impulses through feature matching or a robust equivalent.

The final solver must support warm starting.

## 12. Contact normal convention

One convention must be used everywhere.

Required convention for acceptance:

- manifold normal points from body A toward body B.
- penetration is stored as positive depth or separation as signed value, but never mixed ambiguously.

## 13. Collision-state visualization

When debug collision visualization is enabled:

- colliding body outlines change to a designated collision-highlight appearance.
- each manifold contact point displays a red crosshair centered on the computed point.
- normal vector begins at a contact point or manifold representative point.
- penetration indicator corresponds to actual penetration depth.

## 14. Contact lifecycle

The contact manager must correctly handle:

- new contact.
- persistent contact.
- contact point change.
- separation.
- body deletion.
- shape edit.
- collision filter edit.
- joint-created collision-disable rule.
- waking on new impact.

Stale manifold references must not survive deleted bodies/proxies.

## 15. Required collision tests

At minimum:

### AABB/tree

- touching AABBs according to documented inclusivity.
- separated AABBs.
- containment.
- proxy move inside fat AABB.
- proxy move outside fat AABB.
- insertion/removal integrity.
- randomized oracle coverage.
- tree height sanity under ordered insertion.

### Circle

- separate.
- tangent.
- overlap.
- coincident centers.

### Circle-polygon

- face contact.
- vertex contact.
- circle entirely outside.
- circle center inside polygon.

### Polygon SAT/manifold

- separated on A axes.
- separated only on B axes.
- face-face collision producing two contacts.
- edge/vertex collision producing one contact.
- rotated rectangles.
- nearly parallel edges.
- deep initial overlap without NaN.
- winding normalization.

### Validation

For each generated manifold, test that:

- normal is finite and approximately unit length.
- contact count is within valid range.
- points are finite.
- penetration/separation convention is consistent.

## 16. v1.0 continuous collision integration

Continuous collision detection and Time of Impact are mandatory under `19_CCD_TOI_SHAPE_CAST.md`.

The collision system must share shape geometry, filtering semantics, manifold generation, and Dynamic AABB Tree infrastructure with the continuous path. CCD may not be a disconnected simplified collision engine.

Production filtering for discrete collision, CCD, sensors, and queries follows `20_COLLISION_MATRIX.md`.

Required collision tests now also include all `CCD-*`, `CAST-*`, and `COLF-*` cases. Failure in any of these families blocks the Collision release gate.
