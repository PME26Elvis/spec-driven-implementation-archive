# 02 — Domain Model

## 1. Building

A scenario contains one building.

Required building fields:

- `name` is not required here; scenario name belongs to `scenario.name`;
- `floors`: integer in `[2, 200]`;
- `floor_height_m`: positive finite number;
- `lobby_floor`: integer floor ID.

Floor IDs are 1-based integers from `1` through `floors`.

The vertical coordinate of floor `f` is:

```text
position_m(f) = (f - 1) * floor_height_m
```

A passenger origin and destination MUST be different valid floors.

## 2. Elevator Bank

The building contains between 1 and 32 elevator cars.

Every car has a stable non-empty UTF-8 string ID unique within the scenario. Scenario array order defines the stable **car index** used for tiebreaking.

Each car runtime state contains at minimum:

- continuous position;
- non-negative scalar speed;
- current/last movement direction;
- motion phase;
- door state;
- current committed target when any;
- assigned hall requests or passenger groups;
- registered onboard destination requests;
- ordered stop plan;
- onboard/boarding/alighting passenger ownership;
- reserved occupancy and configured capacity;
- service-floor range;
- movement/energy counters;
- door-cycle/startup/reversal counters;
- utilization counters.

## 3. Elevator Configuration

Each car MUST support:

- `capacity_persons`;
- `max_speed_mps`;
- `acceleration_mps2`;
- `deceleration_mps2`;
- `door_open_ms`;
- `door_close_ms`;
- `door_min_dwell_ms`;
- `boarding_ms_per_person`;
- `alighting_ms_per_person`;
- `boarding_lanes`;
- `alighting_lanes`;
- `initial_floor`;
- `service_min_floor`;
- `service_max_floor`.

Every required algorithm MUST respect car service ranges.

A passenger is **directly serviceable by a car** only if that car's service range contains both passenger origin and destination.

Every configured/generated/imported passenger OD pair MUST be directly serviceable by at least one car. Elevator-to-elevator passenger transfer is outside scope.

For conventional hall-call algorithms, passenger destination MAY be consulted by the simulator only for this hard serviceability filter. Such filtering does not count as destination-aware scoring. The policy MUST NOT use destination to rank conventional hall-call candidates unless its algorithm section explicitly permits it.

## 4. Passenger State

Every passenger has a stable positive integer ID contiguous from 1 in canonical trace order.

Passenger state is exactly one of:

1. `NOT_ARRIVED`
2. `WAITING`
3. `BOARDING`
4. `ONBOARD`
5. `ALIGHTING`
6. `COMPLETED`
7. `UNSERVED`

Required passenger data:

- passenger ID;
- arrival timestamp;
- origin floor;
- destination floor;
- derived direction;
- queue-entry timestamp;
- boarding-start timestamp;
- fully-onboard timestamp;
- alighting-start timestamp;
- completion timestamp;
- current/tentative elevator assignment when applicable;
- `bypassed_full_count`;
- final service outcome.

`UNSERVED` may occur only through the normative end-of-run rules in `03_SIMULATION_ENGINE.md`. It is not a general fallback state for routing bugs.

## 5. Passenger Timing Definitions

For a completed passenger:

```text
waiting_time = fully_onboard_time - arrival_time
ride_time = alighting_start_time - fully_onboard_time
exit_transfer_time = completion_time - alighting_start_time
total_journey_time = completion_time - arrival_time
```

Boarding transfer time therefore counts as waiting time.

Alighting transfer time counts toward total journey time but not ride time.

For an unfinished passenger at run termination, no fabricated completion/ride/journey values may be emitted. Final waiting age is tracked separately where needed for SLA accounting.

## 6. Hall Request

A conventional hall request is keyed by:

```text
(origin_floor, direction)
```

It becomes active when at least one passenger in `WAITING` state exists at that floor/direction.

It becomes inactive when no passenger remains in `WAITING` state for that floor/direction.

Passengers in `BOARDING` are no longer members of the waiting queue; therefore a hall request may clear while transfers are still in progress if no residual waiter remains.

If capacity or serviceability prevents all waiters from boarding, residual waiting passengers keep the hall request active.

## 7. Hall Request Assignment Ownership

For `nearest_car`, `directional_collective`, `scan_look`, `eta_cost`, `zoning`, and `adaptive_peak`, one active hall request has at most one current owning car at a time.

Conventional ownership is **sticky by default** so implementations do not continuously rescore the same call differently. Once assigned, an owner remains until one of these exact conditions occurs:

1. the hall request clears;
2. at a boundary, the owner can no longer directly serve any passenger still `WAITING` in that request, in which case ownership is released immediately and the obsolete uncommitted hall-stop reason is removed;
3. at the origin service episode, residual waiters remain and the car reaches the exact `DOOR_CLOSE_START` boundary because no active transfer remains and no additional immediately eligible passenger can begin boarding; ownership is released at `DOOR_CLOSE_START`, and the continuously active residual request becomes eligible for fresh assignment at the next ordinary phase-5 dispatch boundary;
4. `zoning` explicitly reaches its `overflow_wait_s` reconsideration threshold before pickup commitment;
5. global starvation protection explicitly promotes/reassigns the request before pickup commitment.

No other conventional policy may opportunistically change an owner merely because another car's score becomes slightly better on a later tick. Once the owner has begun the active movement leg targeted at the pickup floor, reassignment is forbidden until that service episode completes. For one continuously active hall-call episode, the first owner emits `CALL_ASSIGN`; any later owner after a permitted release/reconsideration emits `CALL_REASSIGN` and identifies both old and new car. If the hall call clears and later activates again, that is a new episode and its first owner emits a new `CALL_ASSIGN`.

