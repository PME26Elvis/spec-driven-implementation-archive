# 12 — Fixed Acceptance Scenarios and Manual Verification

## 1. Purpose and Integrity Rule

The task pack supplies fixed acceptance inputs under:

```text
fixtures/acceptance/
```

These files are normative benchmark inputs.

An implementation claiming completion MUST run them without editing, replacing, weakening, or regenerating the supplied inputs.

`fixtures/acceptance/SHA256SUMS.txt` lists SHA-256 hashes for normative positive config/trace files, and `fixtures/invalid/SHA256SUMS.txt` protects the fixed negative corpus. SHA-256 verification is reviewer/evidence tooling; the C product is not required to implement SHA-256.

In the command examples below, `<OUT>/name` means any writable **pre-existing parent path plus output prefix** chosen by the reviewer. Directory creation is outside product behavior.

## 2. A01 — Minimal Two-Floor

Inputs:

```text
fixtures/acceptance/a01_minimal_two_floor.json
fixtures/acceptance/traces/a01_minimal.csv
```

Run `eta_cost`.

Verify:

- exactly one passenger completes;
- car moves only upward for passenger service;
- one pickup and one destination service door cycle occur;
- timestamp ordering is valid;
- triangular/trapezoidal profile obeys configured geometry;
- no unserved passenger/invariant error.

## 3. A02 — Concurrent Transfer

Inputs:

```text
fixtures/acceptance/a02_concurrent_transfer.json
fixtures/acceptance/traces/a02_concurrent_transfer.csv
```

Run `eta_cost`.

At the shared transfer floor, event log MUST contain overlapping positive-duration ALIGHT and BOARD windows with two independent lane pools and no capacity violation.

## 4. A03 — Full Car Residual Queue

Inputs:

```text
fixtures/acceptance/a03_full_car.json
fixtures/acceptance/traces/a03_full_car.csv
```

Run `nearest_car`.

Verify:

- capacity 2 never exceeded;
- residual hall demand remains active;
- multiple service trips occur;
- every supplied passenger completes under drain allowance;
- full bypass metrics/events are nonzero;
- a passenger bypass counter increments at most once per physical full-car door-open episode.

## 5. A04 — Short vs Long Kinematics

Inputs:

```text
fixtures/acceptance/a04_kinematics.json
fixtures/acceptance/traces/a04_kinematics.csv
```

Run `eta_cost`.

Verify event/samples demonstrate:

- a short movement leg with triangular profile and no positive cruise duration;
- a long movement leg with a cruise phase;
- analytic arrival tick-ceiling behavior;
- exact target positions/no overshoot.

## 6. A05 — Direction Conflict

Inputs:

```text
fixtures/acceptance/a05_direction_conflict.json
fixtures/acceptance/traces/a05_direction_conflict.csv
```

Run `directional_collective`.

Verify DOWN demand at an intermediate floor is not boarded as normal pickup before the UP sweep turns around.

## 7. A06 — Geometric Nearest vs ETA

Inputs:

```text
fixtures/acceptance/a06_nearest_vs_eta.json
fixtures/acceptance/traces/a06_nearest_vs_eta.csv
```

Run both `nearest_car` and `eta_cost` separately.

The fixture intentionally places the geometrically nearer candidate on an already committed long movement leg and removes nearest-car opposite-direction penalty.

Required relational result for the designated later hall request:

- `nearest_car` assigns the geometrically nearer car according to its specified score;
- `eta_cost` assigns a different faster-pickup car because the active leg/route delay is included.

If an implementation does not produce this divergence, A06 fails; implementer may not retune the input.

## 8. A07 — LOOK Endpoint

Inputs:

```text
fixtures/acceptance/a07_look_endpoint.json
fixtures/acceptance/traces/a07_look_endpoint.csv
```

Run `scan_look`.

No request exists above floor 14 in a 20-floor building.

Verify no car travels to floor 20 merely to finish a sweep.

## 9. A08 — Zoning Overflow

Inputs:

```text
fixtures/acceptance/a08_zoning_overflow.json
fixtures/acceptance/traces/a08_zoning_overflow.csv
```

Run `zoning`.

Fixture gives one zone's primary car a long committed slow leg and a low overflow threshold.

Verify:

- ordinary initial ownership follows static zone;
- after request age reaches overflow threshold while still unserved, ownership is reconsidered across all feasible cars;
- a non-primary feasible car accepts service;
- starvation is not required to trigger this fallback.

## 10. A09 — Morning Up-Peak Adaptive / Seven-Way Compare

Inputs:

```text
fixtures/acceptance/a09_up_peak.json
fixtures/acceptance/traces/a09_up_peak.csv
```

Run `compare`.

Trace is fixed and strongly lobby-origin dominated.

Verify:

- all seven algorithms use identical trace fingerprint/passenger count;
- `adaptive_peak` enters `UP_PEAK` after its exact observation-window rules;
- `ADAPTIVE_MODE` event exists;
- lobby staging movement is observable when idle cars are available;
- comparison outputs contain all seven rows.

## 11. A10 — Evening Down-Peak

Inputs:

```text
fixtures/acceptance/a10_down_peak.json
fixtures/acceptance/traces/a10_down_peak.csv
```

Run `adaptive_peak`.

Verify exact window classification enters `DOWN_PEAK` and produces distributed upper-floor staging behavior where cars are idle/eligible.

## 12. A11 — Interfloor Demand

Inputs:

```text
fixtures/acceptance/a11_interfloor.json
fixtures/acceptance/traces/a11_interfloor.csv
```

Run `adaptive_peak`.

Verify it can enter `INTERFLOOR`, does not retain permanent lobby bias, and uses distributed staging as specified.

## 13. A12 — Generated Burst

Input:

```text
fixtures/acceptance/a12_burst_generated.json
```

Run `generate` to a fresh trace filename, then run `compare` on the same generated scenario to a fresh prefix. The trace produced by `compare` MUST be byte-identical to the separately generated trace and have the same fingerprint. This fixes both generation and seven-policy simulation behavior for A12.

Verify:

- exactly 200 generated passengers;
- every canonical arrival is tick-aligned;
- all raw-generation semantics imply bounded configured window;
- no impossible OD pair;
- capacity queues become visible;
- drain completes with no passenger loss under supplied allowance.

This scenario exercises actual burst sampling; implementer may not substitute the fixed A03 trace for generator evidence.

## 14. A13 — Destination Grouping

Inputs:

```text
fixtures/acceptance/a13_destination_grouping.json
fixtures/acceptance/traces/a13_destination_grouping.csv
```

Run `destination_control`.

The lobby queue contains low-floor and high-floor destination clusters.

Verify:

- at least two distinct pre-boarding groups are formed;
- same hall direction may have different group car owners;
- `PASSENGER_ASSIGN` appears before every corresponding `BOARD_START`;
- group membership obeys FIFO seed/route-span/serviceability rules;
- policy is observably destination-aware before boarding.

## 15. A14 — Starvation Protection

Inputs:

```text
fixtures/acceptance/a14_starvation.json
fixtures/acceptance/traces/a14_starvation.csv
```

Run `nearest_car`.

The fixture first commits the only car to a long active `1 -> 20` leg. While that leg is immutable, an older DOWN request at floor 10 is inserted, followed by newer DOWN requests at floors 18/17/16/15 that would ordinarily be visited earlier on the descent. The low starvation threshold is crossed before the active leg finishes.

Verify:

- one `STARVATION_URGENT` activation occurs for the floor-10 DOWN episode exactly when its oldest wait reaches the threshold;
- the active `1 -> 20` leg is not altered;
- after that active leg, floor 10 is promoted ahead of the newer floor-18/17/16/15 pickup stops as the earliest legal uncommitted service stop;
- the old passenger eventually completes under drain.

## 16. A15 — Equivalent JSON and YAML

Inputs:

```text
fixtures/acceptance/a15_equivalent.json
fixtures/acceptance/a15_equivalent.yaml
```

Both are generated-demand scenarios with identical semantics.

Verify:

- both validate;
- typed defaults/values equal;
- `generate` produces byte-identical canonical trace under same build;
- fingerprints equal;
- `eta_cost` summary/event/passenger outputs match;
- compare headline metrics match if both are compared.

## 17. A16 — Replay Identity

Use:

```text
fixtures/acceptance/a09_up_peak.json
fixtures/acceptance/traces/a09_up_peak.csv
```

Run `eta_cost` normally from scenario trace mode, then `replay` with the same explicit trace to another prefix.

Verify all simulation-derived canonical files are byte-identical; only manifest `run_kind` differs as specified.

## 18. A17 — Empty Demand

Input:

```text
fixtures/acceptance/a17_empty_demand.json
```

Run `eta_cost`.

Verify successful zero-demand behavior, null/N/A distributions, zero SLA percentage, complete utilization partition, and configured idle energy.

## 19. A18 — Hard Stop

Inputs:

```text
fixtures/acceptance/a18_hard_stop.json
fixtures/acceptance/traces/a18_hard_stop.csv
```

Run `eta_cost`.

Verify:

- boundary arrival is injected before stop handling;
- some passengers are UNSERVED;
- no interval advances after duration boundary;
- incomplete state timestamps remain truthful;
- completed+unserved accounting balances;
- newly arrived passenger with insufficient SLA opportunity is not automatically an SLA violation.

## 20. A19 — Drain Cutoff

Inputs:

```text
fixtures/acceptance/a19_drain_cutoff.json
fixtures/acceptance/traces/a19_drain_cutoff.csv
```

Run `nearest_car`.

Drain allowance is intentionally insufficient.

Verify unfinished passengers become UNSERVED exactly at `duration + max_drain`, not earlier.

## 21. A20 — Heterogeneous Cars / Service Ranges

Inputs:

```text
fixtures/acceptance/a20_heterogeneous.json
fixtures/acceptance/traces/a20_heterogeneous.csv
```

Run `compare`; all seven configured algorithms are part of A20 acceptance.

Cars differ in speed, capacity, and service ranges; trace includes a same-floor/direction queue whose passengers require different compatible cars.

