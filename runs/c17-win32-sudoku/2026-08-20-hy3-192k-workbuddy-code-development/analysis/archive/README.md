# Archive provenance

This directory documents archive-maintainer handling of the original WorkBuddy submission. It is intentionally outside `project/`, so provenance metadata does not alter or masquerade as part of the implementer's submitted project.

## Original submission

- File: `0820-sudoku-workbuddy.zip`
- Size: **18,498,151 bytes**
- SHA-256: `f0c6fdc701786bf4c9d12dc0cbbfe567c1aaef9fa8a1bb3db60a58ef717ced14`
- ZIP entries: **918 files**
- Total uncompressed file bytes: **328,903,285 bytes**
- Planned Release tag: `run-c17-win32-sudoku-2026-08-20-hy3-192k-workbuddy`

The raw ZIP is not committed to Git. It is intended to be published unchanged as a GitHub Release asset after this run is merged, so the complete original workspace remains recoverable without placing hundreds of megabytes of generated data into Git history.

## Curated Git snapshot

The repository stores `project/0820-sudoku-workbuddy_curated/`, containing **105 files**. The selection preserves implementation source, headers, tests, developer tools, scripts, configuration, and lightweight textual validation results while omitting large generated/runtime material and a redundant embedded copy of the benchmark task pack.

The raw workspace's `c17-win32-sudoku-task-pack-v1.0.0/` was compared during ingestion with the canonical repository copy under `specs/c17-win32-sudoku/` and matched byte-for-byte. Because the canonical spec is already versioned in this repository and the complete raw ZIP preserves the original workspace, the duplicate task-pack directory is intentionally omitted from the curated project. This also keeps the archived run checkout-friendly on Windows, where the extra nested path depth exceeded Git's default path limit.

Conversation exports retain their original bytes but use archive-normalized short filenames (`workbuddy-export.md` and `workbuddy-output-only.txt`) so the run can be checked out on Windows.

## Omitted raw-workspace material

The exact reviewed omission patterns are recorded in `omitted-paths.txt`. The dominant omitted data is:

- `results/screenshots/**` — 51 BMP evidence images, about 187.8 MB uncompressed.
- `build/bin/**` — compiled executables/PDB-related build outputs, about 58.7 MB.
- `build/obj/**` — 286 compiler intermediate files, about 14.4 MB.
- `build/evidence/**` — generated evidence artifacts, about 65.5 MB.
- `build/logs/**`, `build/tmp/**`, `.tinyvcs/**`, `.workbuddy/**`, and root `objects/**` — generated logs, temporary/runtime state, tool databases, and workspace state.
- `c17-win32-sudoku-task-pack-v1.0.0/**` — verified duplicate benchmark input already preserved canonically under `specs/c17-win32-sudoku/` and in the raw Release ZIP.

The otherwise-generated `build/` tree contained four authored helpers that were deliberately retained:

- `build/_cc.cmd`
- `build/_exe.cmd`
- `build/vault_test/genfont.py`
- `build/vault_test/parse.py`

Lightweight textual files under `results/` are retained in Git because they are useful implementation-run evidence, but they are excluded from the normalized authored-code/token metric.

## Integrity

`SHA256SUMS.txt` records the authoritative raw ZIP digest. Once the Release asset is published, its GitHub-reported digest should be checked against the same SHA-256 before the ingestion is considered fully closed.