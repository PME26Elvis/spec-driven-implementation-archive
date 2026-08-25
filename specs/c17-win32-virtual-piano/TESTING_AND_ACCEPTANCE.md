# Testing and Acceptance Specification

## 1. Test Implementation Rule

Mandatory automated tests shall be implemented in C17 without third-party test frameworks.

Tests return non-zero on failure and print failing IDs.

A missing implementation is a failure, never an automatic skip/pass.

Core modules shall be callable without opening the GUI or physical audio device.

---

## 2. Pitch / Musical State Tests

### PITCH-001 Default C4
Position 0, octave 0, transpose 0 -> MIDI 60 / C4.

### PITCH-002 Transpose +1
Position 0 -> 61 / C#4.

### PITCH-003 Transpose -1
Position 0 -> 59 / B3.

### PITCH-004 Transpose +12
Position 0 -> 72 / C5.

### PITCH-005 Transpose -12
Position 0 -> 48 / C3.

### PITCH-006 Octave +1
Position 0 -> 72.

### PITCH-007 Combined
Position 0, octave +1, transpose -3 -> 69 / A4.

### PITCH-008 Minimum Legal Combination
Position 0, octave -2, transpose -12 -> MIDI 24; valid.

### PITCH-009 Maximum Legal Combination
Position 23, octave +1, transpose +12 -> MIDI 107; valid.

### PITCH-010 Frequency A4
69 -> 440 Hz within relative error 1e-9.

### PITCH-011 Frequency A3
57 -> 220 Hz within relative error 1e-9.

---

## 3. Input / Ownership Tests

### NOTE-001 Keyboard Note On/Off
One down -> one held instance/voice; one up -> held cleared.

### NOTE-002 Auto Repeat
Repeated key-down without key-up -> no duplicate note-on.

### NOTE-003 Same Pitch Two Sources
Pointer and keyboard trigger same position; releasing one leaves the other held/voice state intact.

### NOTE-004 Focus Loss
All keyboard-held instances clear; chord recomputes; no stale key-up corrupts later note.

### NOTE-005 Pointer Capture Loss
Pointer-owned note ends exactly once.

### NOTE-006 No Glissando
Pointer-down C, drag over D/E, pointer-up -> only C note-on/off occurred.

### NOTE-007 Black-Key Hit Priority
Overlap coordinate selects black key.

### NOTE-008 Space Sustain Repeat
Space repeat messages do not repeatedly toggle sustain.

### NOTE-009 Settings Entry Releases Performance Inputs
With pointer/keyboard notes and Space pedal active, opening Settings ends all performance input ownership exactly once, sets Space pedal false, preserves UI sustain latch, and recomputes chord without stuck notes.

---

## 4. Sustain / Voice Tests

### POLY-001 16 Voices
16 overlapping note-ons occupy all 16 slots without corruption.

### POLY-002 Steal Releasing First
With at least one RELEASE voice, 17th note steals oldest RELEASE voice.

### POLY-003 Steal Sustain-Latched Second
With no RELEASE but a non-held sustained voice, steal oldest such voice.

### POLY-004 Steal Oldest Active Last
With 16 held active voices, steal lowest start sequence.

### POLY-005 Stale Note-Off
Release event for a stolen generation does not release replacement voice.

### POLY-006 Repeated Overload
At least 1000 over-limit note-on/off operations preserve invariants.

### SUS-001 UI Latch
Latch on keeps released voice sustained.

### SUS-002 Space Pedal
Space down sustains; Space up releases no-longer-held voices when UI latch is off.

### SUS-003 OR Semantics
UI latch on + Space down/up remains sustained until UI latch turns off.

### SUS-004 Chord Exclusion
Sustained-but-released pitch leaves chord held set immediately.

---

## 5. Envelope / PCM Tests

### AUD-001 Format Constants
48000 Hz, stereo, int16, 256-frame buffers, four live buffers, 16 voices.

### AUD-002 Silence
No active voices -> every PCM sample zero.

### AUD-003 Attack Bounds
At attack start amplitude 0; by 8 ms reaches approximately 1 before other damping, within one sample tolerance.

### AUD-004 Decay Boundary
After attack + 700 ms, envelope base reaches approximately 0.55.

