# 07 — Errors, Reports, and Edge Cases

## 1. Error philosophy

TableTool must fail clearly and deterministically.
It must not attempt to "repair" malformed inputs unless the specification explicitly defines normalization.

The purpose of the report is to make a failed run understandable without requiring terminal scraping.

Exact prose wording is not graded.
Required facts are.

## 2. Exit codes

Required process exit codes:

```text
0  success
2  command-line invocation error
3  script syntax/encoding error
4  input file, table-format, or typed-data error
5  operation/domain error
6  file I/O error
7  memory/resource error
8  internal consistency error
```

No mandatory failure may return 0.

If multiple conditions are present, report the first error encountered under the specified parse/execution order.

## 3. Report file

A processing run requires `--report <path>`.

When opened, the report is UTF-8 plain text written in binary mode with literal LF line endings.
A JSON report is not required.

The report is opened **only after** the script is syntactically valid and the report path has passed all exact collision checks.
Therefore:

- script syntax/encoding error (exit 3): requested report path is not opened or modified; one concise stderr diagnostic is required;
- unsafe report-path collision during preflight: requested report path is not opened or modified; one concise stderr diagnostic is required;
- report-open failure: no command executes and stderr is permitted as defined below.

Once safely open, the report must be understandable by a human and include the mandatory fields below.

### Report string escaping

Every dynamic string inserted into a report field or record—including script paths, input/output paths, column names, short value prefixes, and other user-controlled text—must use this escaping before being placed on a report line:

- `\\` => `\\\\`;
- LF => `\\n`;
- CR => `\\r`;
- tab => `\\t`;
- ASCII control bytes `0x01..0x08`, `0x0B`, `0x0C`, `0x0E..0x1F`, and `0x7F` => `\\xHH` with uppercase hexadecimal;
- ordinary valid UTF-8 text is retained as UTF-8;
- a host-path byte sequence that is not valid UTF-8 must be rendered bytewise with every offending byte as `\\xHH` so the report itself remains valid UTF-8.

NUL cannot occur in script-decoded strings and is not expected from ordinary `argv`; if an internal diagnostic buffer nevertheless contains NUL as data, render it as `\\x00` rather than embedding it.
No dynamic string may inject a literal LF/CR that creates an additional report record.

## 4. Successful report minimum content

A successful run must include lines equivalent to:

```text
STATUS: SUCCESS
EXIT_CODE: 0
SCRIPT: <escaped script path>
COMMANDS_PARSED: <count>
COMMANDS_EXECUTED: <count>
OUTPUTS_WRITTEN: <count>
```

Counter definitions are normative:

- `COMMANDS_PARSED` = number of complete product statements in the syntactically valid script;
- `COMMANDS_EXECUTED` = number of statements for which execution was entered, including a statement that then fails during precondition/domain/data/I/O/resource processing;
- whole-script path-collision preflight failures before command execution have `COMMANDS_EXECUTED: 0` if a report can safely be written;
- `OUTPUTS_WRITTEN` = number of WRITE, FIND, or BARCODE-SHEET statements that completed their output successfully, including checking standard-C write/error state plus `fflush`/`fclose` results as applicable;
- repeated successful writes to the same destination count separately.

For every successful WRITE, FIND, or BARCODE-SHEET, include an output line containing:

- source script line;
- operation kind;
- escaped destination path.

Example form:

```text
OUTPUT: line=8 kind=WRITE path=result.csv
```

Exact key order is flexible.

## 5. Failed report minimum content

A failed run **after the report is safely open** must include:

```text
STATUS: FAILED
EXIT_CODE: <defined nonzero code>
SCRIPT: <escaped script path>
COMMANDS_PARSED: <count>
COMMANDS_EXECUTED: <count>
OUTPUTS_WRITTEN: <count>
```

and one primary error record containing, when applicable:

- script physical line number;
- command name;
- error category;
- concise message;
- escaped input path and physical record line for file parsing errors;
- row position and escaped column name for typed-data errors.

The failing statement counts as executed if execution was entered.
A statement that fails during syntax parsing never executes and no report is opened under the syntax-error rule.

Example:

```text
ERROR: script_line=4 command=TYPE category=DATA row=17 column=deadline message=invalid DATE
```

Exact prose wording is flexible; required facts and escaping are not.

## 6. Sensitive/unbounded data in reports

The tool handles arbitrary text.
A report must not dump an entire megabyte-sized cell merely to explain a conversion error.

For a problematic value, it is sufficient to include:

- row;
- column;
- type expected;
- a short escaped prefix, or no value text at all.

The report must remain valid UTF-8.

## 7. Report-open failure

If the report path cannot be opened before any other failure has been established:

- no script is executed;
- return exit code 6;
- one concise diagnostic may be written to stderr.

If a path-collision preflight failure not involving the report path was already established first and opening the otherwise-safe report to record that failure then also fails, preserve the earlier operation/domain exit code 5 as the primary code and emit a concise stderr diagnostic that the report could not be created.
This follows the first-error rule.

