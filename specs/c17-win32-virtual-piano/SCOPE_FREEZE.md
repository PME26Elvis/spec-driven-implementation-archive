# v1.0 Scope Freeze

## 1. Normative Product Decisions

The following decisions are final for v1.0.

### 1.1 Default Visible Range

The 24 displayed semitone positions are exactly:

- C4 through B4;
- C5 through B5.

Using MIDI-style note numbers where C4 = 60, the default base range is 60–83 inclusive.

### 1.2 Supported Acoustic Pitch Range

The synthesizer accepts effective pitches from A0 through C8 inclusive:

- MIDI-style 21 through 108.

No required control state may generate a pitch outside this range.

### 1.3 Octave Shift

Allowed octave-shift values are exactly:

- `-2`;
- `-1`;
- `0`;
- `+1`.

Default is `0`.

At `-2`, Octave Down is disabled.

At `+1`, Octave Up is disabled.

Combined with the required transpose range, every displayed key remains within A0–C8.

### 1.4 Transpose

Allowed transpose values are every integer semitone from `-12` through `+12` inclusive.

Default is `0`.

Transpose changes affect future note-on events only. Existing note instances retain the effective pitch captured when they were created.

### 1.5 Default User Controls

Startup defaults:

- octave shift: `0`;
- transpose: `0`;
- sustain latch: off;
- Space sustain pedal: up;
- release preset: Medium (`600 ms`);
- master volume: `70%`;
- recording: idle;
- settings modal: closed.

Only keyboard mapping is required to persist across restarts. The settings above reset to these defaults on each process start.

### 1.6 Theme

v1.0 has one mandatory dark theme. A light-theme implementation is optional but is not part of acceptance.

### 1.7 Pointer Drag / Glissando

Drag-to-play/glissando is **out of scope**.

A pointer press owns the single piano key hit at pointer-down until release/cancel. Moving across other piano keys while held does not create additional note-on events.

### 1.8 Velocity

Velocity-sensitive input is **out of scope**.

Every note-on uses fixed normalized velocity `1.0`.

### 1.9 Metronome

Out of scope.

### 1.10 Recording Playback

In-application playback, timeline editing, MIDI export, and event-sequence recording are out of scope.

Only direct mixed-audio PCM WAV recording/export is required.

### 1.11 Chord Recognition

Recognition uses exact distinct pitch-class matching against the v1.0 template vocabulary.

Out of scope:

- omitted-fifth heuristics;
- altered dominants beyond the listed templates;
- 11th/13th interpretation;
- key-signature inference;
- automatic flat/sharp spelling inference;
- fuzzy "best guess" recognition when extra pitch classes are present.

### 1.12 Accessibility

Basic keyboard operation for piano input and settings capture is required as specified.

Screen-reader integration, high-contrast mode, and reduced-motion mode are out of scope for v1.0.

### 1.13 DPI

Mandatory visual/interaction acceptance is at:

- 100% / 96 DPI;
- 125% / 120 DPI;
- 150% / 144 DPI.

175% and 200% are robustness targets only.

### 1.14 Build Tool

No build-system product is mandated. The delivered repository must contain one reproducible build definition or script capable of producing the mandatory native Windows programs.

The task pack does not prescribe a compiler invocation workflow.

---

## 2. Mandatory Program Products

The submission shall produce these logical products, whether as separate executables or clearly separated modes where explicitly allowed:

- native GUI application: `handmade_piano.exe`;
- headless companion: `piano_cli.exe`;
- development utility: `locscan.exe`;
- deterministic automated test program/target.

`piano_cli.exe` shall be a separate console executable so ordinary CLI operations never need to instantiate the GUI subsystem.

---

## 3. No Scope Substitution

Optional features do not compensate for missing mandatory requirements.

Examples:

- adding a metronome does not compensate for missing blur;
- adding MIDI playback does not compensate for invalid WAV recording;
- adding more chord types does not compensate for failing required chord tests;
- supporting 200% DPI does not compensate for broken 125% DPI;
- a sophisticated CLI does not compensate for an incomplete GUI.
