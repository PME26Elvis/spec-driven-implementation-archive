# 06 — `.pbt` Scene File Format and Persistence

## 1. Purpose

Pinball tables use a custom human-readable UTF-8 text format with extension `.pbt`. Parser/writer MUST be implemented in C without third-party serialization library.

## 2. Encoding

Files are UTF-8. Identifiers are ASCII by ID rules. Quoted display names may contain UTF-8. Invalid UTF-8 in user-facing quoted strings is a parse error.

## 3. Line endings/comments

Reader accepts LF and CRLF. Writer SHOULD emit LF.

Blank lines are ignored. A line whose first non-whitespace character is `#` is a comment. Inline comments are not required.

## 4. Header

First non-comment/nonblank line MUST be:

`PINBALL_TABLE 2`

`PINBALL_TABLE 1` is a supported legacy input and is migrated according to document 24. Unsupported major version is a load Error and must not partially replace current scene.

## 5. Sections

Required section forms:

- `[table]`
- `[object TYPE ID]`
- `[event ID]`

Fields use `key = value`.

## 6. Scalars

Supported:

- integer: `123`, `-4`;
- floating decimal/scientific: `1.25`, `-980.0`, `1e-3`;
- boolean: `true`, `false`;
- quoted string: `"Example"`;
- identifier reference: unquoted valid ID;
- enum token: uppercase ASCII token;
- vector2: `(x, y)`;
- tuple4: `(x, y, width, height)`.

NaN/infinity spellings are invalid.

## 7. String escapes

Required escapes:

- `\\`;
- `\"`;
- `\n`;
- `\t`.

Unknown escape is parse Error.

## 8. Unknown fields/types

- unknown key in known section: Warning; field need not be preserved;
- unknown object type: Error;
- unknown event/action type: Error;
- malformed key/value: Error.

Unknown key must be reported, not silently treated as applied.

## 9. Duplicate keys

Duplicate key within one section is an Error. Last-one-wins is prohibited.

## 10. Table fields

Required:

- `name` string;
- `world_size` vector2;
- `gravity` vector2;
- `max_active_balls` integer;
- `starting_turns` integer;
- `default_ball_radius` float;
- `default_ball_mass` float;
- `default_ball_restitution` float;
- `default_ball_friction` float;
- `default_ball_damping` float;
- `default_ball_max_speed` float.

## 11. Object type tokens

- `BALL_SPAWN`
- `WALL`
- `RAMP`
- `BUMPER`
- `FLIPPER`
- `SENSOR`
- `DRAIN`
- `LAUNCHER`
- `ONE_WAY_GATE`
- `SLINGSHOT`
- `DROP_TARGET`
- `STANDUP_TARGET`
- `ROLLOVER`
- `SPINNER`
- `KICKOUT`

## 12. BALL_SPAWN fields

Required:

- `position` vector2;
- `initial_velocity` vector2;
- `enabled` boolean.

Optional:

- `ball_radius` float override.

## 13. WALL fields

- `start` vector2;
- `end` vector2;
- `thickness` float;
- `restitution` float;
- `friction` float;
- `enabled` boolean.

## 14. RAMP fields

Same physical fields as WALL. Optional `visual_style` token may change appearance only, never physics.

## 15. BUMPER fields

- `center` vector2;
- `radius` float;
- `restitution` float;
- `friction` float;
- `impulse` float;
- `base_score` integer;
- `cooldown` float;
- `enabled` boolean.

## 16. FLIPPER fields

- `pivot` vector2;
- `length` float;
- `thickness` float;
- `rest_angle_deg` float;
- `active_angle_deg` float;
- `engage_speed_deg_s` float;
- `return_speed_deg_s` float;
- `restitution` float;
- `friction` float;
- `input` enum `LEFT_FLIPPER` or `RIGHT_FLIPPER`;
- `enabled` boolean.

## 17. SENSOR fields

- `rect` tuple4;
- `enabled` boolean;
- `debug_visible` boolean.

## 18. DRAIN fields

- `rect` tuple4;
- `enabled` boolean.

## 19. LAUNCHER fields

- `position` vector2;
- `spawn` Ball Spawn ID reference;
- `direction` vector2;
- `min_speed` float;
- `max_speed` float;
- `full_charge_time` float;
- `charge_curve` enum, v1 requires `LINEAR` support;
- `enabled` boolean.

