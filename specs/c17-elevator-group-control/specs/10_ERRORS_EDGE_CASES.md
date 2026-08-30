# 10 — Error Handling and Edge Cases

## 1. Error Classes

Normative error classes/exit codes are in `09_CLI_AND_FILES.md`:

- CLI usage;
- config syntax/UTF-8/unsupported YAML;
- config semantic validation;
- trace validation;
- simulation invariant/deadlock;
- output I/O/collision;
- resource/internal.

Invalid input MUST fail before simulation mutates output state where practical.

## 2. Fail-Fast Principle

A simulation MUST NOT start when configuration or trace validation fails.

The validator may collect multiple independent errors, but at least one precise diagnostic and the correct broad error class are mandatory.

No malformed value may be silently replaced by a default.

## 3. Required Configuration Rejections

Reject at minimum:

- floors outside `[2,200]`;
- lobby outside building;
- non-positive/non-finite floor height;
- zero or >32 elevators;
- duplicate/empty/invalid elevator IDs;
- capacity outside `[1,100]`;
- non-positive/non-finite speed/acceleration/deceleration;
- negative or out-of-range door/transfer milliseconds;
- lane count outside `[1,100]`;
- invalid service range/initial floor;
- invalid `tick_ms`, including value not dividing 1000;
- duration outside `[1,604800]`;
- invalid end mode;
- invalid drain/deadlock/starvation ranges;
- duplicate/empty segment IDs;
- segment bounds outside simulation duration;
- overlapping generated segments when disabled;
- non-burst missing/negative/non-finite `rate_per_minute`;
- burst with `rate_per_minute`, missing count, invalid window, or invalid sigma;
- wrong-length/negative/non-finite floor weights;
- OD matrix wrong dimensions, nonzero diagonal, all-zero usable weights, or impossible OD serviceability;
- mixed fractions not summing to 1 within `1e-9`;
- unsupported/duplicate `default_compare` ID;
- invalid algorithm parameter type/range;
- invalid adaptive fraction/window/rate;
- invalid destination grouping size/span;
- negative/non-finite energy coefficient;
- histogram/percentile inconsistency;
- unknown fields by default;
- schema version other than string `1.0`;
- integer overflow/underflow;
- NaN/Infinity spellings or conversions.

## 4. Empty Demand

A valid generated scenario may produce zero passengers.

Run MUST succeed.

- trace/arrived/completed/unserved counts are zero;
- distribution metrics follow null/N/A rules;
- SLA percentage is zero;
- histogram counts/percentages are zero;
- utilization still covers elapsed time;
- idle-closed energy component may be nonzero.

No NaN/Infinity may appear.

## 5. Single Passenger

One passenger must complete normally when serviceable.

For one completed observation:

- population waiting standard deviation = 0;
- Gini = 0;
- percentile ranks select that observation.

## 6. Simultaneous Arrivals

Any number of passengers within supported resource limits may share one tick.

Injection order is passenger ID.

Queue order remains arrival timestamp then passenger ID.

No random/hash/pointer order may change outcome.

## 7. Passenger-Count Ceiling

The normative maximum canonical trace size is 2,000,000 passenger rows.

- An imported trace containing row 2,000,001 is trace semantic error 5.
- A generated configuration whose deterministic sum of `burst.count` values alone is greater than 2,000,000 is configuration semantic error 4 and MUST be rejected before PRNG consumption.
- A valid generated configuration whose stochastic Poisson arrivals cause the actual generated total to attempt passenger 2,000,001 MUST fail with exit 8.
- No above-limit case may succeed by truncating, dropping a segment, wrapping an ID, or committing a partial `trace.csv` / `.info.json`.

Implementations MAY stream imported validation or generated merging internally; the observable ceiling and error classes are unchanged.

## 8. Capacity Burst

For capacity 2 and eight simultaneous eligible waiters:

- at most two reserved occupants may begin boarding before capacity frees;
- residual waiters remain queued;
- hall call remains active while waiters remain;
- repeated service occurs in drain mode;
- bypass count increments once per eligible blocked passenger per physical full-car door-open episode, never once per tick;
- no passenger disappears.

## 9. Concurrent Transfer Lanes

With two boarding lanes and four eligible waiters, first two may start together when capacity permits; next pair starts only after lane release/capacity permits.

