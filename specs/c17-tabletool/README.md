# C17 TableTool — Specification-Driven Implementation Task Pack v1.0.1

## Purpose

This task pack defines a complete software implementation assignment for a standalone typed table-processing tool written in C17.

It defines product behavior, data contracts, algorithms, error handling, tests, deliverables, and Definition of Done.
Build orchestration, development workflow, and execution infrastructure are outside the product specification.

## Product in one sentence

Build a pure CLI program named `tabletool` that reads structured table files, applies a custom batch command language, performs typed row/column operations and deterministic searches/sorts, validates URL and barcode semantic types, and writes table or barcode outputs to files.

## Mandatory engineering profile

- Language: ISO C17.
- Runtime implementation: C standard library only.
- No third-party libraries.
- No POSIX-only, Windows-only, or other platform-specific APIs in product code.
- No external helper executables for parsing, sorting, searching, URL handling, barcode generation, or file conversion.
- No shell/Python/Perl/JavaScript implementation hidden behind a C launcher.
- Core behavior must be implemented in the submitted C source.
- Normal processing results are written to files, not dumped to the terminal.
- UTF-8 handling required by this specification must be implemented by the program and must not depend on the host locale.

## Mandatory feature groups

1. CSV, TSV, and restricted Markdown table import/export.
2. In-memory typed table model with explicit NULL values.
3. Types: STRING, INTEGER, DECIMAL, BOOLEAN, DATE, URL, EAN13, CODE128.
4. Row and column insertion, deletion, movement, swapping, renaming, and cell editing.
5. Stable deterministic multi-key sorting.
6. UTF-8-safe substring search, including Chinese text.
7. A hand-written batch command language parser.
8. HTTP/HTTPS URL parser, validation, and deterministic normalization.
9. EAN-13 validation/check-digit generation and barcode encoding.
10. Code 128 B/C optimal encoding, checksum, module pattern generation, and SVG barcode-sheet output.
11. File-based run report and defined exit codes.
12. Unit and integration tests supplied with the implementation.

## Specification map

- `specs/01_scope_and_constraints.md` — product boundary, C17 restrictions, prohibited substitutes.
- `specs/02_cli_and_script_language.md` — executable interface and batch-language grammar.
- `specs/03_table_model_and_types.md` — table state, NULL semantics, type definitions and conversion.
- `specs/04_file_formats.md` — CSV, TSV, Markdown and UTF-8 contracts.
- `specs/05_operations_and_semantics.md` — commands and exact behavior.
- `specs/06_url_and_barcode.md` — URL normalization, EAN-13, Code 128, SVG output.
- `specs/07_errors_and_edge_cases.md` — diagnostics, failure semantics, resource and malformed-data cases.
- `specs/08_testing_acceptance_and_dod.md` — required tests, human checklist, deliverables, Definition of Done.

## Acceptance fixtures

The `acceptance/` directory contains small fixed fixtures and expected results for representative behavior, including mutation, NULL, and parse-safety cases.
They are intentionally compact rather than exhaustive.
An implementation must satisfy both the written specification and the fixtures; the written specification takes precedence if a fixture is accidentally inconsistent.

## Scope discipline

This is deliberately not a spreadsheet clone and not a database engine.
There is no GUI, formula language, SQL, networking, package manager integration, plugin system, charting, or interactive REPL.
The engineering difficulty is intentionally concentrated in deterministic parsing, typed data handling, table mutation, search/sort algorithms, semantic validation, barcode algorithms, and robust C memory/error handling.
