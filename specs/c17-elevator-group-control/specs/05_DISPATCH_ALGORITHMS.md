# 05 — Required Dispatch Algorithms

## 1. Common Policy Contract

The implementation MUST expose a common internal dispatch-policy abstraction so one simulator can run different policies over identical domain state.

The exact C function signatures are implementation-defined. Policy behavior is not.

Every required policy can inspect:

- simulation timestamp;
- active hall demand and waiting counts;
- car position, active committed leg, future route, direction, load, door state, service range, and transfer workload;
- algorithm parameters;
- passenger destinations only where this specification allows them.

Every policy produces deterministic ownership/group/route decisions.

The simulator-level direct-serviceability filter from `02_DOMAIN_MODEL.md` is always available and does not count as destination-aware scoring.

## 2. Required Algorithm IDs

These seven IDs MUST be recognized:

1. `nearest_car`
2. `directional_collective`
3. `scan_look`
4. `eta_cost`
5. `zoning`
6. `adaptive_peak`
7. `destination_control`

Additional algorithm IDs are permitted, but they do not satisfy or replace any required policy.

Acceptance `default_compare` contains exactly the seven required IDs in the order above.

## 3. Universal Rules

Every policy MUST obey:

- direct serviceability;
- immutable active motion-leg target;
- passenger capacity/reservation;
- deterministic tiebreaking;
- floating policy costs use the normative `nearly_equal` predicate in `03_SIMULATION_ENGINE.md` before applying tiebreaks;
- no passenger loss/duplication;
- no deletion of onboard destination service;
- residual demand after partial/full pickup remains serviceable;
- global starvation protection.

For the six conventional policies, one hall request has at most one owner at a time.
Conventional owner lifecycle/reassignment is exactly the sticky contract in `02_DOMAIN_MODEL.md`; a policy MUST NOT add its own periodic rescore/reassignment behavior.

`destination_control` uses passenger-group ownership and is the explicit exception to exclusive hall-call ownership.

A route decision MUST NOT schedule a hall pickup for a car that cannot directly serve any currently waiting passenger represented by that pickup.

## 4. Shared Future-Route Insertion Semantics

The active movement leg, if any, is immutable.

For a candidate pickup floor, policies that use route insertion consider positions only in the **future uncommitted stop list** after the active target.

For each possible insertion position:

- coalesce with an existing stop at the same floor rather than create a duplicate physical stop;
- preserve every onboard destination reason;
- compute route distance/time using the resulting ordered stops;
- reject any stop outside car service range.

When a policy says "minimum insertion cost" and multiple insertion positions tie, choose the earliest future route index.

## 5. Algorithm 1 — `nearest_car`

Purpose: deliberately simple geometric baseline.

When a conventional hall request first becomes active and has no owner, each candidate car receives:

```text
score_m = abs(car_position_m - request_floor_position_m) + direction_penalty_m
```

Direction penalty:

- idle/stopped with no active leg: `0`;
- active leg moves toward request floor and passenger hall direction matches movement direction: `0`;
- active leg moves toward request floor but hall direction is opposite: `opposite_direction_penalty_m`;
- active leg moves away from request floor: `away_penalty_m`.

Candidate car must directly serve at least one waiting passenger in that hall request.

Choose lowest score. Ties:

1. lower current reserved-load ratio;
2. fewer future physical stops;
3. lower car index.

After car choice, insert/coalesce the pickup at the future route position that minimizes additional physical route distance, with Section 4 tiebreak.

This policy MUST NOT use waiting-passenger destination to rank cars or insertion positions beyond the hard direct-serviceability filter.

## 6. Algorithm 2 — `directional_collective`

Purpose: conventional directional collective control.

Each car has a service direction derived from active leg/future sweep state.

While service direction is UP:

- serve onboard destinations above in ascending floor order;
- serve owned UP hall calls above in ascending floor order;
- do not service a DOWN hall request as a normal pickup while required UP work remains above;
- after no required UP work remains above, reverse toward pending work below or become IDLE.

