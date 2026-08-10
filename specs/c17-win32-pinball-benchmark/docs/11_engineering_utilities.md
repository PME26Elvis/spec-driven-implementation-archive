# 11 — Required Engineering Utilities

## 1. Purpose

The assignment includes small C engineering utilities as required deliverables. They make the project self-auditing and testable; they are not instructions for a particular development platform.

## 2. Required utility capabilities

1. `locscan` — categorized line counting;
2. `scenecheck` — standalone `.pbt` parser/validator;
3. `simcheck` — headless scenario/regression runner;
4. `replaycheck` — replay verification;
5. `detcompare` — deterministic checkpoint/trace first-divergence comparison capability;
6. `releasecheck` — release/evidence/schema consistency validation;
7. unified `tests` runner.

Capabilities may share binaries if command/mode remains clear.

## 3. locscan goal

`locscan` reports authored project scale while excluding generated/irrelevant files.

## 4. Required categories

At minimum:

- `source`;
- `headers`;
- `tests`;
- `docs`;
- `build_scripts`;
- `other_counted`.

Excluded files are counted separately by file count but contribute no line total.

## 5. Physical-line definition

One newline-delimited line counts as one physical line. A final unterminated non-empty line counts as one.

Report:

- physical lines;
- blank lines;
- nonblank lines.

Comment-aware semantic LOC is optional.

## 6. Binary exclusion

Binary files MUST NOT be interpreted as text lines. Use extension rules and/or NUL/content heuristic.

Images, archives, objects, executables, fonts, videos, and similar artifacts are excluded by default.

## 7. Default excluded classes

At minimum:

- `.git/`;
- build output;
- caches;
- logs;
- generated results;
- visual evidence images/videos;
- compiled binaries/objects;
- temporary files;
- coverage/profiling outputs.

Authored text fixtures and schemas remain countable where category rules include them.

## 8. locscan configuration formats

Support both JSON and YAML configuration with the same logical schema. Recognize explicit config paths such as `.locscan.json` and `.locscan.yaml`.

Parsers are implemented by the assignee in C.

## 9. locscan schema

Required logical fields:

- `include_extensions`: list of strings;
- `exclude_extensions`: list of strings;
- `exclude_paths`: list of glob-like path patterns;
- `category_rules`: ordered rules mapping pattern/extension to category;
- `follow_symlinks`: boolean;
- `max_file_bytes`: integer.

## 10. Required YAML subset

Only the subset needed for config is required:

- mappings with string keys;
- scalar string/int/bool;
- block lists using `-`;
- list items that are simple mappings, as used by `category_rules`;
- space indentation;
- `#` comments outside quoted strings.

Anchors, tags, flow collections, multi-document YAML, complex scalar features are not required. Unsupported features produce clear parse Error.

## 11. Required JSON support

Standard JSON features needed for schema:

- object;
- array;
- string escapes;
- integer numbers;
- booleans;
- null where harmless.

Malformed JSON fails clearly.

## 12. locscan human output

Example shape:

```text
Category         Files    Physical    Blank    Nonblank
source              31        8420      913        7507
headers             19        2710      301        2409
tests               28        6250      702        5548
docs                14        3180      412        2768
build_scripts        3         190       21         169
TOTAL               95       20750     2349       18401
Excluded files: 47
```

Exact spacing may differ.

## 13. locscan JSON output

Machine-readable option includes:

- root path;
- config identifier/path;
- totals;
- category totals;
- per-file entries or counted-file list;
- excluded-file count;
- errors.

## 14. Link/reparse-point policy

On Windows, the `follow_symlinks` config key covers directory symbolic links and directory reparse-point redirections such as junctions. Default: do not traverse directory reparse points that redirect traversal. If enabled, cycle detection using stable canonical/file identity is required.

The JSON/YAML key remains `follow_symlinks` for cross-variant config compatibility.

## 15. locscan tests

At minimum:

- empty file;
- final unterminated line;
- CRLF;
- binary/NUL file;
- ignore pattern;
- category precedence;
- JSON config;
- YAML config;
- malformed JSON;
- malformed YAML;
- symlink/junction/reparse-point policy;
- spaces/UTF-8 bytes in paths;
- oversized-file policy.

## 16. scenecheck

Standalone scene validator:

- parses `.pbt`;
- prints syntax errors;
- runs semantic validation;
- prints deterministic Error/Warning order;
- returns 0 only when no Error exists;
- optionally writes JSON report;
- shares production parser/validator modules with main app.

## 17. simcheck

Headless regression utility:

- loads scene;
- optionally loads replay;
- executes fixed steps;
- emits checkpoints;
- compares expected values with declared tolerance;
- returns non-zero on mismatch;
- shares production physics/event modules.

## 18. replaycheck

Implements replay verification defined in replay spec. It may be a `simcheck --replay` mode if functionally complete and discoverable.

## 19. Unified tests runner

Runs required automated suite, human summary, JSON summary. SHOULD support category/filter execution so failures are isolatable.

Useful optional arguments:

- `--list`;
- `--category physics`;
- `--filter ccd_*`;
- `--repeat N`;
- explicit seed for any additional deterministic randomized tests.

## 20. Self-hosted locscan report

Final implementation delivery SHALL include a locscan report generated by the delivered locscan binary over the delivered project tree.

That generated report itself is excluded from counted totals through project config.

## 21. Utility error behavior

All required utilities:

- provide help;
- reject unknown options;
- use non-zero failure exit;
- do not create GUI windows and do not require an interactive desktop;
- do not silently skip unreadable required input;
- do not build unsafe shell commands from user paths.

## 22. detcompare

`detcompare` capability SHALL compare canonical simulation/checkpoint/trace outputs according to document 22 and return non-zero on first semantic divergence. It may be a `simcheck` mode if clearly discoverable.

## 23. releasecheck

`releasecheck` SHALL validate `RELEASE_RESULT.json`, `RELEASE_EVIDENCE.json`, stable requirement coverage, evidence/test/visual references, Gate aggregation, and release version consistency according to document 27.

## 24. trace/statistics outputs

Utilities used for headless trace export must share production trace event generation with the application. Trace file comparison is part of deterministic verification, not a separately reconstructed log.

## 25. locscan document accounting

The delivered locscan self-report SHALL keep human-authored documentation separately identifiable from fixtures, schemas, generated reports, logs, and evidence binaries. Binary visual evidence is excluded from line totals.


## 26. Windows utility binaries and path semantics

Windows delivery may expose utilities as separate `.exe` files or subcommands. Documentation/report capability names remain `locscan`, `scenecheck`, `simcheck`, `replaycheck`, `detcompare`, `releasecheck`, and `tests` independent of filename layout.

Utility path handling SHALL accept Unicode Windows paths without ACP loss. `locscan` normalizes `\` and `/` to a canonical `/` separator for configuration-pattern matching; drive-letter spelling shall not create duplicate traversal roots. Pattern matching is ASCII case-insensitive on the Windows variant, while non-ASCII path bytes/scalars are preserved.
