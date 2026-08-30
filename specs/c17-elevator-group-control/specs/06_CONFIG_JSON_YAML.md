# 06 — JSON and YAML Configuration Contract

## 1. General Requirement

Every valid scenario semantic model MUST be representable in both JSON and the required YAML subset.

The implementation MUST parse both formats itself in C17.

Parser selection is by filename extension:

- `.json` -> JSON;
- `.yaml` or `.yml` -> YAML;
- any other extension -> input/config syntax error.

Extension matching is case-sensitive in v1.0.2.

Both parsers accept LF or CRLF source line endings and valid UTF-8 text.

## 2. Shared Parse-Tree and Typed Model

Both syntaxes MUST map through an equivalent generic value model and then into one typed scenario configuration.

Equivalent JSON/YAML MUST produce:

- identical typed fields/defaults;
- identical semantic validation result;
- identical generated trace within the same build;
- identical trace fingerprint;
- identical algorithm decisions and canonical outputs within the same build.

No invalid scalar type may be silently coerced. String `"12"` is not integer 12.

## 3. Maximum Nesting

Both parsers MUST support at least 128 nested container levels and MUST reject nesting deeper than 128 with a controlled syntax/resource diagnostic.

The depth count increments on entering an object/mapping/array/sequence container.

A22 includes fixed positive depth-128 and negative depth-129 fixtures for both JSON and YAML. The depth count includes the root container as depth 1.

## 4. JSON Syntax

Support the RFC 8259 value grammar required for ordinary JSON:

- object;
- array;
- string;
- number;
- `true`;
- `false`;
- `null`.

Required escapes:

- `\"`
- `\\`
- `\/`
- `\b`
- `\f`
- `\n`
- `\r`
- `\t`
- `\uXXXX`

Valid UTF-16 surrogate pairs in `\uXXXX` sequences MUST decode to UTF-8 correctly.

Unpaired or malformed surrogates, unescaped control bytes U+0000..U+001F, invalid UTF-8, trailing commas, comments, duplicate object keys, NaN, and Infinity spellings MUST be rejected.

Duplicate keys are compared after JSON escape decoding.

## 5. JSON Number Grammar and Integer Preservation

JSON lexical numbers follow:

```text
-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?
```

Fields typed as integer in this schema MUST be represented by a lexical integer token with no decimal point/exponent unless the field explicitly says numeric.

Integer tokens MUST be range-checked without first converting through `double`. In particular, `scenario.seed` must preserve every decimal value through `18446744073709551615` exactly.

Fields typed as number may use integer, decimal, or exponent syntax but MUST convert to a finite representable value.

## 6. YAML Required Subset

Implement this bounded YAML 1.2-style subset from first principles.

Required block features:

- indentation-based mappings;
- indentation-based sequences;
- nested mapping/sequence combinations;
- sequence mapping start such as `- id: morning`;
- plain scalar mapping keys;
- single-quoted and double-quoted string keys;
- blank lines;
- comments;
- UTF-8 text.

Required scalar/container features:

- plain strings;
- single-quoted strings;
- double-quoted strings;
- integers;
- finite decimal/scientific numbers;
- booleans;
- null;
- flow arrays `[a, b, c]` whose elements are supported scalar values;
- flow maps `{key: value}` whose keys/values are supported scalar values;
- empty flow array `[]`;
- empty flow map `{}`.

Nested flow containers are not required and MUST be rejected rather than partially interpreted.

## 7. YAML Unsupported Features

Reject with explicit unsupported-feature syntax diagnostics:

- anchors `&` and aliases `*`;
- tags `!`;
- merge key `<<`;
- directives `%`;
- document markers `---` or `...`;
- multiple documents;
- block literal/folded scalars `|` or `>`;
- explicit/complex keys beginning `?`;
- custom type/schema tags;
- tabs used for indentation.

The parser is not required to be a complete YAML implementation.

## 8. YAML Indentation

Indentation uses spaces only.

