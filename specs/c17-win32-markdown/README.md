# C17/Win32 Markdown Desktop Editor — Specification-Driven Implementation Task Pack

Version: **v1.0 — Windows Edition**

Status: **Frozen implementation assignment**

## 1. Purpose

This task pack defines one fixed, high-difficulty software-engineering assignment for implementing a native Windows Markdown editor in C17 under a deliberately low-level Win32/custom-UI constraint.

It is intended to be given unchanged to different software implementers, models, or agent frameworks so that completion quality, engineering behavior, test discipline, correction effort, and delivered scope can be compared manually.

The package defines the assignment itself. It does not define how an implementer should reason, invoke tools, install dependencies, search documentation, take screenshots, debug, or sequence development work.

This Windows edition is intended to preserve the functional scope and implementation difficulty of the corresponding Linux/X11 v1.0 assignment while replacing platform integration with native Windows equivalents.

## 2. Mandatory Workstreams

### Workstream A — Authored Development/Verification Utilities

Implement all three C17 utilities:

1. `locscan` — source/document inventory and line-count utility.
2. `fixturegen` — deterministic Markdown/workspace/failure/performance fixture generator.
3. `evidencecheck` — evidence-manifest completeness and integrity validator.

These utilities are product deliverables of the assignment and require their own tests.

### Workstream B — Native Markdown Desktop Editor

Implement the complete editor described by the normative specifications.

The editor is not a visual prototype. Mandatory controls, menus, keyboard behavior, editing modes, Markdown parser/renderer, rendered editing, workspace/tabs, images, version history/diff, recovery, preferences, error handling, and performance gates must be real and connected.

## 3. Core Technical Boundary

- Language: ISO C17-compatible C; application sources compile as C, not C++.
- Platform: native 64-bit Windows desktop; normative acceptance target is Windows 11.
- Native integration: Win32/Windows SDK system APIs only within the explicitly allowed low-level/system-integration boundary.
- Window/input/presentation: User32 + application-owned rendering, with GDI/DIB-style presentation permitted.
- No Qt/GTK/SDL/GLFW/WinUI/WPF/Windows Forms/MFC/Electron/CEF/WebView2/high-level GUI framework.
- No Rich Edit/EDIT control as the editor and no native TreeView/Tab/common-control substitution for mandatory custom UI.
- Custom application UI/widget/layout/editing/render-state engine required.
- Markdown parser/renderer authored by the submission.
- Built-in version history and diff authored by the submission; Git is not the implementation.
- Base64 codec authored in C.
- PNG/JPEG/BMP image support mandatory; Windows Imaging Component is permitted only as the narrow system image-codec boundary.
- Windows Unicode/IME/clipboard/path/DPI behavior is mandatory under `docs/16_WINDOWS_PLATFORM_CONTRACT.md`.

See `docs/01_ENGINEERING_CONSTRAINTS.md` and `docs/16_WINDOWS_PLATFORM_CONTRACT.md` for the normative platform boundary.

## 4. Product Summary

The final product includes at least:

- Multi-document tabs.
- User-selected workspace root and file tree.
- Persistent workspace/session state.
- Source, Split, Preview, and direct Rendered Editing modes.
- Live rendered preview.
- CommonMark-oriented Markdown plus required GFM constructs.
- Rendered structural editing for headings, links, lists, tasks, code, images, and tables.
- Live document Outline.
- Find/Replace.
- Complete undo/redo transaction semantics.
- UTF-8 documents, Traditional Chinese, normative grapheme fixtures, and real Windows IME composition.
- Unicode Windows clipboard interoperability and Explorer-compatible file drag/drop.
- Relative-path and Base64-embedded images.
- In-editor image resize and image context actions.
- Portable Markdown exports for embedded or externalized images.
- Autosave/crash Recovery Center.
- External file-change conflict handling.
- Built-in persistent version history with snapshot/delta/compression policy.
- Myers-style line diff plus word/token refinement.
- Side-by-side and inline graphical diff.
- Document statistics and live status-bar counts.
- Command Palette.
- Recent workspaces/files.
- Light/Dark themes and persistent preferences.
- Keyboard-only operation for core flows.
- DPI-aware custom UI on Windows.
- Modern custom hover/ripple/glow/capsule/modal/blur/frosted-navigation UI behavior.
- Deterministic performance and failure/corruption acceptance gates.

## 5. Normative Documents

The v1.0 Windows specification consists of:

1. `docs/01_ENGINEERING_CONSTRAINTS.md`
2. `docs/02_PRODUCT_AND_DOCUMENT_MODEL.md`
3. `docs/03_MARKDOWN_AND_EDITING.md`
4. `docs/04_UI_UX_AND_INTERACTION.md`
5. `docs/05_IMAGES_AND_MEDIA.md`
6. `docs/06_VERSION_HISTORY_AND_DIFF.md`
7. `docs/07_TESTING_ACCEPTANCE_AND_DOD.md`
8. `docs/08_DEV_TOOLING.md`
9. `docs/09_FROZEN_SCOPE_AND_DEFERRED.md`
10. `docs/10_WORKSPACE_TABS_AND_ASSETS.md`
11. `docs/11_EDITING_SAFETY_SEARCH_CLIPBOARD_RECOVERY.md`
12. `docs/12_RENDERED_EDITING_TABLE_OUTLINE_LIFECYCLE.md`
13. `docs/13_COMMANDS_RECENTS_PREFERENCES_KEYBOARD.md`
14. `docs/14_PERFORMANCE_AND_FAILURE_HANDLING.md`
15. `docs/15_DEV_FIXTURES_AND_EVIDENCE.md`
16. `docs/16_WINDOWS_PLATFORM_CONTRACT.md`

`CHANGELOG.md` is informative and records task-pack revisions. `WINDOWS_PARITY_AUDIT.md` is an informative cross-platform review aid and does not override normative requirements.

## 6. Requirement Language

**MUST** and **MUST NOT** are mandatory.

**SHOULD** and **SHOULD NOT** are strong quality expectations; failure to satisfy them must be justified when they are not release gates.

**MAY** is optional.

The v1.0 package contains no unresolved `TBD` that permits omission of a mandatory capability.

## 7. Precedence

If text appears to conflict, use this precedence:

1. Explicit prohibition or Release Gate.
2. Engineering constraint and Windows platform contract.
3. Feature-specific normative contract.
4. General product behavior.
5. UI/UX behavior.
6. Acceptance example.

A more specific requirement overrides a general one when both can reasonably be read together.

For Windows-only ambiguity, `docs/16_WINDOWS_PLATFORM_CONTRACT.md` overrides a platform-generic statement while preserving the intended feature semantics.

## 8. No Substitute Completion

A mandatory feature is not complete when represented only by:

- Placeholder UI.
- Disabled control.
- Hard-coded output.
- Fake preview/diff/statistics.
- Test-only bypass.
- Mock persistence presented as real persistence.
- A hidden Rich Edit/native common control performing mandatory editor/widget logic behind a custom skin.
- Shelling out to another application for a function required to exist in-app, unless expressly permitted.
- Screenshot/mockup presented instead of the running UI.

## 9. Testing and Evidence

The implementation must deliver:

- Unit tests.
- Integration tests.
- End-to-end tests.
- Regression tests.
- Deterministic fixtures.
- Windows platform-integration tests.
- Performance tests.
- Failure/fault-injection tests.
- Required screenshots from the actual application.
- Machine-readable evidence manifest.
- `evidencecheck` validation result.

The task pack specifies what evidence must prove, not the screenshot command, test orchestration framework, compiler vendor, or development workflow.

## 10. Definition of Done

Completion is governed exclusively by `docs/07_TESTING_ACCEPTANCE_AND_DOD.md` together with the Windows-specific mandatory platform cases referenced there.

If any mandatory Release Gate fails or any mandatory feature remains missing, mocked, disconnected, or substituted, the assignment is incomplete.

The implementer must not claim completion until the complete mandatory validation suite passes.

## 11. Deliverable Shape

The final source delivery must include:

- All authored C source required to build Workstream A and Workstream B.
- Build definitions/scripts for the documented Windows build.
- Windows application manifest/resources required for correct runtime behavior, including DPI/long-path declarations when used by the implementation.
- Tests and deterministic test assets.
- Sample/configuration files needed by the authored utilities.
- Human-readable project documentation needed to build, run, test, and interpret evidence.
- Evidence directory/manifest from the final acceptance run.

Disposable caches, object files, temporary recovery files, local AppData state, IDE-local state, and unrelated generated artifacts must not be required as source deliverables.

## 12. Explicitly Deferred Scope

See `docs/09_FROZEN_SCOPE_AND_DEFERRED.md`.

Notable deferred areas include PDF/HTML export, spellcheck, plugin system, Mermaid/LaTeX rendering, cloud collaboration, Windows Store packaging, ARM64 acceptance, native Linux/macOS backends, and a second GUI/windowing backend.