### AUD-005 Release Durations
Short/Medium/Long terminate within one sample of 150/600/1800 ms after release entry.

### AUD-006 Release Capture
Changing preset after release begins does not change that voice's captured release duration.

### AUD-007 Volume Zero
Active voice + volume 0 -> output PCM all zero.

### AUD-008 16-Voice Bounded
All int16 samples remain valid; pre-quantized safety value stays within specified clamp.

### AUD-009 Deterministic Render
Identical event file rendered twice -> byte-identical WAV.

### AUD-010 Exact Duration
1000 ms render -> exactly 48000 frames and 192000 data bytes.

### AUD-011 Same-Time Event Order
Equal timestamp events execute in JSON array order.

### AUD-012 Device-Open Failure
Injected/simulated failure enters nonfatal GUI audio-unavailable state and disables live Record.

### AUD-013 Shutdown Safety
Repeated shutdown/late completion simulation does not double-free/use freed state.

### AUD-014 Live Submit Failure
Injected WinMM prepare/submit failure after startup disables the live path safely. If recording is active, recording does not remain RECORDING and safe finalization/error behavior follows `SETTINGS_AND_RECORDING.md`.

---

## 6. Chord Tests

All CR-001 through CR-034 plus the generated exhaustive 19×12 root-position matrix and required inversion coverage in `CHORD_RECOGNITION.md` are mandatory.

---

## 7. Mapping / Settings Tests

### MAP-001 Factory Table
All 24 entries exactly match `DATA_FORMATS.md`.

### MAP-002 Unique / Reserved
No duplicate; no Space/Escape.

### MAP-003 Capture Free Key
Candidate updates, active mapping does not update before Save.

### MAP-004 Conflict Detection
Occupied key enters CONFLICT without mutating candidate.

### MAP-005 Replace Conflict
Explicit Replace moves binding and preserves uniqueness.

### MAP-006 Cancel Capture
Candidate unchanged for that attempt.

### MAP-007 Clear Disables Save
Unbound position makes candidate invalid.

### MAP-008 Restore Defaults
Candidate equals exact factory table.

### MAP-009 Save / Reload
Persist, reload, normalized mapping identical.

### MAP-010 Malformed JSON
Startup uses full defaults + warning; no partial apply.

### MAP-011 Unsupported Schema
Same safe fallback.

### MAP-012 Atomic Save Failure
Existing valid file remains valid/unchanged after simulated replace failure.

### MAP-013 Unicode Settings Path
Unicode path operations do not corrupt filename where environment permits.

### MAP-014 Key Auto Repeat in Capture
Only first non-repeat keydown is considered.

### MAP-015 Capture Does Not Play
Captured key generates no note-on.

### MAP-016 Captured Key Suppressed Until Key-Up
Capture a mapped candidate key, then close Settings before releasing the physical key. Auto-repeat/down messages do not trigger a piano note. After matching key-up, a subsequent fresh key-down may trigger according to the active saved mapping.

---

## 8. Recording / WAV Tests

### REC-001 Valid Header
RIFF/WAVE/fmt/data structure exact.

### REC-002 Header Sizes
RIFF/data sizes agree with actual file length.

### REC-003 Format Fields
PCM=1, 2 channels, 48000, byte rate 192000, block align 4, 16-bit.

### REC-004 Silent Recording
No notes -> structurally valid silent/zero-data recording according to elapsed frames.

### REC-005 A4 Recording
Contains nonzero PCM and shares bytes with the live mixer tap for the same frames.

### REC-006 Polyphonic Recording
C-E-G live mix recorded/finalized safely.

### REC-007 Volume Zero Recording
Recorded frames all zero.

### REC-008 Stop While Held
WAV stops/finalizes; live voice may continue.

### REC-009 Exit While Recording
Graceful shutdown attempts valid finalization before audio teardown.

### REC-010 Write Failure
Live audio remains safe, state enters ERROR.

### REC-011 Dialog Cancel
No session/file is created.

### REC-012 Unicode Output Path
Successful recording/finalization on non-ASCII path where permitted.

### REC-013 RIFF Size Overflow Guard
Simulated near-limit counter stops/errors rather than wraps 32-bit header fields.

