# 03 — Simulation Engine and Physical Timing Model

## 1. Simulation Type

The simulator MUST use a deterministic fixed-step state engine.

Default `tick_ms` is `100`.

Accepted `tick_ms` values MUST satisfy both:

```text
10 <= tick_ms <= 1000
1000 % tick_ms == 0
```

This ensures every integer-second control boundary is exactly representable as a simulation tick.

The authoritative clock is integer microseconds:

```text
tick_us = tick_ms * 1000
```

No wall-clock sleeping or elapsed real time may advance the simulation.

## 2. Time Quantization

External passenger timestamps use integer microseconds.

A requested/generated arrival becomes visible at:

```text
visible_tick_us = ceil(arrival_us / tick_us) * tick_us
```

where an exact tick-aligned arrival is unchanged.

Door and passenger-transfer durations are configured in integer milliseconds and are quantized upward to whole ticks:

```text
quantized_duration_us = ceil(duration_ms / tick_ms) * tick_us
```

A configured duration of zero remains zero.

Scenario-level fields expressed in seconds are integers in v1.0.2. Because `tick_ms` divides 1000, these boundaries are exact ticks.

## 3. Tick-Boundary Processing Order

Simulation time begins at boundary `t = 0` and then advances by `tick_us`.

At every boundary `t`, process the following phases in exact conceptual order:

1. **complete interval work** whose quantized completion time is `t`: motion-leg arrival, door timer completion, boarding completion, and alighting completion;
2. **inject passenger arrivals** whose visible tick is `t`, in passenger-ID order;
3. **start newly possible alighting transfers** at open cars, freeing capacity at `ALIGHT_START`;
4. **refresh hall-request state** from current `WAITING` passengers;
5. **invoke dispatch/replanning** using the state at this boundary;
6. **apply assignment/route changes** that do not modify an already-moving leg target;
7. **start newly possible boarding transfers** using capacity after phase 3;
8. **start/advance zero-duration logical door/transfer transitions** until the boundary state is stable;
9. **start a door action or a new motion leg** made possible by the stable state;
10. **repeat only the zero-duration closure needed by phase 9**, without advancing simulation time;
11. **evaluate runtime invariants**;
12. **emit boundary events/samples** required at `t`;
13. **apply end-of-run boundary rules** if `t` is a configured stop boundary;
14. if the run continues, advance all positive-duration motion/door/transfer work conceptually toward boundary `t + tick_us`.

An implementation may organize functions differently, but observable event timestamps, ownership changes, capacity behavior, and decisions MUST be equivalent to this ordering.

## 4. Zero-Duration Closure

Zero door/transfer durations are legal.

The simulator MUST resolve all immediately enabled zero-duration transitions at the same timestamp before advancing to the next tick.

The closure loop MUST remain finite. Every iteration must consume a real state transition, passenger transfer, door transition, or route action. A fixed arbitrary iteration cap that rejects a valid large zero-duration passenger batch is non-compliant; the implementation should instead bound work by the finite number of state-changing objects/events.

Event ordering within a zero-duration closure follows `02_DOMAIN_MODEL.md` and Section 22 of this file.

## 5. Normative Motion-Leg Model

Every physical movement leg begins from rest at an exact floor coordinate and ends at rest at the next target floor.

**Once a movement leg starts, its target floor is committed for the entire leg.** Dispatch algorithms may replan stops after that target, but may not insert/cancel an intermediate stop into the active leg.

This v1.0.2 rule deliberately removes platform/numeric ambiguity around mid-leg braking and makes all required kinematic behavior testable.

A new request for a floor that the car is currently passing MUST wait for a later leg or another car.

## 6. Kinematic Parameters

For a leg of distance `D > 0`, each car has:

- maximum speed `v_max > 0`;
- acceleration `a > 0`;
- deceleration `d > 0`.

All use SI units from config.

Direction is the sign from origin floor to target floor. Kinematic formulas below use positive scalar distance/speed; direction is applied to position afterward.

## 7. Triangular vs Trapezoidal Profile

