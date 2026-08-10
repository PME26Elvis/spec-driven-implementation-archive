# Requirements Traceability

This map binds the frozen v1.0 task-pack requirements to implementation and executable evidence. A row is a release claim only when its referenced final log/result is passing. `evidence/manifest.json` is the authoritative byte-integrity index; `evidence/release-report.md` is the concise gate report.

## Engineering, Product, and UI

| Requirement family | Implementation | Automated verification | Human evidence |
|---|---|---|---|
| C17, strict warnings, native Linux/X11 | `Makefile`, `src/ui.c` | clean `make all`; release build log/LOC inventory | all UI screenshots |
| One top-level window; custom controls/layout/focus/animation | `src/ui.c` | `E2E-UI-INTERACTION`, modal/menu/focus/ripple/frost checks | UI-EMPTY-*, UI-MODAL-*, UI-BUTTON-*, UI-FROSTED-* |
| Four source-backed modes | `document.c`, `ui.c` | `E2E-MODE-CYCLE-SOURCE-STABILITY`, RENDER-23 | UI-SOURCE/SPLIT/PREVIEW/RENDERED-EDIT |
| Authoritative source + mapped render model | `MdDocument`, `MdBlock`, `MdHeading` | parser/mapping tests; rendered 01–23; rendered X11 scenario | UI-MARKDOWN-ALL |
| UTF-8 editing units and exact persistence | `core.c`, document movement/delete | Unicode unit tests; XIM scenario; storage reopen | UI-SOURCE |
| XIM composition/commit/cancel/Undo | `initialize_x11`, `handle_key` | `E2E_XIM_SCENARIO` | actual X11 log |
| Selection, drag move/copy, one-step Undo/Redo | `document.c`, pointer handlers | `test_selection_drag_move`; X11 clipboard/editor | UI-SOURCE |
| Search/find/replace/wrap/whole word/non-recursive Replace All | `document.c`, `UiFind` | search unit tests; `E2E_SEARCH_SCENARIO`; performance Replace All | search log/state telemetry |
| Markdown required corpus and malformed preservation | parser/plain-text renderer | corpus unit test; generated markdown-all/failure manifests | UI-MARKDOWN-ALL |
| Rendered paragraph/heading/format/link/list/task/quote/fence | document transactions + UI commands/keys | RENDER-01–17; `E2E_RENDERED_SCENARIO` 01–12 | UI-RENDERED-EDIT |
| Rendered H1–H6 structural action | heading commands/context | RENDER-03; `E2E-RENDER-H2-H4-UNDO` | command palette/context menu |
| Direct table cell/row/column/alignment/navigation | table parser/serializer, table menu, Tab handling | RENDER-18–22; X11 table cell/row/column/alignment/Tab cases | UI-TABLE-EDIT |
| Outline hierarchy/duplicates/navigation | mapped headings/sidebar | duplicate identity unit; workspace Outline E2E | UI-OUTLINE |
| Statistics exact counts/status selection | `md_statistics_compute`, status/modal cache | exact hand-counted unit fixtures; UI modal state | UI-STATISTICS |
| Command palette, disabled commands, keyboard traversal | command registry, custom palette/menus/focus | UI and workspace scenarios; lifecycle flow | UI-COMMAND-PALETTE |
| Preferences/theme/font/spacing/image/autosave/mode/sync/session | `MdPreferences`, prefs modal | storage preference/default/corrupt tests | UI-EMPTY-LIGHT/DARK |

## Files, Workspaces, Safety, and Recovery

| Requirement family | Implementation | Verification |
|---|---|---|
| New/Open/Save/Save As/Close/Reopen | document lifecycle + safe save modals | keyboard lifecycle E2E; storage safe-save tests |
| Save As overwrite and relative-image relocation policy | relocation/overwrite modals, `md_save_as_with_relocation` | `test_save_as_relocation` |
| Atomic safe save and retained dirty buffer | `md_safe_save_document` | safe-save unit; all five injected write faults |
| Workspace scan/order/symlink bounds/create/rename/delete | `md_workspace_*`, tree UI | storage workspace tests; failure symlink case |
| Open file/parent rename and deletion semantics | tree modal path update/orphan marking | UI implementation + storage filesystem operations; external deletion E2E |
| Tabs, duplicate prevention, reorder, overflow, Ctrl+Tab/Shift+Tab/Ctrl+W | tab registry/strip/keyboard | workspace X11 scenario |
| Session order/active/caret/scroll/mode/zoom/split/sidebar | session JSON + restore | workspace restart E2E exact IDs/order and state |
| Recents de-duplicate/reorder/remove/clear | `md_recent_*`, commands/start surface | storage recent tests; command enablement |
| External clean/dirty modification and deletion | periodic digest baseline + conflict modal | five external-conflict E2E branches |
| Autosave, process-kill recovery, corrupt sibling isolation | recovery record codec/center | killed-process E2E exact SHA; storage/failure tests |
| Invalid UTF-8 and malformed metadata | strict open/parser; safe fallback | storage invalid UTF-8; truncated session failure |
| Multi-document Save All partial failure | `save_all` retains failed/untitled dirty tabs | safe-save state tests and fault boundary; command/UI state |

