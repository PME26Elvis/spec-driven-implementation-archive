# Audio Engine Specification

## 1. Fixed PCM Format

The v1.0 synthesis/output format is fixed:

- sample rate: 48,000 Hz;
- channels: 2;
- sample format: signed 16-bit little-endian PCM;
- interleaving: L,R,L,R,...;
- live WinMM buffer size: 256 frames;
- live buffer count: 4;
- voice slots: exactly 16.

Offline rendering and live recording use the same PCM format.

---

## 2. Pitch

A4 is MIDI-style pitch 69 and frequency 440 Hz.

For integer pitch `m`:

`frequency(m) = 440.0 * pow(2.0, (m - 69) / 12.0)`

Valid effective pitch range is 21–108 inclusive.

Automated tests shall compare representative frequencies within relative error `1e-9` before PCM quantization.

---

## 3. Deterministic Piano-Like Voice Model

Every new voice begins with phase zero and fixed velocity 1.0.

No randomness is used.

### 3.1 Harmonic Partials

A voice is the weighted sum of the first five harmonics.

Harmonic numbers:

- 1;
- 2;
- 3;
- 4;
- 5.

Base weights:

- 1.00;
- 0.50;
- 0.28;
- 0.16;
- 0.09.

Age-dependent exponential damping coefficients in inverse seconds:

- 0.20;
- 0.45;
- 0.75;
- 1.10;
- 1.50.

For voice age `t` and fundamental frequency `f`, before the amplitude envelope:

`raw(t) = sum(weight[h] * exp(-damping[h] * t) * sin(2*pi*h*f*t)) / 2.03`

where the sum is over the five listed harmonics.

Equivalent numerically stable incremental phase generation is allowed if it follows the same frequencies/weights/damping semantics.

### 3.2 Envelope

Before note release:

- ATTACK duration: 8 ms, linear from 0 to 1;
- DECAY duration: 700 ms after attack, linear from 1 to sustain level 0.55;
- SUSTAIN: 0.55 while held/sustain-latched, multiplied by the partial damping above.

On entry to RELEASE:

- capture the current envelope amplitude as `release_start`;
- linearly interpolate from `release_start` to 0 over the selected release duration;
- deactivate the voice when release time reaches the duration.

Required release durations:

- Short = 0.150 s;
- Medium = 0.600 s;
- Long = 1.800 s.

A voice that is stolen is terminated immediately without continuing its release tail.

---

## 4. Mixing

For each frame:

1. evaluate every active voice mono sample after envelope;
2. sum active voice samples as `sum`;
3. compute `x = 0.22 * sum`;
4. apply the fixed rational soft limiter `limited = x / (1.0 + fabs(x))`;
5. multiply by master volume `0..1`;
6. clamp to `[-1.0, +1.0]` as a final safety bound;
7. convert to signed 16-bit PCM;
8. write the same sample to left and right channels.

The rational limiter is continuous when active-voice count changes, prevents hard gain-normalization pumping, is bounded below full scale, and is deterministic. A hard clip used in place of the required rational limiter is not equivalent.

PCM conversion shall map:

- `+1.0` to `32767`;
- `-1.0` to `-32768` or `-32767` consistently;
- zero to zero.

The chosen negative full-scale convention must be consistent in GUI, CLI, recording, and tests.

---

## 5. Voice Identity and State

Each slot shall track at least:

- active flag;
- generation identifier;
- input-owner identifier;
- effective pitch captured at note-on;
- frequency;
- start sequence number;
- age in sample frames or equivalent time;
- envelope stage/time;
- input-held flag;
- sustain-latched flag;
- release duration and release start amplitude after release begins.

A note-off is matched by input/voice identity, never only by pitch.

---

## 6. Voice Stealing

Use PR-061 exactly.

The new voice increments/replaces generation identity before future release matching.

The diagnostic counter `voice_steal_count` increments once per stolen note-on.

---

## 7. Sustain Interaction

Input release while effective sustain is true sets the owned voice:

- `input_held = false`;
- `sustain_latched = true`;
- envelope remains in pre-release lifecycle.

When effective sustain becomes false, every sustain-latched non-held voice enters RELEASE and clears `sustain_latched`.

---

## 8. Real-Time WinMM Output

The live GUI audio path shall use WinMM wave output with four reusable buffers of 256 frames each.

Required ownership semantics:

- each buffer is prepared before submission;
- only a completed/free buffer is refilled;
- a buffer is not overwritten while owned by the audio device;
- shutdown waits or resets safely before freeing prepared headers/data;
- callback/thread handling cannot access freed application state.

The exact callback vs event/thread arrangement is implementation-defined but must satisfy these ownership rules.

---

## 9. Audio Device Failure

If device open fails:

- GUI remains running;
- piano visual state/chord/settings continue;
- audio-unavailable status is visible;
- live Record is disabled;
- no repeated tight-loop open attempts occur;
- CLI offline render is unaffected.

A test seam/failure injection or equivalent deterministic path shall verify the nonfatal state transition without requiring a physically broken device.

---

## 10. Offline Renderer

`piano_cli render` shall invoke the same note/voice/envelope/mixer/WAV code without opening WinMM.

Event time is converted to sample frame index:

`frame = time_ms * 48`

Event files must be nondecreasing by `time_ms`. Equal-time events execute in their original array order.

The required `duration_ms` defines exact output duration:

`frame_count = duration_ms * 48`

Events outside `0 <= time_ms < duration_ms` are invalid.

Voices may be truncated at the requested output duration; the renderer does not append an implicit tail beyond `duration_ms`.

---

## 11. Recording Tap

GUI recording consumes the exact final interleaved int16 frames after master volume and PCM conversion, before WinMM submission.

It must not re-synthesize notes independently.

If a live audio frame is successfully produced for output, the corresponding recording frame is byte-identical to that mixer frame.

---

## 12. Diagnostics

The shared diagnostics structure/CLI output shall expose at least:

- `sample_rate = 48000`;
- `channels = 2`;
- `bits_per_sample = 16`;
- `buffer_frames = 256`;
- `buffer_count = 4`;
- `max_voices = 16`;
- current active voices;
- voice steal count;
- audio device available boolean;
- submitted/completed buffer counters where live audio is active;
- recording state.

---

## 13. Audio Automated Acceptance

Mandatory tests include:

- exact pitch-frequency reference values;
- attack/decay/release boundary behavior;
- sustain transition;
- 16-voice allocation;
- deterministic stealing;
- stale note-off rejection;
- no-active-voice zero output;
- volume-zero zero output;
- bounded 16-voice PCM;
- byte-identical repeated offline render;
- WAV header and exact frame count;
- simulated device-open failure;
- safe shutdown state machine.

Detailed IDs are in `TESTING_AND_ACCEPTANCE.md`.
