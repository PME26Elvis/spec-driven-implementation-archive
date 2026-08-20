# Pinball Sandbox Benchmark — Windows Win32 / C17 (v1.0.0)

A self-contained Windows desktop 2D pinball **physics sandbox + table editor**, implemented in
**C17** with a hand-written Win32/User32/GDI software renderer (custom DIB framebuffer — no
Direct2D/OpenGL/SDL/Qt/WebView). Includes a deterministic fixed-timestep physics core, a
`.pbt` scene format, a `.pbr` replay format, a headless CLI, and a suite of pure-C
engineering utilities.

> Built and validated in a headless CI environment. Live-GUI gates are marked NOT RUN and
> require running the editor on a Windows desktop (see `RELEASE_CHECKLIST.md`,
> `KNOWN_ISSUES.md`). All automated/headless gates PASS and `releasecheck` passes.

## Requirements

- Windows (x86-64). The application is a native Win32 desktop program.
- Build toolchain: MinGW-w64 GCC (C17). No third-party runtime is shipped.
- Headless utilities run without any window/interactive desktop.

## Build

From a shell with the toolchain on `PATH`:

```bash
tools/build_core.sh      # core objects + pcheck/rtcheck/simcheck/scenecheck/replaycheck/detcompare/framegen
tools/build_app.sh       # pinball_sandbox.exe (editor + platform layer)
tools/build_tests.sh     # build + run the unified test suite (722 tests)
tools/build_tools.sh     # locscan + releasecheck
```

Produces binaries under `build/`:
`pinball_sandbox.exe`, `tests.exe`, `simcheck.exe`, `replaycheck.exe`, `scenecheck.exe`,
`pcheck.exe`, `rtcheck.exe`, `detcompare.exe`, `framegen.exe`, `locscan.exe`, `releasecheck.exe`.

## Run

- **Editor / game:** `build/pinball_sandbox.exe` — opens a real top-level Win32 window with the
  custom-rendered client area (no native child controls; required modals/file pickers are
  custom surfaces).
- **Headless simulation:** `build/simcheck.exe --headless fixtures/reference_full_game_v2.pbt --steps 1200`
  prints a JSON final/checkpoint state and returns non-zero on parse/validation failure.
- **Replay:** `build/simcheck.exe --headless <scene.pbt> --replay <trace.pbr>` replays logical
  fixed-step actions and verifies the scene fingerprint.
- **Scene validation:** `build/scenecheck.exe <scene.pbt>` prints deterministic Error/Warning
  diagnostics; exits 0 only with no Error.
- **Determinism compare:** `build/detcompare.exe a.jsonl b.jsonl` reports the first divergence.
- **LOC report:** `build/locscan.exe . --config .locscan.json` (or `--config .locscan.yaml`).
- **Release validation:** `build/releasecheck.exe` validates `RELEASE_RESULT.json` and
  `RELEASE_EVIDENCE.json`.

## Test & verification

```bash
tools/build_tests.sh        # runs 722 automated tests (must end 722/722)
python3 tools/make_release.py   # regenerates RELEASE_RESULT.json / RELEASE_EVIDENCE.json /
                                # TEST_SUMMARY.json / VISUAL_EVIDENCE.md
build/releasecheck.exe     # must print: releasecheck: PASS (163 requirements, ...)
```

Current results (headless CI):

- **722 / 722** automated tests pass (>= 420 required).
- Determinism: identical fingerprints across repeated/10x runs (`fsfp 917f266a18e6387a`).
- Stress: 1,000,000 fixed-step run in 14.3 s, **0 runtime errors**, 0 NaN/Inf.
- `releasecheck`: PASS (163/163 requirements, gates consistent, versions consistent).

## Project layout

```
src/core/      production physics/event/scene/replay core (shared by GUI + headless)
src/app/       editor + game UI (Win32/User32/GDI software renderer)
src/platform/  Windows platform binding (window, input, IME, HiDPI)
tools/         engineering utilities + unified test suite (all C17, no 3rd-party deps)
fixtures/      18 acceptance scenes (incl. official reference_full_game_v2.pbt)
out/evidence/  generated visual evidence (V01-V38 static, A01-A25 transition)
build/         compiled binaries
```

## Deliverables / reports

- `RELEASE_RESULT.json` — release gates, tests, stress, determinism, scenarios.
- `RELEASE_EVIDENCE.json` — 163 requirement-to-proof entries (doc 27).
- `TEST_SUMMARY.json` — machine-readable test-id map.
- `TEST_REPORT.md`, `RELEASE_CHECKLIST.md`, `KNOWN_ISSUES.md`, `VISUAL_EVIDENCE.md`.
- `locscan_report.json` — self-hosted line-count report.
- `README.md` (this file), `ARCHITECTURE.md`.

See `RELEASE_CHECKLIST.md` for the per-gate status and the honest NOT RUN scope.
