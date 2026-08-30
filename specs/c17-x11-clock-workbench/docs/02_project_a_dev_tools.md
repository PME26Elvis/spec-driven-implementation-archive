# 02 — Project A: Development Utilities

## 1. Overview

Project A contains mandatory command-line utilities written in C17.

They are part of the assignment because they exercise parsing, filesystem traversal, deterministic reporting, and validation independently from the graphical application.

At minimum, Project A must provide the following executables:

1. `locscan` — classified human/source line counter with JSON/YAML configuration.
2. `cfgcheck` — JSON/YAML configuration parser and schema validator for Project B configuration files.
3. `stateprobe` — deterministic validator for saved Project B state and history fixtures.

The utilities may share reusable C modules with Project B when appropriate.

## 2. Common utility requirements

Every Project A executable must:

- return exit code `0` on successful validation/execution;
- return nonzero on invalid command line, unreadable required input, parse failure, or validation failure;
- write normal machine-consumable output to stdout when requested;
- write diagnostics to stderr;
- avoid absolute-path assumptions;
- behave deterministically for the same input tree/files;
- support paths containing spaces;
- reject malformed UTF-8 only where the relevant file format requires valid UTF-8;
- not silently truncate files or counters due to 32-bit overflow.

## 3. `locscan`

### 3.1 Purpose

`locscan` recursively scans a directory tree and counts lines only for files classified as human-authored source or documentation according to its configuration.

Its purpose is to distinguish implementation/documentation from logs, generated output, binary files, reports produced by tests, caches, and build artifacts.

### 3.2 Required invocation forms

At minimum:

```text
locscan ROOT
locscan ROOT --config FILE
locscan ROOT --config FILE --format text
locscan ROOT --config FILE --format json
```

Unknown options must fail with a concise usage message.

### 3.3 Configuration formats

`locscan` must accept its config in either JSON or YAML.

Both formats map to one logical schema.

A `locscan` config path must end in `.json`, `.yaml`, or `.yml`; the extension selects the parser. Unsupported extensions are rejected. Invalid content for the selected parser is an error.

### 3.4 Classification schema

The exact version-1 configuration keys are:

- `include_extensions`: **required** list of extensions counted as authored files; may be empty;
- `include_names`: optional list of exact basenames counted even when extension rules would not include them, default empty;
- `documentation_extensions`: **required** subset/list classified as human-readable documentation; may be empty;
- `documentation_names`: optional subset/list of included exact basenames classified as documentation, default empty;
- `exclude_extensions`: optional explicit extension exclusions, default empty;
- `exclude_paths`: optional path/glob rules for directories or files, default empty;
- `exclude_names`: optional exact basename exclusions, default empty;
- `binary_detection`: optional boolean, default true;
- `max_file_bytes`: optional integer safety ceiling, default 8 MiB (8,388,608 bytes), valid range 1..1,073,741,824;
- `follow_symlinks`: optional boolean, default false;
- `use_default_exclusions`: optional boolean, default true;
- `categories`: **required** mapping of category names to extension lists; each list may be empty;
- `category_names`: optional mapping of category names to exact basename lists, default empty mapping.

Unknown keys are errors. Missing required keys are schema errors. All list elements and mapping/category names must have the types specified here; values are not coerced from numbers/booleans to strings.

Schema relationships are mandatory:

- every `include_extensions` entry begins with `.` and is compared case-sensitively;
- every included extension must appear in exactly one `categories` list;
- every `include_names` entry must appear in exactly one `category_names` list;
- one extension/name appearing in two categories is a configuration error;
- `documentation_extensions` must be a subset of `include_extensions`;
- `documentation_names` must be a subset of `include_names`;
- category names must match `[A-Za-z][A-Za-z0-9_-]{0,31}`;
- each category referenced by `category_names` must also exist in `categories` (its extension list may be empty);
- `exclude_extensions`, excluded paths, and `exclude_names` win over inclusion/category membership.

If a file matches both an `include_names` basename and an `include_extensions` extension, exact-name classification wins and it is counted once.

### 3.5 Required default exclusions

The shipped default config must exclude common non-authored/output locations such as:

- `**/.git/`;
- `**/build/`;
- `**/dist/`;
- `**/out/`;
- `**/tmp/`;
- `**/cache/`;
- `**/logs/`;
- `**/results/`;
- test-generated snapshots or captures when placed in documented output directories.

These defaults are active when `use_default_exclusions` is true. Setting it to false removes the built-in path exclusions entirely; explicit configured `exclude_paths`, `exclude_names`, and `exclude_extensions` still apply.

When `locscan ROOT` is invoked without `--config`, it must use built-in defaults logically equivalent to `examples/locscan.json`: C/header files, Markdown, JSON/YAML and the exact basename `Makefile` are included; Markdown is documentation; `Makefile` belongs to the `source` category; `.o/.a/.so/.png/.jpg/.ppm/.log` and exact names `core`/`core.dump` are excluded; the required default path exclusions are enabled; binary detection is enabled; the size ceiling is 8 MiB; and symlinks are not followed.


