# Delivery, Definition of Done, Release Gates, and Stop Conditions

## 1. Required Delivery Tree (Logical)

Exact source filenames may differ, but final delivery shall contain logical equivalents of:

```text
/
  README.md
  build definition/script
  src/                 production C sources/headers
  tests/               mandatory C17 automated tests and fixtures
  dev_tools/           locscan C sources/tests
  docs/                architecture/build/run/test/known-limits docs
  config/ or app config schema material as appropriate
  evidence/
    acceptance_report.md
    test_summary.txt or .json
    cli/
    audio/
    screenshots/       when available
    locscan/
```

Transient build/cache artifacts are not required and should be excluded.

---

## 2. Implementation Documentation

The implementer shall provide human-readable documentation covering:

- architecture/module boundaries;
- build and run instructions;
- automated test instructions;
- known limitations;
- synchronization/audio-buffer ownership;
- synth formula/voice state;
- voice-stealing identity strategy;
- chord algorithm;
- UI backbuffer/blur/animation architecture;
- DPI coordinate strategy;
- mapping persistence/atomic save;
- recording/WAV finalization;
- CLI contract implementation;
- `locscan` JSON/YAML/parser/glob/traversal behavior;
- final acceptance status.

---

## 3. Required Evidence

### 3.1 Automated

Include actual output summarizing:

- PITCH/NOTE/POLY/SUS/AUD;
- CR exhaustive + regression;
- MAP;
- REC;
- CLI;
- DPI/UI state logic;
- LOC.

### 3.2 CLI

Include representative command output for:

- pitch;
- chord;
- mapping validate;
- render;
- diag;
- test.

### 3.3 Audio/WAV

Include at least one deterministic CLI-rendered WAV fixture and its parsed header/length result.

If live GUI/audio verification is available, include one live-recorded WAV evidence artifact.

### 3.4 Visual

When screenshots are available, include the VA states listed in `TESTING_AND_ACCEPTANCE.md` and label their DPI/state.

The task pack does not prescribe how screenshots are captured.

### 3.5 `locscan`

Include:

- project config;
- deterministic JSON report on final source tree;
- human totals.

---

## 4. Acceptance Report Status

Every release gate shall be exactly one of:

- PASS;
- FAIL;
- UNVERIFIED;
- NOT_IMPLEMENTED.

The report shall list the blocking test/evidence IDs for every non-PASS gate.

---

## 5. Release Gates

### G1 — Build
PASS only if GUI, CLI, locscan, and mandatory tests build successfully for target Windows environment.

### G2 — Core State / Automated Tests
PASS only if mandatory PITCH, NOTE, POLY, SUS, AUD core tests pass.

### G3 — Chord Recognition
PASS only if CR regressions and exhaustive required template/root tests pass.

### G4 — Audio Engine
PASS only if deterministic synthesis/mixer/voice/audio lifecycle automated tests pass and live audio manual checks pass when a device is available. If device observation is impossible, live portion is UNVERIFIED and therefore the gate is UNVERIFIED, not PASS.

### G5 — GUI Functional
PASS only if required custom controls are wired, pointer/keyboard/settings/resize/focus behavior works, and no mandatory control is a dead placeholder.

### G6 — Visual / Animation
PASS only if mandatory custom visual effects are actually observed at required states. No screenshot/GUI environment -> UNVERIFIED.

### G7 — `locscan`
PASS only if LOC-001..025 pass and the utility produces the final deterministic report.

### G8 — Delivery Integrity
PASS only if required source/tests/docs/evidence are present, prohibited substitutions are absent, and delivery is not padded with transient build/cache output as authored work.

### G9 — Keyboard Mapping / Persistence
PASS only if MAP-001..016 pass and GUI rebinding/persistence is observed when GUI/filesystem verification is available.

### G10 — WAV Recording
PASS only if REC-001..015 automated tests pass and live recording is actually verified when a live audio/GUI environment is available. Otherwise live portion makes gate UNVERIFIED.

### G11 — CLI / Headless
PASS only if CLI-001..014 pass and offline render does not instantiate GUI/audio device.

### G12 — DPI
PASS only if synthetic DPI tests pass and 100/125/150 visual+hit behavior is actually verified. Unavailable visual DPI environment -> UNVERIFIED.

---

## 6. Definition of Done

The assignment is `COMPLETE` only when **G1 through G12 are all PASS**.

No optional/out-of-scope feature is required for COMPLETE.

No mandatory feature may be represented solely by:

- placeholder text;
- mock data;
- prerecorded fake test output;
- hardcoded demonstration path;
- unconnected/dead UI;
- external high-level library that replaces the required subsystem.

---

## 7. Environment-Limited Delivery

If environment restrictions prevent one or more observations, the implementer shall still deliver the best complete code/tests possible.

Final overall status shall then be one of:

- `INCOMPLETE_ENVIRONMENT` — mandatory implementation is believed present, but one or more required gates remain UNVERIFIED solely due to environment;
- `INCOMPLETE_IMPLEMENTATION` — one or more mandatory requirements are NOT_IMPLEMENTED;
- `FAILED_VERIFICATION` — implementation exists but one or more mandatory tests/observations FAIL.

Only all-PASS gates permit `COMPLETE`.

---

## 8. Stop Condition

The implementer may stop normally when:

1. all mandatory implementation work has been attempted;
2. every available mandatory test/verification has been run;
3. all remaining non-PASS items are explicitly recorded with reason/evidence;
4. deliverables are packaged.

The implementer must not stop merely because GUI/audio/screenshot verification is unavailable while unrelated code/tests can still be completed.

---

## 9. Prohibited Completion Claims

Do not claim COMPLETE when:

- any gate is FAIL/UNVERIFIED/NOT_IMPLEMENTED;
- only CLI exists but GUI is missing;
- GUI looks correct but core audio/chord/settings behavior is mocked;
- tests were not actually run yet are reported PASS;
- screenshots/logs are fabricated;
- a third-party framework supplies a prohibited subsystem.
