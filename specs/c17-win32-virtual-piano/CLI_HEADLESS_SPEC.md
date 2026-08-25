# CLI / Headless Companion Specification

## 1. Program

The mandatory console executable is:

`piano_cli.exe`

Ordinary commands shall not create the GUI window.

---

## 2. Common Exit Codes

- `0`: success;
- `2`: command-line usage error;
- `3`: invalid note/value/event/config input;
- `4`: input file I/O error;
- `5`: output file I/O/render/WAV error;
- `6`: mandatory test failure;
- `7`: internal invariant failure.

---

## 3. `pitch`

Syntax:

`piano_cli pitch NOTE --octave N --transpose N [--json]`

Where:

- NOTE follows the canonical note-token grammar in `DATA_FORMATS.md`;
- octave is -2,-1,0,+1 and defaults 0;
- transpose is -12..+12 and defaults 0.

For a note-name/MIDI argument, the command treats it as the displayed/base pitch and applies octave/transpose.

If the resulting effective pitch would fall outside 21..108, the command fails with exit 3 and stable error code `pitch_out_of_range`; it does not clamp or wrap.

Text success output shall include effective note, MIDI-style integer, and frequency.

JSON success fields:

- `ok`;
- `command: "pitch"`;
- `input_midi`;
- `octave_shift`;
- `transpose`;
- `effective_midi`;
- `effective_note`;
- `frequency_hz`.

---

## 4. `chord`

Syntax:

`piano_cli chord NOTE [NOTE ...] [--json]`

Each NOTE follows the same canonical note-token grammar in `DATA_FORMATS.md` and therefore resolves to MIDI-style pitch 21..108.

At least one token is accepted; fewer than three distinct pitch classes return `—`, not a usage error.

JSON success fields:

- `ok`;
- `command: "chord"`;
- `label`;
- `matched`;
- `root_pc` or null;
- `bass_pc` or null;
- `pitch_classes`: ascending integer list.

---

## 5. `mapping validate`

Syntax:

`piano_cli mapping validate --file PATH [--json]`

Validates exactly the v1 settings schema.

Text output shall list 24 normalized position -> scan-code mappings on success.

JSON success fields:

- `ok`;
- `command: "mapping.validate"`;
- `schema_version`;
- `entries`: normalized ascending by position.

Invalid schema returns exit 3 and identifies a stable error code such as `duplicate_key`, `missing_position`, `reserved_key`, or `unsupported_schema`.

---

## 6. `render`

Syntax:

`piano_cli render --events PATH --out PATH [--json]`

Behavior:

- parse event schema in `DATA_FORMATS.md`;
- do not initialize the GUI;
- do not open a physical audio device;
- render exact-duration 48 kHz stereo 16-bit PCM WAV;
- refuse to overwrite an existing output file unless `--overwrite` is also supplied.

With `--overwrite`:

`piano_cli render --events PATH --out PATH --overwrite [--json]`

JSON success fields:

- `ok`;
- `command: "render"`;
- `frames`;
- `duration_ms`;
- `sample_rate`;
- `channels`;
- `bits_per_sample`;
- `data_bytes`;
- `output_path`.

---

## 7. `diag`

Syntax:

`piano_cli diag [--json]`

This command prints compile/runtime core constants without opening live audio.

Required fields include:

- synthesis version `1`;
- sample rate 48000;
- 2 channels;
- 16-bit;
- 256 buffer frames;
- 4 live buffers;
- 16 voices;
- 19 chord templates;
- supported effective MIDI range 21..108;
- default base range 60..83.

---

## 8. `test`

Syntax:

`piano_cli test [--json]`

It runs the mandatory deterministic core test suites or invokes the same shared test runner logic.

Exit:

- 0 only if all invoked mandatory core tests pass;
- 6 if one or more fail.

JSON includes:

- total;
- passed;
- failed;
- list of failing IDs.

No missing implementation may be reported as skipped-pass.

---

## 9. Error Output

Without `--json`, failures write a human-readable diagnostic to stderr.

With `--json`, stdout contains the failure object described in `DATA_FORMATS.md`; stderr may additionally contain diagnostics but tests shall rely on exit code + JSON.

---

## 10. Determinism

`pitch`, `chord`, `mapping validate`, `diag`, and deterministic `render` results shall be stable for identical inputs.

The CLI shall not include volatile timestamps in JSON output for these commands.

---

## 11. Shared Production Core

The CLI may own argument parsing and JSON event-file parsing, but shall call the same production modules as the GUI for pitch, chord, mapping validation, synthesis/mixing, and WAV creation.

Duplicating simplified test-only behavior is prohibited.
