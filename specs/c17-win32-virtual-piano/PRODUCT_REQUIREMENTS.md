# Product Requirements

## 1. Product Definition

The product is a native Windows desktop virtual piano named **Handmade Piano**.

It shall behave as an interactive application, not a static visual demo. The primary performance surface must allow real-time note triggering, simultaneous notes, sustain, transpose, octave changes, chord identification, and direct audio recording.

All defaults and fixed scope decisions are defined in `SCOPE_FREEZE.md`.

---

## 2. Main Window and Piano Range

### PR-001 Two Complete Visible Octaves

The main piano shall always render 24 semitone positions: C4–B5 at octave shift 0.

It contains:

- 14 white keys;
- 10 black keys.

The black-key pattern in each octave is exactly:

- C# and D#;
- gap between E and F;
- F#, G#, A#;
- gap between B and the next C.

### PR-002 Pitch Formula

For a displayed base MIDI-style pitch `P`:

`effective_pitch = P + 12 * octave_shift + transpose`

where:

- `P` is 60–83;
- `octave_shift` is one of `-2,-1,0,+1`;
- `transpose` is an integer `-12..+12`.

The effective result must be in 21–108.

### PR-003 Display Labels

Piano key labels reflect the octave-shifted **display note before transpose**.

Example:

- default leftmost key: C4;
- octave shift +1: leftmost key label becomes C5;
- transpose +2 does not rename that key to D5.

The current transpose value is displayed separately.

---

## 3. Octave Controls

### PR-010 Required Controls

The main control area shall provide:

- Octave Down;
- Octave Reset;
- Octave Up.

The current value shall be visible as `-2`, `-1`, `0`, or `+1`.

### PR-011 Bounds

At `-2`, Octave Down is visibly disabled and does nothing.

At `+1`, Octave Up is visibly disabled and does nothing.

Reset sets octave shift to `0`.

### PR-012 Existing Notes

Changing octave shift does not retune existing held/sounding voice instances.

It changes only future note-on events and visible note labels/range.

---

## 4. Transpose Controls

### PR-020 Required Controls

The main control area shall provide:

- Transpose Down: exactly one semitone per activation;
- Transpose Reset;
- Transpose Up: exactly one semitone per activation.

The current value shall be visibly formatted with sign for non-zero values, for example `+3` or `-5`.

### PR-021 Bounds

At `-12`, Transpose Down is disabled.

At `+12`, Transpose Up is disabled.

Reset sets transpose to `0`.

### PR-022 Existing Notes

Changing transpose does not retune existing note instances.

New note-on events capture the new transpose value.

Chord recognition follows the effective pitches of currently held note instances, not the latest global transpose setting retroactively.

---

## 5. Pointer Piano Input

### PR-030 Primary Pointer Only

The primary/left pointer button triggers piano notes.

Right/middle buttons do not trigger notes.

### PR-031 Hit-Test Priority

Because black-key rectangles overlap the upper area of white-key rectangles, hit testing in the overlap region shall test black keys first.

A single pointer-down can own at most one piano note.

### PR-032 Pointer Ownership

On pointer-down over an enabled piano key:

1. capture that key identity;
2. generate exactly one logical note-on;
3. visually mark that key held by pointer input;
4. retain ownership until pointer-up, capture loss, focus loss, or window destruction.

Moving to another key while still held does not generate another note-on.

Pointer-up anywhere ends the originally owned note.

### PR-033 Capture Loss

Unexpected pointer-capture loss shall release the pointer-owned note exactly once.

---

## 6. Computer Keyboard Input

### PR-040 Default Mapping

The default mapping is fixed and defined in `DATA_FORMATS.md`.

It covers every displayed semitone position 0–23.

### PR-041 Physical Identity

Mappings use Windows keyboard scan-code identity plus the extended-key flag, not localized character text.

The UI may display a localized human-readable key label, but persisted identity remains scan-code based.

### PR-042 Auto Repeat

After the first physical key-down for a mapped key, repeat key-down messages while that physical key remains held shall not create additional logical note-on events.

### PR-043 Reserved Keys

The following are reserved and cannot be assigned to piano positions:

- Escape: cancel/close current modal or binding capture;
- Space: momentary sustain pedal.

### PR-044 Space Sustain Pedal

Space key-down sets `space_pedal_down = true`.

Space key-up sets it false.

