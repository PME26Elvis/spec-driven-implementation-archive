# 11 — Mandatory Test Plan

## 1. Test Requirement

Submission MUST include automated tests executable without a third-party test framework.

A custom C17 test harness is expected.

Mandatory test behavior:

- print stable test identifiers;
- count pass/fail;
- print enough expected/actual information for failures;
- exit nonzero if any mandatory test fails;
- no required test is disabled/skipped in release configuration.

Developer convenience wrappers may invoke the test executable, but assertions themselves must exist in C17 mandatory test code.

## 2. Test Categories

Required categories:

1. JSON parser;
2. YAML parser;
3. UTF-8/schema validation;
4. PCG/samplers;
5. generated traffic profiles;
6. trace CSV/fingerprint;
7. analytic kinematics;
8. door/transfer/capacity;
9. passenger/hall lifecycle;
10. each dispatch algorithm;
11. starvation/adaptive behavior;
12. metrics/energy;
13. output schemas/prefix behavior;
14. replay/compare/determinism;
15. negative corpus;
16. stress/large input;
17. fixed acceptance scenarios.

## 3. JSON Parser Unit Tests

At minimum:

- empty object/array;
- nested object/array;
- all scalar types;
- every required escape;
- direct UTF-8 Chinese text;
- BMP `\uXXXX` decoding;
- surrogate pair decoding;
- unpaired surrogate rejection;
- unescaped control rejection;
- duplicate decoded key rejection;
- trailing comma rejection;
- comment rejection;
- invalid escape;
- truncated string/object/array;
- malformed number forms (`01`, `1.`, `.1`, exponent missing digits, etc.);
- exact uint64 max token preservation;
- uint64 overflow rejection;
- exponent conversion that would become Infinity rejected;
- malformed UTF-8 rejection;
- depth 128 success;
- depth 129 controlled rejection.

## 4. YAML Parser Unit Tests

At minimum:

- nested block mapping;
- nested block sequence;
- sequence of mappings;
- mapping containing sequence;
- `- id: value` form;
- plain/single/double quoted strings;
- doubled single quote;
- every required double-quote escape;
- blank lines/comments;
- comment after scalar;
- quoted hash;
- plain `a#b` retains hash;
- arbitrary consistent indentation increase;
- required indentless direct-child block sequence form used by fixed YAML fixtures;
- `key:`/bare `-` without a required child rejects rather than becoming implicit null;
- inconsistent dedent rejection;
- tab indentation rejection;
- booleans/null/numeric scalars;
- `yes/no/on/off` remain strings;
- date-looking scalar remains string;
- flow scalar array/map and empty `[]`/`{}`;
- nested flow container rejection;
- duplicate decoded key rejection;
- anchor/alias/tag/merge/directive/document-marker/block-scalar/complex-key rejection;
- malformed UTF-8;
- depth limits.

## 5. JSON/YAML Equivalence

Use task-pack fixture pair:

```text
fixtures/equivalence/office_small.json
fixtures/equivalence/office_small.yaml
```

Verify:

1. both validate;
2. typed semantic model is field-for-field equal after defaults;
3. generated canonical trace is byte-identical under same build;
4. trace fingerprint identical;
5. `eta_cost` simulation-derived outputs identical;
6. no parser-specific metadata reaches canonical reports.

## 6. Semantic Validation Tests

Create direct validator tests for every range/cross-field rule in `06_CONFIG_JSON_YAML.md`, including:

- tick divisor requirement;
- service range/initial floor;
- generated profile required/forbidden fields;
- burst count/window/sigma;
- mixed fractions;
- weight/matrix dimensions/diagonal/serviceability;
- unknown fields;
- all algorithm ranges;
- output sample interval;
- invalid percentile order/duplicates;
- impossible direct-serviceability traffic;
- guaranteed sum of deterministic burst counts above 2,000,000 rejects before any PRNG consumption.

At least one test MUST prove string numeric values are not coerced.

## 7. PCG32 Exact Tests

Hard-code expected vectors from `04_TRAFFIC_GENERATION.md` for seeds 0 and 42.

Verify at least first ten outputs each.

Expected values MUST NOT be computed by the PCG implementation under test.