The program is not required to produce another report elsewhere.

## 8. Report write/flush/close failure

If a report was opened successfully but TableTool later detects a write, flush, or close failure on that report:

- the process must return exit code 6 unless a prior nonzero failure code already describes an earlier product failure;
- if the run would otherwise have succeeded, report I/O failure changes the final process result to 6;
- because the report itself is unreliable, one concise stderr diagnostic is permitted;
- script-requested outputs that already completed remain subject to ordinary output sequencing semantics.

If both a product statement failure and a later report-write failure occur, preserve the earlier product failure as the primary exit code; stderr may additionally mention that the report could not be completed.

## 9. CLI errors before report resolution

Examples:

- missing `--report`;
- missing `--script`;
- duplicate option;
- unknown option;
- identical decoded/runtime `--script` and `--report` path values.

Return exit code 2.

No report is required for invocation errors.
One concise stderr diagnostic is permitted.
If an implementation elects to write a report for an invocation error, it must first establish that the report path is unambiguous and safe and must not violate any collision rule.

## 10. Script syntax or encoding error

Because the whole script is parsed before execution:

- no LOAD executes;
- no WRITE/FIND/BARCODE-SHEET output is created or modified;
- the requested report path is **not opened or modified**;
- one concise stderr diagnostic is required;
- exit code is 3.

The diagnostic points to the logical statement's first physical line when a line can be determined.
This includes structural script errors such as an empty/comment-only script or a first executable statement other than LOAD.

## 11. Input parsing errors

Examples:

- invalid UTF-8 in table file;
- malformed CSV quote;
- malformed TSV escape;
- malformed Markdown separator;
- wrong row width;
- duplicate header.

These produce exit code 4.

A failed LOAD does not partially replace the active table.

## 12. Typed-data errors

Examples:

- INTEGER overflow;
- DECIMAL precision over limit;
- invalid leap day;
- unsupported URL scheme;
- EAN check-digit mismatch;
- CODE128 non-ASCII payload.

These produce exit code 4 when encountered during TYPE conversion or direct typed value conversion in a data-bearing operation.

For TYPE failure, the report must identify the first failing current row and column.
For SET CELL it must identify the targeted row and column; for INSERT/APPEND it must identify the intended row position and first failing column; for ADD COLUMN DEFAULT the new column name is required and a row is not applicable.

## 13. Operation/domain errors

Examples:

- unknown column;
- row index out of range;
- column index out of range;
- a syntactically valid numeric position too large for the command domain;
- dropping the final column;
- duplicate SORT key;
- duplicate column in a FIND IN list;
- BARCODE-SHEET on STRING column;
- syntactically valid but out-of-range MODULE/HEIGHT/GAP;
- a prohibited whole-script preflight collision (report versus LOAD/output, or output versus script/LOAD);
- output NULL-token ambiguity;
- Markdown output boundary-space violation;
- Markdown NULL-token boundary-space violation on WRITE/FIND.

A Markdown NULL-token boundary-space violation on LOAD is an input-format/data error (exit 4), because it belongs to loading the requested format.

These produce exit code 5 unless the actual failure is file I/O or allocation.
The command-line case where `--script` and `--report` are exactly equal is handled earlier as invocation error 2 and is not reclassified by this section.

A numeric token consisting of invalid numeric grammar belongs to script syntax error 3 instead.
A protective implementation size ceiling reached despite otherwise valid input belongs to resource error 7 and must identify the relevant limit.

## 14. I/O errors

Examples:

- input file cannot be opened;
- read error;
- output cannot be opened;
- write/flush/close failure detectable by standard C I/O.

These produce exit code 6.

I/O failures are not converted into empty tables or successful empty outputs.

## 15. Resource errors

Allocation failure, size arithmetic overflow during allocation/output geometry planning, or an implementation protective size ceiling reached by otherwise valid input produces exit code 7.

When a documented protective ceiling causes the failure, the diagnostic must name the kind of limit (for example `cell-size limit`, `row-count limit`, or `SVG-size limit`) and must not misreport the input as syntactically malformed.

The current operation must not leave a partial in-memory mutation.

If the report itself cannot grow because of allocation failure, a minimal fixed diagnostic is acceptable.

## 16. Internal consistency error

Exit code 8 is reserved for a state that should be impossible if parsers and invariants are correct.

Examples:

- a typed EAN13 value with wrong canonical length is discovered internally;
- a stable-sort permutation refers to an invalid row index;
- Code128 dynamic programming reports no encoding for a payload already validated as printable ASCII.

This code is not a substitute for ordinary user/data errors.

## 17. Warnings

Warnings do not change success exit code.

At minimum, warn when:

- a later output statement uses a destination path string already used by an earlier output statement in the same run.

Warnings are written to the report.

The program need not warn about harmless no-ops such as moving a row to its current position.

## 18. EOF and truncated input

The implementation must distinguish legitimate EOF from malformed truncation according to the required format rules.

In particular:

- CSV unquoted final field ending at EOF: valid;
- CSV quoted field without closing quote at EOF: invalid;
- TSV final line without LF: valid;
- script quoted string without closing quote: invalid.

