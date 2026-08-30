# 08 — Output Files and Canonical Formats

## 1. Flat Output-Prefix Contract

All runtime output is regular-file based.

For `run`, `compare`, and `replay`, `--out P` supplies an **opaque filename prefix** `P`, not a directory.

The program MUST NOT create directories.

If `P` contains a parent path, that parent must already exist.

The executable forms filenames only by appending the exact suffixes in this specification to the supplied prefix. It MUST NOT enumerate or recursively modify the parent directory.

Example:

```text
--out results/a01
```

creates files such as:

```text
results/a01.summary.json
results/a01.events.log
```

## 2. Single-Algorithm `run` / `replay` Files

For prefix `P`, acceptance output is:

```text
P.manifest.json
P.summary.json
P.summary.txt
P.passengers.csv
P.elevator_samples.csv
P.events.log
P.wait_histogram.txt
P.trace.csv
P.trace_info.json
```

`manifest`, canonical trace, and trace-info are always required on a successful run/replay.

The remaining files are required when their corresponding output boolean is true. Task-pack acceptance configs enable every required boolean, so all listed files exist during release-gate evaluation.

Imported traces MUST also be canonicalized and written to `P.trace.csv`; this is no longer optional.

## 3. Compare Files

For compare prefix `P`, common files are:

```text
P.trace.csv
P.trace_info.json
P.comparison.csv
P.comparison.txt
```

For each algorithm ID `A` in `default_compare`, write:

```text
P.A.manifest.json
P.A.summary.json
P.A.summary.txt
P.A.passengers.csv
P.A.elevator_samples.csv
P.A.events.log
P.A.wait_histogram.txt
```

Acceptance default comparison therefore produces seven algorithm-specific file groups but **no subdirectories**. `P.comparison.txt` is unconditionally required for every successful `compare`; `output.comparison_csv` controls only `P.comparison.csv`. There is no schema-1.0 boolean that disables the text comparison report.

Per-algorithm trace files are intentionally omitted; all child manifests reference the common fingerprint and common `P.trace.csv`.

## 4. Canonical Text Encoding

All generated text files:

- are UTF-8;
- use LF byte `0x0A` line endings;
- end with one final LF unless the exact format says otherwise;
- never depend on process locale for decimal separators;
- contain no UTF-8 BOM.

Acceptance `line_ending` is `lf`.

## 5. CSV Rules

CSV output uses:

- comma delimiter;
- one header row;
- LF line endings;
- RFC 4180-style quoting.

A field is quoted when it contains comma, double quote, CR, or LF.

Inside a quoted field, `"` is represented as doubled `""`.

Numeric fields use ASCII decimal digits and `.` as decimal point with no thousands separator.

Empty/unknown optional values are empty fields, not literal `null`.

## 6. Passenger CSV

Header exactly:

```text
passenger_id,arrival_us,origin_floor,destination_floor,direction,assigned_elevator,boarding_start_us,fully_onboard_us,alighting_start_us,completion_us,waiting_s,ride_s,total_journey_s,final_wait_age_s,bypassed_full_count,outcome
```

Rules:

- rows sorted ascending passenger ID;
- `assigned_elevator` is the car for which passenger entered `BOARDING`; empty if no `BOARD_START` occurred;
- event timestamps are filled only if that event occurred;
- `waiting_s` exists if `fully_onboard_us` exists, even if passenger later becomes UNSERVED;
- `ride_s` exists if `alighting_start_us` exists;
- `total_journey_s` exists only for COMPLETED passenger;
- `final_wait_age_s` is empty for COMPLETED passengers and for unfinished passengers equals SLA observed wait at run end from `07_METRICS_AND_ENERGY.md`;
- `outcome` is exactly `COMPLETED` or `UNSERVED` at final output.

Direction is `UP` or `DOWN`.

## 7. Elevator Samples CSV

Header exactly:

```text
time_us,elevator_id,position_m,nearest_floor,speed_mps,direction,motion_state,door_state,onboard_load,reserved_occupancy,capacity,next_target_floor,route_stop_count,mode
```

Rules:

- rows sorted by `time_us`, then scenario car index;
- direction is `UP`, `DOWN`, or `IDLE`;
- `next_target_floor` empty when none;
- `mode` is adaptive mode only for `adaptive_peak`, otherwise empty;
- sample timing and nearest-floor tiebreak follow `07_METRICS_AND_ENERGY.md`.

