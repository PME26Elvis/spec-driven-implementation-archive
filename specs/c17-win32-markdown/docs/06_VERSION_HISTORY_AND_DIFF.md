# 06 — Version History and Graphical Diff

## 1. Purpose

The application MUST include built-in document version history suitable for recovering earlier document states and visually comparing versions.

The feature MUST be application-owned and MUST NOT be a thin Git GUI.

## 2. Version Object

A stored document version MUST include at least:

- Stable version identifier.
- Creation timestamp.
- Document content representation sufficient for exact restoration.
- Parent/predecessor relationship or equivalent ordering information.
- Human-readable sequence position or timestamp in the history UI.

Optional metadata MAY include a user-authored note. If supported, the note MUST not affect source restoration or diff identity.

## 3. Creation Policy

A version MUST be created after each successful explicit Save/Save All for which source content differs from the most recent stored version. The application MUST also provide an explicit `Create Version` command. Autosave recovery writes MUST NOT create user-visible history versions. Repeated Save with unchanged source MUST NOT create duplicate versions.

Regardless of policy, tests MUST be able to create multiple deterministic historical states without waiting for long real-time intervals.

## 4. History Persistence

Version history MUST survive application restart.

Closing and reopening the same document MUST preserve its available historical versions according to the v1.0 retention policy in Section 17.

History MUST not exist only in process memory.

## 5. Exact Restore

Restoring a historical version MUST restore the Markdown source represented by that version exactly.

The current live document state MUST not be silently destroyed during restore.

The application MUST require an explicit restore action and confirmation when restoration will replace unsaved content.

A successful restore MUST be applied to the live document as one undoable transaction and MUST leave the document dirty until an explicit Save succeeds. One Undo immediately after restore MUST return to the pre-restore live source.

## 6. Version Storage Architecture

The implementation MUST use a space-aware history representation rather than naively keeping an unlimited number of independent uncompressed full-file copies without policy.

The required baseline design is:

- Periodic full snapshot/checkpoint.
- Delta records between compatible versions.
- A deterministic reconstruction path.
- Integrity metadata sufficient to detect malformed/corrupt history records.

History storage MUST use a reconstructable chain with a full source snapshot at least every 20 stored versions and deltas between intervening versions. Deltas MUST be based on the authored Myers-style line diff or an equivalently deterministic representation derived from it. Stored snapshot/delta payloads MUST additionally support the task-pack LZSS compression profile: 4096-byte sliding window, minimum match length 3, maximum match length 18; an implementation MAY store an individual payload uncompressed when compression does not reduce its size. Format records MUST identify compressed/uncompressed form and carry integrity metadata.

## 7. Diff Algorithm

The submission MUST implement a real sequence-diff algorithm.

The required baseline algorithm is Myers-style shortest-edit-script diff for line-level comparison. An implementation MAY optimize the same result, but ordinary test cases MUST produce the deterministic edit regions defined by the fixture expectations.

A hard-coded comparison or simplistic “whole document changed” fallback is insufficient for ordinary edits.

## 8. Two-Level Diff

For modified lines, the UI MUST provide finer-grained comparison so that a small word change is not shown only as a complete line deletion plus insertion when refinement is possible.

Required conceptual stages:

1. Line-level diff.
2. Word/token-level refinement inside paired modified regions.

Word-level refinement MUST tokenize runs of ASCII letters/digits/underscore, individual non-ASCII editing units from the Unicode fixture rules, whitespace runs, and punctuation runs as deterministic token classes. Exact expected refinements for normative fixtures MUST be asserted by tests.

## 9. Diff States

The diff model MUST distinguish at least:

- Unchanged.
- Added.
- Deleted.
- Modified/refined.

The UI MUST not communicate all states using color alone.

Additional glyphs, markers, backgrounds, or gutter indicators MUST provide structural distinction.

## 10. History UI

The application MUST provide a graphical history surface containing:

- Ordered version list/timeline.
- Identifiable current/live state.
- Timestamp for each stored version.
- Ability to choose a version for comparison.
- Ability to restore a selected version.

The history view MUST not require command-line interaction.

## 11. Side-by-Side Diff

A side-by-side diff view is mandatory.

It MUST present an older/base version and newer/target version in separate coordinated panes.

Requirements:

- Added/deleted/modified regions clearly visible.
- Line alignment or gap treatment preserves meaningful correspondence.
- Scrolling one pane MUST keep the counterpart pane aligned to the same diff hunk/nearest corresponding line. The panes MAY allow small independent movement within a hunk, but navigating to a new hunk MUST re-establish alignment.
- Long lines MUST remain inspectable through wrapping or horizontal navigation.

## 12. Inline/Unified Diff

An inline/unified graphical diff view is mandatory.

It MUST show changes in a single reading flow.

Added and removed content MUST be distinguishable.

Modified-line token refinement MUST be visible where applicable.

## 13. Diff Navigation

The diff UI MUST provide navigation to:

- Next change.
- Previous change.

When there are no differences, the UI MUST explicitly communicate that the compared contents are identical.

## 14. Unicode Correctness

Diff processing MUST preserve UTF-8 validity.

A Chinese character must not appear as several unrelated byte-level changes.

Token refinement MUST not create invalid source slices.

## 15. Images in Diff

When image Markdown source changes, the source-level change MUST appear in diff.

When an image source/destination changes, source-level diff is mandatory. If both old and new referenced images can be decoded, the graphical diff SHOULD show old/new thumbnails; thumbnail comparison is not a release gate. Missing/undecodable historical assets MUST not prevent textual diff.

## 16. History Integrity Failure

If a history record cannot be reconstructed or fails its integrity check:

- The application MUST report the problem.
- The live document MUST remain available.
- The application MUST NOT restore corrupted bytes as though successful.
- Other valid historical versions SHOULD remain accessible if independently reconstructable.

## 17. History Retention

The history store MUST retain up to 200 versions per document and up to 64 MiB of encoded history payload per document. When either limit would be exceeded, the oldest reconstructable version group MUST be pruned while preserving a valid full-snapshot starting point for all retained versions. Versions explicitly marked `Pinned` by the user MUST not be automatically pruned; if pinned versions alone prevent compliance with the storage cap, the application MUST warn and stop automatic pruning rather than delete pinned history. The UI MUST allow pin/unpin and manual delete of unpinned versions.