Also test uint64 max seed initialization deterministically.

## 8. Uniform Primitive Tests

Verify:

- uniform real always `0 <= U < 1`;
- integer bound 1 always returns 0;
- non-power-of-two bound such as 10 exercises rejection path;
- result never outside bound;
- Bernoulli p=0/p=1 consumes no PRNG draw (compare subsequent PCG state/output);
- weighted zero bucket never selected;
- all-zero weights reject.

## 9. Sampler Sanity Tests

Fixed seed/sample count/tolerance committed in tests:

- weighted categorical approximate ordering/proportion;
- exponential nonnegative and mean tolerance;
- Poisson nonnegative integer and mean tolerance;
- normal mean/variance tolerance;
- Box-Muller cached-second behavior deterministic;
- truncated normal bounds;
- pathological truncated acceptance hits defensive failure rather than infinite loop where an internal test can force it.

No nondeterministic p-value gate.

## 10. Traffic Arrival Process Tests

For non-burst profile:

- rate 0 emits zero and consumes no exponential draws;
- raw generated times obey half-open segment rule before quantization;
- canonical times are tick-aligned;
- adjacent segments allowed with overlap disabled;
- true overlap rejected unless `allow_overlap=true`;
- overlapping segment merge tiebreak by arrival, declaration index, local sequence;
- final IDs contiguous after merge.

## 11. Profile Tests

At minimum one deterministic test each:

- `up_peak`: p=1 makes every origin lobby and destinations non-lobby;
- `down_peak`: p=1 makes every destination lobby and origins non-lobby;
- `interfloor`: origin never equals destination;
- `mixed`: class fractions route through correct class branches using controlled sampler/internal fixture;
- OD matrix: zero-weight pair never appears and scan order deterministic;
- serviceability filtering never emits impossible OD;
- `burst uniform_window`: exact `low + U*(high-low)` draw consumption, microsecond/tick quantization, exact configured count and bounds;
- `burst normal_window`: emits exactly count, applies the same timestamp quantization, and never leaves bounds;
- burst sorting uses `(arrival_us, local_sequence)` and not raw floating timestamp;
- zero-probability traffic branches do not require a positive OD pair, while a positive-probability impossible branch rejects before PRNG consumption;
- generated total attempting passenger 2,000,001 fails exit 8 without committing partial trace/info output (an internal controlled generator/source seam may be used so the mandatory test need not materialize 2,000,001 ordinary Poisson draws).

## 12. Trace CSV Tests

Using valid and fixed invalid fixtures:

- exact header accepted;
- canonical export LF;
- CRLF input canonicalizes to identical LF bytes/fingerprint;
- quoted comma and doubled quote supported;
- empty segment ID rejected;
- duplicate/gap/nonpositive IDs rejected;
- malformed quoting rejected;
- unsorted time rejected;
- non-tick-aligned arrival rejected;
- arrival beyond duration rejected;
- wrong/same/unserviceable floors rejected;
- imported row 2,000,001 is rejected as trace semantic error 5 (a streaming/count-focused internal test may establish the boundary without checking in a 2,000,001-row fixture);
- import -> canonical export -> reimport preserves every passenger;
- fingerprint exact against independent FNV fixture value for at least one small trace.

## 13. Kinematic K1 — Triangular One-Floor Leg

Choose geometry/parameters satisfying:

```text
D < v_max^2/(2a) + v_max^2/(2d)
```

Independently compute expected `v_peak`, continuous T, and tick-ceiling arrival.

Verify:

- profile contains ACCELERATING and DECELERATING;
- no positive-duration CRUISING phase;
- peak <= v_max;
- sampled positions monotonic and bounded;
- final position exact target;
- final speed zero;
- `CAR_ARRIVE` timestamp equals first tick >= analytic T;
- accumulated leg distance equals D within tolerance.

## 14. Kinematic K2 — Trapezoidal Long Leg

Choose D requiring cruise.

Verify independent expected:

- acceleration time/distance;
- cruise duration >0;
- deceleration time;
- continuous T and tick-ceiling arrival;
- sampled sequence includes ACCELERATING, CRUISING, DECELERATING;
- max speed never exceeds configured value;
- final distance exact.

