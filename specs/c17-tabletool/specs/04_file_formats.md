# 04 — File Formats and Encoding Contracts

## 1. Common text rules

CSV, TSV, and Markdown inputs are UTF-8 text stored/read as binary bytes by TableTool.

For all three formats:

- UTF-8 without BOM is accepted;
- one UTF-8 BOM at the beginning is accepted and ignored;
- any BOM elsewhere is data and, if invalid in context, causes an error;
- invalid UTF-8 is rejected;
- NUL bytes are rejected;
- LF and CRLF record endings are accepted where the format permits record endings;
- output produced by TableTool uses literal LF (`0x0A`) line endings.

Input decoding must not depend on the host locale.
Host C text-mode newline translation must not be used to implement these rules.

## 2. Uniform row width

After headers are resolved, every data record must contain exactly the table's column count.

A row with too few or too many fields is a load error.

The parser must report at least:

- input path;
- physical line where the problematic record begins;
- expected field count;
- actual field count when determinable.

No ragged-row padding or truncation is allowed.

## 3. Headers

When CSV or TSV is loaded with `HEADER YES`, the first record contains column names.

When loaded with `HEADER NO`, every record is data and generated names are:

```text
C1
C2
C3
...
```

Markdown always uses its first table row as the header.

Imported header names must satisfy the table column-name rules:

- valid UTF-8;
- non-empty;
- unique;
- case-sensitive.

Duplicate or empty names are load errors.

## 4. NULL-TOKEN semantics

NULL has no universal representation in CSV, TSV, or Markdown.
TableTool therefore uses an explicit per-command NULL token.

### Import

If `NULL-TOKEN "X"` is supplied to LOAD:

1. decode the field according to the file format first;
2. compare the fully decoded field exactly to the decoded script token `X`;
3. if equal, the cell becomes NULL;
4. otherwise it becomes a non-NULL STRING, including the empty string.

If NULL-TOKEN is omitted, no imported field becomes NULL.
The NULL token is not applied to the header row.

For TSV this ordering matters because TSV has its own backslash escapes.
For example, script token `NULL-TOKEN "\\N"` denotes the two semantic characters backslash + `N`.
A TSV field representing that same text must contain bytes `\\N` (two backslashes followed by `N`) so TSV decoding produces `\N`.
A raw TSV field `\N` is invalid because `\N` is not a supported TSV escape.

### Export

If the active/output rows contain at least one NULL cell:

- WRITE or FIND must include NULL-TOKEN;
- otherwise the output statement fails before opening the destination.

When NULL-TOKEN is supplied:

- NULL is serialized from that semantic token value; the token then passes through the ordinary field writer/escaping rules of the selected output format exactly like other semantic field text (for example CSV quoting or TSV backslash escaping);
- a non-NULL value whose canonical text exactly equals the decoded semantic NULL token makes the output ambiguous and therefore causes the output statement to fail before output-file opening.

If there are no NULL cells, NULL-TOKEN is optional.

### Markdown-specific NULL-token restriction

Restricted Markdown removes formatting spaces around cells.
Therefore a NULL-TOKEN used with `LOAD ... FORMAT MARKDOWN`, `WRITE ... FORMAT MARKDOWN`, or `FIND ... FORMAT MARKDOWN` must not begin or end with ASCII space U+0020.
A violating command is an input-format error for LOAD or an operation/domain error for WRITE/FIND and must fail before opening an output destination.

This rule preserves NULL versus empty-string semantics without inventing a hidden convention.

## 5. CSV input

CSV uses comma as delimiter.

The required parser follows these explicit rules.

### 5.1 Unquoted field

An unquoted field:

- continues until comma, LF, CRLF, or end-of-file;
- may be empty;
- must not contain a double quote;
- preserves all bytes including leading/trailing spaces;
- preserves a bare CR that is not immediately followed by LF as ordinary field data.

### 5.2 Quoted field

If the first byte of a field is `"`, the field is quoted.

Inside a quoted field:

- comma is data;
- LF is data and is preserved as LF;
- CR is data and is preserved as CR, including when immediately followed by LF; therefore CRLF inside quotes decodes to the two data bytes CR + LF rather than a record ending;
- a literal `"` is represented by `""`;
- all other valid UTF-8 bytes are data.

After the closing quote, the next byte must be:

- comma;
- LF;
- CRLF;
- end-of-file.

Whitespace after a closing quote is not silently ignored.

### 5.3 Record endings

LF and CRLF are accepted outside quoted fields.