Normally a child block increases indentation by any positive number of spaces, and all siblings of one block MUST align to the same indentation column. One explicit YAML convenience is also required because normative fixtures use it: a block sequence that is the direct value of a mapping key may be **indentless**, meaning its `-` indicators may appear at the same indentation column as that mapping key. The sequence's own nested mapping/value children still use a greater indentation. No other child mapping may remain at the parent's indentation.

After comment stripping, a mapping entry `key:` with no same-line value denotes a required nested child container; it is not implicit null. Likewise a bare sequence indicator `-` denotes a required nested child value. If no appropriately nested child follows, this is syntax error. Use `null` or `~` for null and `''`/`""` for an empty string.

For the required sequence-mapping shorthand `- id: morning`, that line starts a mapping as the sequence item; subsequent keys belonging to the same mapping align with `id`, not with the `-` indicator.

Dedentation must return exactly to an existing ancestor indentation level (or to the permitted indentless-sequence column).

A tab before the first non-whitespace character is a syntax error.

## 9. YAML Quoted Strings

Single-quoted strings:

- are delimited by `'`;
- represent one literal quote by doubled `''`;
- do not process backslash escapes.

Double-quoted strings support exactly the JSON escape set in Section 4, including `\uXXXX` surrogate-pair decoding.

Unknown backslash escapes are errors.

## 10. YAML Plain Scalars and Comments

For a block mapping entry, the mapping delimiter is a colon followed by whitespace or end-of-line.

Outside quotes, `#` begins a comment only when it is the first scalar character or is preceded by whitespace. Otherwise it is part of a plain string.

Leading/trailing YAML structural whitespace around an unquoted scalar is not part of the scalar.

A plain scalar containing a mapping-delimiter pattern `: ` MUST be quoted when intended as text.

## 11. YAML Scalar Typing

After trimming structural whitespace, recognize in this order:

1. `~` -> null;
2. `null` case-insensitive -> null;
3. `true`/`false` case-insensitive -> boolean;
4. integer grammar `[+-]?[0-9]+` -> integer;
5. finite decimal/scientific grammar below -> number;
6. otherwise -> string.

YAML number grammar accepted for non-integers:

```text
[+-]?(
    ([0-9]+\.[0-9]*) |
    ([0-9]*\.[0-9]+) |
    ([0-9]+[eE][+-]?[0-9]+) |
    ([0-9]+\.[0-9]*[eE][+-]?[0-9]+) |
    ([0-9]*\.[0-9]+[eE][+-]?[0-9]+)
)
```

No hexadecimal, octal prefix, sexagesimal, `.inf`, `.nan`, or timestamp typing is supported.

`yes`, `no`, `on`, `off`, and `2026-08-26` remain strings.

## 12. Duplicate Keys

Duplicate mapping/object keys are errors in both formats.

Detection occurs on decoded UTF-8 key values.

Diagnostics MUST include key text and line number.


## 12.1 String Capacity and Length Measurement

Parser storage MUST support a decoded JSON/YAML string or mapping key of at least **1,048,576 UTF-8 bytes** when memory allocation succeeds. This is a parser-capability floor, not the allowed size of every typed schema field.

Typed schema string limits below are measured in decoded UTF-8 **bytes**, excluding the terminating C NUL used internally. All strings MUST be valid UTF-8 and MUST NOT contain decoded U+0000, because product text fields are ordinary C strings. Other Unicode control characters are not globally forbidden unless a specific grammar forbids them.

Exceeding a typed field's byte limit is semantic error 4; malformed UTF-8 or decoded U+0000 is syntax/text error 3.

## 13. Unknown Fields

Root option:

```text
allow_unknown_fields: false
```

Default is false.

When false, unknown fields at any schema level are semantic validation errors.

When true, unknown fields may be ignored recursively, but malformed known fields and unsupported YAML syntax remain errors.

All ordinary task-pack acceptance scenarios use false. The two positive parser-depth probes intentionally set `allow_unknown_fields: true` solely to carry an otherwise schema-irrelevant nested value through the parser.