## 8. Event Log Syntax

Each line:

```text
<time_us> <EVENT_TYPE> key=value key=value ...
```

`time_us` is unsigned decimal microseconds.

Keys use ASCII identifier characters.

String values that contain whitespace, `=`, quote, backslash, CR, or LF MUST be JSON-style quoted strings. Numeric IDs/timestamps/floors may remain unquoted.

Required event types:

- `PASSENGER_ARRIVE`
- `HALL_CALL_ACTIVE`
- `HALL_CALL_CLEAR`
- `CALL_ASSIGN`
- `CALL_REASSIGN`
- `PASSENGER_ASSIGN`
- `CAR_START`
- `CAR_ARRIVE`
- `DOOR_OPEN_START`
- `DOOR_OPEN`
- `BOARD_START`
- `BOARD_DONE`
- `ALIGHT_START`
- `ALIGHT_DONE`
- `DOOR_CLOSE_START`
- `DOOR_CLOSED`
- `CAR_FULL_BYPASS`
- `DIRECTION_REVERSE`
- `STARVATION_URGENT`
- `ADAPTIVE_MODE`
- `RUN_DRAIN_START`
- `RUN_END`

Minimum identifying fields:

```text
PASSENGER_ARRIVE passenger_id origin destination segment
HALL_CALL_ACTIVE floor direction oldest_passenger_id
HALL_CALL_CLEAR floor direction
CALL_ASSIGN kind car_id floor direction
CALL_REASSIGN kind old_car_id new_car_id floor direction
PASSENGER_ASSIGN passenger_id group_id car_id floor destination
CAR_START car_id from_floor target_floor direction
CAR_ARRIVE car_id floor
DOOR_OPEN_START car_id floor
DOOR_OPEN car_id floor
BOARD_START passenger_id car_id floor
BOARD_DONE passenger_id car_id floor
ALIGHT_START passenger_id car_id floor
ALIGHT_DONE passenger_id car_id floor
DOOR_CLOSE_START car_id floor
DOOR_CLOSED car_id floor
CAR_FULL_BYPASS passenger_id car_id floor
DIRECTION_REVERSE car_id from_direction to_direction
STARVATION_URGENT floor direction oldest_passenger_id
ADAPTIVE_MODE old_mode new_mode
RUN_DRAIN_START
RUN_END status completed unserved
```

`PASSENGER_ASSIGN` is mandatory for destination-control pre-boarding assignment and MUST occur before that passenger's `BOARD_START`.

`CALL_ASSIGN kind=hall` is used for conventional hall ownership. Destination-control may use `kind=group` in addition to per-passenger assignment lines.

Same-time line ordering follows `03_SIMULATION_ENGINE.md`.

## 9. Manifest JSON

Top-level required keys in this canonical order:

```text
schema_version
scenario_name
run_kind
algorithm_id
seed
trace_fingerprint
simulation_tick_ms
configured_duration_s
actual_elapsed_s
end_mode
passenger_count
completed_count
unserved_count
status
```

Values:

- `schema_version`: `"1.0"`;
- `run_kind`: `run`, `replay`, or `compare_child`;
- `status`: `success` only when simulation and required output generation completed with no invariant/runtime error.

No wall-clock timestamp, process ID, host name, absolute temp path, or memory address may appear in canonical manifest content.

If optional non-canonical metadata is added, it MUST be in a separate file not used by determinism gates.

## 10. Summary JSON

Top-level sections in this canonical order:

```text
run
passengers
waiting_time
ride_time
journey_time
sla
fairness
throughput
elevators
movement
energy
```

Every required metric in `07_METRICS_AND_ENERGY.md` MUST appear under an unambiguous key.

Per-car arrays follow scenario elevator order.

Unavailable numeric distribution values are JSON `null`.

JSON output MUST be generated by the C implementation itself and MUST be syntactically valid.

Within one build, key ordering and formatting MUST be stable enough for byte-identical repeated canonical output.

## 11. Summary Text

Minimum sections:

- Scenario / Run
- Algorithm
- Demand / Completion
- Waiting Time
- Ride Time
- Journey Time
- SLA / Fairness
- Elevator Utilization
- Movement / Load
- Simplified Energy Score
- Warnings / Status

The report MUST visibly label the energy quantity as a simplified comparative score.

If there are no warnings, print an explicit `Warnings: NONE` or equivalent deterministic statement.