At most one enabled Launcher may reference a given Ball Spawn. A missing target, wrong target type, or duplicate enabled Launcher ownership is a semantic validation Error.

## 20. ONE_WAY_GATE fields

- `start` vector2;
- `end` vector2;
- `thickness` float;
- `allowed_direction` vector2;
- `restitution` float;
- `friction` float;
- `enabled` boolean.

## 21. SLINGSHOT fields

- `start` vector2;
- `end` vector2;
- `thickness` float;
- `restitution` float;
- `friction` float;
- `impulse` float;
- `base_score` integer;
- `cooldown` float;
- `enabled` boolean.

## 22. Event section

Example:

```text
[event event_bonus]
source = sensor_bonus
trigger = SENSOR_ENTER
action_count = 2
action.0 = ADD_SCORE amount=500
action.1 = START_MULTIBALL spawn=spawn_main add_count=2
```

Required fields:

- source object ID;
- trigger enum;
- action_count;
- exactly action.0 through action.(count-1).

Missing/duplicate action indices are Errors.

## 23. Required action syntax

Examples:

```text
action.0 = ADD_SCORE amount=500
action.1 = SPAWN_BALL spawn=spawn_aux count=3
action.2 = ENABLE_OBJECT target=gate_001
action.3 = DISABLE_OBJECT target=bumper_004
action.4 = START_MULTIBALL spawn=spawn_main add_count=2
action.5 = SET_MULTIPLIER_OVERRIDE multiplier=3 duration=5.0
action.6 = OPEN_GATE target=gate_001 duration=2.0
action.7 = LIGHT_INDICATOR target=bumper_001 duration=0.5
```

Parser must reject missing mandatory parameters, duplicate action parameters, and unknown action type.

## 24. Stable serialization

Saving same authored table twice without edits SHOULD produce byte-identical output.

Canonical writer order:

1. header;
2. table;
3. objects in authored stable order;
4. events in authored stable order;
5. keys in canonical per-type order.

No timestamp field is required.

## 25. Numeric serialization

Writer emits enough significant decimal digits to round-trip scene doubles to approximately 1e-12 relative/absolute where finite/reasonable. Fixed two-decimal serialization for all values is prohibited.

## 26. Save completeness

Save persists every authored property needed to recreate the table. It MUST NOT persist transient runtime/UI state such as active ball coordinates, score, combo, sensor occupancy, replay cursor, hover, or selection.

## 27. Atomic save semantics

Required semantic sequence:

1. serialize to temporary file suitable for final atomic rename;
2. verify writes;
3. flush/close and use available filesystem durability primitive as appropriate;
4. atomically replace/rename destination;
5. only after success clear dirty state.

On failure, report error and preserve previous valid destination where possible.

## 28. Transactional load

Parse/validate into temporary scene state. Replace currently open document only after successful required load checks. Malformed file MUST NOT partially mutate current scene.

## 29. Resource limits

Implementation must safely support at least:

- normal file size up to 16 MiB;
- 10,000 objects before any optional higher cap;
- 10,000 events;
- 256 actions per event;
- user-facing string length at least 4096 bytes during parse.

Exceeding documented hard limits returns clear Error, not allocation runaway.

## 30. Error context

Parser errors SHOULD include line number, section/object ID if known, offending key/token, and readable reason.

## 31. Round-trip acceptance

For canonical fixtures:

`load -> save -> load`

must preserve equivalent authored semantic model field-by-field, including IDs, event/action order, numeric properties, and UTF-8 names.

## 32. Canonical minimal current-format example

```text
PINBALL_TABLE 2

[table]
name = "Minimal Table"
world_size = (1600, 1000)
gravity = (0, 980)
max_active_balls = 16
starting_turns = 3
default_ball_radius = 12
default_ball_mass = 1
default_ball_restitution = 0.78
default_ball_friction = 0.08
default_ball_damping = 0.03
default_ball_max_speed = 3000
scene_seed = 0
nudge_impulse = 85
nudge_tilt_cost = 1
tilt_threshold = 3
tilt_decay_per_second = 0.75
nudge_cooldown = 0.08

[layer gameplay]
name = "Gameplay"
visible = true
locked = false
order = 0

[object BALL_SPAWN spawn_main]
position = (1450, 850)
initial_velocity = (0, 0)
enabled = true
layer = gameplay
locked = false

[object DRAIN drain_main]
rect = (500, 940, 600, 60)
enabled = true
layer = gameplay
locked = false
```