With independent alighting/boarding lanes, exit and entry transfers overlap according to `03_SIMULATION_ENGINE.md`.

The implementation MUST NOT hide all transfer work behind one serial global timer.

## 10. Capacity Freed by Alighting

At one boundary, `ALIGHT_START` capacity frees occur before `BOARD_START` capacity checks.

A newly freed slot may therefore be reserved by boarding at the same timestamp.

Reserved occupancy MUST never exceed capacity even transiently.

## 11. Opposite Direction Demand

A directional-collective UP sweep encountering only DOWN waiters at an intermediate floor while UP work remains above MUST NOT board those DOWN passengers as ordinary pickup.

If the car opens at that floor for an onboard destination, opposite-direction waiters still remain waiting until turnaround eligibility.

## 12. Duplicate Logical Stops

Many passengers may share one destination and a floor may simultaneously contain hall-pickup and onboard-drop reasons.

One car route MUST contain one physical stop per floor occurrence in that future route, with logical reasons coalesced.

Door cycle count is physical, not per logical request.

## 13. New Request During Active Motion Leg

The active movement-leg target is immutable.

If a new request appears at an intermediate floor, even one physically nearby, it MUST NOT cause target replacement or instantaneous braking.

The request remains pending for:

- another eligible car; or
- insertion after the active target in a future route/sweep.

This is required behavior, not merely an allowed simplification.

## 14. New Request at Current Open Floor

If passenger arrival occurs at a floor whose car doors are already OPEN:

- arrival is injected at the boundary;
- alighting starts first;
- hall state/dispatch refresh occurs;
- eligible boarding may start at that same timestamp on a free lane;
- door must remain open until newly started transfer completes and dwell constraints are met.

## 15. Passenger Arrives During Closing

Door reopening is not supported in v1.0.2.

A passenger arriving while door state is CLOSING remains waiting.

The current door completes closing. The same or another car may serve later through normal dispatch.

No boarding occurs during CLOSING.

## 16. Service-Range Mixed Queue

A single floor/direction queue may contain passengers whose destinations require different cars.

A conventional policy may assign the hall call to a car that can serve at least one waiter.

At pickup:

- that car boards only directly serviceable eligible waiters in FIFO-compatible order;
- unserviceable waiters remain queued;
- residual hall request remains active and may be reassigned after the service episode.

Skipping a passenger because destination is outside that car's range is not a full-capacity bypass.


## 16.1 Conventional Owner Stickiness

An already owned conventional hall call MUST NOT move to another car merely because scores change on later ticks. Test at least one case where a second car becomes marginally better after initial assignment but no release/overflow/starvation condition occurs; ownership remains unchanged.

Also test residual service after a full/partial door episode: when the serving car can no longer board remaining waiters in that physical episode, the residual hall request becomes unowned and can be freshly assigned without clearing/recreating passenger identities.

## 17. Destination-Control Multi-Car Hall

Destination control may assign two or more passenger groups from one `(floor,direction)` to different cars concurrently.

Each passenger has at most one tentative car/group assignment.

Conventional exclusive hall ownership invariant MUST NOT incorrectly reject this valid destination-control state.

## 18. Last Arrival at Duration Boundary

An imported/generated canonical arrival exactly at `duration_s * 1_000_000` is valid.

It is injected before hard-stop/drain handling at that same boundary.

An arrival greater than duration is trace-invalid under schema 1.0.

## 19. Drain Mode

At duration boundary:

- no future new demand exists;
- existing waiting/boarding/onboard/alighting passengers continue;
- dispatch remains active;
- actual elapsed time continues;
- run ends when all complete or max drain boundary is reached.

At drain cutoff, every incomplete passenger becomes UNSERVED.

## 20. Hard Stop

At duration boundary:

- complete/inject/dispatch/settle/invariant/sample phases for that timestamp occur;
- no next interval is advanced;
- every incomplete passenger becomes UNSERVED;
- partial timestamps that already occurred remain in passenger output;
- final SLA observed-wait rule applies.

## 21. Zero-Time Door and Transfer Components

Zero milliseconds are legal for door/transfer fields.

All immediately enabled zero-duration transitions resolve at the same timestamp.

The simulator MUST process large finite zero-time passenger batches without arbitrary small iteration limits or infinite loops.

Zero-duration transitions consume zero utilization time.

## 22. Direction Reversal Through Idle