## 12. Comparison CSV

Header exactly:

```text
algorithm_id,passengers,completed,unserved,mean_wait_s,p50_wait_s,p90_wait_s,p95_wait_s,p99_wait_s,max_wait_s,mean_journey_s,p95_journey_s,sla_violations,sla_violation_pct,starvation_triggers,gini_wait,throughput_per_hour,total_distance_m,empty_distance_m,door_cycles,direction_reversals,energy_score
```

Rows follow `algorithms.default_compare` order.

Every value MUST match the corresponding child summary after canonical rounding.

Unavailable scalar distribution values are empty CSV fields.

## 13. Comparison Text

Must include:

- common trace fingerprint;
- common passenger count;
- one row per configured algorithm;
- headline waiting/SLA/throughput/movement/energy values;
- per-metric winner/tie results required by `07_METRICS_AND_ENERGY.md`;
- statement that no hidden universal composite score is implied.

If compare is incomplete because an algorithm failed, the file (if written for diagnostics) MUST begin with a visible `STATUS: FAILED` and identify failed algorithm. It MUST NOT resemble a successful complete comparison.

## 14. Trace Info JSON

Required keys in canonical order:

```text
seed
passenger_count
first_arrival_us
last_arrival_us
trace_fingerprint
source_mode
```

Rules:

- source mode is `generated` or `trace`;
- for zero passengers, first/last arrival are JSON `null`;
- fingerprint is over exact canonical `trace.csv` bytes.

The same schema is used for `<trace.csv>.info.json` from `generate` and `P.trace_info.json` from run/compare/replay.

## 15. Floating Numeric Formatting

Unless field is explicitly integer:

- seconds in CSV/text headline metrics: exactly 3 fractional digits;
- distance meters: exactly 3 fractional digits;
- percentages: exactly 3 fractional digits;
- Gini: exactly 6 fractional digits;
- energy score/components: exactly 3 fractional digits;
- sampled position/speed: exactly 6 fractional digits in `elevator_samples.csv`.

JSON numeric values may omit trailing zeros but MUST round to the same precision as the corresponding canonical metric type.

Required decimal rounding is round-to-nearest; the implementation MUST document how exact halfway binary cases are handled. Tests use tolerance where a mathematically exact decimal half is not representable in binary.

Negative zero MUST be normalized to `0`/`0.000`/`0.000000` as appropriate.

## 16. Canonical File Determinism

Under same build/config/trace/algorithm, these successful-run files MUST be byte-identical across repeated runs when enabled:

- manifest JSON;
- summary JSON;
- summary text;
- passengers CSV;
- elevator samples CSV;
- events log;
- histogram text;
- canonical trace CSV;
- trace info JSON;
- comparison CSV/text for compare.

No file may contain nondeterministic runtime metadata.

## 17. Output Collision and Failure Semantics

Before simulation, the program MUST derive the complete set of enabled output filenames for the requested operation.

Without `--force`, collision protection uses ISO C17 exclusive-create mode:

1. reserve **every** target before simulation by opening it with `fopen(path, "wbx")` (binary write + exclusive create) and immediately closing it;
2. if any reservation open/close fails, call standard `remove()` on every target successfully reserved by this invocation, report exit 7, and do not start simulation/generation;
3. after all reservations succeed, those exact zero-length files belong to this invocation and may later be reopened with `"wb"` for canonical output;
4. if simulation/generation fails before successful output completion, attempt to remove still-placeholder/partial files created by this invocation and never report success.

An implementation MUST NOT replace `"wbx"` with a racy `fopen("rb")` existence probe followed by ordinary `"wb"`. The `x` mode is part of the ISO C17 dependency boundary for this task.

With `--force`:

- reservation by exclusive create is not required;
- overwrite only the exact known target filenames for this invocation using standard C file I/O;
- do not delete unrelated files;
- never recursively remove paths.

If a required output fails to open/write/flush/close successfully:

- overall command exits output-I/O failure;
- it MUST NOT print/report success;
- it MUST attempt `remove()` for any product target that this invocation newly reserved/created and that cannot be proven complete;
- any diagnostic comparison/manifest intentionally retained must visibly indicate failure if it could otherwise be mistaken for success.

Using `rename()`/temporary files for stronger commit behavior is permitted but not mandatory because cross-platform replacement semantics differ even though `rename` is in ISO C.