## 14. Root Schema

Required root keys:

```text
schema_version
scenario
building
elevators
simulation
traffic
algorithms
metrics
output
```

Optional:

```text
allow_unknown_fields
```

`schema_version` MUST be string exactly `1.0`.

## 15. `scenario`

Required:

- `name`: UTF-8 string of 1..256 bytes;
- `seed`: unsigned 64-bit integer `[0, 18446744073709551615]`.

Optional:

- `description`: UTF-8 string of 0..4096 bytes, default empty.

## 16. `building`

Required:

- `floors`: integer `[2,200]`;
- `floor_height_m`: finite number `> 0`;
- `lobby_floor`: integer `[1,floors]`.

Acceptance floor heights are <= 20 m; implementations need not impose that as a product maximum unless documented as a resource limit that still covers acceptance.

## 17. `elevators`

Array length `[1,32]`.

Each object requires:

```text
id
capacity_persons
max_speed_mps
acceleration_mps2
deceleration_mps2
door_open_ms
door_close_ms
door_min_dwell_ms
boarding_ms_per_person
alighting_ms_per_person
boarding_lanes
alighting_lanes
initial_floor
service_min_floor
service_max_floor
```

Types/ranges:

- `id`: UTF-8 string of 1..64 bytes, unique;
- `capacity_persons`: integer `[1,100]`;
- speed/accel/decel: finite number `> 0`;
- door/boarding/alighting millisecond values: integer `[0,600000]`;
- lane counts: integer `[1,100]`;
- service min/max: valid floor IDs with min <= max;
- initial floor: within service range.

At least one car must serve every OD pair admitted by explicit trace or generated traffic weights.

## 18. `simulation`

Required:

- `duration_s`: integer `[1,604800]`;
- `tick_ms`: integer satisfying `10 <= tick_ms <= 1000` and `1000 % tick_ms == 0`;
- `end_mode`: `hard_stop` or `drain`.

Optional/defaulted:

- `max_drain_s`: integer `[0,604800]`, default `3600`;
- `deadlock_window_s`: integer `[1,604800]`, default `300`;
- `starvation_threshold_s`: integer `[1,604800]`, default `120`.

`max_drain_s` is meaningful only for `drain` but may be present in either mode.

There is no door-reopen setting in schema 1.0.

## 19. `traffic`

Required:

- `mode`: `generated` or `trace`.

Optional common:

- `allow_overlap`: boolean, default false; meaningful only in generated mode.

### 19.1 Generated mode

Required:

```text
mode: generated
segments: [...]
```

`segments` length `[1,64]` minimum guaranteed support. Implementations may support more.

`trace_file` MUST NOT be present.

### 19.2 Trace mode

Required:

```text
mode: trace
trace_file: <UTF-8 path string of 1..4096 bytes>
```

`segments` MUST NOT be present.

`trace_file` MUST contain 1..4096 UTF-8 bytes and is passed to standard C file I/O exactly as configured. A relative path is relative to process working directory.

`allow_overlap`, if present, has no effect in trace mode and MUST be rejected as inapplicable when unknown-field strictness is false; task-pack trace configs omit it.

## 20. Generated Segment Common Schema

Required for every segment:

- `id`: unique UTF-8 string of 1..128 bytes;
- `start_s`: integer >= 0;
- `end_s`: integer > start and <= simulation duration;
- `profile`: one of `up_peak`, `down_peak`, `interfloor`, `mixed`, `burst`;
- `params`: mapping/object, may be `{}` only where profile defaults suffice.

For `up_peak`, `down_peak`, `interfloor`, `mixed`:

- `rate_per_minute`: required finite number >= 0;
- `count`: forbidden.

For `burst`:

- `count`: required integer `[1,2000000]`;
- `rate_per_minute`: forbidden.

Segment overlap semantics are normative in `04_TRAFFIC_GENERATION.md`.

## 21. Shared OD Fields Inside `params`

Where allowed:

- `od_matrix`: nested numeric `floors x floors` array.