Verify:

- no car boards a passenger it cannot directly serve;
- conventional residual hall demand remains after partial serviceability pickup;
- service-range skip is not counted as full bypass;
- zoning immediate fallback handles nominal zone owner ineligibility;
- ETA reacts to speed differences.

## 22. A21 — Chinese UTF-8 Labels

Inputs:

```text
fixtures/acceptance/a21_chinese.json
fixtures/acceptance/a21_chinese.yaml
fixtures/acceptance/traces/a21_chinese.csv
```

Verify both configs validate and preserve labels such as:

```text
台北辦公大樓早高峰
午餐跨樓層
電梯A
```

Run `eta_cost` and verify valid UTF-8 appears uncorrupted in relevant manifest/event/report fields.

## 23. A22 — Fixed Parser/Schema Negative Batch

Use entries in:

```text
fixtures/invalid/expected.csv
```

whose `kind` is JSON/YAML/config.

First validate the positive parser-depth probes:

```text
fixtures/acceptance/parser_depth_128.json
fixtures/acceptance/parser_depth_128_yaml.yaml
```

Both MUST validate successfully. The paired `json_depth_129.json` and `yaml_depth_129.yaml` entries in `expected.csv` MUST fail with syntax/resource class 3.

Every listed negative case MUST fail with its expected broad exit class. No crash/hang/success is acceptable. The batch includes `config_burst_total_over_cap.json`, which MUST reject with semantic exit 4 before generation/PRNG consumption.

## 24. A23 — Fixed Trace Negative Batch

Use:

```text
fixtures/acceptance/a23_trace_validation_base.json
fixtures/invalid/expected.csv
```

for all trace-kind entries.

Invoke `replay ... --trace <invalid-file>` or the action specified in the expected manifest.

Cases include header, ID, ordering, floor, same-floor, serviceability, tick-alignment, horizon, UTF-8, and CSV quoting errors.

Every case must fail before simulation.

## 25. A24 — Fixed 100,000-Passenger Office-Day Stress

Inputs:

```text
fixtures/acceptance/a24_stress_office_day.json
fixtures/acceptance/traces/a24_office_100k.csv
```

Properties are fixed:

- 100 floors;
- 16 cars;
- exactly 100,000 explicit passengers;
- mixed morning/interfloor/evening structure;
- all seven algorithms through `compare`.

PASS requires:

- all seven runs complete without invariant/deadlock/resource corruption;
- all manifests share fingerprint/count;
- completed/unserved accounting valid;
- comparison generated;
- no NaN/Infinity/malformed output.

No wall-clock threshold is imposed.

## 26. A25 — Triple-Run Determinism

Use A09 and execute `compare` three independent times to distinct prefixes.

Verify every corresponding canonical file is byte-identical across the three runs.

No nondeterministic metadata may be embedded.

## 27. Required Acceptance Evidence

Submission MUST contain reproducible evidence with at least:

- acceptance and fixed-negative-corpus SHA-256 verification statement;
- automated mandatory-test summary;
- A01-A25 pass/fail table;
- one representative A09 comparison text/CSV;
- A09 common fingerprint;
- A16 replay identity result;
- A24 stress summary;
- A25 byte-comparison result;
- failed/unexecuted gate list, if any.

Huge raw A24 passenger/sample/event output need not be bundled if submission size is impractical, but the fixed input, exact invocation, fingerprint, summary, and test/evidence result MUST be included.

## 28. Manual Checklist

- [ ] Valid JSON validates.
- [ ] Equivalent YAML validates.
- [ ] Fixed invalid JSON/YAML batch fails in expected classes.
- [ ] Fixed invalid trace batch fails before simulation.
- [ ] PCG vectors pass.
- [ ] Generated burst count/bounds pass.
- [ ] Global 2,000,000-passenger trace ceiling rejects deterministic/runtime/import overflow without truncation or partial-success output.
- [ ] Same build+seed config generates same trace.
- [ ] Run/replay use flat output-prefix filenames; product creates no directory.
- [ ] Triangular and trapezoidal kinematics are both observed.
- [ ] Active movement target never changes mid-leg.
- [ ] Door/transfer timing and zero-duration closure pass.
- [ ] Multiple transfer lanes overlap as required.
- [ ] Full-car residual queue/bypass counting is correct.
- [ ] Mixed service-range queue preserves unsupported residual passengers.
- [ ] All seven required algorithms pass focused tests.
- [ ] A06 nearest-vs-ETA divergence is observed.
- [ ] A08 zoning overflow is observed.
- [ ] A09/A10/A11 adaptive modes are detected correctly.
- [ ] A13 destination assignments exist before boarding and allow multi-car groups at same hall.
- [ ] A14 starvation threshold/service promotion works.
- [ ] Required percentile/SLA/Gini/utilization/energy metrics pass exact tests.
- [ ] A16 replay simulation outputs reproduce.
- [ ] A18 hard-stop SLA edge behavior is correct.
- [ ] A24 seven-algorithm 100k stress completes.
- [ ] A25 canonical files are deterministic.
- [ ] Automated mandatory tests return PASS.
