# Task Pack Changelog

## v1.0 — 2026-08-07

Frozen the first complete assignment specification.

Major additions/resolutions from v0.2:

- Mandatory multi-document lifecycle contracts completed.
- Undo/redo transaction boundaries frozen across structural operations.
- Find, Replace, Replace All, case/whole-word semantics frozen.
- UTF-8 clipboard interoperability and file drag/drop frozen.
- External file modification/deletion/conflict handling frozen.
- Autosave and crash Recovery Center frozen.
- Traditional Chinese IME and normative Unicode editing-unit fixtures frozen.
- Rendered Editing behavior expanded per Markdown construct.
- Full rendered table GUI editing required.
- Live heading Outline required.
- Draggable Split divider and anchor-based synchronized scrolling required.
- Command Palette required.
- Recent files/workspaces and start surface required.
- Light/Dark themes and persistent Preferences required.
- Keyboard-only core operation and modal focus model required.
- Deterministic medium/large/long-line performance fixtures and responsiveness gates added.
- I/O fault injection, corrupted state/history/recovery/image handling, symlink-cycle and invalid UTF-8 gates added.
- `fixturegen` and `evidencecheck` added as mandatory C17 Workstream A utilities.
- Machine-readable evidence manifest became mandatory.
- Stable Release Gate families frozen.
- Image formats frozen to PNG/JPEG/BMP.
- Narrow system `libpng`/`libjpeg` codec exception permitted so the benchmark does not become a JPEG/PNG decoder memorization exercise.
- Resized-image persistence frozen to a constrained inline `<img>` representation.
- Version creation, full-snapshot interval, Myers-derived deltas, LZSS compression profile, integrity, retention, and pinned-version behavior frozen.
- All prior v0.2 `TBD` items either resolved or explicitly deferred.

## v0.2-draft — 2026-08-07

- Added multi-document tabs.
- Added workspace root and file tree.
- Added `.mdeditor/` workspace metadata/session model.
- Added relative assets and Base64 data-URI images.
- Added authored Base64 codec requirement.
- Added per-image storage conversion and portable Markdown export forms.

## v0.1-draft — 2026-08-07

- Established Linux C17/X11 constraints and custom UI engine requirement.
- Established four Markdown modes including Rendered Editing.
- Established Markdown, images, statistics, version history/diff, visual requirements, testing/DoD, and initial LOC utility scope.