## Images, Assets, History, and Diff

| Requirement family | Implementation | Verification / evidence |
|---|---|---|
| PNG/JPEG/BMP decode/render | `image.c`, rendered image block | image unit; fixture assets; screenshots |
| Corrupt/missing placeholder, no source mutation | decode errors/render placeholder/relink | failure cases 09–13; UI-ERROR/IMAGE screenshots |
| Relative import and embedded data URI | asset import/Base64/data URI | storage assets/exports; Base64 vectors/all bytes |
| Resize/aspect/gesture transaction/persistence | image pointer gesture + width serialization | image unit/GUI path; UI-IMAGE-RESIZE |
| Convert relative↔embedded and Save Image As exact bytes | image context actions | storage asset tests; source-byte path implementation |
| Collision/nested path/workspace relocation | deterministic asset store/relative paths | `test_assets_and_exports`, relocation test |
| Portable single Markdown / Markdown+assets | export transformer | export tests remove original availability |
| Version creation/no duplicate/explicit/restart | history records and UI | history roundtrip; UI-VERSION-HISTORY |
| Snapshot/delta/LZSS/checksum/reconstruction/retention/pins | `diff.c`, `storage.c` | core delta/LZSS; storage history/corruption/retention |
| Restore as dirty one-step transaction | history restore modal + document replace | history tests / production handler |
| Deterministic line/token diff, Chinese, repeated lines | Myers/token refinement | diff unit matrix | UI-DIFF-SIDE-BY-SIDE/INLINE |

## Required Test and Evidence Layers

| Layer/category | Stable runner | Final count | Final evidence |
|---|---|---:|---|
| Pure logic/parser/render/diff/image | `bin/test_core` + `bin/test_tools` | 30 | `evidence/logs/unit.log` |
| Storage/filesystem/Workstream A | `bin/test_storage` + utility script | 27 | `evidence/logs/integration.log` |
| X11 critical flows | `scripts/run_e2e.sh` | 10 aggregate flows; detailed assertions inside | `evidence/logs/e2e.log` |
| Deterministic performance | `bin/performance` | 10 | performance JSON/log |
| Deterministic failures | `bin/failure_matrix` | 15 | failure JSON/log |
| Rendered/defect/evidence-validator regression | `bin/acceptance_matrix` + mutations | 27 | `evidence/logs/regression.log` |

The manifest reports 119 mandatory cases: 119 passed, 0 failed, 0 skipped. The number is generated only after the final run succeeds.

## Stable Performance and Failure IDs

Performance: `PERF-MEDIUM-OPEN`, `PERF-LARGE-OPEN`, `PERF-LARGE-PREVIEW`, `PERF-MEDIUM-TYPING`, `PERF-LARGE-TYPING`, `PERF-LARGE-FIND`, `PERF-LARGE-REPLACE-ALL`, `PERF-LARGE-OUTLINE`, `PERF-LONG-LINE`, `PERF-MEMORY-20`.

Failure: `FAIL-READONLY-SAVE`, `FAIL-ENOSPC`, `FAIL-PARTIAL-WRITE`, `FAIL-CLOSE`, `FAIL-RENAME`, `FAIL-INVALID-UTF8`, `FAIL-TRUNCATED-SESSION`, `FAIL-CORRUPT-HISTORY`, `FAIL-CORRUPT-RECOVERY`, `FAIL-MISSING-IMAGE`, `FAIL-CORRUPT-PNG`, `FAIL-CORRUPT-JPEG`, `FAIL-CORRUPT-BMP`, `FAIL-SYMLINK-CYCLE`, `FAIL-SPECIAL-PATHS`.

## Evidence Integrity

`evidencecheck` validates all required top-level fields, six categories, aggregate counts, no failure/skip, normalized root-contained paths, SHA-256/size, required screenshots and dimensions, all eight fixture profiles/manifests, exact performance fixture digests and environment schemas, all failure schemas/IDs, and required artifact entries. The mutation suite proves required rejections; visual meaning is reviewed with the human checklist.