A final record may end at EOF without a trailing line ending.
A terminal LF/CRLF terminates the preceding record and does **not** create an additional empty record after it.
Thus `a,b\n` contains one CSV record, while a file containing only `\n` contains one record with one empty field.

A bare CR is never a CSV record separator; outside quotes it is field data as defined above.

### 5.4 Examples

Input:

```csv
name,notes
Alice,"hello, world"
Bob,"line one
line two"
Carol,"quote ""inside"""
```

In the example source above the visible embedded line ending is LF, so the decoded second-row `notes` contains one LF between `line one` and `line two`. If the bytes inside the quotes were CRLF, the decoded cell would contain CR followed by LF.

### 5.5 CSV errors

Examples of errors:

```text
abc"def
"unterminated
"a" trailing
"a","b"oops
```

## 6. CSV output

The writer emits the minimum required quoting.

A non-NULL field is quoted if its canonical text contains any of:

- comma;
- double quote;
- LF;
- CR.

When quoted:

- enclose the field in `"`;
- double every literal `"`;
- preserve LF;
- preserve CR as data.

Header fields follow the same quoting rules.

Every output record ends with LF, including the final record.

## 7. TSV input

TSV uses one physical record per line.
Raw tab separates fields.

Unlike CSV, raw LF/CR may not appear inside one field.
Special characters are represented by escapes.

### 7.1 TSV escapes

Within a field:

- `\\` decodes to backslash;
- `\t` decodes to tab;
- `\n` decodes to LF;
- `\r` decodes to CR.

Any other backslash escape is an input error.

A backslash as the final byte of a record is an input error.

### 7.2 TSV separator behavior

An unescaped raw tab separates fields.
Two adjacent tabs represent one empty field between them.
A leading tab creates an empty first field.
A trailing tab creates an empty final field.

### 7.3 TSV output

The writer escapes, in this order of conceptual intent:

- backslash as `\\`;
- tab as `\t`;
- LF as `\n`;
- CR as `\r`.

All other UTF-8 is written directly.

Records use LF.
A terminal LF/CRLF terminates the preceding TSV record and does **not** create an additional empty record after it.
Thus `a\tb\n` contains one TSV record, while a file containing only `\n` contains one record with one empty field.

## 8. Restricted Markdown table dialect

TableTool supports a deliberately restricted pipe-table format.
It does not parse general Markdown inline syntax.

For this restricted Markdown format, a **blank physical line** is a line whose bytes, after removing its LF/CRLF record ending, consist only of zero or more ASCII spaces and/or ASCII tabs.

A Markdown input file may contain:

- leading blank lines;
- exactly one table;
- trailing blank lines.

Any other non-blank content before or after the table is rejected.

## 9. Markdown table structure

A valid table contains:

1. one header row;
2. one separator row;
3. zero or more data rows.

Example:

```markdown
| name | points | note |
| --- | ---: | :--- |
| Alice | 5 | 中文 |
| Bob | 3 | hello |
```

Outer leading/trailing pipes are optional on input.
TableTool output always includes them.

Alignment markers in the separator are accepted but ignored semantically.

Each separator cell must consist of:

- optional leading colon;
- at least three hyphens;
- optional trailing colon.

ASCII spaces around separator-cell content are ignored.

### 9.1 Normative Markdown row tokenization

For each non-blank table physical line, after removing its LF/CRLF record ending:

1. locate the first and last non-ASCII-space byte; ASCII tab is **not** framing whitespace;
2. if the first non-space byte is an unescaped `|`, remove that one pipe and all ASCII spaces before it as the optional leading framing pipe;
3. after step 2, if the last non-space byte of the remaining row is an unescaped `|`, remove that one pipe and all ASCII spaces after it as the optional trailing framing pipe;
4. split the remaining byte sequence at every unescaped raw `|`;
5. splitting an empty remaining sequence yields exactly one empty raw cell;
6. for each raw cell, remove the maximal run of ASCII space U+0020 at its beginning and end as formatting padding;
7. decode the supported cell escapes listed below.

A pipe is **unescaped** when it is preceded by an even number (including zero) of consecutive backslashes in the raw row.
Backslash escape validity is checked during step 7.

Consequences:

```text
||        -> one empty cell (both pipes are framing)
| |       -> one empty cell
|a|       -> one cell: "a"
a|        -> one cell: "a" (final pipe is framing)
|a        -> one cell: "a" (initial pipe is framing)
a||       -> two cells: "a", "" (last pipe is framing)
||a       -> two cells: "", "a" (first pipe is framing)
```

Thus empty edge cells remain representable without ambiguity.
Header validity rules still reject an empty header name.