## 15. Kinematic K3 — Immutable Active Leg

Start car on a long active leg.

Inject an intermediate hall request after `CAR_START`.

Verify:

- active target unchanged;
- no `CAR_ARRIVE` at new intermediate floor on that leg;
- request remains serviceable later/elsewhere;
- future stop may be inserted only after active target.

## 16. Kinematic K4 — Asymmetric Accel/Decel

Use `a != d`.

Verify `v_peak`/trapezoid formulas use both independently and are not replaced by one shared acceleration constant.

## 17. Door D1 — Positive Durations

At one service stop verify exact quantized timestamps:

- OPENING duration;
- OPEN event;
- minimum dwell;
- CLOSING duration;
- movement never overlaps non-CLOSED door interval;
- one physical service stop creates one door cycle.

## 18. Door D2 — Zero-Duration Closure

Set open/dwell/close = 0 with finite passenger transfer setup.

Verify same-timestamp state closure has correct event order and no infinite loop.

Then test all door and transfer durations zero with a finite passenger batch; all logically possible transitions settle at same timestamp without arbitrary iteration failure.

## 19. Transfer T1 — One Lane

One boarding lane, three passengers, nonzero transfer duration.

Verify BOARD_START/BOARD_DONE serialize by FIFO and each completion gap equals quantized duration.

## 20. Transfer T2 — Multiple Lanes

Two boarding lanes, four passengers.

Verify first pair starts together and second pair starts together at first lane-release boundary when capacity permits.

## 21. Transfer T3 — Concurrent Exit/Entry

Two alighting and two boarding lanes.

Verify:

- alighting starts before boarding at same boundary;
- capacity freed by ALIGHT_START is reusable by BOARD_START;
- active transfer windows overlap;
- reserved occupancy never exceeds capacity.

## 22. Capacity C1 — Residual Queue and Bypass

Capacity 2, five compatible waiters.

For one full service episode verify:

- at most two reserve boarding;
- blocked eligible passengers each increment bypass exactly once for episode;
- repeated open ticks do not re-increment same episode;
- hall call remains active;
- all five eventually complete in sufficient drain.

## 23. Service Range C2 — Mixed Destinations

One hall queue contains destination groups serviceable by different cars.

Verify conventional car boards only its directly serviceable FIFO-compatible passengers; residual queue persists; service-range skip is not full bypass.

## 24. Passenger State / FIFO Test

For passengers with distinct/equal arrival times verify legal state sequence and queue order.

No passenger may:

- occupy two ownership contexts;
- skip BOARDING/ONBOARD/ALIGHTING for a normal completed trip;
- board at wrong origin;
- alight at wrong destination.

## 25. Hall Request Lifecycle

Verify:

- first waiter activates;
- added waiter does not duplicate conventional hall request;
- initial owner stays sticky across ordinary later ticks even if another car becomes slightly cheaper;
- residual-request ownership releases exactly at `DOOR_CLOSE_START`, and a subsequent owner in the same continuous hall episode is logged as `CALL_REASSIGN`, not a fresh `CALL_ASSIGN`;
- BOARD_START removes passenger from WAITING ownership;
- partial pickup leaves hall active;
- owner is released when it can no longer serve any remaining WAITING passenger or after a residual full/partial service episode as defined by the domain model;
- final waiter leaving WAITING clears hall;
- activation after later new demand forms a new episode.

## 26. Direction Reversal Test

Drive sequence:

```text
UP movement -> idle interval -> DOWN movement
```

Verify one reversal at second `CAR_START`.

Then `UP -> idle -> UP` gives no reversal; first movement gives no reversal.

## 27. `nearest_car` Test

Construct equal geometric candidates and independently verify tiebreak order:

1. load ratio;
2. route stop count;
3. car index.

Then verify away/opposite penalties alter score exactly as configured.

Ensure passenger destination does not change car ranking except hard serviceability.

## 28. `directional_collective` Test

UP car has UP work above and DOWN demand at intermediate floor.

Verify DOWN demand is not ordinary-picked before turnaround.

Verify assignment class ordering favors matching sweep over idle/future-sweep class even where raw geometric distance alone would differ.

## 29. `scan_look` Test

Highest required floor below terminal.

