# Normative Reference Fixtures

The `fixtures/` directory contains fixed input data. These files are not implementation code and shall not be modified in order to make a failing implementation pass.

## 1. `default_settings.json`

This file is the canonical serialized factory keyboard mapping.

Expected validation:

- schema version 1;
- 24 entries;
- positions 0..23 exactly once;
- scan-code pairs exactly match `DATA_FORMATS.md`;
- no reserved Space/Escape;
- normalized rewrite is semantically identical.

`MAP-001` and CLI mapping validation shall use this fixture or a byte/semantic equivalent generated directly from the same table.

## 2. `render_c_major.json`

Purpose:

- simultaneous event ordering;
- three-voice polyphony;
- deterministic offline rendering;
- exact WAV duration.

Expected:

- 1200 ms;
- 57600 frames;
- 230400 PCM data bytes;
- total canonical WAV size 230444 bytes when no extra chunks are emitted;
- notes C4/E4/G4 from 0..600 ms;
- release begins at 600 ms using default Medium release, but output is truncated at duration 1200 ms exactly;
- rendering the fixture twice produces byte-identical files.

The file need not have a predetermined cryptographic hash because floating transcendental results may vary slightly across conforming math libraries/compiler targets. Determinism is required within the same delivered implementation/build.

## 3. `render_transpose_sustain.json`

Purpose:

- same-time setting ordering;
- position-based note pitch after transpose;
- long release capture after sustain ends.

Event-array ordering at time 0 means transpose/release/sustain state is applied before the note-on.

Expected note-on effective pitch:

- displayed position 0 = C4 / 60;
- transpose +2 -> D4 / 62.

At 300 ms note input is released but voice stays sustain-latched.

At 900 ms sustain becomes false and the voice enters Long release (1800 ms), but overall rendering still ends at 1800 ms and therefore truncates the remaining tail.

Expected output:

- 86400 frames;
- 345600 PCM data bytes;
- canonical no-extra-chunk WAV size 345644 bytes.

## 4. `locscan_equiv.json` and `.yaml`

These two files represent the same normalized configuration.

`LOC-009` and `LOC-010` shall demonstrate equivalent parsed config state and equivalent results on the same fixture tree.

The YAML file intentionally uses only the required v1.0 subset.

## 5. Minimum `locscan` Fixture Tree

The test suite shall construct an equivalent temporary tree containing at least:

```text
fixture_root/
  src/main.c
  src/no_final_newline.c
  src/autogen/generated.c
  include/app.h
  tests/test_audio.c
  docs/readme.md
  config/settings.json
  build/ignored.c
  results/run.log
  vendor/ignored.c
  scratch/ignored.md
  unicode/測試.c
  binary.txt
```

Suggested controlled contents:

- `src/main.c`: 3 LF-terminated lines;
- `src/no_final_newline.c`: 2 lines, no final LF;
- `include/app.h`: empty included file;
- `tests/test_audio.c`: 4 CRLF lines;
- `docs/readme.md`: 2 lines;
- `config/settings.json`: 1 line;
- `unicode/測試.c`: 1 line;
- `binary.txt`: contains a NUL byte and is never counted as human text.

Generated/excluded paths must not contribute to totals.

With the equivalent JSON/YAML fixture config, the expected included result is:

- production_source: 4 files / 6 lines;
- test_source: 1 file / 4 lines;
- documentation: 1 file / 2 lines;
- config_spec: 1 file / 1 line;
- total: 7 included files / 13 lines;
- `binary.txt`: `excluded_binary`.

## 6. CLI Reference Expectations

Representative commands and semantic results:

- `piano_cli pitch C4 --octave 1 --transpose -3 --json` -> effective MIDI 69, A4, 440 Hz.
- `piano_cli chord C4 E4 G4 --json` -> matched true, label `C`, root_pc 0, bass_pc 0, pitch_classes `[0,4,7]`.
- `piano_cli chord E3 G3 C4 --json` -> `C/E`.
- `piano_cli chord C4 E4 --json` -> matched false, label `—`.
- `piano_cli mapping validate --file fixtures/default_settings.json --json` -> success/24 entries.
- `piano_cli render --events fixtures/render_c_major.json --out out.wav --json` -> 57600 frames.
- `piano_cli diag --json` -> fixed v1.0 constants.

Exact whitespace/order of human text output is not graded. Required JSON fields and values are.

## 7. Visual Reference States

This package intentionally does not provide a reference screenshot that implementations must pixel-match. The visual specification instead fixes palette, geometry ratios, animation timings, and state evidence.

This avoids grading trivial rasterizer/font differences while still preventing a raw stock-widget substitute.

The implementer's evidence should make the following pairs easy to compare:

- idle vs held-key state;
- button rest vs hover vs pressed/ripple;
- modal closed vs open/blurred;
- settings scroll p=0 vs p=1;
- 100% vs 125% vs 150% DPI;
- mapping normal vs capture vs conflict;
- recording idle vs active.
