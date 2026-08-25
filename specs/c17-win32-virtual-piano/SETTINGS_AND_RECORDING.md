# Settings, Mapping, and Recording Specification

## 1. Settings State Machine

States:

- CLOSED;
- OPENING;
- OPEN;
- CLOSING.

While not CLOSED, background application controls do not receive pointer activation.

Before Settings enters OPENING, the application shall end every currently held **performance input instance**:

- pointer-owned piano note;
- mapped computer-keyboard piano notes;
- momentary Space sustain pedal state.

These releases use the normal note-off/sustain semantics. The UI sustain latch is preserved. This guarantees that the modal begins with no keyboard/pointer performance ownership and chord recognition recomputes accordingly.

Every physical keyboard identity that was down when the modal began, plus every identity subsequently consumed by binding capture, is placed in a `suppressed_until_keyup` set. After the modal closes, repeat/down messages for such an identity remain ignored for piano triggering until its matching physical key-up is observed.

While Settings is not CLOSED, computer-keyboard piano triggering and the Space momentary sustain pedal are suppressed. Settings may consume navigation/capture keys instead.

---

## 2. Candidate Mapping State

On Settings open:

- copy active mapping to candidate mapping;
- `dirty = false`.

Editing candidate mapping sets dirty true.

Save:

1. validate 24 complete unique bindings;
2. atomically persist;
3. only after persistence success, copy candidate -> active;
4. clear dirty;
5. transition Settings OPEN -> CLOSING -> CLOSED.

Cancel:

- discard candidate;
- active mapping remains unchanged;
- transition Settings OPEN -> CLOSING -> CLOSED.

Escape while binding capture/conflict is active cancels only that capture/conflict interaction. Escape while binding state is IDLE performs the same discard-and-close semantics as Cancel.

If Save fails, active mapping remains unchanged and the candidate remains visible for retry/cancel.

---

## 3. Binding Capture State Machine

States:

- IDLE;
- CAPTURING(target_position);
- CONFLICT(target_position, existing_position, captured_key).

Transitions:

- Rebind -> CAPTURING;
- eligible key -> if free, update candidate and return IDLE;
- eligible key -> if occupied, enter CONFLICT without mutating candidate;
- Replace Conflict -> move binding and return IDLE;
- Cancel Conflict -> return IDLE with candidate unchanged;
- Escape during CAPTURING/CONFLICT -> cancel interaction, candidate unchanged relative to before that capture attempt.

OS key-repeat events do not retrigger capture.

A physical key accepted or considered by CAPTURING/CONFLICT remains suppressed for musical triggering until its matching key-up is observed, even if Save/Cancel closes the modal before the user releases that key.

---

## 4. Clear and Save Completeness

Clear may temporarily leave a candidate position unbound.

While any position is unbound or invalid:

- Save is disabled;
- the invalid row is visually indicated.

Restore Defaults replaces the entire candidate mapping with the factory table from `DATA_FORMATS.md` and marks it dirty.

---

## 5. Startup Settings Load

At process start:

1. create factory mapping in memory;
2. attempt to read active settings path;
3. if file absent, keep factory mapping with no error;
4. if valid v1 schema, replace factory mapping;
5. if malformed/unsupported, keep factory mapping and set a nonfatal `settings_warning` visible in UI/diagnostics.

A malformed config never partially applies.

---

## 6. Recording State Machine

States:

- IDLE;
- CHOOSING_PATH;
- STARTING;
- RECORDING;
- FINALIZING;
- ERROR.

Required transitions:

- Record click: IDLE -> CHOOSING_PATH;
- dialog cancel: CHOOSING_PATH -> IDLE;
- accepted path: -> STARTING;
- file initialized: STARTING -> RECORDING;
- Stop: RECORDING -> FINALIZING at the next completed 256-frame live mixer buffer boundary -> IDLE on success;
- open/write/finalize failure: relevant state -> ERROR;
- acknowledgement/retry/cancel from ERROR -> IDLE.

A second writer/session cannot exist concurrently.

---

## 7. WAV Format

Live recording output is standard RIFF WAVE PCM:

- `RIFF`;
- RIFF chunk size;
- `WAVE`;
- `fmt ` chunk size 16;
- format code 1 (PCM);
- channels 2;
- sample rate 48000;
- byte rate 192000;
- block align 4;
- bits per sample 16;
- `data`;
- data byte length;
- interleaved int16 PCM data.

For canonical simple output no extra metadata chunks are required.

File size fields use little-endian unsigned 32-bit RIFF lengths. Recording beyond RIFF/WAV 32-bit size capacity is not required; the recorder shall detect impending overflow and stop/finalize with an error rather than wrap size fields.

---

## 8. Recording Clock

The recording duration counter is derived from frames successfully appended to the WAV stream:

`elapsed_seconds = recorded_frames / 48000.0`

UI `MM:SS` uses floor to whole seconds. `MM` means total elapsed minutes, is displayed with at least two digits, and may exceed 59 for long recordings.


## 8.1 Live Recording Buffer Boundaries

Live recording accepts only complete 256-frame mixer buffers.

- after the output file is initialized, the first recorded data is the next complete mixer buffer generated while state is RECORDING;
- no partial leading buffer is written;
- a Stop request allows the currently committed/in-progress complete buffer to finish safely, then accepts no later buffer;
- no partial trailing interleaved frame/buffer is written.

Therefore successful live-recording PCM data length is always a multiple of `256 frames * 4 bytes/frame = 1024 bytes`, including zero bytes for a recording stopped before its first accepted buffer.

---

## 9. Write Errors

The recorder shall detect short/failed writes.

After a write failure:

- no further recording frames are considered successfully recorded;
- attempt header finalization only if safe/possible;
- recording state leaves RECORDING;
- live mixer/device path continues independently;
- error is visible.

---

## 10. Unicode Paths

The native Save dialog and file-open path shall support non-ASCII Windows paths through wide/Unicode APIs.

Mandatory test includes a destination containing non-ASCII characters when filesystem execution is available.

---

## 11. Recording and Audio-Unavailable Mode

If live audio device initialization failed:

- Record control is disabled with a visible explanation/status;
- it does not open the Save dialog;
- offline render via CLI remains the headless way to produce WAV.


## 12. Live Audio Failure During Recording

If WinMM preparation/submission fails after live audio was previously available while a recording session is active:

1. stop accepting new live recording buffers after the currently owned append operation is made safe;
2. attempt to finalize the WAV header using only successfully written frames;
3. transition recording to `ERROR`;
4. stop/disable the failed live-audio path;
5. keep the rest of the GUI usable;
6. expose both audio-unavailable and recording-error status.

The application must not leave the recording state at `RECORDING` after live audio production has been disabled.
