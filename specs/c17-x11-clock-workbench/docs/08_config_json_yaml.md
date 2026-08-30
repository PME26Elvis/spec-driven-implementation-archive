# 08 — Configuration, JSON and YAML

## 1. Shared logical schema

Project B configuration has one logical schema that may be represented as JSON or YAML.

Both formats must result in the same internal configuration model.

Project A `cfgcheck` uses the same parser and validation modules.

## 2. Required file selection

The application must support at least:

```text
analog-clock-workbench --config path/to/config.json
analog-clock-workbench --config path/to/config.yaml
```

`.yml` must be accepted as a YAML extension.

If no `--config` path is specified, the active config path is exactly `./clock-config.json` relative to the process working directory. If that file does not exist, Project B starts with built-in defaults, records that the active file is currently absent, and allows a later successful `Apply`/`Save config` to create `./clock-config.json`.

An explicitly supplied config path must use `.json`, `.yaml`, or `.yml`. If it is missing, unreadable, syntactically invalid, or semantically invalid, startup must exit nonzero before opening the main window and print a diagnostic to stderr. If the implicit `./clock-config.json` **exists** but is unreadable, syntactically invalid, or semantically invalid, startup follows the same fail-fast rule. Built-in fallback is permitted only when that implicit file is absent. Runtime reload failures are handled in-app as specified elsewhere.

## 2A. Active/saved baseline when the implicit config is absent

When startup uses built-in defaults because the implicit `./clock-config.json` is absent, Project B initializes both `active_config` and the in-memory `saved_config` baseline to the same fully normalized built-in defaults. Dirty state therefore begins false even though no config file exists on disk. The application separately tracks that the active config file is absent.

`Apply`/`Save config` must remain available in this state even when logical dirty state is false; a successful Apply creates `./clock-config.json`, then marks the active file present and keeps `saved_config` equal to the written normalized configuration. `Revert` before the first successful save restores the built-in-default baseline. A failed first save leaves the file-presence flag absent and does not change `saved_config`.

At runtime, `Reload config` always means re-read the currently selected active path. If that path is missing, unreadable, invalid, or has an unsupported representation at reload time, Reload fails non-destructively and leaves both `active_config` and `saved_config` unchanged; it does not silently switch to built-in defaults.

## 3. JSON parser scope

The JSON parser must support standard JSON values required by RFC-style JSON syntax:

- object;
- array;
- string;
- number;
- `true`;
- `false`;
- `null`.

## 4. JSON strings

JSON strings must support:

- escaped quote;
- escaped backslash;
- escaped slash;
- `\b`, `\f`, `\n`, `\r`, `\t`;
- `\uXXXX` escapes;
- valid surrogate-pair combination for non-BMP Unicode escapes.

Decoded strings must be stored as UTF-8.

Invalid escape sequences and malformed surrogate usage must be rejected.

## 5. JSON numbers

Support the JSON number grammar including:

- optional minus;
- integer part;
- optional fraction;
- optional exponent.

Leading `+`, hexadecimal, NaN, and Infinity are invalid JSON.

## 6. JSON duplicate keys

Duplicate object keys are errors.

The parser must not use silent last-key-wins behavior.

## 7. JSON trailing content

After the root value, only whitespace is allowed.

Trailing commas are invalid.

Comments are invalid in JSON.

## 8. YAML subset purpose

The assignment requires a deliberately bounded YAML subset rather than complete YAML 1.2.

The implementation must parse all features listed below and must explicitly reject unsupported features rather than misinterpret them.

## 9. YAML supported structures

Mandatory YAML subset:

- block mappings using `key: value`;
- nested mappings by indentation;
- block sequences using `- item`;
- sequences of mappings;
- the exact flow literals `[]` and `{}` only for an empty sequence or empty mapping;
- plain scalar strings;
- single-quoted strings;
- double-quoted strings with common escapes;
- integers;
- finite floating-point numbers;
- booleans `true`/`false`;
- `null`;
- `#` comments outside quoted strings;
- blank lines;
- UTF-8 text;
- indentation using spaces.