DOWN behavior is symmetric.

New unowned hall-call assignment preference classes are:

1. car already sweeping toward request floor with matching direction;
2. IDLE car;
3. car that can serve request after finishing its current sweep.

Within a class choose the smallest predicted **sweep route distance** to pickup, then:

1. lower reserved-load ratio;
2. lower car index.

Predicted sweep route distance includes all mandatory stops before the pickup under collective ordering; it is not just straight-line geometric distance.

A hall call in the opposite direction at a floor may coexist with a destination stop, but passengers of the opposite direction MUST NOT board until the collective turnaround rule allows it.

## 7. Algorithm 3 — `scan_look`

Purpose: explicit LOOK-style sweep routing and insertion-cost baseline.

For each car, all uncommitted stops are partitioned relative to current sweep direction and kept in strict floor order:

- UP sweep: ascending floors through the furthest currently required stop above;
- DOWN sweep: descending floors through the furthest currently required stop below.

The car reverses at the furthest required stop. It MUST NOT travel to building terminal floor merely to complete a sweep.

A request arriving during an active motion leg cannot modify that leg. If it lies ahead **and its hall direction matches the current sweep direction**, it may be inserted into the future portion of the same sweep after the active target. An opposite-direction hall request at a passed/ahead floor belongs to a later matching sweep unless the car is IDLE and begins a new sweep in that direction. A request behind the car likewise waits for a later matching sweep. Onboard destination stops are always served when reached regardless of hall direction.

A later matching sweep may require the car first to travel in the opposite direction to the hall floor. In that case the hall floor acts as a **turnaround anchor**: the car may arrive there while repositioning, changes service direction at that floor once no still-required work exists farther in the incoming direction, and then opens the door to serve the hall call in its requested direction without an extra movement leg. Merely passing/arriving at the anchor while the incoming sweep still has required work beyond it does not authorize opposite-direction boarding. This rule prevents a behind-UP or behind-DOWN call from becoming permanently unreachable.

For assigning an unowned hall call, calculate **additional LOOK route distance** caused by inserting that call into a direction-compatible ordered sweep plan, including any required turnaround-anchor travel, for each car. Choose minimum.

Ties:

1. fewer currently owned hall calls;
2. lower reserved-load ratio;
3. lower car index.

Unlike `directional_collective`, the primary assignment objective is minimum incremental LOOK-route distance, not direction-class preference.

## 8. Shared Frozen-Demand Route Predictor

Every requirement below that uses `predicted_pickup_time_s`, `added_route_time_s`, `predicted_load_ratio_at_pickup`, or destination-control future capacity MUST use this same deterministic predictor. It is part of the normative algorithm, not an implementation-selected heuristic.

At the dispatch boundary, construct a logical predictive clone for the one candidate car being evaluated:

1. copy that car's exact current motion/door/transfer state, reserved occupancy, onboard passengers, future uncommitted stops, and applicable already-owned/assigned commitments;
2. insert the candidate pickup/group using Section 4 rules; for destination-control also insert/coalesce that candidate group's tentative destinations where required by its scoring rule;
3. freeze demand at the current boundary: **no future passenger arrivals, no new hall calls, no adaptive-mode changes, no reassignment by another car, and no PRNG use** occur inside prediction;
4. advance the clone using the same analytic motion, tick ceilings, door timers, dwell, transfer lanes, FIFO, capacity reservation, and zero-duration closure rules as the real simulator;
5. existing positive-duration work already underway contributes its exact remaining quantized time;
6. at pre-pickup conventional hall stops, predict boarding only for passengers already `WAITING` at the decision boundary that are owned/policy-eligible for this car and directly serviceable by it, in normative FIFO/lane/capacity order; their hidden destinations MUST NOT create predicted future stops until they would reach `BOARD_DONE` in the clone;
7. onboard passengers and any passenger that reaches `BOARD_DONE` inside the clone create their ordinary destination reasons at that predicted completion boundary; destination-control tentative group destinations may be known earlier as explicitly allowed;
8. no event/log/metric/assignment from the predictive clone mutates authoritative simulation state or appears in product output.

