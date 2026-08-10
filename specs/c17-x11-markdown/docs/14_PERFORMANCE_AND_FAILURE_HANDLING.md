# 14 — Performance, Responsiveness, Fault Handling, and Corruption Recovery

## 1. Goal

The v1.0 editor MUST remain usable on realistic Markdown workspaces and MUST fail safely when storage, metadata, or input is damaged.

This specification intentionally sets observable responsiveness gates without prescribing threads, event-loop architecture, caching strategy, piece tables, ropes, gap buffers, parsers, or render invalidation algorithms.

## 2. Normative Performance Fixtures

The mandatory fixture generator defined in `15_DEV_FIXTURES_AND_EVIDENCE.md` MUST generate the following profiles.

### 2.1 `medium`

At minimum:

- 10,000 source lines.
- At least 1 MiB UTF-8 source.
- At least 300 headings.
- At least 300 list items.
- At least 30 tables.
- At least 20 local images.
- Mixed Traditional Chinese and English.

### 2.2 `large`

At minimum:

- 50,000 source lines.
- At least 5 MiB UTF-8 source.
- At least 1,000 headings.
- At least 2,000 list items.
- At least 100 tables.
- At least 50 image references, with at least 20 unique image assets.
- Long paragraphs and code blocks.
- Mixed Traditional Chinese, ASCII, combining marks, and fixture emoji.

### 2.3 `stress-long-line`

At least one single logical line of 1 MiB without newline.

The application MAY degrade some syntax highlighting/render optimizations for this fixture but MUST remain editable and must not crash.

## 3. Startup and Open Gates

On a typical contemporary desktop Linux machine meeting the target platform requirements:

- Opening the `medium` fixture from an already-running application MUST produce an interactive source editor within 2 seconds.
- Opening the `large` fixture MUST produce an interactive source editor within 5 seconds.
- Full preview completion MAY continue after source interaction becomes available, but MUST complete within 10 seconds for the `large` fixture when all local images are readable.

The implementation MUST expose no fake completed preview while background work is unfinished; progressive rendering/loading is allowed.

## 4. Typing Responsiveness

After the document is open and idle, ordinary text insertion into a visible paragraph MUST update the source caret/text within 100 ms for `medium` and within 200 ms for `large` in at least 95% of measured fixture keystrokes under the submission's own performance test.

Preview/render updates MAY be debounced, but visible rendered content MUST converge within 500 ms after a short typing burst stops on `medium` and within 1000 ms on `large`.

## 5. Scrolling Responsiveness

Continuous wheel scrolling through `medium` and `large` fixtures MUST remain interactive.

The application MUST NOT intentionally rebuild/reparse the entire document synchronously on every wheel event if that produces repeated visible stalls.

Acceptance fails if normal continuous scrolling produces recurring UI freezes longer than 500 ms on the required fixture/hardware class.

The frosted-navigation animation MUST continue to track scroll state without flicker or stale frames.

## 6. Search Performance

Literal Find across `large` MUST produce the first match and match count within 2 seconds for an ASCII or Traditional Chinese query that occurs throughout the fixture.

Find Next/Previous after indexing/search completion MUST navigate within 250 ms.

Replace All across at least 1000 matches in `large` MUST complete within 5 seconds and remain one undo transaction.

## 7. Outline Performance

Opening/switching to Outline on `large` MUST display heading entries within 2 seconds.

After a heading text edit, the affected Outline label MUST update within the ordinary 250 ms idle target without rescanning causing multi-second input blockage.

## 8. Table and Image Interaction

Resizing a visible image MUST update its visual size continuously enough to follow the pointer, with no intentional one-update-only-on-release implementation.

Dragging table/UI controls MUST remain responsive even when the document contains many off-screen tables/images.

## 9. Memory Baseline

The application MUST not impose a hard-coded document-size limit below 20 MiB UTF-8 source.

Opening the `large` fixture MUST not allocate memory proportional to an obviously pathological expansion such as hundreds of copies of the entire source.

The test report MUST record peak resident memory for `medium` and `large`; v1.0 does not impose one universal byte limit because allocator/font/display behavior varies by Linux environment.

A leak test MUST demonstrate that repeatedly opening and closing the `medium` fixture 20 times does not produce monotonically unbounded retained memory attributable to document objects.

## 10. Event Loop Safety

Long operations that can exceed 500 ms SHOULD expose progress or keep the event loop responsive.

Mandatory operations that can be cancelled without corrupting state SHOULD support cancellation, including very large workspace scans and exports.

The exact concurrency architecture is not prescribed.

## 11. I/O Error Classes

The application MUST explicitly handle at least:

- Permission denied on read.
- Permission denied on write.
- Read-only file or directory.
- Disk/full-space write failure.
- Short/partial write.
- Rename/atomic-replacement failure.
- File disappears between selection and open.
- Asset disappears between parse and render.
- Workspace metadata directory cannot be created.

A failed write MUST leave the in-memory edited document available.

## 12. Safe Save Procedure

Saving an existing document MUST use a failure-safe replacement strategy that avoids truncating the only valid original before the new content is known to be written successfully.

At minimum the implementation MUST:

1. Write replacement content to a temporary file in a compatible destination location.
2. Detect write/close failure.
3. Replace the destination atomically or with the safest platform-appropriate rename sequence only after successful write.
4. Report replacement failure and preserve recoverable temporary content when useful.

