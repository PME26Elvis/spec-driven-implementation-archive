# 04 — Deterministic Passenger Traffic Generation

## 1. Overview

The simulator MUST support two demand sources:

- `generated`: synthesize a canonical passenger trace before simulation;
- `trace`: import an explicit canonicalizable passenger trace.

Every algorithm run in one `compare` invocation receives the same immutable logical trace.

Traffic generation is completed before any dispatch algorithm state is initialized.

## 2. Required PRNG — PCG32

Implement **PCG-XSH-RR 64/32 (PCG32)** from first principles.

State transition:

```text
oldstate = state
state = oldstate * 6364136223846793005 + inc    (mod 2^64)
xorshifted = ((oldstate >> 18) ^ oldstate) >> 27
rot = oldstate >> 59
output = rotr32(xorshifted, rot)
```

Initialization uses scenario seed as `initstate` and fixed `initseq = 54`:

```text
state = 0
inc = (54 << 1) | 1
pcg32_next()
state = state + initstate    (mod 2^64)
pcg32_next()
```

Required first ten outputs after seeding, lowercase hexadecimal:

```text
seed = 42
a15c02b7 7b47f409 ba1d3330 83d2f293 bfa4784b cbed606e bfc6a3ad 812fff6d e61f305a f9384b90

seed = 0
47c28b93 b98f6a27 7d3dcb1e f0761116 9cc33f5b be0e744d 5752c556 43369132 173d6c87 5af69b49
```

These vectors MUST be hard-coded independently in tests.

`rand()` and `random()` are prohibited as the simulation random source.

## 3. Random Stream Discipline

Generated traffic consumes one PCG32 stream in deterministic segment-declaration order.

For overlapping segments, each segment still draws from the same global stream while its own arrival sequence is generated in declaration order; generated passengers are merged afterward. Do not interleave random calls by simulated timestamp.

Dispatch algorithms MUST NOT consume this stream.

All seven required algorithms are deterministic and use no randomness.

## 4. Uniform Primitive Definitions

### 4.1 Uniform real

One PCG32 output `r` maps to:

```text
U = r / 4294967296.0
```

so `U` is in `[0,1)`.

### 4.2 Uniform integer without modulo bias

For an unsigned range of size `bound` where `1 <= bound <= 2^32`:

- if `bound == 2^32`, return one raw `pcg32_next()` value directly;
- otherwise `bound` is representable as `uint32_t` and use:

```text
threshold = (0u - bound) % bound      // uint32 arithmetic
repeat:
    r = pcg32_next()
until r >= threshold
result = r % bound
```

Equivalent overflow-safe unsigned arithmetic is acceptable. Using `r % bound` without rejection is non-compliant for a non-power-of-two bound.

## 5. Required Samplers

Hand-implement and test:

- uniform integer;
- uniform real `[0,1)`;
- Bernoulli;
- weighted categorical;
- exponential;
- Poisson count;
- normal via Box-Muller;
- truncated normal via rejection.

Bernoulli `p=0` and `p=1` MUST consume no random draw and return false/true respectively, so edge configurations do not unexpectedly shift the stream.

Weighted categorical:

- accepts finite non-negative weights;
- rejects all-zero eligible weights;
- scans categories in ascending array index;
- boundary ties select the first cumulative interval containing the sampled value under half-open interval semantics.

Box-Muller MUST use the polar-free two-uniform form and MUST cache the second normal variate for the next normal request. The cache is part of the generator state and is reset when PCG is initialized.

For Box-Muller only, obtain `U1` by repeatedly drawing the Section 4.1 uniform real until `U1 > 0`; zero draws are consumed and discarded. Obtain `U2` from the next ordinary Section 4.1 draw. Then:

```text
radius = sqrt(-2 * ln(U1))
theta = 2 * pi * U2
z0 = radius * cos(theta)
z1 = radius * sin(theta)
```

Use `pi = 3.141592653589793238462643383279502884` as the source constant rounded to the implementation's working floating type. Return `z0` and cache `z1`. This rule prevents `ln(0)` and fixes random-stream consumption semantics.

Truncated normal MUST reject samples outside its closed bounds and MUST fail cleanly after 1,000,000 consecutive rejected candidates rather than loop forever.

## 6. Poisson-Process Arrival Segments