Distances required to accelerate from rest to max speed and then decelerate to rest are:

```text
d_accel_max = v_max^2 / (2*a)
d_decel_max = v_max^2 / (2*d)
```

If:

```text
D >= d_accel_max + d_decel_max
```

use a trapezoidal profile with:

```text
t_accel = v_max / a
d_cruise = D - d_accel_max - d_decel_max
t_cruise = d_cruise / v_max
t_decel = v_max / d
T = t_accel + t_cruise + t_decel
```

Otherwise use a triangular profile with peak speed:

```text
v_peak = sqrt((2 * D * a * d) / (a + d))
t_accel = v_peak / a
t_cruise = 0
t_decel = v_peak / d
T = t_accel + t_decel
```

The triangular profile MUST NOT invent a cruise phase.

The equality case is treated as trapezoidal with zero cruise duration; either phase label at the single transition instant is acceptable, but numeric state must match.

## 8. Position and Speed Within a Leg

Let `tau` be continuous seconds since `CAR_START` and `V` be `v_max` for trapezoidal motion or `v_peak` for triangular motion.

For `0 <= tau < t_accel`:

```text
x = 0.5 * a * tau^2
v = a * tau
phase = ACCELERATING
```

For trapezoidal motion and `t_accel <= tau < t_accel + t_cruise`:

```text
x = d_accel_max + v_max * (tau - t_accel)
v = v_max
phase = CRUISING
```

For the deceleration interval, let:

```text
s = tau - t_accel - t_cruise
x_decel_start = distance already covered before deceleration
```

then:

```text
x = x_decel_start + V*s - 0.5*d*s^2
v = max(0, V - d*s)
phase = DECELERATING
```

At `tau >= T`:

```text
x = D
v = 0
phase = STOPPED after zero-duration LEVELING
```

Implementations MUST clamp only tiny floating drift at phase boundaries; they MUST NOT use a constant-time-per-floor substitute.

## 9. Fixed-Step Sampling of the Motion Profile

A motion leg begins at a tick boundary `t0`.

At each later boundary `t`, evaluate the analytic profile at:

```text
tau = min((t - t0) / 1_000_000, T)
```

The physical car state visible to algorithms changes only at simulation boundaries.

If exact continuous arrival time `t0 + T` falls between boundaries, `CAR_ARRIVE` occurs on the first boundary at or after it. At that boundary:

- position is set exactly to target floor coordinate;
- speed is zero;
- `LEVELING` is a zero-duration logical transition;
- movement distance accumulated for the leg is exactly `D`.

The quantization delay before doors may open is therefore less than one tick.


## 9.1 Normative Floating Comparison Tolerance

Where this specification refers to a tiny floating tolerance for kinematic state, route-cost equality, invariant checks, or tiny-negative clamping, use the following predicate unless a section gives a more specific exact rule:

```text
nearly_equal(a,b) = abs(a-b) <= 1e-9 * max(1, abs(a), abs(b))
```

A value may be clamped to an exact physical boundary only when it is `nearly_equal` to that boundary. Values outside that tolerance MUST NOT be silently repaired.

For dispatch cost minimization, `a` and `b` are treated as tied when `nearly_equal(a,b)` is true; the policy's explicit deterministic tiebreak sequence then applies. This tolerance is part of required behavior and MUST NOT be replaced by an implementation-selected epsilon for acceptance runs.

## 10. Movement Metrics Per Interval

Movement distance for a boundary interval is the non-negative difference between the analytic leg positions sampled at its endpoints.

For the final interval, accumulated increments MUST sum to the exact leg distance `D` within documented floating tolerance; the system total for a completed leg MUST use `D` as the authoritative distance to avoid drift accumulation.

Load used for passenger-meter/empty-vs-loaded movement is the car's onboard count during that movement interval. Doors are closed and no transfer occurs during a leg, so load is constant throughout one leg.

## 11. Stop Commitment and Replanning

The active motion-leg target is committed from `CAR_START` until `CAR_ARRIVE`.

Algorithms may:

