# Compact Human Acceptance Checklist

Use this after implementation. Mark every line `PASS`, `FAIL`, `UNVERIFIED`, or `NOT_IMPLEMENTED`.

## A. Build / Products

- [ ] A01 Native `handmade_piano.exe` product exists.
- [ ] A02 Native console `piano_cli.exe` exists.
- [ ] A03 Native console `locscan.exe` exists.
- [ ] A04 Mandatory automated tests build/run.
- [ ] A05 Production/test code is C17, not C++ substitution.
- [ ] A06 No prohibited GUI/audio/parser/test framework bundled.

## B. Piano Core

- [ ] B01 Exactly two complete visible octaves / 24 semitone positions.
- [ ] B02 Default range is C4–B5.
- [ ] B03 Octave values exactly -2,-1,0,+1 with bounds disabled.
- [ ] B04 Transpose covers every integer -12..+12 with bounds disabled.
- [ ] B05 Existing held notes do not retune when octave/transpose changes.
- [ ] B06 Pointer press/release works with black-key priority.
- [ ] B07 Dragging while held does not create glissando notes.
- [ ] B08 Computer keyboard default mapping works.
- [ ] B09 OS key repeat does not create duplicate notes.
- [ ] B10 Focus/capture loss leaves no stuck input.
- [ ] B11 Opening Settings releases performance inputs/Space safely without stuck state.

## C. Audio / Sustain

- [ ] C01 48kHz stereo 16-bit PCM core.
- [ ] C02 Five-partial deterministic synth, not sample playback.
- [ ] C03 Exactly 16 voice slots.
- [ ] C04 Deterministic voice stealing order works.
- [ ] C05 Stale note-off cannot kill reused voice slot.
- [ ] C06 Sustain UI latch works.
- [ ] C07 Space momentary pedal works with OR semantics.
- [ ] C08 Release presets are 150/600/1800 ms.
- [ ] C09 Master volume 0–100, default 70.
- [ ] C10 Audio-device failure leaves non-audio GUI usable.

## D. Chords

- [ ] D01 Live chord display exists.
- [ ] D02 All 19 required templates implemented for all 12 roots.
- [ ] D03 Exact-match rule rejects unsupported extra notes.
- [ ] D04 Inversions/slash bass work.
- [ ] D05 Duplicate octaves do not break recognition.
- [ ] D06 Sustained-but-released notes leave recognition held set.
- [ ] D07 CR regression + exhaustive tests pass.

## E. Custom Mapping / Settings

- [ ] E01 Custom-rendered Settings modal exists.
- [ ] E02 All 24 mappings viewable/editable.
- [ ] E03 Capture mode does not play note.
- [ ] E04 Duplicate key enters explicit conflict flow.
- [ ] E05 Clear invalidates Save until complete.
- [ ] E06 Restore Defaults matches factory table exactly.
- [ ] E07 Save is atomic; failure preserves previous valid mapping.
- [ ] E08 Restart reloads mapping.
- [ ] E09 Malformed/unsupported settings fall back to full defaults + warning.
- [ ] E10 Captured/rebound physical key cannot accidentally play until matching key-up.

## F. Recording / CLI

- [ ] F01 Record opens native Save dialog and Cancel is harmless.
- [ ] F02 Recording state shows REC + elapsed MM:SS.
- [ ] F03 Recorded data is post-volume live mixer PCM.
- [ ] F04 Stop/finalize produces valid WAV.
- [ ] F05 Shutdown during recording attempts finalization before audio teardown.
- [ ] F06 Unicode output path works when testable.
- [ ] F07 `piano_cli pitch` contract works.
- [ ] F08 `piano_cli chord` contract works.
- [ ] F09 `piano_cli mapping validate` works.
- [ ] F10 `piano_cli render` produces exact-duration WAV without GUI/audio device.
- [ ] F11 `piano_cli diag` fixed constants correct.
- [ ] F12 `piano_cli test` returns failure on failing mandatory test.
- [ ] F13 CLI canonical note grammar/range errors are rejected without clamp/wrap.
- [ ] F14 Live audio failure during recording exits RECORDING safely and attempts valid finalization.
- [ ] F15 Live recording starts/stops on complete 256-frame buffer boundaries.

## G. Custom UI / Visuals

- [ ] G01 App uses application-owned 32-bit backbuffer/custom controls.
- [ ] G02 Dark palette matches specification.
- [ ] G03 Hover elevation works.
- [ ] G04 Press feedback works.
- [ ] G05 Pointer-origin ripple works/clips.
- [ ] G06 Accent border glow works.
- [ ] G07 Release selector capsule slides.
- [ ] G08 Modal opens with 220ms scale+opacity easing.
- [ ] G09 Modal backdrop progressively dims to 0.52.
- [ ] G10 Modal backdrop actually blurs app content to 12 logical px.
- [ ] G11 Settings sticky header collapses 72->52 over 0..96 scroll.
- [ ] G12 Frost blur/shadow vary continuously with scroll.
- [ ] G13 Volume slider custom drag/capture works.
- [ ] G14 No normal stock Windows widget substitutes required custom UI.

## H. DPI / Resize

- [ ] H01 Explicit per-monitor DPI-aware behavior.
- [ ] H02 100% visual/hit acceptance.
- [ ] H03 125% visual/hit acceptance.
- [ ] H04 150% visual/hit acceptance.
- [ ] H05 White/black piano geometry has no cumulative rounding drift.
- [ ] H06 DPI change recomputes hit/layout/backbuffer.
- [ ] H07 DPI change does not reset musical/recording state.
- [ ] H08 Default 1280×720 and minimum 960×600 logical layouts usable.

## I. `locscan`

- [ ] I01 C17 standalone utility.
- [ ] I02 JSON config parser works, including required escapes/Unicode surrogate handling.
- [ ] I03 specified YAML subset parser works.
- [ ] I04 glob semantics/precedence correct.
- [ ] I05 binary NUL probe works.
- [ ] I06 Unicode path traversal works.
- [ ] I07 reparse directories are not followed.
- [ ] I08 physical line definition handles empty/LF/CRLF/no-final-LF.
- [ ] I09 deterministic JSON is byte-stable.
- [ ] I10 BOM-only UTF-8 file counts as 0 lines; invalid UTF-8 fails complete scan.
- [ ] I11 generated/result evidence outputs are excluded by default.
- [ ] I12 final project line-count report included.

## J. Delivery Integrity

- [ ] J01 Architecture/build/run/test docs included.
- [ ] J02 Acceptance report includes every release gate.
- [ ] J03 Failed and UNVERIFIED items are not hidden.
- [ ] J04 Visual evidence present where environment permits.
- [ ] J05 Audio/CLI/WAV evidence present where environment permits.
- [ ] J06 Build/cache/runtime junk omitted from final source delivery.
- [ ] J07 No placeholder/mock/dead UI presented as completed behavior.
