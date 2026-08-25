# Data Formats and Fixed Schemas

## 1. General Text Encoding

All project-owned JSON, YAML, and CLI event/config text files use UTF-8.

JSON output shall use standard JSON escaping and shall not emit comments.


## 1.1 JSON Syntax Requirements

The hand-written JSON parser used by project components shall correctly lex/parse standard JSON structures needed by the schemas:

- objects and arrays;
- strings;
- decimal numbers;
- `true`, `false`, `null`;
- whitespace allowed by JSON.

String escapes shall support `\"`, `\\`, `\/`, `\b`, `\f`, `\n`, `\r`, `\t`, and `\uXXXX`. Valid UTF-16 surrogate-pair escapes shall decode to the corresponding Unicode scalar encoded as UTF-8 internally. Invalid escapes/control characters/unpaired surrogates are parse errors.

Schemas in this task use integer numeric fields; a syntactically valid floating value in an integer-only field is a schema error.


## 1.2 Canonical Note Token Grammar

Shared CLI note parsing uses the following canonical forms:

- decimal MIDI-style integer `21` through `108`; or
- uppercase sharp-spelled scientific pitch token using exactly one pitch-class name from:
  `C`, `C#`, `D`, `D#`, `E`, `F`, `F#`, `G`, `G#`, `A`, `A#`, `B`,
  followed by a decimal octave number.

For a pitch-class index `pc` where C=0 through B=11:

`midi = 12 * (octave + 1) + pc`

The resolved MIDI value must be 21..108 inclusive.

Examples accepted:

- `A0`;
- `C4`;
- `F#5`;
- `C8`;
- `69`.

Canonical v1.0 parsing does not use flat spellings, lowercase note letters, `B#`, `E#`, or embedded whitespace. Such unsupported spellings are input errors rather than enharmonic aliases.

---

## 2. Settings File Location

Keyboard settings are stored at:

`%LOCALAPPDATA%\\HandmadePiano\\settings.json`

If `LOCALAPPDATA` is unavailable or the directory cannot be created:

- application runs with factory defaults;
- Save reports a nonfatal persistence error;
- no fallback file is silently written beside the executable.

---

## 3. Settings JSON Schema v1

Canonical structure:

```json
{
  "schema_version": 1,
  "keyboard_mapping": [
    {"position": 0, "scan_code": 44, "extended": false}
  ]
}
```

Requirements:

- `schema_version` integer exactly 1;
- `keyboard_mapping` array exactly 24 entries;
- `position` integer 0..23, every value exactly once;
- `scan_code` integer 1..255;
- `extended` boolean;
- `(scan_code, extended)` pair unique across all entries;
- reserved Space and Escape scan identities prohibited;
- unknown root or entry fields are rejected in v1.0 rather than silently interpreted.

Array order on read does not matter; normalized writes shall sort by ascending `position`.

---

## 4. Factory Keyboard Mapping

The mapping uses Windows Set-1 style scan codes for the ordinary physical key positions below.

| Position | Display base note | Key label | Scan code (hex) | Extended |
|---:|---|---|---:|---|
| 0 | C4 | Z | 0x2C | false |
| 1 | C#4 | S | 0x1F | false |
| 2 | D4 | X | 0x2D | false |
| 3 | D#4 | D | 0x20 | false |
| 4 | E4 | C | 0x2E | false |
| 5 | F4 | V | 0x2F | false |
| 6 | F#4 | G | 0x22 | false |
| 7 | G4 | B | 0x30 | false |
| 8 | G#4 | H | 0x23 | false |
| 9 | A4 | N | 0x31 | false |
| 10 | A#4 | J | 0x24 | false |
| 11 | B4 | M | 0x32 | false |
| 12 | C5 | Q | 0x10 | false |
| 13 | C#5 | 2 | 0x03 | false |
| 14 | D5 | W | 0x11 | false |
| 15 | D#5 | 3 | 0x04 | false |
| 16 | E5 | E | 0x12 | false |
| 17 | F5 | R | 0x13 | false |
| 18 | F#5 | 5 | 0x06 | false |
| 19 | G5 | T | 0x14 | false |
| 20 | G#5 | 6 | 0x07 | false |
| 21 | A5 | Y | 0x15 | false |
| 22 | A#5 | 7 | 0x08 | false |
| 23 | B5 | U | 0x16 | false |

Space (`0x39`, non-extended) and Escape (`0x01`, non-extended) are reserved.

---

## 5. Atomic Settings Write

A settings Save shall use equivalent semantics to:

1. create/write `settings.json.tmp` in the same directory;
2. fully write UTF-8 JSON;
3. flush the file to the OS where supported;
4. close the temporary handle;
5. atomically replace/move it to `settings.json` using a same-volume replace operation;
6. on failure, preserve the previously valid `settings.json` and report error.

A stale `.tmp` file is never treated as the active settings file on startup.

---

## 6. Offline Render Event JSON v1

Canonical structure:

```json
{
  "schema_version": 1,
  "duration_ms": 1200,
  "events": [
    {"time_ms": 0, "type": "note_on", "id": "n1", "position": 0},
    {"time_ms": 500, "type": "note_off", "id": "n1"}
  ]
}
```

Root fields:

- `schema_version`: integer 1;
- `duration_ms`: integer 1..600000;
- `events`: array, maximum 100000 events.

Events must appear in nondecreasing `time_ms` order.

Equal-time events execute in array order.

All event `time_ms` values are integers satisfying `0 <= time_ms < duration_ms`.

### 6.1 `note_on`

Fields:

- `time_ms`;
- `type`: `"note_on"`;
- `id`: non-empty UTF-8 string unique among currently-held IDs;
- exactly one of:
  - `position`: integer 0..23, processed through current octave/transpose; or
  - `midi`: integer 21..108, used directly as effective pitch.