### 3.5A Exclusion-path grammar and precedence

All scanned paths are normalized to root-relative paths using `/` separators before matching. Matching is case-sensitive. `.` and `..` components are normalized during traversal and matches never operate on an absolute host prefix.

`exclude_paths` uses this required small glob grammar. The matcher itself is part of Project A's required implementation work: it must be implemented in submitted C and must not delegate matching to POSIX `glob(3)`, `fnmatch(3)`, a regex library, a shell, or another process. Ordinary POSIX directory traversal/stat primitives remain allowed.

- ordinary characters match themselves;
- `?` matches exactly one non-`/` character;
- `*` matches zero or more non-`/` characters inside one path segment;
- a path segment consisting exactly of `**` matches zero or more complete path segments;
- a pattern ending in `/` excludes that directory and all descendants;
- patterns are matched against the entire normalized root-relative path, not arbitrary substrings.

Example: `build/` excludes the root `build` tree, `**/generated/` excludes any directory named `generated`, and `src/*.gen.c` excludes generated C files directly under `src`.

Precedence for a regular file is fixed:

1. path/name exclusion;
2. extension exclusion;
3. size ceiling;
4. binary detection;
5. exact-name inclusion, otherwise extension inclusion;
6. exactly one configured exact-name or extension category.

A file rejected at any earlier step is skipped and is not counted by later categories.

### 3.6 Line counting semantics

A line is a sequence ending in LF, plus a final non-empty byte sequence not ending in LF.

CRLF must count as one line ending.

An empty file has zero lines.

A one-byte file containing only LF has one line.

When `binary_detection` is true, any NUL byte anywhere in a candidate file at or below `max_file_bytes` classifies it as binary and skips it. The implementation must inspect the whole candidate file for this purpose, not only the first block. When `binary_detection` is false, NUL has no special meaning for line counting.

A file larger than `max_file_bytes` is skipped with reason `oversize`; it is not a fatal scan error. An unreadable candidate file is a scan error and causes nonzero exit status after a diagnostic rather than silently changing totals.

### 3.7 Output

Text output must include:

- total included files;
- total authored lines;
- total documentation lines;
- total non-documentation authored lines;
- skipped files count;
- per-category file/line counts;
- per-extension file/line counts;
- per-exact-name file/line counts for files whose winning inclusion rule is `include_names`.

`documentation_lines` is the sum of lines in included files whose winning classification is documentation by either `documentation_names` (for an exact-name match) or `documentation_extensions` (for an extension match). Exact-name classification wins when both apply. `non_documentation_lines = authored_lines - documentation_lines`. Category counts are additional mutually exclusive classification counts because every included file belongs to exactly one configured category.

JSON output must use this stable top-level schema:

```json
{
  "version": 1,
  "root": ".",
  "included_files": 13,
  "authored_lines": 920,
  "documentation_lines": 200,
  "non_documentation_lines": 720,
  "skipped_files": 11,
  "categories": {
    "config": {"files": 2, "lines": 50},
    "documentation": {"files": 2, "lines": 200},
    "source": {"files": 9, "lines": 670}
  },
  "extensions": {
    ".c": {"files": 5, "lines": 500},
    ".h": {"files": 3, "lines": 150},
    ".json": {"files": 1, "lines": 25},
    ".md": {"files": 2, "lines": 200},
    ".yaml": {"files": 1, "lines": 25}
  },
  "names": {
    "Makefile": {"files": 1, "lines": 20}
  },
  "skipped_by_reason": {
    "binary": 1,
    "excluded": 5,
    "not_included": 2,
    "oversize": 2,
    "symlink": 1
  }
}
```

The real report includes every configured category, including zero-match categories, every extension-included extension with at least one winning match, and every exact `include_names` basename with at least one winning match in `names`. A file whose exact-name inclusion wins appears in `names` and its category but is not also duplicated in `extensions`, even if its basename has an otherwise included extension. The sum of `extensions.*.files + names.*.files` therefore equals `included_files`, and the corresponding line sums equal `authored_lines`. `skipped_by_reason` always contains the five standard keys shown above, including zero values. `skipped_files` is their sum. Regular files with no matching exact-name/extension inclusion are `not_included`; symbolic links skipped because `follow_symlinks=false` are `symlink`. Excluded directories themselves are not files and their unvisited descendants do not inflate `skipped_files`. An unreadable file is an execution error rather than a skip reason.

`root` is the lexically normalized command-line root argument (redundant `.` components and trailing separators removed, except root `/`) and must not be replaced by a machine-specific resolved absolute path. Object-key ordering in emitted JSON is deterministic lexicographic UTF-8 byte order for category/extension/name/reason maps.

Paths in JSON must use UTF-8 strings.

