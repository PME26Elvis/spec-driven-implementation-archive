# Windows v1.0 Platform-Parity Audit

Status: **Informative review aid**

This document is not a replacement for the normative specification. It records how the Windows edition preserves the Linux/X11 v1.0 assignment's functional difficulty while changing only platform integration where required.

## 1. Audit Goal

The Windows edition must remain the same benchmark problem in product scope:

- same editor modes;
- same Markdown constructs;
- same rendered editing;
- same workspace/tabs;
- same images/Base64/assets;
- same undo/search/statistics;
- same history/diff;
- same custom modern UI effects;
- same development utilities;
- same performance/failure/evidence philosophy.

The port must not become easier merely because Windows offers higher-level native controls.

## 2. Window Creation / Event Delivery

Linux role:

- X11/Xlib/XCB window/event integration.

Windows role:

- Win32/User32 top-level window and message-loop integration.

Parity decision:

- native window/event APIs are allowed;
- mandatory editor/widget behavior remains application-authored.

## 3. Custom Rendering Surface

Linux role:

- application-owned pixel/pixmap rendering presented through X11.

Windows role:

- application-owned software surface presented through GDI/DIB-class facilities.

Parity decision:

- GDI may present/copy the application framebuffer;
- high-level scene/effect systems do not replace authored blur/shadow/ripple/layout behavior.

## 4. Native Controls

Linux risk:

- delegating application UI to GTK/Qt/toolkit widgets.

Windows risk:

- delegating to Rich Edit, EDIT, TreeView, Tab, ListView, Toolbar, WinUI, WPF, Windows Forms, or MFC.

Parity decision:

- mandatory application controls remain custom;
- normal OS title bar/window chrome may remain native;
- native filesystem pickers are the explicit OS-integration exception.

## 5. Font/Glyph Boundary

Linux role:

- system font/text facilities may rasterize glyphs without owning editor layout.

Windows role:

- DirectWrite or GDI/system text facilities may resolve/shape/rasterize glyphs.

Parity decision:

- text editor paragraph layout, Markdown layout, selection, caret, widget layout, and rendered editing remain authored.

## 6. Image Codec Boundary

Linux role:

- narrow system PNG/JPEG codec exception.

Windows role:

- Windows Imaging Component for PNG/JPEG/BMP decode/encode.

Parity decision:

- codec only;
- Base64, asset conversion, resize interaction, path handling, source serialization, rendering layout, and image UI remain authored.

## 7. Clipboard

Linux role:

- interoperable UTF-8 clipboard selection.

Windows role:

- interoperable Unicode `CF_UNICODETEXT` clipboard.

Parity decision:

- document stays UTF-8 internally;
- OS-boundary UTF-16 conversion is permitted;
- Cut remains non-destructive if clipboard publication fails.

## 8. File Drag/Drop

Linux role:

- desktop/file-manager file drop.

Windows role:

- Windows Explorer-compatible file drop.

Parity decision:

- exact underlying native protocol is not prescribed;
- required image/Markdown drop behavior is preserved.

## 9. IME

Linux role:

- X11 input-method composition.

Windows role:

- real Windows IME composition lifecycle.

Windows-specific parity additions:

- candidate/composition placement near custom caret;
- transient preedit state;
- cancellation;
- commit as one Undo transaction;
- no UTF-16-surrogate/UTF-8 corruption.

## 10. Filesystem Selection

Linux role:

- application-visible file operation flow.

Windows role:

- native Common Item Dialog/file/folder picker may be used.

Parity decision:

- this is OS integration only;
- application conflict/error/statistics/history/preferences/recovery modals remain custom UI.

## 11. Workspace State

Unchanged:

- `.mdeditor/` remains the workspace-local state/history/recovery namespace where specified;
- it is hidden by editor policy from the ordinary tree;
- document files are not polluted with editor state.

Windows-specific addition:

- setting the Windows Hidden attribute is optional and cannot be the only exclusion mechanism.

## 12. Global State

Linux role:

- XDG config/state locations.

Windows role:

- Roaming AppData for preferences/recents;
- Local AppData for standalone/untitled recovery and machine-local state.

Parity decision:

- no state beside executable;
- no administrator privilege requirement;
- `%TEMP%` cannot be the sole recovery store.

## 13. Path Encoding

Linux role:

- UTF-8-oriented path environment in acceptance fixtures.

Windows role:

- Unicode Win32 path APIs / UTF-16 OS boundary.

Parity decision:

- Traditional Chinese paths are mandatory;
- ANSI code page dependence is prohibited for required cases.

## 14. Long Paths

Windows-specific risk:

- legacy `MAX_PATH` behavior can make an otherwise functional editor demo-only.

Windows parity decision:

- one >260-character Unicode path is mandatory across editor and Workstream A utilities;
- truncation is prohibited.

## 15. Filename Legality