If the cloned candidate cannot legally reach/serve the candidate pickup under these rules, that candidate is infeasible. Otherwise, `predicted_pickup_time_s` is the simulated elapsed time from the current decision boundary to the candidate pickup's `DOOR_OPEN` boundary. If the candidate car is already at that origin with door state `OPEN` and the candidate can begin boarding in the current service episode after the normative phase ordering, predicted pickup time is exactly 0. `predicted_load_ratio_at_pickup` is reserved occupancy immediately before starting candidate-passenger boarding at that service episode divided by capacity.

`added_route_time_s` compares the clone's time to finish all mandatory/candidate stops with a baseline clone of the same car and same frozen snapshot **without the candidate insertion**. If the candidate causes no later completion because it coalesces/overlaps existing work, the value may be zero. A tiny negative caused only by floating comparison tolerance is clamped to zero; any material negative is an implementation error.

For conventional hall requests, candidate feasibility requires at least one currently waiting passenger in that request to be boardable at the candidate service episode. For a destination-control group, the **entire group** must be predicted boardable in that episode.

## 9. Algorithm 4 — `eta_cost`

Purpose: predictive assignment using estimated pickup delay and route impact.

For each candidate car and unowned hall request, evaluate the best future insertion position and compute:

```text
score =
    w_wait     * predicted_pickup_time_s
  + w_detour   * added_route_time_s
  + w_stops    * added_stop_count
  + w_load     * predicted_load_ratio_at_pickup
  + w_reverse  * added_direction_reversals
  - w_fairness * request_age_s
```

All weights are finite non-negative values. The age term is an explicit credit: larger age lowers score.

The predictive terms use the Section 8 frozen-demand route predictor exactly.

`added_stop_count` is 0 when the candidate pickup coalesces with an already-existing physical stop in the candidate route, otherwise 1. The count does not include destination stops that become known only after future conventional boarding.

Ties:

1. lower predicted pickup time;
2. lower added route time;
3. lower reserved-load ratio;
4. lower car index.

A geometric-distance implementation relabeled as ETA is non-compliant.

## 10. Algorithm 5 — `zoning`

Purpose: floor-region preference with overflow.

Required mode is `static_equal`.

Construct conceptual zones from all **non-lobby** floors in ascending order and the scenario car count:

- split non-lobby floor count as evenly as possible;
- lower car indexes receive one extra floor until the remainder is exhausted;
- each car index owns at most one contiguous conceptual zone;
- when cars exceed non-lobby floors, later cars may receive an empty zone;
- lobby is globally unzoned.

A car is a **primary candidate** for a request when:

- request origin lies in that car's conceptual zone, or request origin is lobby;
- the car can directly serve at least one currently waiting passenger in the request.

Because heterogeneous service ranges may make the nominal zone owner ineligible, an empty primary candidate set triggers immediate fallback; this is not an input error.

Normal assignment:

1. if primary candidates exist and request age < `overflow_wait_s`, choose lowest ETA among primary candidates;
2. otherwise choose lowest ETA among all feasible cars.

Fallback is also immediate when no primary candidate satisfies direct serviceability and the Section 8 predictor's candidate-feasibility rule. Current fullness by itself does not make a car infeasible if the predictor shows capacity will be available at pickup.

Ties use ETA-cost tiebreaks.

Zone ownership MUST NOT override global starvation rules.

## 11. Algorithm 6 — `adaptive_peak`

Purpose: adapt dispatch/staging to observed demand.

The policy begins in `BALANCED`.

At each tick boundary `t >= window_s`, examine passenger arrivals in the half-open historical window:

```text
(t - window_s, t]
```

Classify each trip into exactly one category:

- `UP_CLASS`: origin is lobby;
- `DOWN_CLASS`: destination is lobby;
- `INTERFLOOR_CLASS`: neither endpoint is lobby.