Effective sustain is:

`ui_sustain_latched OR space_pedal_down`

Space auto-repeat has no additional effect.

### PR-045 Focus Loss

On focus loss:

- every computer-keyboard held input is logically released;
- `space_pedal_down` becomes false;
- the UI sustain latch remains unchanged;
- chord recognition recomputes;
- no keyboard key may remain visually pressed because of a lost key-up.

---

## 7. Multi-Source Ownership

### PR-050 Independent Note Instances

Pointer and physical keyboard presses are independent input instances.

If two sources trigger the same effective pitch, each note-on owns its own voice identity if a voice is available.

Releasing one source must not release the other source's voice.

### PR-051 Visual Held State

A piano key is visually in held/pressed state while at least one active input instance owns its displayed position.

Voice stealing does not by itself clear logical input ownership.

---

## 8. Polyphony and Voice Limit

### PR-060 Fixed Voice Pool

The synthesizer contains exactly 16 simultaneous voice slots.

### PR-061 Deterministic Voice Stealing

If all 16 slots are occupied, a new note-on steals one slot using this order:

1. oldest voice already in RELEASE;
2. otherwise oldest sustain-latched voice whose input is no longer physically held;
3. otherwise oldest remaining voice.

"Oldest" is the lowest monotonically increasing voice-start sequence number.

The stolen voice becomes invalid immediately and its slot receives a new generation/identity.

A later stale note-off for the stolen voice must not release the replacement voice.

---

## 9. Sustain

### PR-070 UI Sustain Toggle

The main UI contains a visible latched Sustain toggle.

Default: off.

### PR-071 Effective Sustain

Effective sustain is true while either:

- UI sustain latch is on; or
- Space pedal is held.

### PR-072 Release Under Sustain

When an input note is released while effective sustain is true:

- it is removed immediately from the held-note set used by chord recognition;
- its voice becomes sustain-latched and continues sounding;
- it does not begin release until effective sustain becomes false.

### PR-073 Sustain Ends

When effective sustain transitions true -> false, every sustain-latched voice whose input is no longer held enters RELEASE using the current release preset captured at that transition.

---

## 10. Release Control

### PR-080 Presets

The UI contains exactly three release presets:

- Short: 150 ms;
- Medium: 600 ms;
- Long: 1800 ms.

Default: Medium.

A three-segment control with sliding capsule indicator shall be used.

### PR-081 Capture Rule

A voice captures its release duration when it enters RELEASE.

Changing the preset does not alter voices already in RELEASE.

---

## 11. Master Volume

### PR-090 Range and Default

The UI contains a master-volume slider:

- integer range: 0–100;
- default: 70;
- step for keyboard/button adjustment if provided: 1.

The runtime gain is `volume / 100.0`.

### PR-091 Zero Volume

At volume 0:

- output PCM is silent after master gain;
- input state, voice lifecycle, visuals, chord recognition, and recording state still run normally;
- recorded PCM is silent because recording captures the post-volume mixed stream.

---

## 12. Chord Display

### PR-100 Live Update

The main window contains a prominent live chord label.

It updates after every held-note-set change.

The recognition contract is in `CHORD_RECOGNITION.md`.

### PR-101 No Chord

When there is no supported exact match, the chord label is exactly the em dash character:

`—`

One-note and two-distinct-pitch inputs also display `—`.

---

## 13. Keyboard Mapping Settings

### PR-110 Settings Surface

Opening Settings first releases all active pointer/keyboard performance inputs and the Space momentary pedal through the state rules in `STATE_MODEL.md`; the UI sustain latch is preserved.

The custom settings surface shall allow the user to:

- view all 24 mappings;
- choose one position;
- begin key capture;
- assign an eligible physical key;
- clear a mapping;
- resolve a conflict through explicit Replace action;
- Restore Defaults;
- Save;
- Cancel.

### PR-111 Temporary Edit Model

Opening settings creates an editable candidate mapping copy.

Changes do not affect live performance until Save succeeds.

Cancel discards all candidate changes.

### PR-112 Capture

While waiting for a binding:

- the target row is visibly marked;
- the next eligible non-repeat key-down is captured;
- that event does not play a piano note;
- the captured physical identity remains suppressed from piano triggering until matching key-up even if the modal closes first;
- Escape cancels capture without changing the candidate mapping.

### PR-113 Conflict