Velocity is fixed 1.0 and cannot be specified.

### 6.2 `note_off`

Fields:

- `time_ms`;
- `type`: `"note_off"`;
- `id`: ID of a currently held note-on.

Unknown/already-released IDs are errors.

### 6.3 `transpose`

- `type`: `"transpose"`;
- `value`: integer -12..+12.

Affects future position-based note-on events only.

### 6.4 `octave`

- `type`: `"octave"`;
- `value`: one of -2,-1,0,+1.

Affects future position-based note-on events only.

### 6.5 `sustain`

- `type`: `"sustain"`;
- `value`: boolean.

This event directly controls effective sustain for offline rendering; the CLI event model does not emulate a separate UI latch and Space pedal.

### 6.6 `release`

- `type`: `"release"`;
- `value`: one of `"short"`, `"medium"`, `"long"`.

Affects voices when they subsequently enter release.

### 6.7 `volume`

- `type`: `"volume"`;
- `value`: integer 0..100.

Affects output from that event time forward.

Unknown event types or unknown fields are errors.

---

## 7. CLI JSON Output

When `--json` is requested, `piano_cli` prints one JSON object to stdout and diagnostics/errors to stderr.

Success objects contain:

- `ok: true`;
- `command`;
- command-specific fields.

Failure objects when `--json` is requested contain:

- `ok: false`;
- `command` if known;
- `error_code` stable string;
- `message` human-readable string.

Key ordering in deterministic mode is fixed by the implementation and stable across repeated runs.

---

## 8. `locscan` Config Schema

JSON canonical example:

```json
{
  "schema_version": 1,
  "include_extensions": [".c", ".h", ".md", ".txt", ".json", ".yaml", ".yml"],
  "exclude_extensions": [".obj", ".exe", ".pdb", ".log", ".png", ".jpg", ".bmp", ".wav"],
  "exclude_patterns": ["build/**", "out/**", ".cache/**", "results/**"],
  "generated_patterns": ["generated/**"],
  "categories": {
    "production_source": ["src/**", "include/**"],
    "test_source": ["tests/**"],
    "documentation": ["docs/**", "*.md"],
    "config_spec": ["*.json", "*.yaml", "*.yml"]
  }
}
```

Required `schema_version`: integer 1.

All other listed fields are optional; missing fields use built-in defaults.

Paths are normalized to `/` separators before pattern matching.

Matching is case-insensitive on Windows.

### 8.1 Pattern Semantics

Supported glob grammar:

- `*` matches zero or more characters except `/`;
- `?` matches exactly one character except `/`;
- `**` as a complete path-segment wildcard matches zero or more path segments;
- a pattern without `/` matches the basename at any depth in any pattern-list/category field;
- no brace expansion, character classes, regex syntax, or negated patterns.

Exclusion wins over inclusion/category rules.

Within included files, category priority is:

1. test_source;
2. production_source;
3. documentation;
4. config_spec;
5. uncategorized included text.

---

## 9. YAML Subset for `locscan`

The YAML reader shall support only the constructs needed to represent the same schema:

- UTF-8 text;
- indentation using spaces only;
- mappings `key: value`;
- nested mappings;
- sequences with `-`;
- plain scalars without YAML type tags;
- single-quoted strings;
- double-quoted strings with common escaped characters;
- booleans `true` / `false`;
- decimal integers;
- `null`;
- comments beginning `#` outside quoted strings.

Tabs for indentation, anchors, aliases, merge keys, flow collections, block scalars, tags, directives, multi-document streams, and complex keys are unsupported and shall produce a parse error rather than be silently ignored.

Semantically equivalent JSON and YAML configs shall produce equivalent normalized config state and scan results.

---

## 10. `locscan` Built-In Category Rules

Before optional user category path patterns, built-in fallback categorization is fixed:

1. included `.c`/`.h` under normalized `tests/**` -> `test_source`;
2. other included `.c`/`.h` -> `production_source`;
3. included `.md`/`.txt` -> `documentation`;
4. included `.json`/`.yaml`/`.yml` -> `config_spec`;
5. any other user-added included text extension -> `uncategorized_text` unless a configured category path pattern selects it.

Configured `categories` path patterns are evaluated with the global priority documented in Section 8 and may classify user-added file types. Exclusion always wins.

### 10.1 Config Merge Semantics

- `include_extensions`, when present, replaces the built-in include-extension list.
- `exclude_extensions`, when present, is appended to the built-in exclusion-extension list.
- `exclude_patterns`, when present, is appended to built-in exclusion patterns.
- `generated_patterns`, when present, is appended to built-in generated patterns.
- `categories`, when present, adds category path patterns; built-in extension fallback still applies if no category path rule matches.

Duplicates are normalized away for matching purposes.

### 10.2 Generated Patterns

The config schema additionally supports:

```json
"generated_patterns": ["generated/**", "src/autogen/**"]
```

Built-in generated patterns contain at least `generated/**`.

Exclusion-reason priority is:

1. generated pattern -> `excluded_generated`;
2. ordinary exclude pattern/extension -> `excluded_pattern`;
3. NUL/binary probe -> `excluded_binary`.

A file is reported under only one exclusion reason.


---

## 11. `locscan` Config Strictness

Recognized root keys are exactly:

- `schema_version`;
- `include_extensions`;
- `exclude_extensions`;
- `exclude_patterns`;
- `generated_patterns`;
- `categories`.

Recognized category names are exactly:

- `production_source`;
- `test_source`;
- `documentation`;
- `config_spec`.

Unknown root keys or category names are schema errors (exit 3). This prevents a misspelled ignore field from silently producing incorrect counts.