## 19. CRLF handling

CRLF must be treated as one line ending where specified.

A parser must not accidentally leave the CR byte of a **record-ending CRLF** in:

- CSV unquoted fields;
- TSV final field;
- script tokens;
- Markdown cells.

Inside a quoted CSV field, CR and LF are field data: a CRLF pair is preserved as CR + LF.
A bare CR in an unquoted CSV field is also data and is preserved.
Only LF and CRLF encountered outside quoted CSV fields are record endings.

## 20. Very long fields

Fields must be accumulated dynamically.

The implementation must not:

- use `fgets(buf, 1024, ...)` and treat each chunk as a complete record;
- truncate without error;
- split one CSV multiline record into independent records;
- overwrite a fixed stack array.

Protective maximums are allowed only as defined in the scope document or more generous.

## 21. Empty strings

Empty STRING is valid.

Examples:

CSV:

```csv
a,b
,x
```

first data cell is empty STRING if no NULL-TOKEN is in effect.

Script:

```text
SET CELL 1 "name" = ""
```

is an empty STRING for a STRING column.

It is not NULL.

## 22. NULL-token collision

Suppose:

```text
WRITE "out.csv" FORMAT CSV HEADER YES NULL-TOKEN "NULL"
```

and the table contains:

- one actual NULL;
- one non-NULL STRING whose canonical text is exactly `NULL`.

The output is ambiguous and must fail before opening `out.csv`.

Quoting the text as `"NULL"` does not solve the semantic ambiguity because LOAD compares decoded fields.

## 23. Duplicate headers

Duplicate names are exact code-point duplicates.

Examples:

```text
name,name
```

is invalid.

```text
Name,name
```

is valid because names are case-sensitive.

No automatic suffix such as `name_2` may be invented.

## 24. Invalid UTF-8

Invalid UTF-8 is always an error in scripts and supported text table formats.

The validator must reject at least:

- isolated continuation bytes;
- truncated multibyte sequences;
- overlong sequences;
- UTF-16 surrogate scalar values;
- scalar values above U+10FFFF.

The implementation does not have to reject Unicode noncharacters.

## 25. Search boundaries

A search query is a sequence of Unicode scalar values.
It may start/end at any scalar-value boundary inside cell text.

It must not produce false matches by starting in the middle of a UTF-8 multibyte sequence.

Because all text is validated, byte-based acceleration is allowed only if its results are equivalent to scalar-boundary matching.

## 26. Numeric overflow

All arithmetic that can overflow during:

- INTEGER parsing;
- DECIMAL digit/scale counts;
- row/column capacity growth;
- SVG width/height calculation;
- Code128 checksum accumulation;
- file-buffer sizing;

must be implemented so C signed overflow is not invoked.

Checksum accumulation may reduce modulo 103 as it proceeds.

## 27. SORT allocation failure

If SORT needs temporary memory and allocation fails:

- row order remains exactly as before SORT;
- return resource error;
- do not leave half-sorted rows.

An implementation may sort an index array first and commit the permutation only after success.

## 28. Invalid DATE

A DATE parser must reject impossible calendar values rather than normalizing them.

Examples rejected:

```text
2026-00-10
2026-13-01
2026-04-31
1900-02-29
0000-01-01
2026-1-01
```

## 29. Invalid URL

Do not repair:

- whitespace;
- missing scheme;
- unsupported scheme;
- userinfo;
- invalid port;
- malformed percent escapes;
- invalid DNS labels;
- unsupported IPv6.

Normalization occurs only after validity under the restricted grammar is established.

## 30. Invalid EAN13

A 13-digit value with wrong check digit fails conversion.

The program must not silently replace a supplied wrong check digit.

A 12-digit input intentionally asks TableTool to generate the check digit and is valid.

## 31. Invalid CODE128

Reject:

- empty non-NULL payload;
- bytes below ASCII 32;
- DEL 127;
- bytes above ASCII 126;
- payload over 256 bytes.

UTF-8 multibyte text is therefore not valid CODE128 payload in this assignment.

## 32. Cleanup on failure

Before process exit the implementation must close files and free reachable owned heap allocations under ordinary error paths.

The assignment does not require artificial recovery from operating-system process termination or hardware failure.

Memory leaks in short-lived error paths are still defects and should be covered by code review/tests where practical.

## 33. Error precedence

For a syntactically valid statement, required externally observable validation order is:

1. active-table preconditions;
2. referenced row/column existence and duplicate-reference checks;
3. value/domain validation;
4. destination semantic validation, including NULL-token and Markdown-losslessness rules;
5. allocation needed to prepare the operation;
6. destination file open;
7. file write/flush/close.

If an earlier step fails, later steps for that statement must not be attempted merely to discover another error.
In particular, a destination must not be opened/truncated before all applicable steps 1 through 5 succeed.

Whole-script preflight path-collision checks occur before any statement execution and therefore take precedence over statement-level errors that would only be discovered later.