If the captured physical key already belongs to another position:

- show both conflicting positions;
- do not mutate the candidate mapping yet;
- offer explicit Replace and Cancel Conflict actions.

Replace removes the old assignment and assigns the captured key to the target position.

### PR-114 Save Validity

Save is enabled only when all 24 positions have exactly one valid, unique, non-reserved key binding.

### PR-115 Persistence

On successful Save, write the v1 settings schema in `DATA_FORMATS.md` atomically, update the active mapping only after persistence succeeds, and close the Settings modal.

Malformed or unsupported persisted settings on next startup shall:

- not prevent application startup;
- load factory defaults;
- show a nonfatal warning in the settings surface or main status area.

---

## 14. Recording

### PR-120 Start

Pressing Record while idle opens the native Windows Save dialog.

- file filter includes WAV;
- default extension is `.wav`;
- overwrite confirmation is required;
- cancel leaves recording idle and creates no recording session.

Recording begins only after a valid destination is accepted and opened successfully.

### PR-121 Active State

While recording:

- Record changes to a Stop action;
- a visible red recording dot/icon and the text `REC` are shown;
- elapsed time `MM:SS` is displayed;
- elapsed time is derived from recorded sample frames.

### PR-122 Content

Recording writes the exact post-master-volume PCM frames produced by the live mixer.

It does not record microphone input.

### PR-123 Stop

A Stop request is latched by the recording state machine. Recording accepts no mixer buffers after the next completed **256-frame live mixer buffer boundary**. The accepted final buffer, if any, is fully appended before WAV finalization. The header is then finalized and recording returns to idle.

This rule makes the recording stop point deterministic at the live buffer granularity and prevents partial interleaved frames.

### PR-124 Empty Recording

An accepted recording that is stopped before any non-silent notes still produces a valid PCM WAV file. It may contain zero or silent data frames depending on elapsed time, but its header must be valid.

### PR-125 Failure

Recording open/write/finalize failure:

- does not terminate live audio unless the same underlying failure makes audio impossible;
- enters visible recording error state/message;
- clears active recording ownership;
- leaves the application usable.

### PR-126 Shutdown

If the program is closed while recording, it shall stop accepting input, finalize the WAV if possible, then tear down audio and other resources.

---

## 15. CLI / Headless Companion

### PR-130 Required Separate Console Program

`piano_cli.exe` is mandatory.

It must run ordinary non-GUI commands without creating the main window or opening a physical audio device.

The exact command contract is fixed in `CLI_HEADLESS_SPEC.md`.

### PR-131 Shared Core

The CLI and GUI shall share the same production implementations of:

- note parsing/pitch conversion;
- chord recognition;
- settings validation;
- synthesis/envelope/mixer;
- WAV writer.

A simplified CLI-only fake implementation is prohibited.

---

## 16. Window, Resize, and DPI

### PR-140 Default and Minimum Logical Size

At 96 DPI:

- default client size: 1280 × 720 logical pixels;
- minimum client size: 960 × 600 logical pixels.

Equivalent physical pixel sizes scale with DPI.

### PR-141 Resize

The main window is resizable and shall not expose client sizes below the specified logical minimum.

At supported sizes:

- all 24 piano keys remain available;
- core controls remain visible and operable;
- no mandatory label is clipped beyond recognition.

### PR-142 DPI

The application shall be DPI-aware and satisfy `DPI_SCALING.md` at 100%, 125%, and 150%.

DPI change shall not alter musical state.

---

## 17. Responsiveness and Safety

### PR-150 Event Handling

Visual key press state and logical note-on are updated in the same input-message handling cycle.

No animation waits before scheduling the note.

### PR-151 No UI Blocking in Audio Path

Audio generation/refill shall not call modal UI, filesystem dialogs, or expensive UI rendering.

### PR-152 Focus/Capture Recovery

Focus loss, pointer capture loss, close, or equivalent interrupted input paths shall not leave logically stuck keyboard/pointer notes.

---

## 18. Application Exit

### PR-160 Shutdown Order

Shutdown shall perform these semantic phases:

1. stop accepting new input;
2. finalize active recording if possible;
3. stop/close audio submission safely;
4. ensure callbacks/worker activity cannot access freed memory;
5. release graphics/window resources;
6. free owned memory;
7. terminate process normally.

No intentionally orphaned worker thread or audio callback may remain.