Each row length must equal floors. Entries finite and >=0. Diagonal exactly zero.

Floor-weight fields are flat numeric arrays of length floors, finite >=0.

An `od_matrix` may coexist syntactically with profile fields only if those profile fields are otherwise valid, but the matrix overrides OD sampling. Acceptance avoids redundant overridden fields.

## 22. `up_peak.params`

Allowed:

- `lobby_origin_probability`: finite `[0,1]`, default `0.90`;
- `destination_weights`: floor weights, default equal non-lobby;
- `background_origin_weights`: floor weights, default equal non-lobby;
- `background_destination_weights`: floor weights, default equal all;
- `od_matrix`.

## 23. `down_peak.params`

Allowed:

- `lobby_destination_probability`: finite `[0,1]`, default `0.90`;
- `origin_weights`: floor weights, default equal non-lobby;
- `background_origin_weights`: floor weights, default equal all;
- `background_destination_weights`: floor weights, default equal non-lobby;
- `od_matrix`.

## 24. `interfloor.params`

Allowed:

- `origin_weights`: floor weights, default equal all;
- `destination_weights`: floor weights, default equal all;
- `od_matrix`.

## 25. `mixed.params`

Required unless `od_matrix` is used; task-pack acceptance still supplies fractions explicitly:

- `up_fraction`;
- `down_fraction`;
- `interfloor_fraction`.

Each finite >=0; sum must equal 1 within absolute tolerance `1e-9`.

Allowed weights:

- `up_destination_weights`: default equal non-lobby;
- `down_origin_weights`: default equal non-lobby;
- `interfloor_origin_weights`: default equal all;
- `interfloor_destination_weights`: default equal all;
- `od_matrix`.

If `od_matrix` is present, fractions are optional and ignored for OD selection; if present they must still be valid.

## 26. `burst.params`

Required:

- `distribution`: `uniform_window` or `normal_window`;
- `center_s`: integer;
- `window_s`: integer >0;
- `origin_weights`: floor array unless `od_matrix` present;
- `destination_weights`: floor array unless `od_matrix` present.

Optional:

- `normal_sigma_s`: finite >0, default `window_s / 6.0`;
- `od_matrix`.

Window containment and OD rules are in `04_TRAFFIC_GENERATION.md`.

## 27. `algorithms`

Required keys:

```text
default_compare
nearest_car
directional_collective
scan_look
eta_cost
zoning
adaptive_peak
destination_control
```

`default_compare` is a non-empty array of unique recognized algorithm IDs.

Task-pack acceptance configs use exactly:

```text
[nearest_car, directional_collective, scan_look, eta_cost, zoning, adaptive_peak, destination_control]
```

### 27.1 `nearest_car`

Allowed/defaults:

- `away_penalty_m`: finite >=0, default `100.0`;
- `opposite_direction_penalty_m`: finite >=0, default `50.0`.

### 27.2 `directional_collective`

No parameters in schema 1.0; object must be `{}`.

### 27.3 `scan_look`

No parameters in schema 1.0; object must be `{}`.

### 27.4 `eta_cost`

Allowed finite non-negative weights:

- `w_wait`: default `1.0`;
- `w_detour`: `0.35`;
- `w_stops`: `4.0`;
- `w_load`: `20.0`;
- `w_reverse`: `8.0`;
- `w_fairness`: `0.15`.

At least one of `w_wait`, `w_detour`, `w_stops`, `w_load`, `w_reverse` must be >0 so the score is not pure age credit.

### 27.5 `zoning`

- `mode`: only `static_equal` supported, default `static_equal`;
- `overflow_wait_s`: integer >=0, default `45`.

### 27.6 `adaptive_peak`

- `window_s`: integer >=1, default `300`;
- `min_mode_hold_s`: integer >=0, default `120`;
- `up_lobby_origin_fraction`: finite `[0,1]`, default `0.55`;
- `down_lobby_destination_fraction`: finite `[0,1]`, default `0.55`;
- `interfloor_fraction`: finite `[0,1]`, default `0.55`;
- `peak_min_rate_per_minute`: finite >=0, default `5.0`;
- `lobby_reserve_fraction`: finite `[0,1]`, default `0.50`.