The `[]` and `{}` forms are special empty-value literals only. No other flow-style collection syntax is supported.

## 9A. YAML token/separator rules

For the required subset:

- a mapping key may be a plain scalar or a single/double-quoted string; after decoding it must be a non-empty UTF-8 string;
- the mapping separator `:` must be followed by whitespace or end-of-line;
- a sequence marker `-` must occur at the current indentation column and be followed by whitespace or end-of-line;
- trailing spaces outside quoted scalars are ignored;
- duplicate keys are detected after quoted-string decoding, not by raw source spelling;
- comments are stripped only according to the `#` rule in the scalar-typing section;
- indentation is structural only outside quoted scalars.

The parser does not need to implement YAML implicit-key, folded-flow, or context-sensitive plain-scalar rules beyond this explicitly defined subset.

## 10. YAML unsupported features

The parser must reject with a clear diagnostic:

- tab indentation;
- anchors and aliases;
- merge keys;
- explicit tags;
- directives;
- YAML document markers `---` and `...`;
- multiple documents;
- non-empty flow mappings such as `{a: 1}`;
- non-empty flow sequences such as `[1, 2]`;
- block scalar `|` and `>`;
- complex keys;
- arbitrary type coercions such as dates/timestamps;
- sexagesimal numbers;
- custom schemas.

## 11. YAML indentation

Indentation must be structurally validated.

The parser must not guess wildly on inconsistent indentation.

The parser must accept indentation increases of one or more spaces, but a single nesting level must use one consistent indentation width within its parent collection. Dedenting must return to a previously established indentation column. The serializer must always emit exactly two spaces per nesting level.

## 12. YAML scalar typing

Only the exact lowercase unquoted tokens `true`, `false`, and `null`, or a token matching the JSON number grammar, receive non-string scalar types. Leading `+`, hexadecimal, octal, `.inf`, `.nan`, timestamps, and YAML 1.1 aliases such as `yes`/`no` are not typed specially.

Other plain scalars remain strings. A `#` begins a comment only when it is the first non-indentation character or is preceded by whitespace; `#` inside quoted strings is data. A `:` inside a plain scalar is data when it is not followed by whitespace.

Single-quoted strings escape a literal apostrophe by doubling it (`''`). Double-quoted strings support the same escape set required for JSON strings. Strings such as `10:10:30` must remain strings, not timestamps or special YAML time values.

## 13. YAML duplicate keys

Duplicate keys within one mapping are errors.

## 14. Configuration root schema

The root is an object/mapping.

Mandatory top-level sections:

- `version`;
- `window`;
- `clock`;
- `playback`;
- `display`;
- `animation`;
- `effects`;
- `persistence`.

Unknown top-level keys are errors. All listed top-level sections are mandatory in an explicit config file. Unless a nested leaf is explicitly described as required, nested leaf keys are optional and receive the documented defaults; this is why a section may omit individual leaves while the section itself remains present.

## 15. Version

Required:

```text
version: 1
```

Unsupported future major versions must fail clearly.

## 16. Window section

Required/allowed keys:

- `width`: integer, default 1100, range 720..3840;
- `height`: integer, default 760, range 540..2160;
- `ui_scale`: number, allowed range 1.0..1.5; mandatory tested values 1.0, 1.25, 1.5;
- `remember_geometry`: boolean, default true.


## 16A. Window size and UI-scale coordinate contract

`window.width` and `window.height` are initial **X11 client-area physical pixel** dimensions. `window.ui_scale` is the logical-to-physical scale factor used by the renderer and hit tester:

```text
physical_px = logical_px * ui_scale
logical_viewport = physical_client_size / ui_scale
```

The mandatory minimum usable logical viewport is `720×540`. Therefore the runtime physical minimum is `ceil(720*ui_scale) × ceil(540*ui_scale)`. If configured or remembered physical dimensions are smaller than that runtime minimum, they remain syntactically valid but Project B must request/use at least the scaled minimum at runtime.

Changing `ui_scale` live must recompute the physical minimum, update X11 size hints where used, and request a larger client size when necessary so the logical viewport does not become smaller than `720×540`. If a window manager temporarily delivers a smaller size anyway, the renderer must remain memory-safe and use a constrained fallback until a compliant size is restored.

