# Task-Pack Changelog

## v1.0

Scope-frozen release after full consistency review of v0.3.

Major specification hardening:

- fixed default range to C4–B5;
- fixed octave range to -2..+1 and acoustic range A0–C8;
- fixed startup control defaults;
- fixed exact factory keyboard scan-code mapping;
- defined Space momentary sustain and Escape reservation;
- made drag-to-play, velocity, metronome, and playback explicitly out of scope;
- fixed settings path/schema and atomic-save semantics;
- fixed CLI executable, commands, exit codes, event JSON, and output expectations;
- fixed 48 kHz stereo 16-bit PCM, live buffering, 16 voices, synth harmonic/envelope model, mixing, and release durations;
- fixed voice-stealing and stale-event behavior;
- fixed 19-template chord labels, ambiguity policy, no-chord output, exhaustive test requirement;
- fixed dark UI palette, piano geometry, animation durations/easing, modal blur/dim values, frosted-scroll math, and logical window sizes;
- tightened DPI transition/geometry rules;
- defined `locscan` command, config discovery, glob semantics, traversal, binary/text rules, line definition, exit codes, and 22 tests;
- added unified state/event model;
- added data schemas;
- added compact manual checklist and traceability matrix;
- reorganized release gates and environment-limited completion statuses;
- removed unresolved v0.x open-decision document.

## v0.3

Added mandatory DPI scaling.

## v0.2

Added customizable keyboard mapping, WAV recording, and CLI/headless companion.

## v0.1

Initial substantial product/engineering baseline.