Because origin != destination, these categories are mutually exclusive and exhaustive.

Compute:

```text
up_fraction = up_count / total_count
down_fraction = down_count / total_count
interfloor_fraction = interfloor_count / total_count
rate_per_minute = total_count * 60 / window_s
```

If total_count is zero, candidate mode is `BALANCED`.

Otherwise candidate mode is selected in this exact precedence:

1. if rate < `peak_min_rate_per_minute`: `BALANCED`;
2. else if up_fraction >= `up_lobby_origin_fraction`: `UP_PEAK`;
3. else if down_fraction >= `down_lobby_destination_fraction`: `DOWN_PEAK`;
4. else if interfloor_fraction >= `interfloor_fraction`: `INTERFLOOR`;
5. else `BALANCED`.

A candidate mode replaces current mode only if current mode has been held for at least `min_mode_hold_s`. Initial `BALANCED` is considered to start at `t=0`.

Every actual mode change emits one `ADAPTIVE_MODE` event.

### 11.1 `BALANCED`

Use the `eta_cost` assignment rule and configured ETA weights.

### 11.2 `UP_PEAK`

Use ETA assignment plus lobby staging.

Let:

```text
reserve_count = ceil(lobby_reserve_fraction * car_count)
```

clamped to `[0, car_count]`.

Among currently idle, unassigned cars that serve lobby, lower car indexes are selected until up to `reserve_count` cars are either at lobby or committed to a lobby staging leg.

A staging arrival does not open doors unless service demand exists.

### 11.3 `DOWN_PEAK`

Use ETA assignment and stage otherwise idle unassigned cars across the conceptual zones from Section 10.

A non-empty zone's staging floor is its lower-middle floor:

```text
floor = zone_start + (zone_size - 1) / 2
```

If a car cannot serve that floor, choose the nearest floor inside its service range to the zone midpoint; if none overlaps, leave it idle.

### 11.4 `INTERFLOOR`

Use ETA assignment and the same distributed zone-midpoint staging rule as DOWN_PEAK, without lobby preference.

Staging movement participates in movement/energy/utilization metrics.

## 12. Algorithm 7 — `destination_control`

Purpose: use destination before boarding and assign passenger groups to cars.

This policy does not use exclusive hall-call ownership. It uses tentative passenger-group assignments.

At each floor/direction, inspect unassigned `WAITING` passengers in FIFO order and form groups deterministically:

1. first unassigned passenger seeds a group;
2. scan later FIFO passengers in order;
3. a passenger may join if group size remains <= `max_group_size` and absolute destination difference from the seed destination is <= `route_similarity_floor_span`;
4. a passenger may join only if at least one car can directly serve every passenger currently in the candidate group **and** that car's configured `capacity_persons` is at least the candidate group size;
5. passengers that do not join remain for later groups;
6. continue until every currently waiting passenger is assigned to a candidate group or left temporarily unassigned because no feasible car capacity/route exists.

For each candidate group, evaluate every car that:

- serves origin and every group destination;
- has a feasible future pickup route under the Section 8 predictor;
- under that predictor, has enough reserved-capacity headroom for the entire group to begin boarding during that pickup episode.

Score:

```text
group_score =
    w_pickup       * predicted_pickup_time_s
  + w_route        * added_route_time_s
  + w_dest_stops   * added_destination_stop_count
  + w_load         * predicted_load_ratio_at_pickup
  - w_age          * oldest_group_passenger_age_s
```

Choose minimum; ties:

1. fewer added destination stops;
2. lower pickup ETA;
3. lower car index.

Mark every passenger in the group with that car assignment **before** any `BOARD_START`.

At pickup, only passengers assigned to that car may board, and they board in original queue FIFO order subject to lanes/capacity.

Multiple groups from the same hall direction may be assigned to different cars concurrently.