All non-`burst` profiles use an exponential inter-arrival Poisson process.

For `rate_per_minute = R`:

```text
lambda = R / 60.0
```

If `R = 0`, the segment produces no passengers and consumes no exponential draws.

Otherwise initialize continuous generation time `g = start_s` and repeat:

```text
U = uniform_real()
delta = -ln(1 - U) / lambda
g = g + delta
if g >= end_s: stop
emit one raw arrival at g
```

The continuous segment interval is half-open `[start_s, end_s)`.

Convert raw `g` to integer microseconds using round-to-nearest microsecond, ties-to-even, then apply the arrival tick ceiling from `03_SIMULATION_ENGINE.md`. Because of tick quantization, a raw arrival before `end_s` may become visible exactly at `end_s` when that is a tick boundary.

Generated canonical `arrival_us` stored in the trace is the **visible tick timestamp**, not the unquantized raw timestamp.

## 7. Segment Bounds and Overlap

Every generated segment has integer `start_s` and `end_s` with:

```text
0 <= start_s < end_s <= simulation.duration_s
```

`traffic.allow_overlap` defaults to false.

When false, half-open intervals `[start_s,end_s)` MUST NOT overlap; adjacent segments such as `[0,600)` and `[600,1200)` are valid.

When true, segments are generated independently in declaration order and their rows are merged by:

1. `arrival_us`;
2. segment declaration index;
3. local generation sequence.

Final passenger IDs are assigned only after this merge.

## 8. Common OD Selection Override

Any traffic segment MAY contain `params.od_matrix`.

Representation is a `floors x floors` nested numeric array. Entry `[origin-1][destination-1]` is a non-negative finite weight.

Rules:

- every diagonal entry MUST be exactly zero;
- at least one off-diagonal entry MUST be positive;
- any OD pair not directly serviceable by at least one car MUST have zero weight;
- when present, `od_matrix` overrides all profile-specific origin/destination sampling fields for that segment.

OD selection scans matrix rows then columns in ascending floor order for deterministic weighted categorical selection.

## 9. Floor Weight Arrays

A floor weight array contains exactly `building.floors` finite non-negative numbers indexed by floor ID minus one.

When a profile conditions out an ineligible floor (for example current origin as destination), that floor's effective weight becomes zero for that draw.

For every OD branch with nonzero selection probability, semantic validation MUST be able to prove at least one positive directly serviceable effective OD pair before generation. A branch whose selection probability is exactly zero need not have a positive effective pair, although any explicitly supplied array entries must still satisfy length/finite/non-negative rules. Because building/service ranges and weights are fully known from configuration, an implementation MUST NOT defer an impossible configured OD branch until after random draws have begun.

Default `equal_non_lobby` means weight 0 at lobby and weight 1 at every non-lobby floor.

Default `equal_all` means weight 1 at every floor before conditioning.

## 10. Profile `up_peak`

Top-level required field:

- `rate_per_minute`.

`params` fields:

- `lobby_origin_probability`: optional `[0,1]`, default `0.90`;
- `destination_weights`: optional floor array, default `equal_non_lobby`;
- `background_origin_weights`: optional floor array, default `equal_non_lobby`;
- `background_destination_weights`: optional floor array, default `equal_all`;
- optional `od_matrix` override.

Without `od_matrix`:

1. Bernoulli on `lobby_origin_probability` chooses lobby-origin vs background trip;
2. lobby-origin trip uses origin = lobby and samples destination from `destination_weights` with lobby conditioned out;
3. background trip samples origin from `background_origin_weights`, then destination from `background_destination_weights` with origin conditioned out.

## 11. Profile `down_peak`

Top-level required field:

- `rate_per_minute`.

`params` fields:

- `lobby_destination_probability`: optional `[0,1]`, default `0.90`;
- `origin_weights`: optional floor array, default `equal_non_lobby`;
- `background_origin_weights`: optional floor array, default `equal_all`;
- `background_destination_weights`: optional floor array, default `equal_non_lobby`;
- optional `od_matrix` override.

Without `od_matrix`:

1. Bernoulli chooses lobby-destination vs background trip;
2. lobby-destination trip samples origin from `origin_weights` with lobby conditioned out and sets destination = lobby;
3. background trip samples origin then destination, conditioning out same-floor trips.

## 12. Profile `interfloor`