Verify no terminal travel.

Create two cars where incremental LOOK route distance differs from directional preference and verify minimum incremental route-distance rule.

## 30. `eta_cost` Test

Car A is geometrically nearer but has door/transfer/mandatory stops; car B farther idle.

Independently compute/inspect frozen-demand predictor components and verify B wins when configured fixture says so. Also include a focused test proving that changing only a not-yet-boarded conventional passenger's hidden destination (while preserving direct serviceability) cannot change a conventional car score/route before `BOARD_DONE`; `destination_control` is the explicit exception.

Also verify:

- request age enters as negative credit;
- coalesced physical stop adds zero stop count;
- deterministic tiebreak sequence;
- changing only destination of a conventional hall passenger does not influence score except serviceability.

## 31. `zoning` Test

Verify exact static-equal zone partition/remainder.

Cases:

- primary zone car chosen before overflow;
- primary service-range ineligible -> immediate fallback;
- request age reaching overflow threshold -> all-car ETA fallback;
- more cars than non-lobby floors handles empty zones;
- lobby globally unzoned.

## 32. `adaptive_peak` Test

Use explicit deterministic trace.

Verify exact historical window `(t-window,t]` counts and mode precedence.

Cases:

- insufficient rate -> BALANCED;
- UP threshold -> UP_PEAK;
- DOWN -> DOWN_PEAK;
- INTER -> INTERFLOOR;
- candidate change before minimum hold does not switch;
- eligible change after hold switches exactly once/event;
- up-peak lobby reserve count uses ceiling;
- staging movement contributes energy/distance but does not open useless doors.

## 33. `destination_control` Test

At one lobby-direction queue use low/high destination clusters and >=2 cars.

Verify:

- groups are seeded/scanned in FIFO order;
- route-span/max-size rule exact;
- no group contains passenger with no common serving car;
- multiple groups at same hall direction may own different cars simultaneously;
- every `PASSENGER_ASSIGN` precedes BOARD_START;
- only assigned passengers board arriving car;
- no passenger has two tentative owners.

## 34. Starvation Test

Sustain newer traffic around an older isolated request.

Verify:

- urgent activation at first boundary age >= threshold;
- one urgent event for episode;
- urgent work precedes nonurgent optimization;
- commitment is earliest uncommitted feasible pickup after active leg/onboard obligations;
- eventual service under sufficient drain.

## 35. Exact Metric Test

Use direct synthetic completed timestamps giving waits `[0,10,20,30,40]` seconds.

Verify independently:

- mean;
- population standard deviation;
- P50/P90/P95/P99 nearest-rank;
- min/max;
- Gini;
- histogram boundary;
- SLA strict `>` boundary.

Also test N=0 and N=1 rules.

## 36. Unserved SLA Test

Hard-stop fixture with:

- one unserved passenger arrived >SLA ago;
- one unserved passenger arrived <SLA ago;
- one onboard-but-unfinished passenger with known boarding wait.

Verify all-arrived SLA rule counts only actual observed waits > threshold.

## 37. Utilization / Movement Exact Test

Use known small run and verify:

- category microseconds sum to run elapsed;
- zero-duration transitions consume none;
- empty+loaded distance=total;
- startup count equals movement legs;
- physical stops vs staging arrivals distinct;
- mean moving load uses time-weighted movement intervals.

## 38. Energy Exact Test

Feed independently known movement/utilization/passenger-meter counters from a deterministic simulator fixture and compute expected component totals in test code using explicit constants.

Verify every component and total.

Do not compare a value to another call of the same energy function under test.

## 39. Output Schema Test

Run a small acceptance fixture and verify:

- exact required filenames from prefix;
- exact CSV headers;
- valid JSON parsing by the project's own JSON parser or an independent minimal test reader;
- LF-only output;
- final LF;
- event minimum fields;
- negative zero absent;
- summary/comparison consistency.

The test MUST NOT require directory creation by product.

## 40. Output Collision Test

Create one target file before run.

Without `--force`, verify exit 7 before it is overwritten.

With `--force`, verify known target replaced while an unrelated neighboring file remains unchanged.

## 41. Replay Test

Generate/run scenario S with algorithm A.