### REC-014 Live Audio Failure During Recording
With recording active, inject live WinMM submit failure. The recorder accepts no indefinite future buffers, attempts safe finalization, leaves RECORDING, and surfaces recording/audio error state while unrelated GUI functionality remains usable.

### REC-015 Live Buffer Boundary Contract
Synthetic live recording start/stop around a partial timing point writes only complete 256-frame buffers. Successful PCM data length is zero or an exact multiple of 1024 bytes, with no partial leading/trailing frame buffer.

---

## 9. CLI Tests

### CLI-001 Pitch
`pitch C4 --octave 0 --transpose 2` -> D4/MIDI 62.

### CLI-002 Chord
`chord C4 E4 G4` -> C.

### CLI-003 Mapping Valid
Factory settings file validates and normalizes 24 entries.

### CLI-004 Mapping Duplicate
Exit 3 with duplicate-key error.

### CLI-005 Render
Finite fixture -> valid exact-duration WAV.

### CLI-006 Overwrite Guard
Existing output without `--overwrite` -> exit 5; existing file unchanged.

### CLI-007 Diagnostics
Constants match v1.0 fixed values.

### CLI-008 No GUI Dependency
CLI command works without main-window creation.

### CLI-009 No Audio Device for Render
Offline render never requires WinMM device open.

### CLI-010 Test Exit
Injected failing test -> exit 6 and failing ID reported.

### CLI-011 Invalid Event Order
Decreasing time_ms -> exit 3.

### CLI-012 Unknown Event Field/Type
Strict schema -> exit 3.

### CLI-013 Effective Pitch Out of Range
`pitch C8 --octave 1 --transpose 0` -> exit 3 with `pitch_out_of_range`; no clamp/wrap.

### CLI-014 Canonical Note Syntax
Canonical `A0`, `C4`, `F#5`, `C8`, and integer `69` parse successfully where legal. Flat/lowercase/noncanonical spellings such as `Bb4` or `c4` fail with exit 3.

---

## 10. `locscan` Tests

LOC-001 through LOC-025 in `DEV_TOOLS.md` are mandatory.

---

## 11. DPI / Geometry Tests

### DPI-001 96 Layout
Main layout/hit geometry valid.

### DPI-002 120 Layout
Scale=1.25; geometry valid.

### DPI-003 144 Layout
Scale=1.5; geometry valid.

### DPI-004 Round Trip
Representative logical->physical->logical error within one physical pixel equivalent.

### DPI-005 White-Key Coverage
First/last boundary equals keyboard bounds at 96/120/144; no cumulative drift.

### DPI-006 Black Pattern
10 black keys and correct 2/3 grouping at all mandatory DPI.

### DPI-007 Hit Centers
Every key center selects itself at all mandatory DPI.

### DPI-008 Modal Relayout
Synthetic DPI change while modal open preserves selected state/scroll and refreshes hit geometry.

### DPI-009 Animation Duration
Normalized progress reaches endpoints at same elapsed duration independent of DPI.

### DPI-010 Resize Minimum at 150%
Mandatory controls/keyboard remain reachable/valid at minimum logical client.

### DPI-011 Repeated Transitions
96->144->120->96 does not compound scale values.

### DPI-012 Backbuffer Size
Physical backbuffer dimensions track client size after resize/DPI transition.

---

## 12. UI State Logic Tests

These tests exercise UI state/math without requiring screenshot comparison.

### UI-001 Button Capture
Down inside/up inside activates once.

### UI-002 Release Outside
Ordinary button down inside/up outside does not activate.

### UI-003 Disabled
Disabled musical bound control does not mutate state.

### UI-004 Ripple Lifetime
Progress clamps 0..1 and expires after 360 ms.

### UI-005 Capsule Endpoint
At 180 ms final segment geometry exact.

### UI-006 Modal Open Endpoint
At 220 ms opacity=1, scale=1, blur=12, dim=0.52.

### UI-007 Modal Close Endpoint
At 160 ms modal becomes CLOSED and background hit testing re-enables.

### UI-008 Frost Progress
scroll 0 -> p0; 48 -> p0.5; >=96 -> p1 with specified interpolation endpoints.

