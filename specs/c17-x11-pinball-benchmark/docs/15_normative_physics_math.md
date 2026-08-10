# 15 — Normative Physics Math and Solver Constants

This document removes avoidable solver freedom for v1 acceptance. Where it conflicts with a looser recommendation in another physics section, this document controls numeric behavior.

## 1. Core constants

- fixed timestep `dt = 1/240` s;
- numeric scalar type: `double`;
- geometry epsilon: `1e-9` for zero-length/parallel comparisons where appropriate;
- penetration slop: `0.02` logical units;
- penetration correction fraction: `0.60`;
- discrete contact solver iterations: 8;
- maximum impact/TOI resolutions for one ball in one fixed step: 16;
- maximum event actions per fixed step: 4096.

An implementation may use a smaller internal epsilon for a particular stable formula, but externally observable acceptance must remain equivalent and the above constants remain the reference.

## 2. Damping

For ball linear damping coefficient `d >= 0`, after acceleration update and before collision traversal:

`v = v * exp(-d * dt)`

Use the C math-library exponential function or a numerically equivalent implementation. This makes damping independent of render frame rate.

## 3. Gravity/integration recurrence

For a free ball over one fixed step:

```text
v_{n+1} = v_n + g * dt
v_{n+1} = v_{n+1} * exp(-d * dt)
x_{n+1} = x_n + v_{n+1} * dt
```

When CCD is active, the final position advance is partitioned by time-of-impact while using the step's updated velocity and later collision-modified velocity for remaining sub-time.

## 4. Speed clamp

The configured ball maximum is a scalar magnitude clamp.

If `|v| > vmax`, set:

`v = v * (vmax / |v|)`

Do not clamp individual X/Y components independently.

Apply safety clamp after acceleration/damping and again after active energy-adding impulses for the step. Passive collision response with restitution ≤1 should not require the clamp to remain stable.

## 5. Material combination

For ball vs static/moving surface:

- restitution `e = min(ball.restitution, surface.restitution)`;
- friction `mu = sqrt(ball.friction * surface.friction)`.

For ball vs ball:

- `e = min(e_a, e_b)`;
- `mu = sqrt(friction_a * friction_b)`.

All inputs are nonnegative validated finite values.

## 6. Contact normal orientation

Normal `n` points from the contacted surface/other body toward the ball being resolved.

For ball A vs ball B pair, canonical normal points from A center toward B center when resolving the pair equations below. The implementation must stay internally consistent with impulse signs.

## 7. Ball vs static surface normal impulse

Let `v_rel = v_ball - v_surface` where `v_surface=(0,0)` for static geometry.

Let `vn = dot(v_rel, n)` with normal pointing surface→ball.

If `vn >= 0`, no restitution impulse is applied because the ball is separating/non-closing.

If `vn < 0`:

`jn = -(1 + e) * vn * mass`

`v_ball += (jn / mass) * n`

For a static infinite-mass surface this simplifies to reflecting the normal component according to restitution.

## 8. Tangential friction for ball vs surface

After normal impulse, recompute relative velocity.

`t_raw = v_rel - dot(v_rel,n)*n`

If `|t_raw| <= epsilon`, no tangential impulse.

Otherwise `t = normalize(t_raw)` and desired impulse magnitude to cancel tangential relative speed is:

`jt_desired = -dot(v_rel,t) * mass`

Coulomb limit:

`|jt| <= mu * jn`

Choose `jt = clamp(jt_desired, -mu*jn, +mu*jn)`.

Apply:

`v_ball += (jt/mass) * t`

For a separating overlap with no normal impact (`jn=0`), this simple collision-friction impulse is zero; damping/other contact handling may still reduce motion through subsequent valid contacts.

## 9. Ball vs ball normal impulse

For pair A/B, define normal `n` from A→B and relative velocity:

`rv = v_b - v_a`

`vn = dot(rv,n)`

If `vn >= 0`, bodies are separating along normal and receive no restitution impulse.

Otherwise:

`invA = 1/m_a`

`invB = 1/m_b`

`j = -(1+e)*vn/(invA+invB)`

Apply:

`v_a -= j*invA*n`

`v_b += j*invB*n`

## 10. Ball vs ball friction

After normal impulse:

`rv = v_b - v_a`

`t_raw = rv - dot(rv,n)*n`

If nonzero, normalize to `t`.

`jt_desired = -dot(rv,t)/(invA+invB)`

`|jt| <= mu*j`

Apply:

`v_a -= jt*invA*t`

`v_b += jt*invB*t`

## 11. Positional correction — static surface

For penetration depth `p`:

`c = max(p - 0.02, 0) * 0.60`

Move ball outward:

`x_ball += c*n`

Correction changes position, not velocity.

## 12. Positional correction — ball pair

For penetration `p` and A→B normal `n`:

`c = max(p - 0.02, 0) * 0.60 / (invA+invB)`

`x_a -= c*invA*n`

`x_b += c*invB*n`

## 13. Coincident-center fallback

If two circle centers are coincident within epsilon, choose a deterministic unit normal from runtime IDs.

Normative acceptable policy:

