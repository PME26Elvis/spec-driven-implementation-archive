# C17 Win32 Markdown Editor

A native Win32 Markdown desktop editor written entirely in ISO C17, with a custom
software-rendered UI, authored Markdown parser/renderer, file/workspace/history/
recovery subsystems, and three accompanying Workstream A utilities (locscan,
fixturegen, evidencecheck).

## Build

Requires the bundled MinGW-w64 toolchain at `../toolchain/mingw64/`.

```bash
# In D:\0901-workbuddy-markdown-editor\c17-markdown-editor
export PATH=/d/0901-workbuddy-markdown-editor/toolchain/mingw64/bin:$PATH

# Build everything
mingw32-make          # build libcore, libengine, tools, app, tests
mingw32-make tools    # just the three Workstream A utilities
mingw32-make editor   # just the GUI editor
mingw32-make tests    # just the test runner

# Run the GUI editor
./build/mdeditor.exe

# Capture a screenshot (automation mode)
./build/mdeditor.exe --screenshot UI-EMPTY-LIGHT "D:/out.png"
```

There is no C-drive consumption during build; all artefacts are produced under
`build/`. The MinGW-w64 toolchain is provided at `../toolchain/mingw64/`.

## Layout

```
c17-markdown-editor/
├── Makefile                   build entry point
├── README.md                  this file
├── RELEASE_REPORT.md          honest assessment vs the spec
├── build_evidence.py          generates evidence/manifest.json
├── config/
│   ├── locscan.json           Workstream A tool config (JSON)
│   └── locscan.yaml           same, YAML
├── src/
│   ├── core/                  shared pure-C core (UTF-8, buffer, Base64, SHA-256,
│   │                          LZSS, PRNG, JSON, YAML, match, Win32 helpers, WIC codec)
│   ├── engine/                Markdown engine (parser/AST, render model,
│   │                          statistics, search, Myers diff + word refine,
│   │                          version history, document/undo model)
│   ├── tools/                 Workstream A utilities
│   │   ├── locscan.c
│   │   ├── fixturegen.c
│   │   └── evidencecheck.c
│   ├── app/                    Win32 GUI editor
│   │   ├── app.h              shared types
│   │   ├── render.c           software framebuffer + effects
│   │   ├── view.c             nav/tabs/sidebar/source/preview/rendered rendering
│   │   ├── app.c              app lifecycle, file/workspace/history/recovery,
│   │                          commands, screenshot setup
│   │   └── input.c            window proc, WinMain, input handling, editing
│   └── ...
├── tests/
│   └── test_main.c            test runner (unit + integration + perf + failure)
└── evidence/                  generated artefacts (see RELEASE_REPORT.md)
```

## Workstream A

| Tool          | Purpose                              | Exit codes (per spec) |
|---------------|--------------------------------------|------------------------|
| `locscan`     | Repository inventory / line count   | 0, 2, 3, 4, 5         |
| `fixturegen`  | Deterministic fixture generation    | 0, 2, 4, 5, 6         |
| `evidencecheck` | Evidence manifest validation       | 0, 2, 3, 4, 5, 6, 7   |

All three are authored C17 with the no-third-party-library principle. `locscan`
accepts both JSON and YAML configurations. `fixturegen` covers small / unicode /
markdown-all / workspace / medium / large / stress-long-line / failure profiles
deterministically (xorshift64* PRNG, fixed default seed). `evidencecheck`
validates schema, required screenshot IDs, test categories, digests, path
security, fixture references, and the failed=0 / skipped=0 release gates.

## Workstream B

The GUI editor (`mdeditor.exe`) is a native Win32 application. It uses:

- `CreateDIBSection` + a memory DC for the application framebuffer
- Per-Monitor V2 DPI awareness
- All shape drawing in authored software pixel code (rounded rects with AA,
  alpha blending, box blur, drop shadow, glow, ripple)
- GDI `TextOutW` only for glyph rasterization (the allowed system boundary)
- A Markdown parser authored in `src/engine/md_parse.c`
- A Myers shortest-edit-script line diff + word/token refinement in
  `src/engine/diff.c`
- LZSS (4096 window, min 3, max 18) for version history payloads
- SHA-256 for integrity (history records, fixture digests, evidence digests)
- Windows Imaging Component (WIC) as the narrow system codec exception
  (PNG/JPEG/BMP only)
- Per-user Roaming AppData for preferences/recents, Local AppData for
  standalone recovery, workspace-local `.mdeditor/` for state

The editor provides Source / Split / Preview / Rendered Editing modes, a
sidebar (file tree + outline), tabs, command palette, find/replace, light/dark
themes, version history with side-by-side and inline diff, autosave + recovery
center, drag/drop from Explorer, Windows clipboard Unicode interoperability, and
Windows IME composition.

Automation: `--screenshot <id> <out.png>` opens, sets state, renders, and
captures the framebuffer to PNG.

## Honesty

This deliverable is substantial but does not pass every mandatory Release Gate
of the v1.0 specification. See `RELEASE_REPORT.md` for the full accounting of
what is complete, what is partial, and the exact `evidencecheck` error output.