Top-level required field:

- `rate_per_minute`.

`params` fields:

- `origin_weights`: optional, default `equal_all`;
- `destination_weights`: optional, default `equal_all`;
- optional `od_matrix` override.

Without `od_matrix`, sample origin then destination with the selected origin conditioned out of destination weights.

## 13. Profile `mixed`

Top-level required field:

- `rate_per_minute`.

When `od_matrix` is absent, required `params` fields are:

- `up_fraction`;
- `down_fraction`;
- `interfloor_fraction`.

They are finite non-negative values and MUST sum to 1 within absolute tolerance `1e-9`.

When `od_matrix` is present, the three fractions are optional and are ignored for OD selection; if supplied, they MUST still satisfy the same range and sum rule.

Optional fields reuse the same names/defaults from the corresponding profiles:

- `up_destination_weights` default `equal_non_lobby`;
- `down_origin_weights` default `equal_non_lobby`;
- `interfloor_origin_weights` default `equal_all`;
- `interfloor_destination_weights` default `equal_all`;
- optional `od_matrix` override.

Without `od_matrix`, first choose trip class by the three fractions, then apply the selected class's origin/destination rule with lobby probability fixed to 1 for the up/down classes.

## 14. Profile `burst`

`burst` does **not** use `rate_per_minute`.

Required top-level field:

- `count`: integer in `[1, 2,000,000]`.

Required `params`:

- `distribution`: `uniform_window` or `normal_window`;
- `center_s`: integer satisfying `start_s <= center_s <= end_s`;
- `window_s`: positive integer.

When `od_matrix` is absent, `origin_weights` and `destination_weights` floor arrays are also required. When `od_matrix` is present, those two arrays are optional and ignored for OD selection; if supplied, they MUST still be individually well-formed.

Optional:

- `normal_sigma_s`: positive finite number, default `window_s / 6.0`;
- `od_matrix` override.

Validation requires the closed burst window:

```text
[center_s - window_s/2, center_s + window_s/2]
```

(using real arithmetic) to lie within `[start_s,end_s]`.

For `uniform_window`, draw one Section 4.1 uniform real and set `g = low + U * (high - low)`, giving a raw timestamp in the half-open continuous interval `[low, high)`. For `normal_window`, obtain `g` from the required truncated-normal sampler for `N(center_s, normal_sigma_s)` bounded to closed `[low, high]`.

Every burst generation iteration receives a monotonically increasing `local_sequence` starting at 0 before its time/OD draws. Convert raw `g` to visible `arrival_us` by the same round-to-nearest-microsecond, ties-to-even, then tick-ceiling rule used in Section 6.

OD selection uses `od_matrix` if present; otherwise sample origin from `origin_weights`, then destination from `destination_weights` conditioned on destination != origin and direct serviceability.

Within a burst segment, ordering is exactly `(arrival_us, local_sequence)`. Raw floating time is **not** an additional sort key. The later global merge still uses `(arrival_us, segment declaration index, local_sequence)`.

## 15. Global Passenger-Count Ceiling

The canonical scenario trace contains at most **2,000,000 passengers total across all generated segments**. The per-burst `count` range does not override this scenario-wide ceiling.

Before consuming any PRNG draws, semantic validation MUST sum all deterministic `burst.count` values using overflow-checked integer arithmetic. If that guaranteed subtotal alone exceeds 2,000,000, the scenario is invalid and MUST fail with configuration semantic exit code 4. No output may be committed.

For Poisson-generated segments, the final count is not known during semantic validation. Generation MUST count canonical candidate passengers as they are produced. If accepting the next generated passenger would make the merged scenario total exceed 2,000,000, generation MUST abort with resource/representational exit code 8. It MUST NOT truncate the trace, resample to force a smaller count, wrap an ID, or commit either the trace or its companion info file.

For mixed burst + Poisson scenarios, the same single 2,000,000 total applies. The deterministic burst subtotal check is still performed first; the runtime guard then covers all generated passengers together.

The implementation may detect a guaranteed overflow earlier than the exact point above only when the conclusion follows without consuming or changing the normative PRNG stream.

## 16. Direct Serviceability During Generation

Generated OD selection MUST never knowingly emit an OD pair for which no car serves both floors.

For every weight-array profile, serviceability filtering is applied **before each categorical draw**, not by sampling an impossible pair and retrying afterward:

1. for each candidate origin, determine whether at least one positive-weight destination remains after excluding same-floor destinations and OD pairs not directly serviceable by any car;
2. origins with no such destination receive effective origin weight zero;
3. sample the origin from the remaining effective origin weights;
4. for that chosen origin, zero all same-floor or non-directly-serviceable destination weights, then sample the destination.

For a profile with a fixed lobby origin, apply step 4 directly to its destination weights. For a profile with a fixed lobby destination, apply the symmetric rule to its origin weights. For `mixed`, apply these rules inside the chosen class. For `burst`, apply them to its origin/destination arrays. `od_matrix` already expresses the effective joint weights directly.

The generator MUST NOT use rejection/retry of an otherwise positive but non-serviceable OD pair as a substitute for this conditioning, because that would alter both the distribution and random-stream consumption.

If the configured weights contain no positive directly serviceable OD pair, configuration validation MUST reject the segment.

## 17. Canonical Trace CSV

Header exactly:

```text
passenger_id,arrival_us,origin_floor,destination_floor,segment_id
```

Fields:

- `passenger_id`: positive decimal integer contiguous from 1;
- `arrival_us`: non-negative decimal integer microseconds, tick-aligned;
- origin/destination: valid floor integers, different, directly serviceable by at least one car;
- `segment_id`: valid UTF-8 text of 1..128 bytes, containing no U+0000, using CSV quoting rules in `08_OUTPUT_FORMATS.md`.

Rows are sorted by:

1. `arrival_us`;
2. `passenger_id`.

Canonical output always uses LF line endings.

## 18. Imported Trace Validation and Canonicalization

Importer MUST reject:

- more than 2,000,000 passenger rows;
- header mismatch or duplicate header names;
- extra or missing columns;
- invalid integer syntax/overflow;
- passenger ID gap/duplicate/non-positive ID;
- negative arrival;
- arrival timestamp not divisible by `tick_us`;
- arrival later than simulation duration boundary;
- invalid origin/destination or same floor;
- OD pair not directly serviceable by any car;
- rows out of canonical time/ID order;
- empty, over-128-byte, U+0000-containing, or invalid-UTF-8 segment ID;
- malformed CSV quoting.

Input CRLF is accepted.

The importer MUST canonicalize accepted rows to the exact CSV schema/LF format before fingerprinting and output. Therefore equivalent LF/CRLF source files yield the same canonical trace fingerprint.

## 19. Generate Command

`generate` MUST:

1. parse and validate generated-mode scenario;
2. initialize PCG32 and Box-Muller cache;
3. generate all segments in declaration order;
4. merge rows and assign IDs;
5. validate canonical trace;
6. write exact requested `<trace.csv>`;
7. write exact companion `<trace.csv>.info.json`;
8. compute fingerprint from canonical CSV bytes.

The info JSON fields are defined in `08_OUTPUT_FORMATS.md`.

## 20. Same-Trace Comparison Rule

`compare` generates/imports/canonicalizes the trace once before the first algorithm run.

Each algorithm receives an immutable logical copy of those passenger rows.

The implementation MUST NOT re-read a changing source file between algorithms or regenerate demand per algorithm.

## 21. FNV-1a Trace Fingerprint

Fingerprint exact canonical CSV UTF-8 bytes, including header and final LF.

Use 64-bit FNV-1a:

- offset basis `14695981039346656037`;
- prime `1099511628211`;
- multiplication modulo `2^64`.

Output is lowercase hexadecimal with exactly 16 digits.

The fingerprint is an integrity/equality marker, not cryptographic security.

## 22. Statistical Sampler Tests

Mandatory fixed-seed sanity tests MUST avoid nondeterministic p-value acceptance.

At minimum:

- one million uniform integer draws across 10 buckets exercise rejection and keep each count within a predeclared generous deterministic bound;
- weighted categorical preserves expected ordering/proportions within fixed bounds;
- exponential sample mean matches configured mean within fixed tolerance;
- Poisson sample mean matches lambda within fixed tolerance;
- normal mean and population variance fall within fixed tolerance;
- truncated normal never escapes bounds;
- edge probabilities and all-zero invalid weights behave exactly as specified.

The delivered tests MUST state sample counts and numeric tolerances in source/test data.