- add/remove/reorder uncommitted stops after the active target;
- reassign hall calls whose pickup leg has not begun;
- choose the next target after the current leg completes.

Algorithms MUST NOT:

- cancel the active movement leg;
- teleport to a newly requested intermediate floor;
- delete onboard destination service.

This rule supersedes any generic route-insertion intuition from textbook elevator systems.

## 12. Door Cycle

At a serviced floor:

1. car is stopped at exact floor;
2. `DOOR_OPEN_START` changes `CLOSED -> OPENING`;
3. after quantized `door_open_ms`, state becomes `OPEN`;
4. transfer lanes operate;
5. door remains `OPEN` for at least quantized `door_min_dwell_ms` measured from `DOOR_OPEN`;
6. door remains open while any transfer is active or an immediately eligible transfer can start;
7. when minimum dwell and transfer work are both complete, `DOOR_CLOSE_START` changes `OPEN -> CLOSING`;
8. after quantized `door_close_ms`, state becomes `CLOSED`;
9. movement may start at that same boundary only after `DOOR_CLOSED` and dispatch/route state permit it.

A physical service stop that opens doors increments `door_cycle_count` once at `DOOR_OPEN_START`.

A staging-only arrival with no service reason does not open doors and therefore does not increment door cycles.

## 13. Passenger Transfer Timing

When a transfer starts at boundary `t`:

```text
completion = t + ceil(duration_ms / tick_ms) * tick_us
```

For zero duration, completion is resolved in the same boundary closure.

For nonzero duration, the lane remains occupied through all intervening boundaries and becomes free during phase 1 at the exact completion boundary.

Example:

```text
tick_ms = 100
boarding_ms_per_person = 550
```

quantizes to 600 ms; a `BOARD_START` at `1,000,000 us` yields `BOARD_DONE` at `1,600,000 us`.

## 14. Transfer-Lane Start Rules

At an open car boundary:

1. start as many eligible alighting transfers as free alighting lanes allow, ascending passenger ID;
2. free each alighting passenger's capacity slot at `ALIGHT_START`;
3. update service/hall state as required;
4. after dispatch, start as many boarding transfers as free boarding lanes and reserved capacity allow;
5. conventional boarding uses queue FIFO among passengers directly serviceable by the car and policy-eligible;
6. destination-control boarding uses preassigned group/car rules.

When a positive-duration transfer completes, a newly freed lane may start another transfer at the same boundary after the prescribed state refresh.

## 15. Boarding Eligibility

A `WAITING` passenger may enter `BOARDING` only if:

- car is at passenger origin;
- doors are `OPEN`;
- the car directly serves that passenger OD pair;
- policy permits the pickup at this stop;
- passenger is eligible by FIFO/group ordering;
- a boarding lane is free;
- reserved occupancy is below capacity.

For conventional algorithms, a serviceability check may inspect destination only as a hard eligibility filter; destination must not influence ranking/assignment cost.

## 16. Full Car Behavior

When no capacity slot is free:

- no additional `BOARD_START` may occur;
- residual passengers remain `WAITING`;
- hall demand remains active;
- full-bypass accounting follows `02_DOMAIN_MODEL.md` exactly;
- another car or later trip must remain able to service residual demand.

A car MUST NOT begin a new movement leg solely toward a hall request for which it cannot directly serve any currently waiting passenger.

## 17. Initial State

At `t = 0` before boundary processing:

- every car is stopped at its configured initial floor;
- speed is zero;
- doors are `CLOSED`;
- no passenger is onboard/boarding/alighting;
- no hall/destination request is active;
- all passengers are `NOT_ARRIVED`;
- algorithm internal state is initialized deterministically;
- last non-IDLE movement direction is unset.

Passenger arrivals with visible timestamp 0 are then injected during the `t = 0` boundary.

## 18. Simulation End Modes

Required values are `hard_stop` and `drain`.

### `hard_stop`

At exact boundary `duration_s * 1_000_000`:

- process the entire boundary through invariants/samples;
- do not advance another movement/door/transfer interval;
- every passenger not `COMPLETED` becomes `UNSERVED`;
- emit final accounting and `RUN_END`.