If a car last moved UP, later remains IDLE, then next starts a DOWN leg, count one reversal and emit `DIRECTION_REVERSE` at the DOWN `CAR_START` boundary.

UP -> IDLE -> UP is not a reversal.

The first movement is not a reversal.

## 23. Burst Profile Boundary

All `burst` raw timestamps must remain inside configured closed burst window and segment interval before tick quantization.

Canonical visible timestamps may coincide at the same tick.

`uniform_window` with count N produces exactly N rows; `normal_window` also produces exactly N or fails generation after the defensive rejection bound.

## 24. Numeric Extremes

Guard against:

- seconds/milliseconds/microseconds conversion overflow;
- tick-count overflow;
- passenger ID overflow;
- allocation-size multiplication overflow;
- metric sum/count overflow;
- `size_t` overflow;
- invalid `sqrt/log` domain caused by malformed data;
- division by zero;
- NaN/Infinity propagation;
- uint64 seed precision loss.

A value that cannot be represented safely must be rejected/fail cleanly, never wrap silently.

## 25. Allocation Failure

Every required allocation result is checked.

On failure:

- no null dereference/use-after-failure;
- concise diagnostic;
- exit 8;
- no success status/evidence.

## 26. Output Failure and Collision

If any required target cannot be opened/written/closed:

- exit 7;
- do not claim success;
- diagnostic identifies logical target filename.

Without `--force`, pre-existing target files cause exit 7 before simulation.

With `--force`, only exact known target suffix files may be overwritten.

## 27. Parser Edge Cases

Mandatory coverage includes:

- empty/whitespace-only file;
- truncated JSON string;
- JSON valid nesting depth 128;
- depth 129 rejection;
- duplicate decoded JSON key;
- duplicate YAML key;
- YAML tab indentation;
- inconsistent dedent;
- YAML comment after scalar;
- quoted `#` preserved;
- `a#b` plain scalar preserves hash under subset rule;
- unsupported anchor/alias/tag/document marker/block scalar;
- Chinese UTF-8 direct text;
- JSON BMP escape and surrogate pair;
- unpaired surrogate;
- malformed UTF-8;
- uint64 seed maximum accepted exactly;
- seed overflow rejected;
- numeric overflow/exponent to Infinity rejected;
- invalid type string-vs-number not coerced.

## 28. CSV Trace Edge Cases

Mandatory coverage includes:

- exact header;
- quoted comma in segment ID;
- doubled quote in segment ID;
- CRLF source line endings accepted/canonicalized to LF;
- empty segment ID rejected;
- malformed quote rejected;
- duplicate/gap/nonpositive passenger ID rejected;
- unsorted arrival rejected;
- arrival not tick-aligned rejected;
- arrival beyond duration rejected;
- invalid/unsupported OD rejected.

Segment IDs in schema 1.0 MUST NOT contain CR or LF, so multiline CSV fields are not required for trace input.

## 29. No-Candidate Request

A request may temporarily have no feasible candidate because all serviceable cars are committed/full.

It remains active and is reconsidered.

A permanently impossible state caused by corruption/algorithm bug must eventually fail invariant/deadlock logic; it must not silently convert passengers to UNSERVED before run cutoff.

## 30. Starvation Boundary

At the first tick where oldest waiting age is >= `starvation_threshold_s`, urgency activates.

If threshold is integer seconds and tick divides one second, exact-threshold activation is representable.

Emit one urgent event per activation episode, not once per tick.

## 31. Adaptive Window Boundaries

Adaptive historical window is `(t-window_s, t]` exactly.

An arrival at `t-window_s` is excluded; one at `t` is included.

No mode classification occurs before `t >= window_s`.

Mode hold duration is measured from last actual mode-change timestamp, with initial BALANCED start at zero.

## 32. Deterministic Ties

Every equal-cost test MUST have an explicit tiebreak path.

Never use pointer address, allocation order, unstable sort order, hash bucket traversal, or unspecified floating comparison of NaN.

## 33. Unicode / Identifier Constraints

All textual fields require valid UTF-8 and MUST reject U+0000.

Identifiers used in filenames/log keys (`scenario.name` is a label, not a filename component) are never automatically used as output filenames except fixed ASCII algorithm IDs.

`elevator.id` and `traffic.segment.id` MUST reject CR and LF characters so CSV/event rows remain single logical records.

Chinese characters and ordinary spaces are valid.
