# 06 — Snapshot Diff, Restore, and Reports

## 1. Diff identity rules

Diff compares two snapshot trees by canonical relative path and entry semantics.

At each path, classify exactly one primary status:

- `A` only in NEW;
- `D` only in OLD;
- `T` exists in both but semantic entry family differs (directory, symlink, or regular-file family); a normal-file vs hardlink representation change remains in the regular-file family and is evaluated as `H`, not `T`;
- `M` regular-file-family resolved content differs;
- `P` content/type same but tracked metadata differs;
- `H` byte content and basic metadata may be same but hard-link topology differs;
- unchanged entries are omitted from normal text output but counted internally.

If multiple conditions apply, precedence is `T > M > H > P`.

## 2. File content equality

Regular file content is equal when FILE logical size and `file_content_sha256` match. Chunk sequence differences with equal full-file bytes should not normally occur under the same chunking profile but MUST not cause a false content change.

## 3. Chunk reuse metric

For a modified file, report byte-weighted chunk reuse from OLD in NEW:

```text
reused_bytes = sum(length of NEW chunk references whose CID occurs in OLD file)
reuse_percent = reused_bytes / new_logical_size * 100
```

Multiplicity matters: matching occurrences may not be counted more times than available in OLD. Implement with a multiset of `(CID,length)` occurrences.

Empty-to-empty is unchanged; empty NEW modified from nonempty reports 0% reuse.

## 4. Metadata diff

Tracked metadata fields:

- mode;
- mtime_ns;
- symlink target;
- hard-link group/topology;
- entry type.

Diff JSON MUST enumerate which fields changed.

## 5. Directory summaries

Text and SVG reports SHOULD aggregate changes by top-level directory. JSON MUST provide sufficient raw entry data for callers to aggregate.

## 6. Rename behavior

Rename detection is not required for format v1. A moved path is represented as `D` + `A`. Implementations MAY add an informational same-content hint but MUST NOT suppress the canonical add/delete entries.

## 7. Restore target safety

Restore writes only beneath `--to PATH`.

Before writing any entry, path joining MUST reject:

- absolute archived paths;
- `..` traversal;
- embedded NUL;
- an existing symlink in the destination prefix that would redirect writes outside the target;
- path-type conflicts inconsistent with overwrite policy.

This protection applies even if repository metadata is corrupted or maliciously edited.

## 8. Restore overwrite policies

`never`:

- destination root may exist;
- no existing non-directory target entry may be replaced;
- existing directories may be reused only where directory type matches.

`files`:

- existing regular files and symlinks at exact target paths may be replaced atomically;
- nonempty directory replacement and type replacement involving directories are rejected.

`all`:

- conflicting files/symlinks/directories under the selected restore root may be replaced as needed, but the implementation MUST stay confined to restore root.

## 9. Restore order

Safe canonical order:

1. create required directories with temporary permissive owner access;
2. restore primary regular files to temporary names, verify, then rename into place;
3. create hard links;
4. create symlinks;
5. apply final file modes/mtimes;
6. apply directory modes/mtimes bottom-up.

Equivalent safe ordering is acceptable if behavior matches.

## 10. Restore file verification

For each restored regular file:

- decode each chunk;
- verify chunk CID;
- stream bytes to a temporary destination file;
- compute full-file SHA-256;
- verify it matches FILE object digest;
- fsync and rename to final path.

A failed integrity check MUST not leave a successfully named corrupt final file.

## 11. On-demand parity recovery during restore

If a referenced chunk is missing or corrupt:

- restore MAY attempt parity reconstruction automatically;
- reconstructed bytes MUST match expected CHUNK CID;
- successful reconstruction may be used for this restore;
- repository mutation/repair of the canonical object requires explicit repair permission unless the command specification documents restore as calling the same repair path under an explicit `--repair` option.

Mandatory behavior: without repair permission, the repository itself is not silently modified.

## 12. Partial restore

`--path PATH` restores one snapshot subtree/path.

- for a regular file, restore that file;
- for a directory, restore the entire subtree;
- path lookup is bytewise/canonical;
- hard links whose primary is outside the selected subtree are recreated as independent content only if an explicit documented fallback is enabled; mandatory v1 behavior instead restores the file content as a regular file and reports that hard-link topology could not be preserved for the partial selection.

This partial-restore hardlink exception MUST be reported and tested.

## 13. Restore reports

Text output MUST report:

- selected snapshot;
- selected subtree;
- files/dirs/symlinks/hardlinks restored;
- logical bytes written;
- chunks read;
- chunks parity-reconstructed for the operation;
- skipped existing entries if policy permits any skip;
- final success/failure status.

JSON/NDJSON equivalents MUST expose structured counters and per-error details.

## 14. Stats command

`darc stats` MUST report repository-wide:

- snapshot count;
- canonical object count by type;
- logical bytes referenced by current HEAD;
- logical bytes across snapshots;
- unique chunk logical bytes;
- stored chunk bytes;
- compression savings;
- cross-snapshot dedup savings;
- parity overhead;
- unreachable object estimates if cheaply available;
- index load factor/capacity;
- repository format/profile summary.

## 15. Stats SVG

Standalone SVG MUST include:

- repository headline totals;
- bar comparing logical data, unique chunk bytes, compressed stored bytes, and parity overhead;
- snapshot trend table or simple polyline for up to the newest 50 snapshots;
- dedup/compression percentages;
- no external resources.

## 16. SVG escaping and determinism

SVG serialization MUST:

- XML-escape all text;
- produce stable element ordering;
- avoid random IDs;
- use fixed locale-independent decimal formatting;
- emit the same bytes for the same report data and CLI options.

## 17. NDJSON streaming

For potentially large diff/verify result sets, NDJSON MUST allow streaming without buffering the full entry list. A summary record MUST be clearly typed, e.g. `{"record":"summary",...}`.
