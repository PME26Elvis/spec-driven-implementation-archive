# 14 — `stateprobe` Fixture Schema

## 1. Purpose

`stateprobe` fixtures are deterministic JSON snapshots used to validate Project B state-model and history invariants without inspecting live process memory.

They are test artifacts, not the application's runtime persistence file. Runtime persistence is defined separately in `08_config_json_yaml.md` and never stores undo/redo history.

## 2. Root object

A fixture root must contain exactly these keys:

```json
{
  "version": 1,
  "state": {},
  "undo": [],
  "redo": [],
  "expected": {}
}
```

Unknown root keys are errors.

`version` must be integer `1`.

## 3. `state` object

The exact required keys are:

```json
{
  "canonical_time": 36630.25,
  "playback_rate": -1.0,
  "is_playing": false,
  "selected_field": "minute",
  "hour_mode": 24,
  "second_hand": "smooth"
}
```

Validation rules:

- `canonical_time`: finite number in `[0,86400)`;
- `playback_rate`: finite number in `[-100,100]`;
- `is_playing`: boolean;
- `selected_field`: one of `none`, `hour`, `minute`, `second`, `ampm`;
- `ampm` is valid only when `hour_mode` is `12`;
- `hour_mode`: integer `12` or `24`;
- `second_hand`: string `smooth` or `tick`;
- unknown keys are errors.

The fixture represents a stable state: no pointer drag, digital edit buffer, modal transition, or animation is active.

## 4. `undo` and `redo`

Both are arrays ordered from oldest to newest. The last element is the next entry consumed by Undo or Redo respectively.

Each history entry is an object with exact keys:

```json
{
  "kind": "time",
  "before": {"canonical_time": 36000.0},
  "after": {"canonical_time": 36630.25}
}
```

Optional diagnostic labels are deliberately excluded from fixture normalization so different user-facing wording does not change fixture identity.

## 5. History kind `time`

Exact shape:

```json
{
  "kind": "time",
  "before": {"canonical_time": 36000.0},
  "after": {"canonical_time": 36630.25}
}
```

Both values must be finite and normalized into `[0,86400)`.

`before.canonical_time` and `after.canonical_time` must not be equal within `1e-9` seconds; no-op history records are invalid.

This kind covers hand drags, digital scrub drags, typed time edits, AM/PM changes, and Reset Time.

## 6. History kind `rate`

Exact shape:

```json
{
  "kind": "rate",
  "before": {"playback_rate": 1.0},
  "after": {"playback_rate": -2.0}
}
```

Both values must be finite and in `[-100,100]` and must differ by more than `1e-12`.

## 7. History kind `play_pause`

Exact shape:

```json
{
  "kind": "play_pause",
  "before": {"is_playing": false},
  "after": {"is_playing": true}
}
```

The two boolean values must differ.

## 8. History kind `config`

Exact shape:

```json
{
  "kind": "config",
  "before": {
    "path": "effects.nav_blur_radius",
    "value": 10
  },
  "after": {
    "path": "effects.nav_blur_radius",
    "value": 16
  }
}
```

Rules:

- `before.path` and `after.path` must be identical;
- the path must identify one defined scalar configuration leaf from the version-1 Project B schema in `08_config_json_yaml.md`; this deliberately includes non-GUI-editable leaves because a successful Reload can change them;
- `value` must have the correct JSON scalar type and satisfy that setting's schema range/enum;
- before/after logical values must differ;
- `null`, arrays, and objects are not valid `value` payloads for version 1 fixtures.

Grouped `Reload`, `Revert`, or `Discard` actions that alter multiple settings use `config_batch` instead of inventing a fake single path.

## 9. History kind `config_batch`

Exact shape:

```json
{
  "kind": "config_batch",
  "before": {
    "values": {
      "display.hour_mode": 24,
      "effects.nav_blur_radius": 10
    }
  },
  "after": {
    "values": {
      "display.hour_mode": 12,
      "effects.nav_blur_radius": 16
    }
  }
}
```

Rules:

- the two `values` objects must have exactly the same set of paths;
- every path must be a defined scalar configuration leaf from the version-1 Project B schema;
- values must satisfy the normal setting schema;
- at least one logical value must differ;
- paths are serialized in lexicographic byte order by `stateprobe normalize`.

## 10. History-stack structural rules

`stateprobe validate` must reject:

- unknown history `kind`;
- missing/extra fields for a kind;
- before/after payload with wrong types/ranges;
- no-op entries;
- more than 100 entries in either fixture stack; version 1 fixtures intentionally use a fixed 100-entry validation ceiling independent of any larger runtime history capacity chosen by the implementation;
- a fixture where both undo and redo contain the exact same object at their top as an obvious malformed duplicate transition.

The tool is not required to prove that every entire historical chain could have arisen from a particular real UI sequence; it validates entry structure and local invariants.

## 11. `expected` object

Exact shape:

```json
{
  "digital": "10:10:30",
  "ampm": null,
  "angles_deg": {
    "hour": 305.2520833333,
    "minute": 63.025,
    "second": 181.5
  }
}
```

Rules:

- `digital` is normalized display text derived from `state.canonical_time` and `hour_mode`;
- in 24-hour mode it is exactly `HH:MM:SS` and `ampm` must be `null`;
- in 12-hour mode it is exactly `HH:MM:SS` using hours `01..12`, and `ampm` is string `AM` or `PM`;
- angle zero is 12 o'clock and degrees increase clockwise;
- angles are normalized into `[0,360)`;
- hour/minute are always continuous;
- second angle uses the configured `second_hand` policy.

For `tick`, second angle is based on `floor(seconds-within-minute)`.

## 12. Derived-value tolerance

`stateprobe validate` recomputes expected values from `state`.

- digital text and AM/PM must match exactly;
- each expected angle must be finite;
- circular angular distance between expected and recomputed angle must be at most `1e-6` degrees.

Comparisons use circular distance so `0` and `360` are equivalent mathematically, but normalized fixture output always emits `0`, never `360`.

## 13. `stateprobe normalize`

`stateprobe normalize FILE` must:

1. parse and fully validate the input fixture;
2. emit semantically identical JSON to stdout;
3. use UTF-8;
4. use two-space indentation;
5. use one final LF;
6. emit root keys in `version`, `state`, `undo`, `redo`, `expected` order;
7. use the field order shown in this document for fixed-shape objects;
8. sort `config_batch.values` paths lexicographically by UTF-8 byte sequence;
9. format finite numbers with enough precision to round-trip to the same C `double`, while never emitting NaN/Infinity or a negative-zero textual form.

Running normalize on already normalized output must be byte-for-byte idempotent.

## 14. Required shipped fixtures

The task pack provides these reference examples:

- `examples/stateprobe_good.json` — valid fixture;
- `examples/stateprobe_bad_angle.json` — derived angle mismatch;
- `examples/stateprobe_bad_history.json` — malformed/no-op history record.

The submitted implementation must validate/reject them as appropriate and must add further project-owned fixtures for mandatory tests.