A destination-control group assignment is sticky once made. Newly arriving passengers do not join an already assigned group; they are considered in later groups. The Section 8 predictive clone for each new group MUST include earlier still-active group assignments and their tentative destination commitments on that car, so an implementation cannot overbook the same future pickup capacity and repair it nondeterministically later. Group reassignment is not part of schema 1.0. Starvation may promote the assigned pickup to the earliest legal uncommitted position on the **same assigned car**, but does not silently move the group to another car.

It is non-compliant for this policy to behave exactly like `eta_cost` until passengers are already onboard.

## 13. Starvation Protection

Global `starvation_threshold_s` applies to all policies.

At a boundary where the oldest `WAITING` passenger in a conventional hall request reaches age >= threshold, that hall request becomes urgent and emits `STARVATION_URGENT` once for that activation episode.

For `destination_control`, urgency is passenger based: any waiting passenger at/over threshold forces the group containing that passenger to urgent priority, and the hall direction emits one urgent event per activation episode.

Urgent work MUST be processed before all non-urgent assignment optimization.

For a conventional urgent request:

1. if the current owner has already started an active leg whose committed target is that pickup floor, keep that owner/leg;
2. otherwise evaluate every candidate car that can directly serve at least one current waiter, forcing the urgent pickup to the **first uncommitted route position after the active target** (coalescing if that position/floor already exists);
3. use the Section 8 frozen-demand predictor on that forced route and choose the lowest predicted pickup time; ties choose lower car index;
4. assign/reassign the request to that car and keep the urgent pickup at that first uncommitted position until its pickup leg becomes active or the hall episode clears. Existing uncommitted onboard destination stops remain mandatory but may be delayed behind this urgent pickup; active motion and active transfers are never changed.

For `destination_control`, an already assigned urgent group remains on its sticky assigned car and its pickup is promoted to that car's first uncommitted position after the active target. If an urgent passenger is still in an unassigned candidate group, choose among whole-group-feasible cars by the same forced-first-position/lowest-predicted-pickup-time rule, tie by lower car index, then make the assignment sticky.

Starvation priority never permits cancellation of an active motion leg, interruption/reordering of active transfers, deletion of onboard destinations, or splitting an already assigned destination-control group.

## 14. Route Replanning

At dispatch boundaries, policies may replan only uncommitted future stops.

They MUST NOT:

- modify an active motion leg target;
- delete an onboard destination reason;
- remove the only urgent commitment without replacing it in the same boundary;
- create a stop outside service range;
- reorder active passenger transfers;
- create duplicate physical stops for the same floor in one car route.

## 15. Algorithm Configuration

Each of the seven policy objects MUST be present in config, even when `{}` selects defaults.

Unknown policy fields are rejected under default unknown-field rules.

All algorithm numeric weights/penalties/fractions MUST be finite. Parameter ranges and defaults are normative in `06_CONFIG_JSON_YAML.md`.

## 16. Comparison Integrity

In `compare`:

- algorithms execute sequentially in `default_compare` order;
- each starts with freshly initialized cars/passengers/policy state;
- each receives the same canonical trace and trace fingerprint;
- no algorithm can consume another algorithm's result as input;
- metrics are merged only after each independent run finishes.

A failure in one algorithm makes overall compare fail; successful prior results may remain diagnostic but are not allowed to masquerade as complete comparison success.

## 17. Mandatory Algorithm-Specific Tests

Tests MUST demonstrate at minimum:

- nearest-car geometric/tiebreak behavior;
- directional collective defers opposite-direction pickup before turnaround;
- LOOK does not travel to a terminal floor without demand and uses route insertion cost;
- ETA chooses a farther car when nearer car's door/route workload makes pickup slower;
- zoning uses primary zone and immediate/age overflow correctly, including heterogeneous service range fallback;
- adaptive mode enters and exits peak modes under exact window/hold semantics;
- destination control produces distinct pre-boarding destination groups and supports two group owners at one hall direction;
- starvation overrides normal optimization;
- active movement leg target never changes after `CAR_START`.
