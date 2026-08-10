# Lattice Markdown

Lattice Markdown is a native Linux C17/X11 Markdown editor. It has four source-backed modes—Source, Split, Preview, and Rendered Edit—plus workspaces/tabs, custom X11 controls, Unicode editing units, XIM, X11 clipboard/Xdnd, images, history/diff, recovery, portable export, deterministic fixtures, fault injection, and reproducible release evidence.

The application does not keep a hidden rich-text document. `MdDocument.source` is the sole document authority; rendered interactions serialize transactions into that UTF-8 byte buffer and use the same Undo/Redo stack as Source mode.

## Dependencies

Required build/runtime packages on Linux:

- A C17 compiler and POSIX make.
- `pkg-config`.
- Xlib and Xft development files.
- FreeType/fontconfig through Xft.
- libpng and libjpeg development files (the only image-codec exceptions).
- Fonts containing Traditional Chinese, symbols, and emoji; the default names are Noto Sans CJK TC, Noto Sans Mono CJK TC, Noto Sans Symbols 2, and Noto Color Emoji.
- Xvfb for automated GUI and screenshot acceptance.
- POSIX shell plus `rg`, `sed`, `cmp`, and ordinary core utilities for release orchestration. Product file I/O does not shell out.

On Debian/Ubuntu, equivalent package names are commonly `build-essential`, `pkg-config`, `libx11-dev`, `libxft-dev`, `libpng-dev`, `libjpeg-dev`, `xvfb`, `fonts-noto-cjk`, `fonts-noto-color-emoji`, and `ripgrep`.

## Build and Run

```sh
make -j4 all
./bin/mdeditor
./bin/mdeditor --open /absolute/path/document.md
./bin/mdeditor --workspace /absolute/path/workspace
```

The program creates one top-level X11 window and all application widgets, menus, dialogs, hit testing, focus traversal, animation, and drawing are authored in this repository.

## Validation

```sh
make unit
make integration
make regression
make e2e
make sanitize
make evidence
make release
```

`make release` performs a clean strict build and a complete evidence run. Mandatory release tests are never converted to skips. Its final integrity gate is:

```sh
./bin/evidencecheck --root . --manifest evidence/manifest.json
```

The final human-readable status is `evidence/release-report.md`; machine-readable results and digests are in `evidence/manifest.json`. Screenshot review is intentionally separate from digest validation; see `docs/HUMAN_ACCEPTANCE_CHECKLIST.md`.

## Development Utilities

```sh
./bin/locscan --root . --config config/locscan.json --json evidence/loc-report.json --details
./bin/fixturegen --list-profiles
./bin/fixturegen --profile unicode --output /tmp/unicode-fixture --seed 424242
./bin/fixturegen --verify /tmp/unicode-fixture
./bin/evidencecheck --root . --manifest evidence/manifest.json
```

`fixturegen` uses authored xorshift64* with a fixed seed contract. All manifests use the repository's C17 SHA-256 implementation; no hashing utility process is used.

## Main Shortcuts

| Action | Shortcut |
|---|---|
| New / Open / Save / Save As | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Close / Reopen closed file | Ctrl+W / Ctrl+Shift+T |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z |
| Find / Replace | Ctrl+F / Ctrl+H |
| Bold / Italic / Strike / Inline code | Ctrl+B / Ctrl+I / Ctrl+Shift+X / Ctrl+` |
| Source / Split / Preview / Rendered Edit | Ctrl+1 / Ctrl+2 / Ctrl+3 / Ctrl+4 |
| Command palette | Ctrl+Shift+P |
| Statistics / History | Ctrl+Shift+D / Ctrl+Alt+H |
| Files / Outline | Ctrl+Alt+1 / Ctrl+Alt+2 |
| Preferences / Shortcut reference | Ctrl+, / F1 |
| Focus traversal / context menu | F6 / Shift+F10 |

Heading levels 1–6 and task toggling are available from the command palette; table and image structural actions are available through custom context menus.

## Repository Map

- `src/`, `include/mdedit/`: product and shared C17 implementation.
- `tools/`: the three required Workstream A tools plus evidence generation.
- `tests/`: unit, integration, X11 E2E, performance, fault, and rendered acceptance suites.
- `scripts/`: deterministic orchestration, actual-app screenshot capture, and evidence self-tests.
- `docs/`: architecture, traceability, utility contracts, and human acceptance.
- `evidence/`: final logs, generated fixture manifests, screenshots, results, LOC report, and integrity manifest.

## Safety Notes

Existing saves use a same-directory temporary file, checked write/close, and atomic replacement only after successful completion. External changes block unaware overwrite. Invalid UTF-8 is rejected without mutation. Save failures preserve the dirty in-memory source and surface recovery actions. Workspace metadata, history, and recovery corruption never overwrite authored Markdown.
