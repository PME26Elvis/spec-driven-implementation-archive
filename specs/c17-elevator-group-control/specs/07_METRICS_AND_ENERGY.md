# 07 — Metrics, Fairness, SLA, and Energy Score

## 1. General Rule

All metrics MUST be derived from actual simulation state, canonical trace rows, event timestamps, movement profiles, and utilization intervals.

No metric may be fabricated from expected/estimated algorithm behavior.

Passenger-time distributions normally use `COMPLETED` passengers only. Unfinished passengers are accounted separately as `UNSERVED` and participate in the all-arrived SLA rule below.

## 2. Required Passenger Counts

Per algorithm report:

- `trace_passengers`;
- `arrived_passengers`;
- `completed_passengers`;
- `unserved_passengers`;
- passengers with `bypassed_full_count > 0`;
- total full-capacity passenger-bypass events;
- count of starvation-urgent activation episodes.

Under schema 1.0 trace-horizon rules, every trace passenger arrives by the end boundary, so final `trace_passengers == arrived_passengers`. Both fields remain required as an integrity check.

Final identity:

```text
completed_passengers + unserved_passengers = arrived_passengers
```

## 3. Completed-Passenger Waiting Metrics

Using exact waiting-time definition from `02_DOMAIN_MODEL.md`, report:

- mean;
- minimum;
- maximum;
- population standard deviation;
- P50;
- P90;
- P95;
- P99.

All values are seconds in reports, derived from integer-microsecond timestamp differences before formatting.

## 4. Ride-Time Metrics

Over completed passengers:

- mean;
- minimum;
- maximum;
- P50;
- P95;
- P99.

## 5. Total-Journey Metrics

Over completed passengers:

- mean;
- minimum;
- maximum;
- P50;
- P90;
- P95;
- P99.

## 6. Empty Distribution Rule

For a distribution with zero completed observations:

- JSON scalar metrics are `null`;
- text metrics are `N/A`;
- CSV comparison metric fields are empty;
- no NaN or Infinity is emitted.

For one observation, population standard deviation is exactly zero.

## 7. Throughput

```text
throughput_per_hour = completed_passengers * 3600 / actual_elapsed_s
```

`actual_elapsed_s` is final simulation boundary time in seconds.

It includes drain time when used.

Configured duration is at least one second, so denominator is never zero in a valid run.

## 8. SLA Per-Passenger Rule

Configured threshold is `sla_wait_s`.

For each final passenger:

- if `fully_onboard_us` exists, SLA observed wait is `fully_onboard_us - arrival_us`;
- otherwise SLA observed wait is `run_end_us - arrival_us`.

The passenger violates SLA only when observed wait is **strictly greater than** the threshold.

Therefore an unserved passenger arriving shortly before hard stop does not automatically count as an SLA violation unless it actually had more than the threshold amount of waiting opportunity.

Required SLA fields:

- completed-passenger wait violations;
- all-arrived wait violations under the rule above;
- all-arrived violation percentage using `arrived_passengers` denominator;
- count with observed wait > `2 * sla_wait_s`.

If arrived count is zero, SLA percentage is `0.000%`/JSON `0` rather than null because the count ratio is defined by convention as zero for empty demand.

## 9. Percentile Definition

Sort N integer-microsecond observations ascending.

Nearest-rank percentile:

```text
rank = ceil(P * N / 100)
```

Rank is 1-based and clamped to `[1,N]`.

Implement this without introducing unnecessary floating rank error; integer arithmetic is recommended.

No interpolation is used.

For N=0 use Section 6.

## 10. Mean and Population Standard Deviation

Mean is arithmetic mean.

Population standard deviation:

```text
sqrt(sum((x - mean)^2) / N)
```

A numerically stable algorithm such as Welford is strongly recommended.

Calculations may be done in seconds or microseconds as long as final values agree within documented tolerance and output formatting.

## 11. Fairness Metrics

Required:

- maximum completed-passenger wait;
- P99 completed-passenger wait;
- all-arrived count with observed wait > `2 * SLA`;
- starvation-urgent activation count;
- maximum observed age of any conventional hall request during the entire run;
- Gini coefficient of completed-passenger waiting times.

`maximum observed hall-request age` is the maximum, over every tick where a hall request is active, of `current_time - oldest WAITING passenger arrival` for that request. It is not merely the age of requests remaining at run end.

## 12. Gini Coefficient

For sorted non-negative waits `x_i` with 1-based i:

```text
G = (2 * sum(i*x_i) / (N * sum(x_i))) - (N + 1) / N
```

Rules:

- N=0 -> null/N/A;
- all x_i=0 -> 0;
- N=1 -> 0;
- clamp only tiny numerical drift to `[0,1]`.

## 13. Elevator Utilization

Using exact interval categories from `03_SIMULATION_ENGINE.md`, per car report elapsed seconds and percentage for:

- moving;
- door opening;
- door open/dwell/transfer;
- door closing;
- idle closed.