## 17. Clock section

Keys:

- `reset_time`: string `HH:MM:SS` using 24-hour `00..23:00..59:00..59` syntax regardless of display mode, default `10:10:30`;
- `second_hand`: enum `smooth` or `tick`, default `smooth`;
- `drag_snap`: enum `off`, `on_release`, `live`, default `off`;
- `snap_seconds`: integer range 1..30, default 1;
- `snap_minutes`: integer range 1..30, default 1;
- `snap_hours`: integer range 1..6, default 1;
- `face_style`: enum `minimal`, `classic`, `technical`, default `minimal`.

## 18. Playback section

Keys:

- `default_rate`: finite number, default `1.0`, range `-100.0..100.0`;
- `initially_playing`: boolean, default false;
- `large_gap_clamp_seconds`: finite number, default 5.0, range 0.1..30.0.

The mandatory playback slider range is fixed at exactly `-100×..+100×`; configuration must not redefine its endpoints.

## 19. Display section

Keys:

- `hour_mode`: enum `12` or `24`, default `24`;
- `accent_hue`: integer 0..359, default 210;
- `show_all_hour_numbers`: boolean, default false;
- `show_status_strip`: boolean, default true.

## 20. Animation section

Keys:

- `enabled`: boolean, default true;
- `speed_multiplier`: finite number 0.25..4.0, default 1.0;
- `hover_ms`: integer 50..1000, default 140;
- `ripple_ms`: integer 100..1500, default 420;
- `capsule_ms`: integer 80..1500, default 240;
- `toggle_ms`: integer 80..1500, default 180;
- `modal_open_ms`: integer 80..2000, default 280;
- `modal_close_ms`: integer 80..2000, default 220;
- `page_ms`: integer 80..2000, default 240.

## 21. Effects section

Keys:

- `modal_blur_radius`: integer 0..32, default 12;
- `nav_blur_radius`: integer 0..32, default 10;
- `shadow_strength`: finite number 0..1, default 0.35;
- `glow_strength`: finite number 0..1, default 0.5;
- `frost_scroll_range`: integer 20..400, default 120.

## 22. Persistence section

Keys:

- `save_on_exit`: boolean, default true;
- `remember_time`: boolean, default true;
- `remember_rate`: boolean, default true;
- `remember_play_pause`: boolean, default false;
- `state_file`: non-empty UTF-8 string path of at most 4096 bytes, default `clock-state.json`, resolved relative to the active config directory when not absolute.

## 23. Defaults

Missing optional keys receive documented built-in defaults.

Missing mandatory root/version structure is an error if a config file exists and is being loaded.

`cfgcheck --dump-normalized` must expose all defaults explicitly.

## 24. Unknown nested keys

Unknown keys anywhere in the known schema are errors.

This catches misspellings such as `modal_blur_raduis`.

## 25. Config save format

When the active config path ends in `.json`, the application must save JSON. When it ends in `.yaml` or `.yml`, it must save the supported YAML subset.

The built-in default path is `./clock-config.json`, so a session started without an existing config file saves JSON when the user first applies/saves configuration. Unsupported config filename extensions are rejected rather than guessed.

## 26. Serializer requirements

JSON serializer must:

- escape required characters;
- emit valid UTF-8;
- use deterministic key ordering for normalized output;
- use locale-independent numeric formatting.

YAML serializer must:

- emit only the supported subset;
- quote strings when ambiguity exists;
- use deterministic indentation;
- avoid features the parser cannot read back.

## 27. Round-trip requirement

For every valid normalized application configuration:

```text
internal -> JSON -> parse -> internal
internal -> YAML -> parse -> internal
```

must preserve all logical values.

## 28. Cross-format equivalence

Equivalent supplied JSON and YAML examples must normalize to identical canonical JSON via `cfgcheck --dump-normalized`.

## 29. Atomic save

Config persistence must not overwrite the original file directly in a way that risks leaving a truncated file after failure.

Required strategy:

1. serialize to a temporary file in the same target directory;
2. flush/close and check errors;
3. atomically replace/rename target;
4. handle failure and preserve previous target if replacement did not succeed.

Exact durability guarantees beyond ordinary filesystem semantics are not required.

## 30. GUI-editable configuration

At minimum these keys must be editable through Settings:

- `clock.reset_time`;
- `clock.second_hand`;
- `clock.drag_snap`;
- `clock.face_style`;
- `playback.default_rate`;
- `display.hour_mode`;
- `window.ui_scale`;
- `animation.enabled`;
- `animation.speed_multiplier`;
- `effects.modal_blur_radius`;
- `effects.nav_blur_radius`;
- `persistence.save_on_exit`;
- `persistence.remember_time`;
- `persistence.remember_rate`;
- `persistence.remember_play_pause`;
- `window.remember_geometry`.

Other schema settings may be GUI-editable as well.

## 31. Config reload and active gestures

Reload must be disabled or deferred during an active drag/digital edit.

It must never mutate coordinate/behavior settings halfway through a gesture.

## 32. Example files

The task pack includes equivalent example JSON and YAML configurations under `examples/`.

Implementations must successfully validate both.

## 33. Parser resource limits

For both configuration formats, the mandatory limits are:

- maximum input file size: **1 MiB (1,048,576 bytes)**;
- maximum syntactic nesting depth: **64**;
- maximum decoded string length: **256 KiB**;
- maximum object/mapping or array/sequence element count at any single level: **16,384**.

Inputs exceeding these limits must fail with a diagnostic; they must never be partially applied. Implementations may use lower internal allocation chunks, but may not lower these accepted limits.

## 34. Persisted runtime-state file

Runtime state is separate from persistent configuration. The state file is always JSON and has this exact logical schema:

```json
{
  "version": 1,
  "canonical_time": 36630.25,
  "playback_rate": -1.0,
  "is_playing": false,
  "window": { "width": 1100, "height": 760 }
}
```

Rules:

- `version` is mandatory and must equal `1`;
- the other four members are optional because persistence toggles may suppress them;
- `canonical_time`, when present, must be finite and in `[0,86400)`;
- `playback_rate`, when present, must be finite and in `[-100,100]`;
- `is_playing` is boolean;
- `window.width` and `window.height` are integers inside the normal configured size ranges;
- no additional keys are accepted;
- undo/redo history, transient focus, active gestures, animation progress, and open modal state are never persisted.

A relative `persistence.state_file` path is resolved relative to the directory containing the active config path. For the built-in `./clock-config.json` path this is the current working directory. An absolute path remains absolute.


## 34A. Runtime startup baseline

Before optional runtime-state restoration, the runtime values are derived from validated configuration exactly as follows:

- canonical simulated time = parsed `clock.reset_time`;
- runtime playback rate = `playback.default_rate`;
- Play/Pause state = `playback.initially_playing`;
- requested client dimensions = `window.width`/`window.height`, raised to the scaled minimum when necessary.

Project B never substitutes operating-system wall-clock time for this baseline.

## 35. Runtime-state load precedence

Startup order is fixed:

1. establish built-in defaults;
2. load and validate the active configuration if it exists;
3. if a state file exists, parse it completely;
4. apply a persisted member only when its controlling setting is true:
   - `window.remember_geometry` -> `window`;
   - `persistence.remember_time` -> `canonical_time`;
   - `persistence.remember_rate` -> `playback_rate`;
   - `persistence.remember_play_pause` -> `is_playing`;
5. any non-restored runtime member comes from configuration/default startup semantics.

A missing state file is normal. A corrupt state file never prevents a valid configuration from launching: it is ignored as a whole and a warning is surfaced after the main UI appears.

## 36. Runtime-state save semantics

If `persistence.save_on_exit` is false, Project B must not create or replace the runtime-state file on normal exit.

If it is true, Project B atomically writes a version-1 state object containing only members whose corresponding remember toggle is enabled. `window` contains dimensions only; desktop position is intentionally out of scope. The same temp-write/close/check/rename safety rule used for configuration applies to the state file.