Windows-specific risk:

- invalid characters, trailing dot/space, and reserved DOS device basenames.

Windows parity decision:

- New/Rename must reject representative invalid/reserved cases non-destructively;
- fixture generator must model valid Windows punctuation separately from invalid cases.

## 16. Path Identity / Case

Linux edition acceptance is naturally case-sensitive.

Windows normative acceptance uses ordinary case-insensitive NTFS.

Parity decision:

- two case-equivalent path spellings cannot create independent buffers for the same file;
- UI may preserve original casing;
- Markdown search case rules remain unrelated to filesystem case rules.

## 17. Relative Markdown Paths

Unchanged:

- generated Markdown links/assets use `/` separators;
- image paths are relative to the Markdown file;
- moving a whole workspace with its assets keeps references valid.

Windows addition:

- native `C:\...` path syntax must never leak into newly generated portable Markdown references when a relative Markdown path is required.

## 18. Link / Reparse Traversal

Linux role:

- symlink-cycle/root-escape protection.

Windows role:

- directory reparse points, junctions, and symbolic links.

Parity decision:

- no recursive following by default;
- no unbounded cycle;
- no evidence/root escape;
- host-policy fallback still must exercise production traversal classification rather than skip the gate.

## 19. External File Changes

Unchanged:

- clean external change → Reload / Keep Current / Compare;
- dirty conflict → no silent overwrite;
- external delete → in-memory buffer survives;
- external rename need not always be inferred.

Windows addition:

- the editor must not keep needless exclusive handles that prevent required external-change scenarios;
- sharing semantics are part of acceptance.

## 20. Safe Save

Linux role:

- temp write + flush + atomic/safe rename.

Windows role:

- same-directory staging + full write + durable flush + final Windows Replace/Move semantic.

Parity decision:

- never truncate the only valid target first;
- pre-commit failure preserves old target;
- commit failure preserves in-memory dirty buffer.

## 21. Sharing Violation

Windows-specific failure class:

- another process may hold an incompatible handle.

Parity decision:

- Save visibly fails;
- buffer stays dirty/in memory;
- Save As/retry/cancel recovery remains available;
- no data loss and no forced privilege/lock bypass.

## 22. DPI

Windows-specific custom-rendering requirement:

- 100%, 150%, 200% acceptance;
- correct hit testing;
- correct caret/selection geometry;
- logical breakpoints;
- open-window DPI transition;
- scaled screenshot evidence.

This prevents a custom pixel renderer from passing only at 96 DPI.

## 23. Timing

Windows permitted timing primitive:

- high-resolution monotonic Windows performance counter or equivalent.

Parity decision:

- animation/performance timing cannot depend on wall-clock jumps.

## 24. Workstream A — `locscan`

Windows additions:

- Unicode root/config paths;
- >260-character path;
- deterministic ASCII case-insensitive path-pattern behavior;
- `/`-normalized report paths;
- no directory reparse recursion by default.

Core authored JSON/YAML parsing and line-count requirements remain unchanged.

## 25. Workstream A — `fixturegen`

Windows additions:

- long Unicode path fixture;
- Windows-invalid/reserved filename descriptors;
- locked-target case preparation;
- reparse traversal case/fallback.

Core deterministic PRNG/manifest/digest requirements remain unchanged.

## 26. Workstream A — `evidencecheck`

Windows additions:

- reject drive-letter absolute paths;
- reject UNC paths;
- reject extended namespace absolute paths;
- reject rooted-backslash paths;
- reject `..` root escape;
- reject reparse root escape.

Core manifest/digest/completeness behavior remains unchanged.

## 27. Visual Evidence

Unchanged screenshot philosophy:

- screenshots must be from the actual running application;
- specification defines required state, not screenshot tooling.

Windows addition:

- `UI-DPI-SCALED` at 150% or 200%.

## 28. New Windows Release Gates

Added:

- `RG-WINPLATFORM`.
- `RG-DPI`.

Existing feature gates remain mandatory and are not replaced by these additions.

## 29. Deliberately Not Added

The Windows port does not require:

- installer authoring;
- Microsoft Store packaging;
- registry-based document associations;
- shell context-menu extensions;
- Windows service/background agent;
- ARM64 build;
- UWP/WinUI backend;
- PDF/HTML export;
- cloud integration.

These would change project scope rather than preserve platform parity.

## 30. Final Audit Conclusion

The Windows edition keeps the benchmark centered on the same hard engineering work: custom C UI/editor/rendering, Markdown parsing/rendered editing, workspace state, images/Base64/assets, version history/diff, Unicode correctness, recovery, deterministic tooling, testing, and evidence.

Windows-native system APIs are allowed only where the Linux edition already needed a platform integration boundary or where Windows itself requires a platform-specific semantic. Higher-level native Windows controls are explicitly prevented from collapsing the intended implementation difficulty.
