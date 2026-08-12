# 01 — CLI Contract and Presentation Standard

## 1. General invocation

Canonical executable:

```text
darc [global-options] <command> [command-options]
```

Global options:

```text
--repo PATH
--config PATH
--color auto|always|never
--format text|json|ndjson
--quiet
--verbose
--help
--version
```

`--repo` selects the repository directory. Commands that do not need a repository, such as `config validate`, ignore it.

## 2. Required commands

```text
darc init PATH
darc snapshot create SOURCE... [--name NAME] [--parent SNAPSHOT] [--timestamp NS]
darc snapshot list
darc snapshot show SNAPSHOT
darc snapshot delete SNAPSHOT --yes [--dry-run]
darc snapshot diff OLD NEW [--path PATH] [--format text|json|ndjson|svg]
darc restore SNAPSHOT --to PATH [--path PATH] [--overwrite POLICY]
darc verify [--level quick|full|scrub] [--repair] [--format text|json|ndjson|svg]
darc gc [--dry-run] [--repack-parity]
darc stats [--format text|json|ndjson|svg]
darc index rebuild
darc repo inspect
darc config validate FILE
```

Command aliases are optional, but the canonical forms above MUST work.

## 3. Exit codes

The executable MUST use stable exit codes:

| Code | Meaning |
|---:|---|
| 0 | success |
| 2 | CLI usage or configuration validation error |
| 3 | repository not found / wrong repository |
| 4 | requested snapshot/path not found |
| 5 | I/O failure |
| 6 | repository corruption detected |
| 7 | recovery attempted but data remains unrecoverable |
| 8 | restore destination conflict / unsafe destination |
| 9 | lock conflict / repository busy |
| 10 | unsupported format version or feature flag |
| 11 | internal invariant failure |

When multiple failures occur, the most severe repository-integrity status wins over presentation-only warnings.

## 4. Stdout and stderr contract

- Successful primary command output goes to stdout.
- Diagnostics, warnings, progress, and errors go to stderr.
- `--format json` MUST emit exactly one valid JSON document to stdout, with no human preamble.
- `--format ndjson` MUST emit one valid JSON object per line.
- `--format svg` MUST emit a complete standalone UTF-8 SVG document to stdout and no ANSI escape sequences.
- Progress MUST NOT corrupt JSON/NDJSON/SVG stdout.
- `--quiet` suppresses non-error diagnostics, not required command data.
- Error messages MUST include a stable machine code such as `E_REPO_CORRUPT` in addition to human text.

## 5. Color and terminal behavior

- `--color=never` emits no ANSI SGR sequences.
- `--color=always` may use ANSI colors in text mode only.
- `--color=auto` uses color only when stdout is an interactive terminal.
- JSON, NDJSON, and SVG never contain terminal escape sequences.
- Human output MUST remain understandable with color disabled.
- Unicode box-drawing MAY be used when locale/display capability is appropriate, but ASCII fallbacks MUST preserve meaning.

## 6. Snapshot list presentation

Text output MUST have a stable header and aligned columns similar in information density to:

```text
SNAPSHOT       CREATED                  PARENT         FILES   LOGICAL     STORED   DEDUP
9f31ab2c1e4d   2026-08-11T05:12:03Z     4aa91d...      1842    6.41 GiB    1.92 GiB 70.0%
```

Requirements:

- short snapshot ID;
- UTC creation time;
- parent ID or `-`;
- file count;
- logical byte total;
- newly stored byte total;
- deduplication ratio or reuse ratio;
- deterministic sorting newest-first unless explicitly overridden.

## 7. Snapshot show presentation

Must include:

- full snapshot ID;
- snapshot name if any;
- creation timestamp;
- parent snapshot;
- source roots;
- root Merkle ID;
- file/dir/symlink/hardlink counts;
- logical bytes;
- unique chunks referenced;
- newly introduced chunks;
- stored compressed bytes;
- parity protection summary;
- config/chunking profile hash.


## 7A. Snapshot deletion

`darc snapshot delete SNAPSHOT --yes` removes only the selected published snapshot ref. It does not immediately delete canonical objects; reclamation belongs to `gc`.

Requirements:

- `--yes` is mandatory for actual deletion so scripts cannot accidentally rely on an interactive prompt.
- `--dry-run` performs resolution and reports what ref/HEAD change would occur with zero mutation and does not require `--yes`.
- a snapshot still reachable only as another snapshot's informational parent may have its ref deleted; parent linkage is soft history metadata, not a GC root.
- if deleting HEAD, HEAD moves to the remaining snapshot with greatest `created_ns`; ties use lexicographically greatest full CID. If no refs remain, HEAD becomes empty.
- ref deletion and any HEAD replacement must follow the atomic write/journal rules.
- deletion output must explicitly state that objects are retained until GC.

## 8. Diff human presentation

`darc snapshot diff OLD NEW` is a high-quality report, not a raw line dump.

Text mode MUST contain:

1. A summary block with:
   - added / removed / modified / metadata-only / type-changed counts;
   - logical byte delta;
   - reused chunk count and bytes;
   - new chunk count and bytes;
   - removed-but-still-referenced chunk count where knowable.
2. A path table sorted bytewise by canonical path unless `--sort` is later extended.
3. Status symbols:
   - `A` added
   - `D` deleted
   - `M` content modified
   - `P` metadata/permission modified only
   - `T` entry type changed
   - `H` hard-link topology changed
4. Old/new logical sizes for files.
5. A compact chunk-reuse indicator for modified files.

Example style:

```text
Summary: +12  -3  M7  P4  T0  H1    logical +18.2 MiB    chunk reuse 91.4%

ST  OLD SIZE   NEW SIZE   REUSE   PATH
M   48.2 MiB   48.2 MiB   99.1%   src/video.bin
A       -       12 KiB      -     notes/新檔案.txt
P    4.1 KiB    4.1 KiB  100.0%   scripts/run.sh
```

`--path PATH` restricts reporting to that subtree while preserving correct aggregate totals for the selected subtree.

## 9. Diff JSON schema requirements

JSON output MUST include at least:

```json
{
  "old_snapshot": "...",
  "new_snapshot": "...",
  "summary": {
    "added": 0,
    "deleted": 0,
    "modified": 0,
    "metadata_only": 0,
    "type_changed": 0,
    "hardlink_changed": 0,
    "logical_delta": 0,
    "reused_chunk_bytes": 0,
    "new_chunk_bytes": 0
  },
  "entries": []
}
```

Each entry MUST identify status, canonical path representation, old/new entry types, old/new sizes when applicable, metadata deltas, and content/chunk reuse metrics when applicable.

## 10. Diff SVG report

`--format svg` MUST generate a standalone SVG 1.1-compatible document without external CSS, JavaScript, fonts, raster images, or network references.

The SVG MUST contain:

- title with both snapshot short IDs;
- summary cards for A/D/M/P/T/H counts;
- logical-byte delta;
- chunk reuse percentage;
- a horizontal reuse/new-data bar;
- a legible top-N changed-path table, default N=50;
- an explicit note when rows are truncated;
- XML-escaped paths;
- a `viewBox` and readable dimensions;
- no dependence on GUI libraries.

SVG generation is text serialization and is part of the CLI/reporting functionality.

## 11. Verify presentation

Text verify output MUST clearly separate:

- repository metadata;
- canonical objects;
- chunk payload integrity;
- Merkle reachability;
- parity stripe status;
- index consistency;
- repair actions if any;
- final result: `HEALTHY`, `DEGRADED-REPAIRABLE`, `REPAIRED`, or `UNRECOVERABLE`.

Failures MUST list object IDs and affected snapshot/path references where derivable.

SVG verify output MUST visualize counts of healthy/corrupt/missing/repaired/unrecoverable objects and parity stripes, plus a textual final status.

## 12. GC presentation

`gc --dry-run` MUST report:

- reachable objects;
- unreachable objects;
- reclaimable stored bytes;
- parity stripes to rewrite/drop;
- index changes;
- zero repository mutations.

Normal `gc` MUST report actual removed/repacked counts after success.

## 13. Progress reporting

Long operations MAY emit progress to stderr. If implemented, progress MUST:

- avoid one line per file by default;
- update at a bounded rate;
- provide scanned files, logical bytes, chunks, new chunks, reused chunks, and elapsed time where available;
- degrade cleanly to periodic plain lines when stderr is not a terminal.

## 14. Help quality

Every required command and subcommand MUST have `--help` text describing mandatory arguments, destructive behavior, output formats, and at least one usage example. Help text is part of acceptance.