### 3.8 Sorting

Output categories and per-extension rows must have deterministic ordering.

Filesystem traversal order must not affect the report.

### 3.9 Symlinks

Default behavior is to skip all symbolic links (file and directory links).

If `follow_symlinks` is enabled, file symlinks may be counted through their target and directory symlinks may be traversed, but the scanner must prevent recursive cycles by tracking underlying filesystem identity (for example device+inode) rather than only path strings.

### 3.10 Required tests

Tests must cover:

- empty tree;
- one text file without terminal newline;
- CRLF file;
- binary file containing a NUL near the end of the file, proving whole-file binary detection;
- the same NUL-containing fixture with `binary_detection=false`;
- a file above `max_file_bytes` producing the `oversize` reason;
- excluded directory;
- excluded exact filename;
- exact-name inclusion for an extensionless file such as `Makefile`, including its `names` report row;
- `documentation_names` classification for an included exact basename;
- exact-name classification winning over extension classification;
- overlapping inclusion and exclusion rules with exclusion winning;
- `*`, `?`, `**`, root-relative matching, and trailing-`/` directory-subtree semantics;
- `use_default_exclusions=true` and `false`;
- paths containing spaces;
- UTF-8 filename;
- default symlink skipping and its `symlink` count;
- a symlink cycle with `follow_symlinks=true` when supported by the filesystem;
- extension/name detail sums matching authored totals, plus all five `skipped_by_reason` keys and `skipped_files` sum consistency;
- unreadable included input producing nonzero execution status when the test environment can create one reliably;
- JSON config;
- YAML config with equivalent meaning;
- malformed JSON;
- malformed YAML;
- deterministic text and JSON output independent of traversal/creation order.

## 4. `cfgcheck`

### 4.1 Purpose

`cfgcheck` validates Project B JSON/YAML configuration without launching the GUI.

This tool must use the same parser and schema-validation implementation used by Project B, not a separate permissive parser.

### 4.2 Required invocation

```text
cfgcheck FILE
cfgcheck FILE --dump-normalized
```

`FILE` must end in `.json`, `.yaml`, or `.yml`; the extension selects the required parser and unsupported extensions fail.

### 4.3 Validation responsibilities

The tool must validate:

- syntax;
- UTF-8 where required;
- duplicate-key policy;
- required values;
- type correctness;
- ranges;
- unknown-key policy;
- all schema relationships and normalization rules documented in `08_config_json_yaml.md`.

### 4.4 Normalized dump

`--dump-normalized` must write canonical JSON representing the fully resolved configuration after defaults are applied.

Equivalent YAML and JSON configurations must produce byte-for-byte identical normalized JSON, except for a final newline policy that must itself be consistent.

### 4.5 Diagnostics

Parse errors must include at least:

- filename;
- 1-based line number;
- 1-based column number;
- concise reason.

Schema errors must identify the logical configuration path, for example:

```text
animation.modal_open_ms: expected integer in range 80..2000
```

### 4.6 Required tests

Tests must include equivalent JSON/YAML pairs, syntax failures, range failures, duplicate keys, unsupported YAML features, unknown properties, and UTF-8 string preservation.

## 5. `stateprobe`

### 5.1 Purpose

`stateprobe` verifies deterministic state fixtures used by Project B tests.

It is not a general debugger and is not required to inspect live process memory.

### 5.2 Fixture format

A state fixture is a JSON file using the exact versioned schema in `docs/14_stateprobe_fixture_schema.md`. The schema contains canonical simulated time, playback state/rate, selected digital field, the presentation settings needed for derivation, undo/redo entries, and expected derived analog/digital values.

### 5.3 Required operations

At minimum:

```text
stateprobe validate FILE
stateprobe normalize FILE
```

### 5.4 Validation

Validation must catch:

- times outside the canonical range;
- NaN/Infinity where numbers must be finite;
- invalid playback-rate range;
- malformed undo records;
- inconsistent before/after history states;
- derived hand angles inconsistent with canonical time beyond documented tolerance;
- invalid digital field selection.

### 5.5 Normalization

Normalization emits canonical JSON in deterministic key order suitable for fixture comparison.

## 6. Shared parser requirement

Project A must not solve JSON/YAML by invoking another executable or shell command.

The parsers must be linked C code in the submitted source tree. JSON syntax, the bounded YAML subset (including only the special empty flow literals `[]`/`{}`), UTF-8 rules, duplicate-key rules, and parser resource limits from `08_config_json_yaml.md` apply equally to Project A JSON/YAML inputs; each tool then applies its own schema.

The JSON/YAML lexer/parser may be shared between `locscan`, `cfgcheck`, `stateprobe`, and Project B.

## 7. Project A documentation

Project A must include a concise README with examples of every supported command-line form and exit-code semantics.

## 8. Project A completion condition

Project A is complete only when all three required utilities build, their mandatory tests pass, and their behavior matches this document.