- hash ordered pair `(min_id,max_id)`;
- select one of four axis directions deterministically;
- orient according to A/B ordering.

A simpler lexicographic policy such as +X for A<B is acceptable if it is deterministic and antisymmetric.

## 14. Capsule geometry

A segment from A to B with thickness `T` is treated as a capsule whose geometric radius is `T/2` around the finite centerline segment.

Collision of ball radius R with capsule is equivalent to point/swept-center contact against expanded capsule radius `R + T/2`.

This definition controls endpoint behavior.

## 15. Static capsule CCD

CCD must compute or conservatively identify first time `t_hit` in remaining interval where moving ball center touches the expanded capsule.

Required properties:

- `0 <= t_hit <= remaining_dt`;
- position advances to contact, not beyond;
- impact response occurs at contact;
- remaining time continues with new velocity;
- up to 16 impacts per ball/step;
- if cap reached, stop further impact traversal safely and increment diagnostic.

The exact analytic derivation is implementation-owned; the behavior is normative.

## 16. Bumper CCD

Treat bumper as static circle expanded by ball radius. Solve first swept point-circle intersection. On qualified impact:

1. perform normal material collision;
2. apply extra bumper impulse `J_bumper*n`;
3. apply speed safety clamp;
4. queue hit/score event if cooldown qualifies.

## 17. Bumper cooldown timing

Store last qualified hit step/time per ball/object pair. A hit qualifies if no previous qualified hit or elapsed simulation time `>= cooldown`.

Continuous residual overlap after an impact does not create new hit until both cooldown and a new closing impact condition are satisfied.

## 18. Slingshot ordering

Same as bumper ordering, except static capsule contact geometry. Base collision first, then extra outward normal impulse, then speed clamp, then hit event.

## 19. Flipper angular convention

Because +Y is downward, positive authored/runtime angular velocity is **clockwise** when viewed on screen.

Angles are stored/displayed in degrees but runtime calculations use radians.

For a point relative to pivot `r=(rx,ry)` and scalar clockwise angular velocity `omega`, contact-point surface velocity is:

`v_surface = (-omega*ry, omega*rx)`

where `omega` is rad/s.

## 20. Flipper motor integration

At each fixed step before ball collision traversal:

- if engaged, target active angle; move by at most `engage_speed * dt`;
- if released, target rest angle; move by at most `return_speed * dt`;
- clamp exactly at target;
- runtime angular velocity equals actual angle delta/dt for that step.

No easing is applied to physical flipper motor unless an optional mode can still satisfy normative fixtures; default must use constant angular speed toward target.

## 21. Moving-flipper contact

At contact point, compute `v_surface` from flipper angular velocity. Use surface-relative impulse equations from Sections 7–8, then transform back to ball world velocity by applying impulse to ball.

A flipper endpoint is part of the capsule and contributes the corresponding rotational surface velocity.

## 22. One-Way Gate direction test

`allowed_direction` is normalized at scene-load/runtime construction.

Normative v1 rule:

- if `dot(v_ball, allowed_direction) > 1e-9`, gate is non-solid for that ball's current traversal;
- otherwise it behaves as a solid capsule.

This is velocity-direction permission, not a graphical arrow-only hint.

## 23. Sensor swept crossing

Sensor trigger is based on the swept disk center against the Sensor AABB expanded by ball radius.

If a ball begins outside, crosses the entire expanded rectangle, and ends outside within one fixed step, emit ENTER at first intersection and EXIT at exit intersection, ordered by time. Legacy format-1 `SENSOR_LEAVE` is a parser alias only and is not the current runtime trigger name.

## 24. Drain swept crossing

Drain uses the same expanded-rectangle swept detection, but first intersection is sufficient to mark ball drained. No LEAVE event is required after Drain.

## 25. Collision/event timestamp ordering

When multiple TOI events occur in a fixed step, use sub-time ascending order. Equal-within-epsilon events use deterministic object order/ball ID tie-breakers.

## 26. World bounds

The table world rectangle is an editor/authoring extent, not an implicit solid wall. Authors build boundaries with Wall/Ramp/Drain objects.

A ball outside table bounds is not automatically reflected. Implementations MAY diagnose extreme out-of-bounds state but MUST NOT introduce hidden walls that alter acceptance fixtures.

## 27. Physics version

These formulas/constants define `physics_version = 1` for replay metadata. Any future incompatible solver-contract change requires a new physics version in a later task-package revision.

## 28. Deterministic same-time contact set

When multiple contacts have TOI values within `1e-12 s`, they form one simultaneous contact set. Candidate ordering and collision-class tie breakers are those in document 20. The solver SHALL iterate the stable ordered active set for 8 passes unless all contacts are separating earlier. A container/hash iteration order is never a valid tie breaker.

## 29. Drop/Stand-up target collision

While raised/enabled, Drop Target and Stand-up Target use static capsule geometry exactly as Sections 14–15. Their qualified-hit test uses pre-impact closing normal speed magnitude and the authored `min_hit_speed`. Penetration correction alone cannot qualify a hit.

For a qualified Drop Target, the passive collision for that impact completes before the target transitions to DROPPED and becomes non-solid for later sub-times.

