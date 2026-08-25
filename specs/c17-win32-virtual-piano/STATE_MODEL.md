# Application State and Event Model

## 1. Purpose

This file fixes ownership and transition semantics so GUI, audio, chord recognition, settings, and recording cannot each invent inconsistent state.

---

## 2. Global Musical Settings

Runtime state:

- `octave_shift`: -2,-1,0,+1;
- `transpose`: -12..+12;
- `release_preset`: short/medium/long;
- `master_volume`: 0..100;
- `ui_sustain_latched`: bool;
- `space_pedal_down`: bool;
- `effective_sustain = latch OR pedal`.

These settings are not persisted except keyboard mapping.

---

## 3. Input Instance

Every logical note-on creates/updates an input instance with:

- source kind: pointer or keyboard;
- source identity: pointer capture token or physical scan-code identity;
- displayed position 0..23;
- effective MIDI pitch captured at note-on;
- voice generation/handle if the sound voice still exists;
- held boolean.

Chord recognition reads held input instances.

Audio voices may outlive an input instance because of sustain/release.

---

## 4. Note-On Ordering

For a valid input press:

1. reject repeat/already-held source;
2. determine displayed position;
3. calculate effective pitch using current octave/transpose;
4. create held input instance;
5. allocate/steal voice and bind generation identity;
6. update piano visual held count;
7. recompute chord from all held input instances;
8. invalidate/render UI;
9. audio mixer observes the new voice through the synchronization strategy.

Steps may be grouped atomically, but externally visible semantics must match this ordering.

---

## 5. Note-Off Ordering

For a held input instance:

1. mark/remove logical held ownership;
2. decrement visual held count for displayed position;
3. recompute chord immediately;
4. if its voice generation still exists:
   - if effective sustain true: mark voice sustain-latched;
   - else enter RELEASE using current release preset;
5. stale/stolen generation -> no audio action.

A duplicate note-off is ignored safely and does not affect other voices.

---

## 6. Transpose/Octave Change

Changing global octave/transpose:

- validates fixed bounds;
- updates control labels/key note labels as appropriate;
- does not mutate existing input instances' effective pitches;
- does not recompute a different chord solely from the setting change;
- affects future position-based note-on events.

---

## 7. Sustain Transition

Recompute `effective_sustain` whenever UI latch or Space state changes.

Only a transition true -> false triggers action:

- every voice marked sustain-latched and not input-held enters RELEASE;
- release duration is captured from the current preset.

Changing false -> true does not alter already releasing voices.

---

## 8. Focus Loss

On window focus loss:

- release all keyboard-owned held input instances using normal note-off semantics;
- set `space_pedal_down=false` and process effective-sustain transition;
- pointer capture is released if owned by the application and pointer note is ended;
- UI latch is preserved;
- mapping capture may be cancelled for safety; if cancelled candidate mapping remains otherwise unchanged.

No subsequent late key-up may release a different new note.

---

## 9. Voice Steal vs Input Ownership

If a held note's audio voice is stolen:

- its input instance remains held;
- its visual key remains held;
- it remains in chord recognition;
- its voice handle becomes invalid/stale;
- release of that input later changes held/chord visuals but does not touch the replacement voice.

This is required to keep input truth independent from limited audio resources.

---

## 10. Modal State

Settings OPENING/OPEN/CLOSING blocks background pointer activation and all performance keyboard routing.

Before Settings enters OPENING:

1. end the active pointer piano input, if any, through normal note-off semantics;
2. release all mapped keyboard-owned held input instances through normal note-off semantics;
3. set `space_pedal_down=false` and process the resulting effective-sustain transition;
4. preserve the UI sustain latch;
5. recompute chord from the resulting held set.

Every physical keyboard identity that was down at modal entry, and every identity consumed by mapping capture/conflict, is tracked in `suppressed_until_keyup`. After Settings becomes CLOSED, such identities remain ineligible for piano note-on until matching key-up removes them from the suppression set. Auto-repeat cannot bypass this rule.

While Settings is not CLOSED, mapped piano triggering and Space momentary sustain are suppressed.

Audio voices already sounding continue according to note-off/sustain/release semantics; recording continues independently unless another recording rule stops it.

---

## 11. Recording State

Recording state is independent of musical held state.

Starting/stopping recording does not:

- release notes;
- reset transpose/octave;
- change sustain;
- clear chord.

Recording merely taps final mixed PCM while RECORDING.

---

## 12. DPI/Resize

DPI/resize changes geometry/resources only.

They do not mutate musical settings or voice pitch.

If pointer capture must be cancelled to avoid stale geometry, end that pointer input through normal note-off semantics.

---

## 13. Shutdown

Shutdown is idempotent: a repeated shutdown request must not double-free resources or re-finalize already-finalized recording state.

Late callbacks/events after shutdown begins shall be ignored or handled without touching destroyed state.