## 10. Markdown cell parsing

A raw unescaped `|` separates cells under the tokenization algorithm above.

Supported escapes inside a Markdown table cell are:

- `\|` => literal pipe;
- `\\` => literal backslash;
- `\n` => LF;
- `\r` => CR;
- `\t` => tab.

Any other backslash escape is rejected.
A raw CR byte that is not the CR of a CRLF record ending is rejected in Markdown input; semantic carriage return inside a cell must use the `\r` escape.

ASCII spaces immediately surrounding each raw cell are formatting padding and are removed before escape decoding.
There is no escape for a leading or trailing semantic ASCII space in this restricted Markdown dialect.
CSV or TSV must be used when such boundary spaces matter.

No Markdown interpretation is performed for:

- `*emphasis*`;
- backticks;
- links;
- HTML;
- entities.

They remain literal text.

## 11. Markdown row width

Header and every data row must have the same number of cells.

The separator row must have exactly that many separator cells.

An empty table is represented by a header/separator pair with zero data rows.
A zero-column Markdown table is not supported because tables must have at least one column.

## 12. Markdown output

Writer form:

```markdown
| <header1> | <header2> |
| --- | --- |
| <cell1> | <cell2> |
```

Before opening a Markdown destination, the writer must validate that semantic boundary spaces will not be lost.
It must reject the output if **any** emitted column name or non-NULL emitted cell canonical text begins or ends with ASCII space U+0020.
This applies to STRING and CODE128 values as well as any future type whose canonical text could have boundary spaces.

When a Markdown NULL-TOKEN is present, it is separately subject to the no-leading/trailing-space rule in the NULL-TOKEN section.

For header and data cells the writer escapes:

- backslash;
- pipe;
- LF;
- CR;
- tab.

It writes one ASCII space inside each side of the cell for readability.
Those writer-added padding spaces are not part of the semantic cell value.

Separator cells are exactly `---`.
No alignment inference is performed.

This deliberate rejection rule makes successful TableTool-generated Markdown semantically round-trippable under this restricted dialect instead of silently trimming user data.

## 13. Typed export

CSV, TSV, and Markdown serialize every non-NULL cell using the type's canonical text.

Consequences include:

- INTEGER `+0007` exports as `7` after INTEGER conversion;
- DECIMAL `001.2300` exports as `1.23`;
- BOOLEAN `1` exports as `true`;
- URL exports normalized;
- a 12-digit EAN input exports its generated canonical 13-digit value.

If a column remains STRING, its original decoded content is retained exactly except for format-specific escaping/padding rules.

## 14. Paths and file naming

Script path operands are decoded quoted strings and are then treated as opaque runtime path values passed to C standard file functions.
CLI paths are the byte/string values supplied by the host C runtime.
The product must not assume `/` or `\\` semantics beyond what the host implementation accepts.

Mandatory fixtures use portable ASCII path names.
Portable behavior for non-ASCII filesystem path encodings is out of scope; implementations must not corrupt memory or the report if such a path is encountered.

The program is not required to:

- create parent directories;
- expand `~`;
- expand environment variables;
- resolve glob patterns;
- canonicalize paths.

A missing parent directory is an I/O error.
Exact collision comparisons use the decoded runtime path values described in `01_scope_and_constraints.md`.

## 15. Destination overwrite

WRITE, FIND, and BARCODE-SHEET are allowed to replace an existing regular destination file if the host C library permits opening it for replacement.

Input-path equality is checked first as specified in the scope document.

All semantic validation that can be completed before opening the output must be completed first.
A low-level I/O failure during writing may leave a partial destination; crash-safe filesystem transactions are out of scope.

If a later successful output statement intentionally reuses a destination path, ordinary replacement semantics apply.
If that later write truncates the path and then suffers an I/O failure, content produced earlier at the same destination is **not** guaranteed to survive.
The "earlier outputs remain" rule applies to distinct destinations, not to a destination the script explicitly chose to overwrite again.

## 16. Empty input cases

CSV/TSV:

- a completely empty file is an error because it has no columns;
- with HEADER YES, a header record followed by EOF is a valid zero-row table;
- with HEADER NO, the first record establishes the column count and generated names.

Markdown:

- blank-only file is an error;
- header plus valid separator and no data rows is valid.

## 17. No format auto-detection

The FORMAT keyword controls parsing.
Filename extension does not.

For example:

```text
LOAD "data.txt" FORMAT CSV HEADER YES
```

is valid if the bytes are valid CSV.

The program must not guess a format from extension or contents.
