# 08 — Mandatory Development Utility Workstream

## 1. Status

This workstream is mandatory.

It is deliberately separate from the end-user Markdown editor while remaining part of the same assignment package.

The repository inventory and line-count tool is the first of three mandatory Workstream A utilities. `fixturegen` and `evidencecheck` are additionally mandatory and are specified in `15_DEV_FIXTURES_AND_EVIDENCE.md`.

## 2. Implementation Constraint

The utility MUST be written in C17 under the same no-third-party-library principle unless a later requirement explicitly creates an exception.

It MUST not shell out to an existing LOC counter as its implementation.

## 3. Purpose

The utility provides deterministic source/document inventory metrics for the delivered project.

It is intended to distinguish authored source/documentation from generated outputs and disposable artifacts.

## 4. Inputs

The tool MUST accept:

- Repository/root directory.
- Configuration file.

The configuration MUST be accepted in both JSON and YAML forms representing the same logical options.

The submission MUST implement the parsing necessary for the documented configuration subset rather than requiring a third-party JSON/YAML parser.

## 5. Configuration Model

The configuration MUST support at least:

- Included file extensions.
- Authored source extensions.
- Human-readable documentation extensions.
- Excluded directory patterns.
- Excluded file/path patterns.
- Generated-output patterns.
- Optional explicit include overrides.

The v1.0 logical schema keys are: `include_extensions`, `source_extensions`, `test_extensions`, `documentation_extensions`, `config_build_extensions`, `exclude_dirs`, `exclude_paths`, `generated_paths`, `include_overrides`, and `follow_directory_reparse_points`. JSON and YAML forms MUST map these names to string arrays except `follow_directory_reparse_points`, which is boolean and defaults to `false`.

## 6. Ignore/Pattern Semantics

The tool MUST implement deterministic path matching.

At minimum the final pattern syntax must support:

- Exact path component.
- Extension matching.
- Directory subtree exclusion.
- Simple wildcard matching.

Path matching MUST be deterministic and, by default, ASCII case-insensitive on Windows to match the normative case-insensitive NTFS acceptance volume. The tool MUST normalize manifest/report paths to `/` separators. This pattern behavior is tool-defined and MUST NOT depend on whichever case behavior happens to be enabled on a particular directory.

## 7. Required Classification

Every counted regular file MUST be assigned to one of at least:

- Source code.
- Test code.
- Human-readable documentation.
- Configuration/build authored files.
- Other included authored text.

Excluded/generated files MUST not contribute to authored totals.

## 8. Default Exclusion Intent

The default/sample configuration MUST demonstrate exclusion of typical disposable content such as:

- Build directories.
- Object files.
- Executables/binaries.
- Cache directories.
- Logs.
- Test-result output.
- Coverage output.
- Screenshot/evidence binary images.
- Temporary files.
- Generated reports that are not authored documentation.

The exact path names are project-specific and will be represented in configuration rather than hard-coded as the only supported names.

## 9. Line Counting

For included human-readable text files, the utility MUST report physical line counts.

A non-empty file whose final line has no terminating newline MUST still count that final line as one physical line. An empty file counts as zero lines.

Binary files MUST not be interpreted as text line sources merely because their names match an extension accidentally.

## 10. Documentation Total

The utility MUST report a dedicated total for human-readable documentation lines.

This total exists specifically so task-pack/project scale can be inspected without accidentally counting screenshots, binaries, logs, or generated results as “documentation.”

## 11. Output

The tool MUST produce:

- Human-readable summary.
- Per-category totals.
- Grand authored-text total.
- Human-readable documentation total.
- Per-file detail option.
- Machine-readable JSON report.

## 12. Determinism

Given the same directory tree, file contents, and configuration, output counts MUST be deterministic.

File traversal order MUST not cause numeric differences.

Machine-readable output SHOULD use deterministic ordering.

## 13. Error Handling

The utility MUST report and return failure for:

- Missing root path.
- Unreadable configuration.
- Malformed supported JSON configuration.
- Malformed supported YAML configuration.
- Invalid configuration values.

Unreadable individual files MUST be surfaced in the report rather than silently treated as zero lines.

The utility MUST use the shared Workstream A exit-code contract in `15_DEV_FIXTURES_AND_EVIDENCE.md`: 0 success, 2 CLI usage, 3 config/schema parse, 4 input/read, 5 output/write, 6 verification mismatch, 7 internal consistency; additional codes >=8 MAY be documented.

## 14. Required Tests

Tests MUST cover:

- Empty directory.
- Traditional Chinese root path and filenames through Unicode Windows path APIs.
- Absolute local path longer than 260 characters.
- Directory reparse point is not recursively followed by default.
- Case-insensitive matching of configured Windows path patterns.
- One source file.
- One documentation file.
- Mixed categories.
- Nested excluded directory.
- Wildcard exclusion.
- Explicit include override of an otherwise excluded matching path.
- Final line with newline.
- Final line without newline.
- Empty text file.
- Binary-looking input.
- JSON configuration.
- YAML configuration.
- Equivalent JSON/YAML configurations produce equivalent counts.
- Malformed JSON.
- Malformed YAML.
- Unreadable file behavior.

## 15. Self-Use Requirement

The final project delivery MUST run this utility against the delivered repository using the project-provided configuration and include its resulting authored-source/documentation metrics in the final acceptance evidence.

The generated report itself MUST not recursively inflate the authored documentation total unless the configuration intentionally classifies it as authored documentation.
## 16. Windows Path/Unicode Requirement

`locscan` MUST accept a root directory and configuration path containing Traditional Chinese characters without depending on the active Windows ANSI or console code page.

Recursive enumeration MUST use Unicode-capable Windows path semantics and MUST support the mandatory >260-character path fixture.

Directory reparse points MUST not be followed by default.

Machine-readable output paths MUST use `/` separators and UTF-8 JSON so reports are deterministic and portable.

