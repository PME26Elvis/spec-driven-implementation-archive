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

On a typical contemporary Windows 11 x64 desktop machine meeting the target platform requirements:

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

The test report MUST record peak process working-set/resident-memory equivalent for `medium` and `large`; v1.0 does not impose one universal byte limit because allocator/font/display behavior varies by Windows environment.

A leak test MUST demonstrate that repeatedly opening and closing the `medium` fixture 20 times does not produce monotonically unbounded retained memory attributable to document objects.

## 10. Event Loop Safety

Long operations that can exceed 500 ms SHOULD expose progress or keep the event loop responsive.

Mandatory operations that can be cancelled without corrupting state SHOULD support cancellation, including very large workspace scans and exports.

The exact concurrency architecture is not prescribed.

## 11. I/O Error Classes

The application MUST explicitly handle at least:

- Access denied on read.
- Access denied on write.
- Windows Read-only attribute / unwritable target.
- Disk/full-space write failure.
- Short/partial write.
- Flush failure.
- Final replace/move failure.
- Sharing violation / target locked incompatibly by another process.
- File disappears between selection and open.
- File is externally renamed/deleted while open.
- Asset disappears between parse and render.
- Workspace metadata directory cannot be created.
- Path invalid/reserved under the Windows filename contract.
- Long-path operation failure without truncating or silently retargeting the path.

A failed write MUST leave the in-memory edited document available.

The application MUST NOT attempt privilege elevation, ACL bypass, or silent read-only-attribute removal merely to convert these failures into a successful Save.

## 12. Safe Save Procedure

Saving an existing document MUST use a Windows failure-safe replacement strategy that avoids truncating the only valid original before the new content is known to be written successfully.

At minimum the implementation MUST:

1. Serialize the complete intended UTF-8 bytes before or during staging without modifying the destination.
2. Create a temporary replacement file in the same destination directory/volume.
3. Write all intended bytes and detect short/partial writes.
4. Perform a durable file-data flush appropriate to Windows before committing.
5. Close/finalize the staging writer before the destination replacement operation.
6. If replacing an existing target, use a Windows same-volume replacement primitive with semantics equivalent to `ReplaceFileW` or another design that preserves the old valid target until commit.
7. If creating a new target, use a safe final move/rename after successful staging, with write-through/durability behavior appropriate to the chosen primitive.
8. Clear dirty state only after the final commit succeeds.
9. On pre-commit failure, leave the previous target bytes intact.
10. On commit failure, keep edited content in memory, report the failure, and preserve or clean the temporary file according to a documented recovery-safe policy.

The application MUST NOT implement Save as `open(target, truncate) → write`, nor the Win32 equivalent of truncating the only valid target before the replacement bytes are proven complete.

Exact NTFS metadata preservation (ACLs, alternate data streams, compression/encryption flags, owner metadata) beyond ordinary safe replacement behavior is not a v1.0 feature. The editor MUST NOT deliberately clear a read-only attribute or bypass ACLs to force Save, and SHOULD preserve ordinary attributes/timestamps where the chosen safe-replacement method naturally permits it.

The detailed Windows safe-save contract in `16_WINDOWS_PLATFORM_CONTRACT.md` is normative.

## 13. Fault Injection

The submission MUST include a test-only fault-injection boundary for file operations or an equivalent deterministic harness capable of forcing:

- Failure after N bytes written.
- Failure on flush.
- Failure during staging-file close/finalization where meaningful.
- Failure on final Replace/Move commit.
- Windows disk-full-class failure.
- Windows access-denied-class failure.
- Windows sharing-violation/locked-target failure.

Fault injection MUST exercise the same production save/recovery logic; it MUST NOT replace the product logic with a fake save implementation.

Failure injection MUST be controllable deterministically enough for the evidence suite to prove that the original target remains intact and the in-memory document remains dirty after each failed commit path.

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

## 23. Link/reparse-points

Workspace/file-tree and development-utility traversal MUST avoid unbounded directory reparse-point, junction, or symbolic-link cycles.

By default, directory reparse points MUST NOT be recursively followed.

A reparse-point entry MAY be displayed in the workspace tree, but recursive traversal outside the workspace root through a reparse point MUST be disabled by default.

The line-count tool MUST expose the `follow_directory_reparse_points` boolean defined in the Windows platform contract, defaulting to `false`.

`evidencecheck` MUST reject a reparse-point traversal that causes a referenced path to resolve outside its selected root.

If host policy prevents creating a real reparse-point cycle for E2E testing, the mandatory gate MUST use the deterministic production-traversal boundary fallback defined in `16_WINDOWS_PLATFORM_CONTRACT.md`; it MUST NOT simply be skipped.

## 24. Path Length and Special Characters

Fixtures MUST include paths containing spaces, Traditional Chinese characters, parentheses, square brackets, `#`, `&`, hyphen, and underscore.

Mandatory path tests MUST include an absolute local path longer than the legacy 260-character `MAX_PATH` limit as defined in `16_WINDOWS_PLATFORM_CONTRACT.md`.

New/Rename handling MUST reject Windows-invalid/reserved filename cases non-destructively.

The product MUST not depend on `cmd.exe`, PowerShell, or shell command concatenation for ordinary file operations.

## 25. Error Presentation

Errors MUST be presented in a way that distinguishes:

- What operation failed.
- Which file/object was involved when useful.
- Whether user data remains in memory.
- Available recovery actions.

Technical `GetLastError()`/Win32 error-code information MAY be included but MUST NOT be the only user-facing explanation.

## 26. No Error-State Dead Ends

After a recoverable error, the user MUST retain access to an appropriate action such as Retry, Save As, Close, Locate, Compare, or Cancel.

A modal error MUST not permanently trap focus or leave background controls disabled after dismissal.

## 27. Mandatory Failure Acceptance Matrix

The final evidence MUST demonstrate at least:

1. Read-only source file open succeeds, edit in memory succeeds, Save reports failure, Save As succeeds.
2. Injected Windows disk-full failure during Save leaves original file intact and edited buffer dirty.
3. Injected partial write leaves original intact.
4. Injected final Replace/Move failure leaves original intact and reports recovery/temp status.
5. A sharing-violation/locked-target Save failure leaves the edited buffer dirty and offers recovery actions.
6. Invalid UTF-8 file is rejected without mutation.
7. Truncated workspace-state file does not prevent workspace opening.
8. Corrupt history record does not change live document.
9. Corrupt recovery entry does not block other recoveries.
10. Missing image shows placeholder and source survives Save.
11. Corrupt PNG/JPEG/BMP shows decode error placeholder without crash.
12. Directory reparse-point recursion does not hang workspace scan or LOC scan and does not silently escape the root.
13. A >260-character Unicode path fixture can be opened, edited, saved, rescanned, and verified.
14. Windows-invalid/reserved New/Rename filename cases fail without corrupting tree/tab state.
15. 1 MiB single line can be opened, navigated, edited, and saved.
16. Large fixture meets open/search/replace responsiveness gates.
17. Twenty open/close cycles complete without unbounded document-memory growth.