## 33. Format-2 table fields

In addition to baseline fields, `PINBALL_TABLE 2` `[table]` requires:

- `scene_seed` unsigned decimal integer in 0..18446744073709551615;
- `nudge_impulse` float;
- `nudge_tilt_cost` float;
- `tilt_threshold` float;
- `tilt_decay_per_second` float;
- `nudge_cooldown` float.

## 34. Layer sections

Format:

```text
[layer layer_gameplay]
name = "Gameplay"
visible = true
locked = false
order = 0
```

At least one layer is required in format 2. Layer IDs use normal ID syntax. `order` is unique integer 0..N-1 after canonical serialization.

## 35. Object editor metadata

Every format-2 object section additionally requires:

- `layer = <layer_id>`;
- `locked = true|false`.

These fields affect Edit Mode only and not runtime physical enable state.

## 36. Group sections

Format:

```text
[group group_left_bank]
name = "Left bank"
pivot = (420, 350)
member_count = 3
member.0 = target_a
member.1 = target_b
member.2 = target_c
```

Groups may reference objects only, not groups. Duplicate membership inside one group is Error. An object may belong to at most one group in v1.0.0.

## 37. DROP_TARGET fields

Required:

- `start`, `end`, `thickness`;
- `restitution`, `friction`;
- `min_hit_speed`;
- `base_score`, `cooldown`;
- `initially_raised` boolean;
- `reset_mode` enum `MANUAL_EVENT`, `AFTER_DELAY`, `ON_NEW_BALL`;
- `reset_delay` float, required and meaningful for AFTER_DELAY, otherwise serialized as 0;
- `enabled`;
- format-2 editor metadata.

## 38. STANDUP_TARGET fields

Required: `start`, `end`, `thickness`, `restitution`, `friction`, `min_hit_speed`, `base_score`, `cooldown`, `enabled`, and editor metadata.

## 39. ROLLOVER fields

Required: `start`, `end`, `width`, `base_score`, `activation_mode = ON_ENTER`, `enabled`, and editor metadata.

## 40. SPINNER fields

Required: `pivot`, `half_length`, `thickness`, `rest_angle_deg`, `angular_damping`, `inertia`, `restitution`, `friction`, `score_per_tick`, `tick_angle_deg`, `enabled`, and editor metadata.

## 41. KICKOUT fields

Required: `center`, `capture_radius`, `eject_direction`, `eject_speed`, `hold_time`, `base_score`, `enabled`, and editor metadata.

## 42. Format-2 trigger tokens

Canonical current trigger tokens include:

- `SENSOR_ENTER`, `SENSOR_STAY`, `SENSOR_EXIT`;
- `BUMPER_HIT`, `SLINGSHOT_HIT`, `BALL_DRAINED`;
- `TARGET_HIT`, `TARGET_DROPPED`;
- `ROLLOVER_ENTER`, `SPINNER_TICK`;
- `KICKOUT_CAPTURE`, `KICKOUT_EJECT`;
- `TILT_STARTED`, `TILT_CLEARED`.

Legacy format-1 `SENSOR_LEAVE` migrates to `SENSOR_EXIT`.

## 43. Format-2 action syntax

Add canonical actions:

```text
action.0 = RESET_TARGET target=target_001
action.1 = SET_TARGET_DROPPED target=target_002 dropped=true
action.2 = EJECT_KICKOUT target=kickout_001
action.3 = CLEAR_TILT
```

## 44. Canonical section order for format 2

Writer order:

1. header;
2. table;
3. layers by `order`;
4. objects in authored stable order;
5. groups in authored stable order;
6. events in authored stable order.

## 45. Migration

Format-1 input is never rewritten in place merely by opening. Its current in-memory representation uses deterministic migration defaults from document 24; next explicit Save writes format 2.

## 46. Current full-format example

`acceptance/fixtures/reference_full_game_v2.pbt` is the normative comprehensive current-format example and exercises all 15 object types, multiple layers, a group, and current event/action syntax.
