# Error Handling Matrix

This matrix fixes the minimum externally observable response to common failure paths.

| ID | Failure | Required product behavior | Automated status expectation |
|---|---|---|---|
| ERR-001 | top-level window creation fails | terminate with diagnosable fatal error; no partial GUI claim | failure path test/review |
| ERR-002 | initial backbuffer allocation fails | terminate or show bootstrap fatal error; never render through null buffer | failure path test |
| ERR-003 | resize/DPI backbuffer reallocation fails | never write out of bounds; preserve old buffer if safe or enter fatal render error | DPI/backbuffer test |
| ERR-004 | WinMM device open fails | GUI remains usable for non-audio features; visible audio-unavailable status; Record disabled | AUD-012 |
| ERR-005 | WinMM prepare/submit fails after startup | stop/disable live audio safely, surface nonfatal audio error; if recording is active, finalize safely where possible and enter recording ERROR; no buffer reuse corruption | AUD-014 / REC-014 |
| ERR-006 | settings file absent | use defaults; no warning required | MAP startup fixture |
| ERR-007 | settings JSON malformed | use complete defaults; nonfatal warning; no partial mapping apply | MAP-010 |
| ERR-008 | settings schema unsupported | use complete defaults; warning | MAP-011 |
| ERR-009 | settings atomic save temporary write fails | active mapping/file unchanged; candidate remains available | MAP-012 |
| ERR-010 | settings replace/rename fails | previous valid settings preserved; active mapping unchanged; visible save error | MAP-012 |
| ERR-011 | binding captures reserved Space/Escape | reject as ineligible; remain capture or show message; do not mutate candidate | MAP suite |
| ERR-012 | binding duplicate key | enter explicit conflict state; no silent mutation | MAP-004 |
| ERR-013 | recording Save dialog canceled | return IDLE; no session/file/error | REC-011 |
| ERR-014 | recording output cannot open | recording ERROR/IDLE recovery; live audio continues; no active writer | REC-010 |
| ERR-015 | recording write short/fails | leave RECORDING; live audio continues; attempt safe finalize; visible error | REC-010 |
| ERR-016 | recording RIFF size would overflow | stop/finalize or error before 32-bit wrap | REC-013 |
| ERR-017 | app closes during recording | stop input, finalize recording if possible, then tear down audio | REC-009 |
| ERR-018 | pointer capture lost | release pointer-owned note once | NOTE-005 |
| ERR-019 | keyboard focus lost | release keyboard notes, Space pedal false, preserve UI sustain latch | NOTE-004 |
| ERR-020 | stale note-off after voice steal | ignore for replacement generation | POLY-005 |
| ERR-021 | duplicate note-off | no-op; no other voice affected | state test |
| ERR-022 | CLI unknown command/invalid flags | exit 2; usage diagnostic/JSON failure when applicable | CLI usage tests |
| ERR-023 | CLI invalid pitch/chord/event value | exit 3; stable input error | CLI input tests |
| ERR-024 | CLI event file read fails | exit 4 | CLI I/O test |
| ERR-025 | CLI output exists without `--overwrite` | exit 5; existing file byte-unchanged | CLI-006 |
| ERR-026 | CLI render output write fails | exit 5; no success JSON | CLI render failure test |
| ERR-027 | locscan invalid config syntax/schema | exit 3; useful parse/schema diagnostic | LOC-011/012 |
| ERR-028 | locscan traversal cannot complete | exit 4; partial result not reported as complete success | LOC traversal test |
| ERR-029 | locscan output write fails | exit 5 | LOC output test |
| ERR-030 | locscan reparse directory | do not follow; no loop | LOC-017 |

## 1. Error-State Principles

Errors must not be converted into fabricated PASS evidence.

Nonfatal error UI shall use the custom UI system after normal GUI initialization. A stock MessageBox is reserved for bootstrap failures where the custom renderer cannot be established.

## 2. Recovery

After a recoverable error, repeated user actions must start from a valid state. Examples:

- failed recording must not leave Record permanently believing it is active;
- failed settings Save must not swap active mapping in memory;
- audio-device failure must not cause a tight reopen loop;
- canceled key capture must not leave piano keyboard routing permanently disabled.
