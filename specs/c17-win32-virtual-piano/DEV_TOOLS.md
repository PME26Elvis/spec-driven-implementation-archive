# Workstream A — `locscan`

## 1. Product

Implement a standalone native Windows console utility:

`locscan.exe`

Language: C17.

No third-party parser, glob, filesystem traversal, or test library.

---

## 2. Command Contract

Syntax:

`locscan ROOT [--config PATH] [--json PATH|-] [--deterministic]`

Config discovery when `--config` is omitted:

1. `ROOT/.locscan.json` if it exists;
2. otherwise `ROOT/.locscan.yaml` if it exists;
3. otherwise built-in defaults.

If both files exist, JSON wins by this fixed precedence.

`--json -` writes machine JSON to stdout.

Human summary goes to stdout only when JSON is not being written to stdout; diagnostics go to stderr.

---

## 3. Exit Codes

- `0`: success;
- `2`: CLI usage error;
- `3`: config parse/schema error;
- `4`: root traversal/read error;
- `5`: output write error;
- `6`: internal invariant/error.

A traversal error that prevents complete counting returns non-zero; a partial result must not be labeled complete success.

---

## 4. Built-In Includes

Text extensions included by default:

- `.c`;
- `.h`;
- `.md`;
- `.txt`;
- `.json`;
- `.yaml`;
- `.yml`.

Extension matching is case-insensitive on Windows.

---

## 5. Built-In Exclusions

Exclude by default:

Directories/patterns:

- `.git/**`;
- `.vs/**`;
- `build/**`;
- `out/**`;
- `bin/**`;
- `obj/**`;
- `.cache/**`;
- `results/**`;
- `evidence/screenshots/**`;
- `evidence/audio/**`;
- `evidence/cli/**`;
- `evidence/locscan/**`;
- `evidence/test_summary.*`.

Extensions:

- `.obj`, `.o`;
- `.exe`, `.dll`, `.lib`;
- `.pdb`, `.ilk`;
- `.log`;
- `.tmp`, `.cache`;
- `.png`, `.jpg`, `.jpeg`, `.gif`, `.bmp`, `.ico`;
- `.wav`, `.mp3`, `.ogg`;
- `.zip`, `.7z`.

Built-in generated pattern:

- `generated/**`.

User config may add exclusions. It may add included text extensions but cannot force a file containing a NUL byte to be counted as human-authored text.

---

## 6. Traversal

Use Unicode-capable Windows path handling.

Rules:

- recurse ordinary directories;
- do not follow directory reparse points/symlinks/junctions;
- count an ordinary file at most once per discovered path;
- normalize relative report paths to forward slashes;
- sort deterministic file output by normalized path using case-insensitive primary comparison and bytewise tie-break.

---

## 7. Binary/Text Probe

For a candidate included extension:

- an optional initial probe may reject obvious binary data early;
- while streaming the complete file for UTF-8 validation/line counting, if **any** NUL byte is encountered anywhere in the file, classify `excluded_binary` and discard that file's provisional line count;
- UTF-8 BOM is permitted and, when present as the initial `EF BB BF` sequence, is removed from the logical text stream before line counting;
- a BOM-only file therefore has 0 physical text lines;
- invalid UTF-8 in an otherwise included source/document file is a read/classification failure: the scan returns exit 4 and must not emit a complete-success report.

---

## 8. Physical Line Definition

Line counting operates on the validated logical UTF-8 byte stream **after removal of an optional leading UTF-8 BOM**.

An empty logical text stream has 0 lines.

A non-empty logical text stream has:

`number_of_LF_bytes + (1 if final byte is not LF else 0)`

CRLF therefore counts as one line break because LF is the delimiter.

A final line without LF is counted. A CR byte not followed by LF is treated as ordinary content; CR-only line-ending files are not a separate required line-ending mode.

---

## 9. Categories

Required output categories:

- `production_source`;
- `test_source`;
- `documentation`;
- `config_spec`;
- `uncategorized_text`;
- `excluded_generated`;
- `excluded_binary`;
- `excluded_pattern`.

Category matching/priority follows `DATA_FORMATS.md`.

Files under `tests/**` with source extensions are `test_source` even if another broad source pattern also matches.

---

## 10. Human Summary

Human output includes:

- scanned normalized root;
- config source (`built-in` or path);
- each included category file and line total;
- excluded counts by reason;
- total included file count;
- total included physical lines.

---

## 11. JSON Report

Required root fields:

- `schema_version: 1`;
- `root`;
- `config`;
- `deterministic`;
- `categories`;
- `excluded`;
- `totals`;
- `files` when detailed reporting is enabled by the implementation.

At minimum category/exclusion/totals fields are always emitted.

In `--deterministic` mode:

- no current timestamp;
- stable ordering;
- normalized separators;
- identical tree/config -> byte-identical JSON.

---

## 12. JSON and YAML Config

`locscan` shall implement parsers sufficient for the exact schemas/subsets in `DATA_FORMATS.md`.

Malformed/unsupported syntax returns exit 3 with line/column or equivalent useful location information.

No malformed config is silently treated as defaults.

---

## 13. Required Tests

### LOC-001 Empty Directory
0 files, 0 lines, success.

### LOC-002 Three-Line C With Final LF
1 production file, 3 lines.

### LOC-003 Final Line Without LF
Final line is counted.

### LOC-004 CRLF
Three CRLF logical lines count as 3.

### LOC-005 Empty Included File
1 file, 0 lines.

### LOC-006 Build Exclusion
`build/a.c` excluded.

### LOC-007 Test Priority
`tests/test_audio.c` -> test_source.

### LOC-008 NUL Probe
Candidate file with NUL -> excluded_binary, no crash.

### LOC-009 JSON Config
Rules apply exactly.

### LOC-010 YAML Equivalent
Same semantic result as LOC-009.

### LOC-011 Malformed JSON
Exit 3 + diagnostic.

### LOC-012 Unsupported/Malformed YAML
Exit 3 + diagnostic.

### LOC-013 Unicode Filename
Included/countable using Unicode path APIs.

### LOC-014 `**` Pattern
Nested ignored subtree is excluded.

### LOC-015 Generated Directory
`generated/foo.c` -> excluded_generated/pattern according to normalized reason policy, never included.

### LOC-016 Deterministic JSON
Two runs byte-identical.

### LOC-017 Reparse Directory
Not followed; no recursion loop/duplicate scan.

### LOC-018 Case-Insensitive Extension
`FOO.C` is treated as `.c`.

### LOC-019 UTF-8 BOM
BOM does not create an extra line.

### LOC-020 Invalid UTF-8 Fixture
Included candidate with invalid UTF-8 -> exit 4; no complete-success report; never silently inflates line count.

### LOC-021 Exclusion Precedence
A file matching both include/category and exclude is excluded.

### LOC-022 Config Discovery
`.locscan.json` wins over `.locscan.yaml` when both exist.

### LOC-023 JSON Unicode Escape
A config pattern/string containing a valid `\uXXXX` escape (including a surrogate-pair case) decodes correctly; unpaired surrogate input fails with exit 3.

### LOC-024 BOM-Only File
An included file containing only UTF-8 BOM bytes has 0 physical text lines.

### LOC-025 Evidence Output Exclusion
Default scan excludes generated/result evidence under `evidence/cli/**`, `evidence/audio/**`, `evidence/screenshots/**`, `evidence/locscan/**`, and `evidence/test_summary.*`; a human-authored `evidence/acceptance_report.md` remains eligible as documentation unless project config excludes it.

---

## 14. Project Use

The final submission shall include its project-specific `.locscan.json` or `.locscan.yaml` and a deterministic JSON report generated from the final delivered source tree.

The human-authored total shall include production source, tests, and human-readable docs/config as configured, and exclude build/result/evidence binary artifacts.
