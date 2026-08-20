# Architecture & Implementation Notes — Pinball Sandbox v1.0.0

## Overview

A single C17 codebase delivers both a **Win32 desktop editor/game** and a **headless simulator**
from one production physics/event core. The same `src/core/sim.c` (fixed-timestep deterministic
solver) is linked into the GUI app and into every headless utility, so GUI and headless can never
diverge.

## Layers

```
src/core/      scene parse/write/validate, physics (sim), events, replay, hashing, rng
src/app/       editor state, rendering commands, game loop, UI widgets (logical)
src/platform/  Win32 window/input/IME/HiDPI; DIB framebuffer present to GDI
tools/         locscan, releasecheck, scenecheck, simcheck, replaycheck, detcompare,
               framegen, pcheck, rtcheck, and the unified tests runner (tests.c)
```

## Rendering (no GPU, no framework)

- A software **DIB framebuffer** (32-bit RGBA) is the single source of client-area pixels.
- The platform layer blits the framebuffer with `BitBlt`/GDI. All required UI (panels, buttons,
  capsules, ripples, glow, frosted toolbar, modal blur) is drawn with custom 2D raster routines.
- No Direct2D/DirectWrite/GDI+/OpenGL/Vulkan, no Common Controls, no WebView. This satisfies the
  Windows Platform Binding and Dependency gates (audited in `releasecheck`).

## Physics core (deterministic)

- Fixed timestep `dt = 1/240 s`, double precision, semi-implicit (symplectic) Euler baseline.
- Ball/static and ball/ball collisions with bounded iteration count; penetration correction;
  friction/restitution; CCD swept test for high-speed thin-wall acceptance; Sensor/Drain swept
  crossing.
- All state advances only on integer step indices — playback speed and UI scale never change a
  step-indexed result (verified by the determinism suite: `fsfp 917f266a18e6387a`).
- `sim_step()` returns a status code; `sim_error` diagnostic is surfaced (no NaN/Inf in the
  1,000,000-step stress run).

## Scene & replay formats

- `PINBALL_TABLE 2` (UTF-8). 15 object types (`OBJ_BALL_SPAWN=0 … OBJ_KICKOUT=14`). Authoring
  round-trips byte-stable; malformed input is rejected with deterministic Error/Warning classes
  and a known error-code vocabulary (`PbtCode`, doc 31).
- `PINBALL_REPLAY 1` (`.pbr`): 9 logical action classes (RPL_*). The recorder captures both
  button DOWN and UP edges in order (a prior bug caused 2 replay-determinism failures; fixed in
  `src/core/sim.c`). Replay validation checks the embedded scene fingerprint.

## Headless path

- `simcheck`/`replaycheck`/`scenecheck`/`detcompare`/`locscan`/`releasecheck` create **no HWND**
  and need no interactive desktop. They emit/consume JSON and exit non-zero on mismatch, so they
  are CI-friendly and double as regression oracle.

## Engineering utilities (doc 11)

- `locscan` — categorized physical-line counter with **JSON and YAML** config (hand-written
  `json.c`/`yaml.c`, no third-party parser). Binary/NUL detection, extension + path exclusion,
  ordered `category_rules`. Self-hosted report: `locscan_report.json`.
- `releasecheck` — validates `RELEASE_RESULT.json` + `RELEASE_EVIDENCE.json`: 25 gate keys,
  exactly 163 requirement entries, PASS proof references, artifact existence, visual-ID presence
  in `VISUAL_EVIDENCE.md`, gate↔requirement consistency, and version coherence (doc 27).

## Determinism evidence chain

1. Two identical headless runs produce byte-identical final JSON (confirmed).
2. 10x replay determinism test passes; manual replay repetition confirmed identical.
3. `detcompare` first-divergence test passes (non-zero on divergence).
4. `RELEASE_RESULT.determinism` records `all_match=true`, `gui_headless_match=true`,
   `trace_toggle_match=true`, `ui_scale_match=true`, and the two fingerprints.

## Known scope (headless CI)

Interactive/visual gates (Main UI, Editor, Advanced Editor, Physics Inspector, Desktop
Interaction/HiDPI, Visual Evidence) and live Windows Platform Binding checks are **NOT RUN** in
this environment and require running `pinball_sandbox.exe` on Windows. See `KNOWN_ISSUES.md` and
`RELEASE_CHECKLIST.md`. No mandatory automated/headless gate fails.