Adaptive assignment uses the `eta_cost` weights from the sibling `eta_cost` object.

### 27.7 `destination_control`

- `max_group_size`: integer `[1,100]`, default `16`;
- `route_similarity_floor_span`: integer `[0,199]`, default `5`;
- `w_pickup`: finite >=0, default `1.0`;
- `w_route`: finite >=0, default `0.35`;
- `w_dest_stops`: finite >=0, default `4.0`;
- `w_load`: finite >=0, default `20.0`;
- `w_age`: finite >=0, default `0.15`.

At least one non-age cost weight must be >0.

## 28. `metrics`

Required/defaulted:

- `sla_wait_s`: integer >=0, default `60`;
- `histogram_bucket_s`: integer >=1, default `10`;
- `histogram_max_s`: integer >= histogram bucket, default `300`;
- `percentiles`: array of unique ascending integers in `[1,99]`, default `[50,90,95,99]`;
- `energy`: required object below.

Acceptance requires percentiles 50, 90, 95, and 99 to be present.

### 28.1 `metrics.energy`

Required finite non-negative coefficients:

- `move_per_meter`;
- `startup`;
- `door_cycle`;
- `direction_reversal`;
- `idle_per_second`;
- `load_meter_factor`.

This is a comparative score, not an electrical-energy claim.

## 29. `output`

Allowed/defaults:

- `event_log`: boolean, default true;
- `passenger_csv`: boolean, default true;
- `elevator_csv`: boolean, default true;
- `summary_json`: boolean, default true;
- `summary_txt`: boolean, default true;
- `histogram_txt`: boolean, default true;
- `comparison_csv`: boolean, default true;
- `sample_interval_s`: integer >=1, default `1`;
- `line_ending`: only `lf` is required/supported for canonical task-pack acceptance; default `lf`.

Task-pack acceptance configs set all output booleans true.

An implementation may support other line endings as an extension, but acceptance and fingerprints use LF.

## 30. Cross-Field Semantic Validation

Before simulation/generation, validate at minimum:

- generated segments obey duration/overlap/profile field rules;
- the overflow-checked sum of all deterministic `burst.count` values is <= 2,000,000; a larger guaranteed subtotal is semantic error 4 before PRNG consumption;
- every generated OD branch with nonzero selection probability has at least one positive directly serviceable effective pair after conditioning; exactly-zero probability branches are not required to contain a positive pair, but supplied arrays/matrices remain structurally/range valid;
- imported trace exists/readable when `validate` checks trace mode;
- every car service range/initial floor is valid;
- all seven acceptance algorithm IDs exist;
- numeric operations for configured duration/tick conversions are representable;
- histogram and percentile settings are coherent;
- algorithm thresholds/weights satisfy ranges;
- no NaN/Infinity can enter typed config;
- expected task-pack fixture paths are ordinary strings, not shell-expanded by the product.

## 31. Example JSON

