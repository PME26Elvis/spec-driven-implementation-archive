# 07 — Replay, Headless Simulation, and Command-Line Interfaces

## 1. Replay definition

Replay is a deterministic logical input trace, not a captured video. It records enough information to reproduce gameplay against an identified authored table.

## 2. Replay file

Extension: `.pbr`  
Encoding: UTF-8 text  
Line endings: LF/CRLF accepted  
Comments: line beginning `#`

First non-comment line:

`PINBALL_REPLAY 1`

## 3. Required replay metadata

- scene fingerprint;
- physics version/fixed-timestep identity;
- replay seed;
- optional human-readable label.

## 4. Scene fingerprint

Implementation SHALL compute deterministic fingerprint over canonical authored scene content. Minimum required algorithm is FNV-1a 64-bit or stronger deterministic hash. Cryptographic collision resistance is not required.

Canonicalization method must be documented. Strict verification blocks/fails on mismatch and MUST NOT silently claim success.

## 5. Replay logical actions

Required actions:

- `LEFT_FLIPPER_DOWN`
- `LEFT_FLIPPER_UP`
- `RIGHT_FLIPPER_DOWN`
- `RIGHT_FLIPPER_UP`
- `LAUNCH_DOWN`
- `LAUNCH_UP`

Pause itself need not be recorded because it does not advance simulation time.

Replay MUST NOT depend on raw Win32 message timestamp or hardware scan code.

## 6. Replay event syntax

Normative:

`STEP <index> <ACTION>`

Example:

```text
PINBALL_REPLAY 1
scene_hash = 7f0a0b8d52c9e314
seed = 12345
physics_version = 1

STEP 0 LAUNCH_DOWN
STEP 180 LAUNCH_UP
STEP 720 LEFT_FLIPPER_DOWN
STEP 748 LEFT_FLIPPER_UP
```

Same-step events execute in file order unless the implementation documents and writes a stricter canonical action order.

## 7. Recording

Play Mode SHALL expose start/stop replay recording in visible UI.

Normative v1 behavior: recording begins from a fresh deterministic game session. If requested mid-session, user is prompted/clearly informed that session will restart.

Recording captures logical transitions at exact fixed-step indices.

## 8. Saving recorded replay

Stopping recording allows `.pbr` save. If writing fails, in-memory replay remains available until explicitly discarded or application exits through a warning path.

## 9. Playback

Playback:

- resets runtime to fresh state;
- verifies scene fingerprint;
- disables conflicting direct gameplay input;
- applies recorded actions at exact steps;
- permits pause and Physics Inspector;
- permits Single Step;
- permits real-time playback-speed changes without changing result;
- displays replay progress/current step.

## 10. Replay verification summary

At completion compute:

- final fixed-step index;
- final score;
- final active-ball count;
- final remaining turns;
- final game state;
- event/diagnostic counters;
- deterministic final-state fingerprint.

## 11. Headless mode

Product MUST provide headless mode without creating Win32 window. It may be main binary mode or separate executable.

Required semantic forms:

```text
pinball.exe --headless <scene.pbt> --steps <N>
pinball.exe --headless <scene.pbt> --replay <trace.pbr>
```

Equivalent documented binary name is acceptable.

## 12. Headless JSON output

Required machine-readable final state fields:

- `ok` boolean;
- `scene_hash` string;
- `physics_version` integer;
- `steps_executed` integer;
- `simulation_time` number;
- `score` integer;
- `combo_multiplier` integer;
- `override_multiplier` integer;
- `active_ball_count` integer;
- `turns_remaining` integer;
- `game_state` string;
- `diagnostics` object;
- `balls` array sorted by runtime ID.

Each ball entry includes at least runtime ID, position, velocity, and alive/drained state where retained in output.

## 13. Checkpoint output

Headless mode SHALL support checkpoint sampling by either:

- every K steps; or
- explicit step index list.

Checkpoint output is JSON or JSON Lines and must be deterministic.

## 14. Exit status

Non-zero required for:

- scene parse failure;
- scene semantic Error when simulation requested;
- replay parse failure;
- strict scene/replay fingerprint mismatch;
- internal non-finite physics state;
- failed expected comparison in regression mode;
- invalid command-line arguments.

Successful completed simulation returns 0.

## 15. GUI/headless equivalence

For same scene, seed, and replay trace:

- GUI replay and headless replay match simulation-step state within deterministic tolerance;
- score/event/discrete state match exactly;
- both invoke same production physics/event modules.

## 16. Replay validator

Required capability:

1. load scene;
2. load replay;
3. verify scene fingerprint;
4. execute replay headlessly;
5. optionally compare expected summary/checkpoints;
6. report PASS/FAIL plus mismatches;
7. return non-zero on verification failure.

May be a dedicated `replaycheck` binary or a clearly documented mode of `simcheck`.

## 17. Deterministic state fingerprint

Regression tooling SHOULD produce a deterministic 64-bit or stronger fingerprint over canonical runtime state. It MUST NOT include pointer addresses, uninitialized padding, wall-clock timestamps, or GUI state.

## 18. Replay corruption handling

Reject:

- unsupported header;
- malformed metadata;
- malformed/decreasing step index;
- unknown action;
- step beyond documented safety limit;
- malformed scene hash;
- truncated partial record.

A replay load error never mutates authored scene.

## 19. Replay scale

Implementation MUST safely support replay execution of at least 10,000,000 fixed steps subject to runtime availability. A higher documented hard cap is allowed.

## 20. Command help

Each required CLI program/mode provides `--help` or equivalent. Unknown option returns non-zero with readable error/help hint.

## 21. No display dependency

Headless simulation, scene validation, replay validation, locscan, and unified non-GUI tests MUST run without creating HWNDs, initializing the custom GUI/render path, opening the clipboard, or requiring an interactive desktop session. Linking a system DLL is not itself a GUI dependency; invoking GUI-only initialization is.

## 22. v1.0 logical actions

Replay input vocabulary additionally includes `NUDGE_LEFT`, `NUDGE_RIGHT`, and `NUDGE_UP`. Tilt itself is derived runtime state and is never recorded as a forced result.

## 23. Trace export

Headless/replay execution SHALL support production Event Trace and Collision Trace export as defined in document 22. Enabling trace output SHALL NOT change fingerprints.

## 24. Determinism comparison

The delivered engineering utilities SHALL provide `detcompare` capability for first-divergence reporting across checkpoint/trace outputs.

## 25. Current semantic fingerprint

Format-2 scene fingerprint is computed from normalized current semantic model after legacy migration and excludes editor view state, autosave metadata, backing-file metadata, and wall-clock recovery timestamp.

## 26. Runtime fingerprint expansion

Runtime fingerprint includes new mechanism states, sensor occupancy, nudge/Tilt state, deterministic PRNG state if used, and pending timers/events per document 20.


## 23. Windows command-line and encoding contract

The Windows CLI boundary SHALL preserve Unicode scene/replay/output paths. Implementations may choose their Win32 argument-decoding mechanism, but a Chinese path supplied from a Unicode-capable terminal/shell must reach the same UTF-8 core path value without ACP/code-page corruption.

Machine-readable JSON output is UTF-8. A UTF-8 BOM is not required and SHOULD NOT be emitted. Human console diagnostics may use a Unicode-capable Windows console path but must not alter deterministic JSON bytes based on the active ANSI code page.

Normal GUI launch SHALL not leave an unwanted console window visible. A separate CLI/headless executable is permitted if this keeps GUI and console subsystem behavior clean, provided all required production modules are shared.
