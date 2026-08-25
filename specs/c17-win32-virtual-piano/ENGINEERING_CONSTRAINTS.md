# Engineering Constraints

## 1. Language

### ENG-001 C17

All production code, `locscan`, and mandatory automated test code shall be C17.

C++ compilation to obtain C++ language/library features is prohibited.

Third-party unit-test frameworks are prohibited.

---

## 2. Windows Native Boundary

### ENG-009 Target OS / Architecture

Target is native x64 Windows 10 22H2 or later, including Windows 11.

Supporting 32-bit Windows, Windows 7/8, Wine, or non-Windows hosts is not a v1.0 requirement.


### ENG-010 Primary Allowed Libraries

Allowed without special justification:

- ISO C standard library;
- User32;
- GDI32;
- Kernel32;
- WinMM;
- Comdlg32 only for native Save-file path selection/overwrite confirmation.

### ENG-011 Additional OS API Exception

An additional Windows system DLL/API may be used only if all are true:

1. it is required for an OS-boundary operation;
2. it does not implement a subsystem this assignment requires the submission to hand-write;
3. there is no reasonable equivalent in the primary allowed list;
4. the dependency and reason are recorded in implementation notes.

This exception cannot be used for convenience to import a GUI toolkit, renderer, audio engine, parser, image filter, JSON/YAML library, or test framework.

---

## 3. Prohibited Frameworks and Substitutions

Prohibited include, but are not limited to:

- Qt, GTK, wxWidgets;
- SDL, GLFW, SFML;
- JUCE;
- Dear ImGui, Nuklear;
- DirectUI/third-party retained-mode UI libraries;
- Electron, Chromium embedding, WebView UI;
- .NET/WPF/WinForms/WinUI as the application implementation;
- third-party synthesizers or virtual-instrument engines;
- third-party JSON/YAML parsers;
- third-party test frameworks;
- per-note WAV/MP3/OGG sample packs used as the piano engine.

Copying source of a prohibited library into the repository is equally prohibited.

---

## 4. Custom Software UI Engine

### ENG-020 Application-Owned Backbuffer

The application shall render into a 32-bit application-owned off-screen pixel buffer and present that buffer to the native window.

Normative in-memory pixel order for project-owned effect/composition code is BGRA8 (8 bits each B,G,R,A). The final window backbuffer is treated as opaque (A=255); temporary layers may carry alpha.

Project-owned alpha composition uses straight-alpha source-over semantics. With source alpha `sa` and destination alpha `da` normalized to 0..1:

`out_a = sa + da * (1 - sa)`

If `out_a > 0`:

`out_rgb = (src_rgb * sa + dst_rgb * da * (1 - sa)) / out_a`

The final window backbuffer normally has `da=1`, reducing the RGB expression to ordinary source-over blending. Integer/fixed-point implementations are allowed with <=1 channel value difference from the floating reference on 8-bit inputs.

The backbuffer shall match the current physical client size.

### ENG-021 Required Hand-Written Visual Composition

Project-owned code shall implement at minimum:

- RGBA/ARGB alpha composition used by UI effects;
- rounded-rectangle coverage/composition;
- clipping;
- border/glow composition;
- shadows;
- ripple rendering;
- modal dimming;
- application-content blur;
- layout/hit geometry;
- animation interpolation.

GDI may be used to create/present buffers and rasterize text/glyphs. Stock GDI/Windows widgets may not substitute for required custom controls.

### ENG-022 Stock Controls

Stock Windows buttons, edit boxes, sliders, tab controls, list controls, and stock MessageBox UI are prohibited for normal product UI.

A MessageBox is permitted only for a fatal bootstrap error before the custom UI can be created.

The native Save-file dialog is permitted for PR-120.

---

## 5. Audio Boundary

### ENG-030 WinMM Output

Real-time device submission shall use a low-level Windows PCM output path, with WinMM `waveOut` as the normative v1.0 target.

The submission is not required to implement an audio driver.

### ENG-031 Hand-Written Synthesis

Oscillators/partials, envelopes, voice management, sustain, mixing, PCM conversion, and WAV writing shall be project-owned code.

---

## 6. Required Module Separation

The repository shall contain identifiable modules/translation units for at least:

- process/window lifecycle;
- DPI/layout;
- software renderer;
- UI widgets/animation;
- piano/input mapping;
- settings/config persistence;
- note/pitch utilities;
- chord recognition;
- synth voices/envelopes/mixer;
- WinMM output;
- WAV writer/recording;
- CLI argument/event handling;
- diagnostics/test hooks;
- `locscan` parsing/scanning/reporting.

Exact filenames are not prescribed.

A single monolithic C file for the application is not acceptable.

---

## 7. Memory and Concurrency

### ENG-050 Ownership

Every heap allocation shall have a clear owner and teardown path.

Required failure handling includes:

- allocation failure;
- backbuffer reallocation failure;
- audio buffer allocation failure;
- settings/recording path allocation failure.

### ENG-051 Audio Concurrency

UI and audio execution contexts shall not concurrently mutate shared voice/mixer state without a defined synchronization or message-passing strategy.

The strategy must avoid:

- use-after-free;
- reentrant buffer ownership corruption;
- deadlock caused by UI dialogs waiting on an audio callback that waits on the UI thread.

### ENG-052 Generation Identity

Reused voice slots shall carry generation/identity state sufficient to reject stale release events after voice stealing.

---

## 8. Unicode and Paths

### ENG-060 Windows Paths

User-facing filesystem paths shall use Unicode-capable Windows APIs/representations.

A valid path containing non-ASCII characters must not be corrupted by narrowing to the active ANSI code page.

### ENG-061 Project Text Files

Project-owned JSON, YAML, Markdown, and event files are UTF-8. UTF-8 BOM may be accepted but emitted JSON shall not require a BOM.

---

## 9. Error Handling

The application shall handle at minimum:

- top-level window creation failure;
- backbuffer allocation/reallocation failure;
- WinMM device-open failure;
- WinMM buffer preparation/submission failure;
- settings parse/read/write failure;
- recording file open/write/finalize failure;
- invalid CLI input;
- invalid event/settings/locscan config schemas.

A missing audio device is nonfatal to the GUI: the application shall remain usable for visual/input/chord/settings functions and display an audio-unavailable status. Live recording is disabled in this state. CLI offline rendering remains available.

---

## 10. Determinism

Given equal deterministic inputs, the following shall be deterministic:

- note parsing and frequency calculation within floating tolerance;
- octave/transpose mapping;
- chord recognition and label;
- voice-stealing choice;
- offline synthesis PCM bytes;
- settings validation/normalization;
- `locscan` deterministic JSON.

No random phase, random detune, or random noise is required in v1.0 synthesis.

---

## 11. Warnings and Undefined Behavior

The project shall support a warnings-enabled build.

Known warnings indicating uninitialized values, out-of-range conversion, missing returns, suspicious pointer lifetime, or buffer misuse shall not be left unresolved.

Mandatory tests shall include boundary cases designed to expose out-of-bounds indexing and integer-size mistakes in WAV/backbuffer calculations.

---

## 12. Generated and Binary Artifacts

Generated source output may not replace hand-written required subsystems.

If small generated lookup tables are used:

- generator source/config must be delivered;
- generated files must be marked;
- `locscan` must exclude them from human-authored line counts.

Runtime WAV files, screenshots, build output, PDBs, logs, and test-result artifacts are not source code and are excluded by default from `locscan` human-authored totals.