```json
{
  "schema_version": "1.0",
  "scenario": {"name": "Small Office", "seed": 42},
  "building": {"floors": 12, "floor_height_m": 3.6, "lobby_floor": 1},
  "elevators": [
    {
      "id": "E1",
      "capacity_persons": 12,
      "max_speed_mps": 2.5,
      "acceleration_mps2": 1.0,
      "deceleration_mps2": 1.0,
      "door_open_ms": 900,
      "door_close_ms": 900,
      "door_min_dwell_ms": 1200,
      "boarding_ms_per_person": 600,
      "alighting_ms_per_person": 500,
      "boarding_lanes": 2,
      "alighting_lanes": 2,
      "initial_floor": 1,
      "service_min_floor": 1,
      "service_max_floor": 12
    }
  ],
  "simulation": {
    "duration_s": 3600,
    "tick_ms": 100,
    "end_mode": "drain",
    "max_drain_s": 3600,
    "deadlock_window_s": 300,
    "starvation_threshold_s": 120
  },
  "traffic": {
    "mode": "generated",
    "allow_overlap": false,
    "segments": [
      {
        "id": "morning",
        "start_s": 0,
        "end_s": 3600,
        "profile": "up_peak",
        "rate_per_minute": 8.0,
        "params": {"lobby_origin_probability": 0.9}
      }
    ]
  },
  "algorithms": {
    "default_compare": ["nearest_car", "directional_collective", "scan_look", "eta_cost", "zoning", "adaptive_peak", "destination_control"],
    "nearest_car": {},
    "directional_collective": {},
    "scan_look": {},
    "eta_cost": {},
    "zoning": {},
    "adaptive_peak": {},
    "destination_control": {}
  },
  "metrics": {
    "sla_wait_s": 60,
    "histogram_bucket_s": 10,
    "histogram_max_s": 300,
    "percentiles": [50,90,95,99],
    "energy": {
      "move_per_meter": 1.0,
      "startup": 4.0,
      "door_cycle": 2.0,
      "direction_reversal": 3.0,
      "idle_per_second": 0.01,
      "load_meter_factor": 0.02
    }
  },
  "output": {
    "event_log": true,
    "passenger_csv": true,
    "elevator_csv": true,
    "summary_json": true,
    "summary_txt": true,
    "histogram_txt": true,
    "comparison_csv": true,
    "sample_interval_s": 1,
    "line_ending": "lf"
  }
}
```

## 32. Equivalent YAML

```yaml
schema_version: "1.0"
scenario:
  name: Small Office
  seed: 42
building:
  floors: 12
  floor_height_m: 3.6
  lobby_floor: 1
elevators:
  - id: E1
    capacity_persons: 12
    max_speed_mps: 2.5
    acceleration_mps2: 1.0
    deceleration_mps2: 1.0
    door_open_ms: 900
    door_close_ms: 900
    door_min_dwell_ms: 1200
    boarding_ms_per_person: 600
    alighting_ms_per_person: 500
    boarding_lanes: 2
    alighting_lanes: 2
    initial_floor: 1
    service_min_floor: 1
    service_max_floor: 12
simulation:
  duration_s: 3600
  tick_ms: 100
  end_mode: drain
  max_drain_s: 3600
  deadlock_window_s: 300
  starvation_threshold_s: 120
traffic:
  mode: generated
  allow_overlap: false
  segments:
    - id: morning
      start_s: 0
      end_s: 3600
      profile: up_peak
      rate_per_minute: 8.0
      params:
        lobby_origin_probability: 0.9
algorithms:
  default_compare: [nearest_car, directional_collective, scan_look, eta_cost, zoning, adaptive_peak, destination_control]
  nearest_car: {}
  directional_collective: {}
  scan_look: {}
  eta_cost: {}
  zoning: {}
  adaptive_peak: {}
  destination_control: {}
metrics:
  sla_wait_s: 60
  histogram_bucket_s: 10
  histogram_max_s: 300
  percentiles: [50, 90, 95, 99]
  energy:
    move_per_meter: 1.0
    startup: 4.0
    door_cycle: 2.0
    direction_reversal: 3.0
    idle_per_second: 0.01
    load_meter_factor: 0.02
output:
  event_log: true
  passenger_csv: true
  elevator_csv: true
  summary_json: true
  summary_txt: true
  histogram_txt: true
  comparison_csv: true
  sample_interval_s: 1
  line_ending: lf
```

## 33. Parser Diagnostics

Syntax errors MUST report:

- source filename/path string;
- 1-based line number;
- 1-based column when reasonably available;
- concise syntax/unsupported-feature message.

Semantic errors MUST report a configuration path, for example:

```text
elevators[2].capacity_persons: must be integer in [1,100]
traffic.segments[0].params.destination_weights: no directly serviceable positive destination remains
```

The program MUST exit using the syntax-vs-semantic error class defined in `09_CLI_AND_FILES.md`.