## 30. Rollover trigger geometry

A Rollover segment with authored width `W` is a non-solid capsule trigger of radius `W/2`. Ball activation is swept point-vs-capsule using expanded radius `W/2 + ball.radius`. ENTER semantics match Sensor ENTER/EXIT occupancy rules; a high-speed complete crossing in one fixed step still activates once.

## 31. Spinner coordinate convention

Spinner angular velocity uses the same clockwise-positive scalar convention as flippers. Let contact point relative to fixed pivot be `r=(rx,ry)`. Define:

`q = perp_cw(r) = (-ry, rx)`

For spinner angular velocity `omega` in rad/s, contact surface velocity is:

`v_surface = omega * q`

Authored inertia `I` is a positive scalar rotational inertia in simulation units.

## 32. Spinner damping and free motion order

At the beginning of each fixed step, before spinner/ball collision traversal, apply:

`omega = omega * exp(-angular_damping * dt)`

During the step, spinner orientation advances continuously over each traversed sub-interval using the current `omega`. Collision impulses may change `omega` at a TOI and the new value applies for the remaining sub-time. No second damping application occurs until the next fixed step.

The moving spinner capsule SHALL participate in CCD or a conservative method that prevents a ball/spinner from tunneling through one another in the mandatory spinner stress cases. The exact TOI search algorithm is implementation-owned; contact impulse behavior below is normative.

## 33. Spinner normal impulse

At contact, with normal `n` pointing spinner surface toward ball:

`v_rel = v_ball - v_surface`

`vn = dot(v_rel,n)`

If `vn >= 0`, no restitution normal impulse. Otherwise define:

`a_n = dot(n,q)`

`K_n = 1/mass + (a_n*a_n)/I`

`jn = -(1+e)*vn / K_n`

Apply equal/opposite generalized impulse:

`v_ball += (jn/mass) * n`

`omega -= jn * a_n / I`

This formula is the normative fixed-pivot rigid spinner response.

## 34. Spinner tangential friction

After the normal impulse, recompute `v_rel`. Let tangent `t` be normalized tangential relative-velocity direction when its magnitude exceeds epsilon. Define:

`a_t = dot(t,q)`

`K_t = 1/mass + (a_t*a_t)/I`

`jt_desired = -dot(v_rel,t) / K_t`

`jt = clamp(jt_desired, -mu*jn, +mu*jn)`

Apply:

`v_ball += (jt/mass) * t`

`omega -= jt * a_t / I`

If `jn=0`, Coulomb collision friction is zero.

## 35. Spinner tick crossings

Maintain an unwrapped cumulative spinner angle rather than relying only on normalized display angle. A tick boundary occurs each time cumulative angle crosses an integer multiple of authored `tick_angle_deg` relative to the initial reference angle.

If one sub-interval crosses multiple boundaries, emit one `SPINNER_TICK` per boundary in chronological crossing order. Reverse rotation can cross boundaries in the opposite direction and also emits ticks. Exact landing on a boundary counts once when entering it from the prior side; remaining on the boundary does not retrigger.

## 36. Kickout swept capture

Kickout capture tests the swept **ball center** against the authored capture circle radius; do not expand by ball radius. First crossing time in the current sub-interval is the capture TOI. At capture:

1. advance ball center to capture crossing;
2. set free velocity to zero;
3. remove it from later free collision candidates for that step;
4. preserve runtime ID and enter CAPTURED state;
5. queue capture event at that sub-time.

Captured balls do not collide, integrate gravity/damping, receive nudge, or participate in ball-ball contacts until ejected.

## 37. Kickout ejection placement

Normalize authored eject direction `d`. At ejection attempt, candidate ball center is:

`x_eject = center + d * (capture_radius + ball.radius + penetration_slop)`

Candidate is valid only if it does not overlap an enabled solid or active free ball by more than penetration slop. If blocked, leave ball captured and defer exactly one fixed step. After 240 consecutive failed attempts, raise `RT_E_KICKOUT_EJECT_BLOCKED`.

On successful ejection:

`position = x_eject`

`velocity = eject_speed * d`

then apply normal speed clamp and queue `KICKOUT_EJECT` at the deterministic timer-expiry phase.

## 38. Nudge ordering and clamp

Logical nudge press edges are sampled in input phase before kinematic motor update and free-ball acceleration. For each accepted nudge, add the document-21 velocity delta to every free active ball in ascending runtime-ID order, then apply each ball's scalar maximum-speed clamp.

The Tilt meter is updated once per accepted nudge after velocity change. If threshold is reached by that input, TILTED state takes effect for subsequent player-control/scoring decisions in the same fixed step.

## 39. Tilt and physical solver

Tilt does not alter gravity, collision material parameters, CCD, or active table impulses. It only suppresses the player/control/scoring behaviors specified in document 21. Therefore a tilted simulation remains physically deterministic and continues until drain/turn resolution.

## 40. Physics behavior version

The complete formulas in Sections 1–39 define `physics_version = 1` for this task package. The v1.0 task package expands the original draft contract without changing the declared replay physics version; implementations are evaluated only against this final v1.0.0 definition.