### UI-009 Large Frame Gap
>250 ms does not yield opacity/geometry outside valid range.

### UI-010 Slider Capture
Drag outside clamps 0/100 and releases correctly.

### UI-011 Modal Elastic Bézier
The x/time mapping is finite and invertible over normalized progress 0..1. The eased y value is allowed to exceed 1 and is not required to be monotonic. Scale preserves the specified overshoot; opacity remains clamped to 0..1; both end exactly at scale 1/opacity 1 at normalized time 1.

### UI-012 Blur Reference
Small fixed pixel matrix blurred at R=1 matches direct separable clamped-edge box-blur reference within 1 channel value.

### UI-013 Scroll Wheel / Clamp
120 wheel units advance 48 logical px and clamp at both ends.

### UI-014 Scrollbar Capture
Dragging custom thumb updates the same scroll offset used by frosted progress and releases capture safely.

### UI-015 Monotonic Animation Clock
Injected monotonic elapsed-time samples produce nondecreasing normalized time progress; simulated wall-clock reversal does not affect animation timing logic.

### UI-016 Active Animation Tick Request
While an animation remains active, the scheduler requests/reposts a subsequent frame on a nominal <=17 ms cadence; completed idle state does not require continuous redraw.

---

## 13. Manual Functional Checklist Cases

### MAN-001 Rapid Note Repeat
Repeated fast use produces no stuck note/crash.

### MAN-002 3+ Simultaneous Notes
Visual held keys and chord agree.

### MAN-003 Transpose While Holding
Existing chord/pitches unchanged; new notes use new transpose.

### MAN-004 Octave While Holding
Existing voices unchanged; labels/future notes update.

### MAN-005 Sustain Toggle/Pedal
OR semantics and release behavior audible/visible.

### MAN-006 Custom Mapping
GUI rebind -> Save -> restart -> mapping persists.

### MAN-007 Recording
Record short passage -> Stop -> independently parseable WAV.

### MAN-008 Focus Loss
No stuck computer-key notes.

### MAN-009 Resize
Default/minimum/larger sizes usable.

### MAN-010 Settings Scroll
Header collapse/blur/shadow varies continuously.

---

## 14. Visual Acceptance IDs

Each is PASS/FAIL/UNVERIFIED:

- VA-001 custom dark main UI;
- VA-002 correct two-octave black/white layout;
- VA-003 3+ held keys visibly distinct;
- VA-004 hover elevation;
- VA-005 press state;
- VA-006 active ripple;
- VA-007 border glow;
- VA-008 release capsule animation/state;
- VA-009 modal scale/opacity;
- VA-010 backdrop progressive dim;
- VA-011 actual application-content blur;
- VA-012 settings header scroll-dependent blur;
- VA-013 settings header scroll-dependent shadow/collapse;
- VA-014 transpose nonzero visible;
- VA-015 sustain active visible;
- VA-016 recognized chord visible;
- VA-017 mapping list;
- VA-018 capture state;
- VA-019 conflict state;
- VA-020 recording REC/time state;
- VA-021 100% DPI reference;
- VA-022 125% DPI reference;
- VA-023 150% DPI reference.

---

## 15. Audio Manual Acceptance IDs

- AA-001 A4 audible and plausibly correct pitch;
- AA-002 semitone transpose audibly changes pitch upward/downward;
- AA-003 octave shift audibly changes octave;
- AA-004 polyphonic chord audible;
- AA-005 Short/Medium/Long tails audibly ordered;
- AA-006 sustain holds released audio;
- AA-007 release occurs when sustain ends;
- AA-008 volume 0 silent;
- AA-009 rapid playing does not produce persistent dropout/stuck tone;
- AA-010 close stops audio safely.

No physical audio device -> these may be UNVERIFIED; automated PCM tests remain mandatory.

---

## 16. Evidence Integrity

A text claim of success is not sufficient.

Final evidence must include actual test output/status, checklist, and artifacts specified in `DELIVERY_AND_DOD.md`.

Fabricated screenshots, fabricated passing logs, hand-edited test summaries misrepresenting failures, and precomputed fake CLI output are prohibited.
