# Handmade Piano — Specification-Driven Implementation Task Pack v1.0

## 1. Purpose

This package is a fixed-scope software implementation assignment intended to be given unchanged to different implementers for manual comparison.

It defines the product, engineering constraints, required behavior, state transitions, file formats, tests, evidence, release gates, and Definition of Done.

It deliberately does **not** prescribe the implementer's development workflow, editor, repository workflow, environment setup procedure, or implementation sequence. The requirements describe the software and its acceptance, not how the implementer organizes the work.

The assignment is analogous to a demanding programming course project: the requirements are fixed, while the implementer chooses how to do the work.

---

## 2. Mandatory Workstreams

### Workstream A — Development Utility

Implement `locscan.exe` in C17.

It recursively classifies and counts human-authored source/test/document files, supports JSON and a defined YAML subset for configuration, excludes generated/binary/results/build artifacts, and can emit deterministic JSON.

### Workstream B — Handmade Piano

Implement a native Windows desktop virtual piano in C17 with:

- custom software-rendered UI engine;
- two visible octaves;
- octave shifting and ±12-semitone transpose;
- pointer and customizable computer-keyboard input;
- 16-voice real-time polyphonic synthesis;
- sustain and release controls;
- live deterministic chord recognition;
- direct PCM WAV recording;
- CLI/headless companion for core functionality;
- DPI-aware rendering at 100%, 125%, and 150%;
- deterministic tests and acceptance evidence.

---

## 3. Fixed Technology Boundary

Production and test code shall use **C17**.

Permitted platform boundary libraries are limited to:

- the ISO C standard library;
- User32;
- GDI32;
- Kernel32;
- WinMM;
- Comdlg32 solely for native file-path selection/overwrite confirmation.

Other Windows libraries may be used only when a mandatory OS-boundary operation cannot reasonably be performed with the list above, and the use must be documented in the implementer-delivered implementation notes. They must not provide a high-level GUI, audio engine, synthesizer, image effect, parser, test framework, or other subsystem required to be implemented by the assignment.

High-level frameworks and third-party libraries are prohibited. See `ENGINEERING_CONSTRAINTS.md`.

---

## 4. Normative Priority

All Markdown files in this package are normative except `CHANGELOG.md` and `DOC_LINE_COUNT.md`, which are informative metadata. A section explicitly labeled informative is also non-normative.

If two requirements appear to conflict, use this priority order:

1. `SCOPE_FREEZE.md`;
2. `DATA_FORMATS.md` and explicit numeric/state rules in subsystem specifications;
3. `PRODUCT_REQUIREMENTS.md`;
4. subsystem specifications;
5. `TESTING_AND_ACCEPTANCE.md`;
6. examples and explanatory prose.

A conflict discovered during implementation must be documented rather than silently resolved in a way that reduces scope.

---

## 5. Verification Status Vocabulary

Every mandatory acceptance item has exactly one status:

- `PASS` — implemented and actually verified;
- `FAIL` — verification ran and the requirement failed;
- `UNVERIFIED` — implementation exists, but the execution environment prevented actual verification;
- `NOT_IMPLEMENTED` — required implementation is absent.

`UNVERIFIED` is never equivalent to `PASS`.

---

## 6. Execution-Independent Completion Rule

Lack of a visible desktop, physical audio device, screenshot capability, input injection, multi-DPI monitor setup, or similar verification facility does **not** justify omitting production code.

The implementer shall:

- implement all reasonably implementable required code;
- implement deterministic tests and headless validation surfaces;
- run every verification that the environment permits;
- mark genuinely blocked observations `UNVERIFIED`;
- continue with unrelated work instead of stopping early.

The implementation must not be reduced to CLI-only because GUI execution is unavailable.

---

## 7. Canonical Files

Read these first:

- `SCOPE_FREEZE.md` — final v1.0 product decisions and explicit out-of-scope items;
- `PRODUCT_REQUIREMENTS.md` — user-visible behavior;
- `STATE_MODEL.md` — event/state ownership and transitions;
- `UI_UX_SPEC.md` — custom UI and visual behavior;
- `AUDIO_ENGINE.md` — synthesis, mixing, real-time output;
- `CHORD_RECOGNITION.md` — chord vocabulary and deterministic naming;
- `SETTINGS_AND_RECORDING.md` — mapping persistence and WAV recording;
- `CLI_HEADLESS_SPEC.md` — fixed CLI contract;
- `DATA_FORMATS.md` — settings/events/locscan schemas;
- `REFERENCE_FIXTURES.md` — fixed acceptance input fixtures and expectations;
- `ERROR_HANDLING_MATRIX.md` — required failure/recovery behavior;
- `DEV_TOOLS.md` — `locscan`;
- `DPI_SCALING.md` — DPI behavior;
- `TESTING_AND_ACCEPTANCE.md` — detailed test catalog;
- `ACCEPTANCE_CHECKLIST.md` — compact human checklist;
- `TRACEABILITY_MATRIX.md` — requirement-to-test/gate map;
- `DELIVERY_AND_DOD.md` — delivery, release gates, stopping conditions.

---

## 8. Release Version

This is the scope-frozen **v1.0** task pack.

Items listed as out of scope must not be treated as hidden requirements and must not be used to excuse incomplete mandatory work.