Exact permission/metadata preservation beyond ordinary file permissions is not required, but existing file mode SHOULD be preserved.

## 13. Fault Injection

The submission MUST include a test-only fault-injection boundary for file operations or an equivalent deterministic harness capable of forcing:

- Failure after N bytes written.
- Failure on flush/close.
- Failure on rename.
- ENOSPC-style error.
- EACCES-style error.

Fault injection MUST exercise the same production save/recovery logic; it MUST NOT replace the product logic with a fake save implementation.

## 14. Invalid UTF-8

Opening a file containing invalid UTF-8 MUST NOT silently normalize, replace, or overwrite it.

The application MUST show an error identifying that the file is not valid UTF-8.

The user MAY be offered a read-only hex/raw diagnostic view, but it is optional.

The original file MUST remain unchanged unless the user explicitly performs an external conversion outside the mandatory feature set.

## 15. Malformed Markdown

Malformed/incomplete Markdown is valid editor input.

The editor MUST preserve and save it as text.

The renderer MUST recover at sensible construct boundaries and MUST NOT crash, loop indefinitely, or delete source because parsing failed.

The fixture corpus MUST include unterminated fences, unmatched emphasis delimiters, malformed links, incomplete tables, and stray HTML-like text.

## 16. Missing Images

A missing/unreadable image MUST render a stable placeholder containing at least:

- Missing-image indicator.
- Alt text when available.
- Enough path/source identity to diagnose the reference, safely abbreviated if long.

The Markdown source MUST remain unchanged simply because an asset is missing.

Right-click/context actions MUST offer Locate/Relink or equivalent repair behavior.

## 17. Corrupt Image Data

A supported-format file with invalid/corrupt image bytes MUST not crash the application.

It MUST be treated as failed image decode with placeholder/error state.

The source image reference MUST remain intact.

## 18. Workspace State Corruption

Workspace session state under `.mdeditor/` MUST be versioned and integrity-checkable or structurally validated.

If state is truncated or malformed:

- Authored Markdown files MUST still open.
- The application MUST fall back to a safe empty/default session.
- The corrupt state MUST be preserved/renamed for diagnosis or explicitly reported before replacement.

## 19. Version History Corruption

Corruption of a history record MUST NOT corrupt the live Markdown file.

The history viewer MUST identify the affected version/range as unavailable.

If a later version can still be reconstructed from an intact full snapshot chain, it MAY remain available.

The application MUST NOT fabricate reconstructed content when integrity verification fails.

## 20. Recovery State Corruption

A corrupt recovery record MUST be skipped/reported rather than opened as trustworthy document text.

Other valid recovery records MUST remain usable.

## 21. Preferences Corruption

Corrupt global preferences MUST fall back to defined defaults.

The application MUST not fail to start solely because the preferences file is malformed.

## 22. Line-Count/Dev-Tool Corruption

Malformed JSON/YAML configuration for development utilities MUST result in a non-zero documented exit code and a diagnostic containing file and approximate location/context where practical.

Utilities MUST NOT silently proceed with default inclusion rules after a supplied config was rejected.

## 23. Symlinks

Workspace/file-tree and development-utility traversal MUST avoid unbounded symlink cycles.

The editor MAY display symlinked files/directories, but traversal outside the workspace root through symlinked directories MUST be disabled by default for file-tree recursion.

The line-count tool MUST have explicit configuration semantics for following symlinks, defaulting to not following directory symlinks.

## 24. Path Length and Special Characters

Fixtures MUST include paths containing spaces, Traditional Chinese characters, and shell-special punctuation that is legal in Linux filenames.

The product MUST not depend on shell command concatenation for ordinary file operations.

## 25. Error Presentation

Errors MUST be presented in a way that distinguishes:

- What operation failed.
- Which file/object was involved when useful.
- Whether user data remains in memory.
- Available recovery actions.

Technical `errno` text MAY be included but MUST NOT be the only user-facing explanation.

## 26. No Error-State Dead Ends

After a recoverable error, the user MUST retain access to an appropriate action such as Retry, Save As, Close, Locate, Compare, or Cancel.

A modal error MUST not permanently trap focus or leave background controls disabled after dismissal.

## 27. Mandatory Failure Acceptance Matrix

The final evidence MUST demonstrate at least:

1. Read-only source file open succeeds, edit in memory succeeds, Save reports failure, Save As succeeds.
2. Injected ENOSPC during Save leaves original file intact and edited buffer dirty.
3. Injected partial write leaves original intact.
4. Injected rename failure leaves original intact and reports recovery/temp status.
5. Invalid UTF-8 file is rejected without mutation.
6. Truncated workspace-state file does not prevent workspace opening.
7. Corrupt history record does not change live document.
8. Corrupt recovery entry does not block other recoveries.
9. Missing image shows placeholder and source survives Save.
10. Corrupt PNG/JPEG/BMP shows decode error placeholder without crash.
11. Symlink cycle does not hang workspace scan or LOC scan.
12. 1 MiB single line can be opened, navigated, edited, and saved.
13. Large fixture meets open/search/replace responsiveness gates.
14. Twenty open/close cycles complete without unbounded document-memory growth.