### `drain`

At the duration boundary:

- no generated/imported future arrival exists beyond this boundary under v1.0.2 validation;
- emit `RUN_DRAIN_START`;
- continue full dispatch/physical service;
- stop when every arrived passenger is `COMPLETED`, or at `duration_s + max_drain_s`;
- if the drain cutoff is reached, remaining incomplete passengers become `UNSERVED`.

Default `end_mode` is `drain`; default `max_drain_s` is 3600.

## 19. Trace Horizon Rule

Canonical v1.0.2 traces MUST NOT contain `arrival_us` greater than `duration_s * 1_000_000`.

An arrival exactly at the duration boundary is valid and is injected before hard-stop/drain boundary handling.

Generated segments MUST lie within `[0, duration_s]` as defined in `04_TRAFFIC_GENERATION.md`.

There is no `allow_trace_after_duration` extension in schema 1.0.

## 20. Deadlock Detection

Default `deadlock_window_s` is 300.

Define **progress** as at least one of:

- passenger state advances toward completion (`WAITING -> BOARDING -> ONBOARD -> ALIGHTING -> COMPLETED`);
- a car begins or completes a physical movement leg;
- a door begins or completes a required service transition;
- ownership/assignment of an active serviceable request changes to a different feasible commitment;
- an urgent request receives a new service commitment.

If arrived unfinished passengers exist and no progress occurs for `deadlock_window_s` while no future arrival can alter demand, the run MUST fail with simulation/deadlock exit class.

Repeatedly rewriting an assignment to the same logical owner/route does not count as progress.

A request temporarily waiting behind legitimate long motion/door activity does not deadlock because those activities themselves are progress-capable scheduled work.

## 21. Runtime Invariants

At every boundary after zero-duration closure, check at minimum:

- car position within building coordinates;
- speed finite, non-negative, and <= configured maximum plus documented tiny tolerance;
- moving car has `CLOSED` doors;
- non-closed door implies zero speed;
- active leg target is a valid floor within car service range;
- reserved occupancy never exceeds capacity;
- each passenger exists in exactly one state/ownership context;
- `WAITING` passenger is in exactly one correct directional queue;
- `BOARDING` passenger owns exactly one boarding lane/car reservation;
- `ONBOARD` passenger belongs to exactly one car;
- `ALIGHTING` passenger owns exactly one alighting lane/car;
- `COMPLETED`/`UNSERVED` passenger is in no queue/car/lane;
- no state timestamp precedes arrival or violates required state ordering;
- passenger boards only at origin and alights only at destination;
- car handling a passenger directly serves both OD floors;
- active conventional hall request iff at least one matching `WAITING` passenger exists;
- conventional hall ownership is unique;
- destination-control passenger-group assignment is unique per passenger;
- transfer lane occupancy does not exceed lane count;
- onboard destination service is not lost from route planning.

Any invariant violation fails the simulation; it is not a warning.

## 22. Event Ordering at Equal Timestamps

When multiple events of the same event class share timestamp, order by:

1. lower passenger ID when passenger-specific;
2. lower car index when car-specific;
3. lower floor number;
4. `UP` before `DOWN` only if earlier rules do not distinguish.

Across different event classes at one boundary, event-log order follows the tick-processing phases in Section 3.

Algorithm-specific same-cost ties MUST use the explicit tiebreaks in `05_DISPATCH_ALGORITHMS.md`. Pointer values, hash-table iteration order, allocation order, or unspecified sort behavior may not break ties.

## 23. Utilization Interval Accounting

Every elapsed interval `[t, t + tick_us)` before termination belongs to exactly one utilization category per car, selected from the state that governs that interval:

- `moving` if an active movement leg spans the interval;
- `door_opening` if opening timer spans it;
- `door_open_transfer` if door is open, including dwell/transfer;
- `door_closing` if closing timer spans it;
- `idle_closed` otherwise.

These categories are mutually exclusive and exhaustive.

Zero-duration transitions consume no utilization time.