Categories MUST partition every elapsed interval exactly once.

Before display rounding:

```text
sum(category_time_us) = run_end_us
```

Percentages must sum to 100 within output rounding tolerance.

Zero-duration transitions consume no utilization time.

## 14. Load Metrics

Per car report:

- maximum reserved occupancy observed;
- time-weighted mean onboard load over movement intervals;
- time-weighted mean onboard load ratio over movement intervals;
- passenger-meter;
- number of elapsed simulation intervals that begin with reserved occupancy equal to capacity (`full_capacity_ticks`);
- number of full-capacity passenger-bypass events caused by that car.

If the car has zero moving intervals, moving-load mean/ratio are null/N/A.

## 15. Movement Metrics

Per car and system total:

- distance traveled meters;
- floor-equivalent distance = distance / building floor height;
- physical service stops (door-opening service visits);
- staging arrivals separately;
- door cycles;
- movement startups;
- direction reversals;
- empty movement distance;
- loaded movement distance.

A startup occurs once per `CAR_START` movement leg.

A physical service stop counts once when a stopped-floor service episode begins opening doors, regardless of how many logical stop reasons are coalesced.

A staging arrival with no door opening is not a physical service stop.

For every movement leg:

- if onboard count at `CAR_START` is zero, entire leg distance is empty;
- otherwise entire leg distance is loaded.

Because transfer cannot occur during a leg, this partition is exact.

Required identity:

```text
empty_distance + loaded_distance = total_distance
```

within documented tiny floating tolerance.

## 16. Passenger-Meter

For each movement leg or sampled movement increment:

```text
passenger_meter += distance_increment_m * onboard_person_count
```

Onboard count is constant during one movement leg.

BOARDING passengers are not onboard yet; however movement cannot start until transfer/door cycle closes, so no ambiguity remains during movement.

## 17. Simplified Energy/Cost Model

The required score is intentionally comparative, not a claim of electrical kWh.

```text
energy_score =
    move_per_meter      * total_distance_m
  + startup             * startup_count
  + door_cycle          * door_cycle_count
  + direction_reversal  * reversal_count
  + idle_per_second     * idle_closed_seconds
  + load_meter_factor   * passenger_meter
```

`idle_closed_seconds` is exactly the utilization category from Section 13, not all non-moving time.

Report every additive component and total per car and system-wide.

No regenerative braking credit is required.

## 18. Comparison Ranking

Comparison must expose raw metrics for all algorithms.

It MUST NOT claim one universal best algorithm through an undisclosed composite score.

`comparison.txt` MUST identify winner(s) by at least:

- mean wait;
- P95 wait;
- P99 wait;
- all-arrived SLA violation percentage;
- maximum completed wait;
- energy score;
- throughput.

For a metric where lower is favorable, all exactly tied values after canonical report rounding are listed as tied winners in `default_compare` order.

Throughput uses highest value.

If a metric is unavailable for all algorithms (for example empty demand wait metrics), report `N/A`, not an arbitrary winner.

## 19. Waiting-Time Histogram

Configured bucket width `b = histogram_bucket_s` and cutoff `M = histogram_max_s`.

Required buckets are:

```text
[0,b)
[b,2b)
...
[k*b, min((k+1)*b, M))
[M,+inf)
```

Generate finite buckets until their lower bound reaches M; the last finite bucket may be shorter when M is not divisible by b.

An interior boundary value belongs to the higher bucket.

Histogram uses completed-passenger waiting times.

Each line MUST include numeric lower/upper label, count, and percentage. ASCII bar width/style is presentation-only and may vary, but deterministic within one build/config.

All bucket counts sum to completed count.

For zero completed passengers, every emitted bucket count/percentage is zero.

## 20. Elevator Time-Series Samples

`output.sample_interval_s` defaults to 1.

Sample every car at:

- `t=0` after the boundary has settled;
- each exact multiple of `sample_interval_s` not after run end;
- final `run_end_us` if it is not already a scheduled sample timestamp.

Rows include:

- timestamp;
- elevator ID;
- position;
- nearest floor;
- speed;
- direction;
- door state;
- onboard load;
- capacity;
- next target;
- future route stop count;
- adaptive mode where applicable.

Nearest floor is the floor with minimum physical-coordinate distance; exact midpoint ties choose the lower floor ID.

Sampling is observational and MUST NOT affect dispatch/simulation state.

## 21. Numeric Consistency Checks

Mandatory tests verify at least:

- trace count = arrived count at final state;
- completed + unserved = arrived;
- wait + ride + exit transfer = total journey for each completed passenger;
- system distance = sum car distance;
- empty + loaded distance = total distance;
- system energy components = sum per-car components;
- energy total = sum configured component costs;
- histogram counts = completed count;
- SLA counts <= arrived count;
- utilization microseconds partition full elapsed time;
- max reserved occupancy <= capacity;
- comparison-row values agree with corresponding per-algorithm summary after canonical rounding.
