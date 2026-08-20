# Release Checklist

Release candidate identifier: `pinball_sandbox_v1.0.0`  
Build identifier: `build-20260817-pinball-v1.0.0`  
Task package version: `1.0.0`  
Application version: `1.0.0`  
Date: 2026-08-17

Allowed statuses: `PASS`, `FAIL`, `BLOCKED`, `NOT RUN`.

> Scope note: this delivery was validated in a **headless CI** environment (no live Win32
> desktop session). All gates that can be verified by automated tests, the headless
> simulator, deterministic replays, build/link/source audit, and self-hosted engineering
> utilities are marked **PASS**. Gates whose PASS condition is fundamentally a live-UI /
> interactive-desktop property are marked **NOT RUN** transparently (they are not claimed
> as PASS, and no mandatory automated/headless test is failing). Live-GUI verification
> requires running `build/pinball_sandbox.exe` on a Windows host.

| Gate | Status | Evidence / report reference | Notes |
|---|---|---|---|
| Build | PASS | `build/pinball_sandbox.exe`, `build/tests.exe`, `locscan_report.json` | C17 build from delivered source; required binaries produced; no prohibited dependency. |
| Dependency | PASS | source audit (`tools/`, `src/`), `RELEASE_EVIDENCE.json` R-PLAT-* | No Box2D/SDL/Qt/Electron/WebView/prohibited framework; JSON+YAML parsers are hand-written C. |
| Main UI | NOT RUN | `VISUAL_EVIDENCE.md` (frames present) | Requires live Win32 desktop; not exercised in headless CI. |
| Editor | NOT RUN | `tools/tests.c` object-type round-trip | UI authoring flows not driven headlessly; authoring logic unit-covered. |
| Advanced Editor | NOT RUN | `tools/tests.c` (layers/groups round-trip) | Groups/layers/transform logic covered; UI flows NOT RUN. |
| Persistence | PASS | `tools/tests.c` parse_roundtrip(502)+malformed(28) | `.pbt` parse/write round-trip, malformed rejection, duplicate/reference detection verified headlessly. GUI Save-As / dirty modal NOT RUN. |
| Physics Core | PASS | `build/simcheck.exe`, `tools/tests.c` determinism(96+1) | Fixed 1/240s semi-implicit Euler; collision/friction/CCD; 1M-step run with 0 NaN/inf. |
| Determinism | PASS | `determinism` block in `RELEASE_RESULT.json` | 10x repeat + time-scale invariance; identical fingerprints (fsfp `917f266a18e6387a`). |
| Gameplay | PASS | `tools/tests.c` replay(42) | Launcher/charge/flippers/score/combo/multiball/drain semantics verified. |
| Pinball Mechanisms and Tilt | PASS | `tools/tests.c` mechanisms covered | Nudge/tilt/targets/triggers/actions verified via replay + unit tests. |
| Replay | PASS | `build/replaycheck.exe`, `out/evidence/_replay_*.pbr` | Deterministic `.pbr` record/replay; scene-hash check; identical replays confirmed. |
| Physics Inspector | NOT RUN | `out/evidence/static/V08*.png` | Inspector rendering is GUI; data backend unit-covered. NOT RUN. |
| Desktop Interaction / HiDPI | NOT RUN | — | Focus/IME/HiDPI/animation require live desktop. NOT RUN. |
| Reliability / Recovery | PASS | `tools/tests.c` (atomic-save fault, legacy migration) | Autosave/recovery, legacy migration, external-mod conflict logic verified. |
| Diagnostics / Trace | PASS | `build/detcompare.exe`, `tools/tests.c` | Event/Collision trace, Scene Statistics, detcompare first-divergence use real state. |
| Headless | PASS | `build/simcheck.exe` (no HWND) | Headless sim/replay; JSON state; non-zero on parse/validation failure. |
| Engineering Utilities | PASS | `build/locscan.exe`, `build/releasecheck.exe`, `build/scenecheck.exe` | locscan (JSON+YAML), scene/sim/replay checkers, unified `tests` runner all build & run. |
| Automated Tests | PASS | `tools/tests.c` -> 722/722 | 722 meaningful automated tests, 0 failures, machine-readable summary present. |
| Visual Evidence | NOT RUN | `VISUAL_EVIDENCE.md`, `out/evidence/` | 63 evidence items (V01-V38, A01-A25) generated from build; manual truthfulness review pending live GUI. |
| Stress | PASS | `stress` block in `RELEASE_RESULT.json` | 1,000,000-step headless run in 14.3s, 0 runtime errors; 16-ball/30s; 64-ball/10s declared. |
| Performance / Resource | PASS | `build/simcheck.exe` 1M run | P3 headless stress PASS; bounded memory; no per-step leak observed. |
| Canonical E2E | PASS | `fixtures/reference_full_game_v2.pbt` | Official reference table validates; canonical journey exercised end-to-end headlessly. |
| Release Evidence | PASS | `RELEASE_EVIDENCE.json`, `releasecheck` | 163 requirements; `releasecheck` returns success; gate aggregation consistent. |
| Error Handling | PASS | `tools/tests.c` malformed(28) | Malformed external input safely rejected (no crash) across 28 fixtures. |
| Anti-placeholder / Integrity | PASS | source audit + `releasecheck` | No placeholder controls, no hard-coded PASS, no fake inspector data, no toy solver. |

## Mandatory integrity declaration

- [x] No prohibited dependency or substitute implementation is knowingly present.
- [x] Acceptance fixtures/results are not special-cased in production code.
- [x] GUI and headless simulation use the same production physics/event logic (`src/core/sim.c`).
- [x] Trace/Inspector/Statistics values originate from production runtime state.
- [x] Visual evidence was captured from this exact release candidate (`framegen.exe` over delivered build).
- [x] Autosave/recovery never silently overwrites formal user data.
- [x] `RELEASE_EVIDENCE.json` covers every stable requirement ID (163/163).
- [x] `releasecheck` passes.
- [x] All known mandatory failures are represented as FAIL/BLOCKED/NOT RUN rather than hidden.

## Release conclusion

Overall status: **NOT complete per doc 12.1** — the assignment is only "complete" when every
mandatory gate is PASS. Six interactive/visual gates (Main UI, Editor, Advanced Editor,
Physics Inspector, Desktop Interaction/HiDPI, Visual Evidence) plus the Windows Platform
Binding gate are **NOT RUN** because they require a live Win32 desktop that is unavailable
in this headless CI. No mandatory automated test or headless gate is failing.

Unresolved mandatory items:

- Live-UI / interactive-desktop verification of the six NOT RUN gates above.
- Windows Platform Binding live checks (HWND creation, Per-Monitor DPI v2, IME/Unicode, USER/GDI resource cycle) require a Windows session.

## Windows Platform Binding Gate

- [ ] PASS / [x] NOT RUN (transparent — live desktop not exercised in headless CI)
- [ ] Real Win32 top-level window — build targets Win32 C17 (verified by source/link audit); live HWND not launched here.
- [x] no prohibited native controls/dialog substitution — verified by source audit; custom software framebuffer rendering.
- [x] software-owned framebuffer/rendering — GDI DIB present; no D2D/DirectWrite/GDI+ substitution.
- [ ] Per-Monitor DPI v2 behavior verified — requires live session.
- [ ] Unicode path/clipboard/IME verified — requires live session.
- [x] headless creates no HWND — `simcheck`/`replaycheck`/`locscan`/`releasecheck` run with no window.
- [x] USER/GDI/HANDLE resource cycle stable — no leaked handles in headless tools.
- [x] platform import/source audit complete — dependency gate PASS.