Replay `P.trace.csv` with same scenario/algorithm to a new prefix.

Verify simulation-derived files match byte-for-byte:

- summary JSON/text;
- passengers CSV;
- elevator samples;
- events;
- histogram;
- trace/fingerprint.

Manifest differs only in normative `run_kind`; all other simulation fields equal.

## 42. Compare Same-Trace Test

Run all seven algorithms.

Verify:

- common trace generated/imported once logically;
- every child manifest has identical fingerprint/passenger count;
- child order follows `default_compare`;
- comparison rows match child summaries;
- no child reads previous result state.

## 43. Triple-Run Determinism

Execute same nontrivial acceptance input three times with distinct output prefixes.

Every canonical file listed in `08_OUTPUT_FORMATS.md` MUST be byte-identical between equivalent runs.

No pointer/hash/allocation ordering differences are tolerated.

## 44. Empty-Demand Integration

Verify all `10_ERRORS_EDGE_CASES.md` Section 4 empty-demand rules, including utilization, idle energy, null distributions, zero SLA %, and valid histogram.

## 45. Hard-Stop Integration

Demand cannot finish by duration.

Verify exact boundary handling, no interval after duration, incomplete -> UNSERVED, partial timestamps preserved, SLA rule correct.

## 46. Drain Integration

Same/similar demand under drain.

Verify service continues after duration and terminates either all-complete or exact max-drain boundary.

## 47. UTF-8 Integration

Use supplied Chinese-label fixture in JSON/YAML where provided.

Verify valid bytes preserve across parse -> trace/event/report labels and U+0000/CRLF identifier violations reject.

## 48. Fixed Negative Corpus

`fixtures/invalid/expected.csv` is normative. `fixtures/invalid/SHA256SUMS.txt` protects the complete fixed negative corpus.

Mandatory test runner MUST execute every listed invalid fixture/action and verify expected exit class.

The implementer may add more negative tests but may not remove/edit supplied entries when claiming completion.

## 49. Stress S1 — Fixed 100k Office Trace

Use task-pack A24 fixed explicit trace:

- 100 floors;
- 16 cars;
- exactly 100,000 passengers;
- all seven algorithms.

PASS requires:

- all child runs finish;
- no invariant/deadlock failure;
- accounting consistent;
- outputs well-formed;
- no NaN/Infinity;
- common fingerprint.

No wall-clock threshold is imposed.

## 50. Stress S2 — Capacity Burst

Use A12 for the fixed 200-passenger generated-burst acceptance. Separately, the implementation MUST include a deterministic generated-demand stress variant with at least 10,000 passengers if its ordinary test suite separates quick/full modes; this additional stress fixture is implementation-authored and MUST NOT replace A12 or A24.

The release acceptance must run the task-pack-defined required stress input(s), not silently downgrade counts.

## 51. Large Trace Parser Test

Generate or include a deterministic 1,000,000-row valid trace during test/evidence preparation and validate/import it under a compatible scenario.

At minimum importer/canonicalizer must handle it without integer/size overflow or corruption.

Full seven-algorithm simulation of one million rows is not a release gate; A24 100,000-row seven-policy compare is the mandatory simulation stress gate.

## 52. Acceptance Corpus Integrity

Before acceptance execution, verify both fixture manifests using reviewer tooling or the supplied evidence process:

- `fixtures/acceptance/SHA256SUMS.txt`;
- `fixtures/invalid/SHA256SUMS.txt`.

Every listed file MUST match before the fixed corpus is accepted as evidence.

The C product is not required to implement SHA-256.

Evidence MUST state that acceptance inputs were unmodified.

## 53. Test Reporting

At end print at least:

```text
mandatory_tests_passed=<N>
mandatory_tests_failed=<M>
status=PASS|FAIL
```

Each failure includes stable test ID.

Release gate G14 separately covers task-pack acceptance scenarios.

## 54. No Fake Tests

Non-compliant examples:

- `assert(true)`;
- comparing constants to themselves;
- computing expected output by calling the same function under test twice;
- silently skipping because feature unavailable;
- hard-coded PASS output;
- tests that only check command exit 0 when exact state/result behavior is required;
- replacing supplied fixture with implementer-authored easier one.