`destination_control` is the explicit exception to hall-call ownership: it assigns passenger groups rather than one exclusive hall-call owner, so multiple cars may hold distinct passenger-group commitments for the same `(origin_floor, direction)` concurrently. No passenger may belong to more than one group/car assignment.

## 8. Destination Request

An onboard destination request represents one or more onboard/alighting passengers requiring a physical stop. For the six conventional policies, the passenger's destination becomes a routable onboard-destination reason at `BOARD_DONE`; before that moment the ground-truth destination may be consulted only for the mandatory direct-serviceability filter and output/invariant bookkeeping, not as a soft dispatch, route-order, grouping, or cost signal.

Duplicate passenger destinations MUST coalesce to one physical stop while retaining all passenger identities.

For `destination_control`, destination information exists before boarding and may create tentative group destination commitments and stop-plan cost during group assignment. The authoritative onboard-destination reason is still registered at `BOARD_DONE`; tentative pre-boarding reasons must remain distinguishable from onboard reasons for invariants/accounting.

## 9. Waiting Queues

Each floor maintains separate UP and DOWN queues.

Queue order is strict FIFO by:

1. arrival timestamp;
2. passenger ID.

Conventional algorithms may choose which hall request/car serves demand but MUST NOT cherry-pick later arrivals ahead of earlier eligible waiters at the same floor/direction.

A passenger that the arriving car cannot directly serve is skipped for that car without losing its FIFO position relative to passengers competing for a car that can serve it.

`destination_control` may split the queue into preassigned groups under `05_DISPATCH_ALGORITHMS.md`; FIFO remains the primary ordering input and all group tiebreaks are deterministic.

## 10. Direction and Reversal

Direction values are:

- `DOWN = -1`
- `IDLE = 0`
- `UP = +1`

Passenger direction never changes.

A car stores both its current movement direction and its **last non-IDLE movement direction**.

A direction reversal is counted when a new non-IDLE movement begins opposite to the last non-IDLE movement direction. Therefore `UP -> IDLE -> DOWN` counts as one reversal; entering or leaving IDLE in the same direction does not.

The first movement from the initial state is not a reversal.

## 11. Car Motion States

Required externally observable motion states:

- `STOPPED`
- `ACCELERATING`
- `CRUISING`
- `DECELERATING`
- `LEVELING`

`LEVELING` is a zero-duration logical arrival transition in v1.0.2. There is no configurable leveling delay.

A car MUST be at zero speed and exact floor coordinate before door opening begins.

Doors MUST be `CLOSED` for the entire interval in which car movement occurs.

## 12. Door States

Required states:

1. `CLOSED`
2. `OPENING`
3. `OPEN`
4. `CLOSING`

A car MUST NOT move while doors are `OPENING`, `OPEN`, or `CLOSING`.

Passenger transfer may start only while the door is `OPEN`.

Door reopening during `CLOSING` is **not supported in v1.0.2**. A passenger arriving during `CLOSING` waits for a later service opportunity.

## 13. Stop

A stop is a planned physical service visit to one floor.

A stop tracks logical reason flags including at least:

- hall pickup;
- onboard destination;
- destination-control group pickup;
- staging/repositioning where required by `adaptive_peak`.

Multiple reasons for the same floor MUST coalesce into one physical stop.

A staging stop with no service demand does not open doors unless demand becomes eligible by arrival time and policy rules.

## 14. Capacity and Reservation

Capacity is measured in persons. Every passenger consumes one person slot.

Define:

```text
reserved_occupancy = ONBOARD + BOARDING
```

`ALIGHTING` passengers free their slot at `ALIGHT_START` and therefore do not count toward reserved occupancy after that event.

`reserved_occupancy` MUST never exceed `capacity_persons`.

A boarding passenger reserves a slot at `BOARD_START`, not at `BOARD_DONE`.

## 15. Full-Car Bypass Semantics

A **full-car service opportunity** occurs once per physical door-open episode at a floor when:

- the car is open at that floor;
- at least one waiting passenger is policy-eligible and directly serviceable by that car;
- no capacity slot is available for that passenger after all boarding starts possible at that boundary have been assigned.

For each still-waiting eligible passenger blocked by capacity during that episode:

- increment that passenger's `bypassed_full_count` exactly once for the episode;
- increment the system full-bypass event count once per blocked passenger;
- emit the required bypass event.

The same passenger MUST NOT receive repeated bypass increments on every tick while the same door-open episode remains full.

A passenger skipped only because the car cannot serve its destination does not count as a full-capacity bypass.

## 16. Simultaneous Boarding and Alighting

Once doors are fully `OPEN`, alighting and boarding use independent lane pools:

- at most `alighting_lanes` passengers may be in `ALIGHTING` concurrently;
- at most `boarding_lanes` passengers may be in `BOARDING` concurrently.

Each lane handles one passenger transfer at a time.

Alighting and boarding may overlap.

At a single simulation boundary, all eligible `ALIGHT_START` reservations/frees are processed before `BOARD_START` capacity checks, so capacity freed by an alighting start may be reused by a boarding start at the same boundary.

Lane-selection ordering is deterministic:

- alighting: ascending passenger ID;
- conventional boarding: waiting-queue FIFO;
- destination-control boarding: its normative assigned-group/FIFO rule.

## 17. UTF-8 Text

Scenario names, descriptions, elevator IDs, traffic segment IDs, and text labels are UTF-8 byte strings. Typed field byte limits are normative in `06_CONFIG_JSON_YAML.md`; implementations MUST NOT replace them with smaller fixed buffers.

The implementation need not normalize Unicode or perform locale-sensitive collation.

Valid UTF-8 text MUST be preserved byte-for-byte when re-emitted as the same logical label.

Invalid UTF-8 in a field defined as text MUST be rejected.
