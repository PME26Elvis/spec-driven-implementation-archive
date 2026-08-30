# 02 — CLI and Batch Script Language

## 1. Command-line interface

The required invocation is:

```text
tabletool --script <script-path> --report <report-path>
```

Both options are mandatory for a processing run.
Option order is arbitrary.
Each option must appear exactly once.

The following informational forms are also required:

```text
tabletool --help
tabletool --version
```

`--help` and `--version` must not require `--script` or `--report`.
Each informational form must appear alone as the sole option; `--help` and `--version` are mutually exclusive and may not be combined with processing options or positional arguments.
They must not execute a batch script.
A valid `--help` or `--version` invocation returns exit code 0 and does not create/open a report.

`--version` outputs exactly one line:

```text
tabletool 1.0.1
```

`--help` wording is flexible but must mention the four required options/forms: `--script`, `--report`, `--help`, and `--version`.

The script filename extension is not semantically significant.
The acceptance fixtures use `.tts` only as a readable convention.

Unknown options, missing option values, duplicate required options, or additional positional arguments are invocation errors.

## 2. Script encoding

A batch script is UTF-8 text.

The parser must:

- accept UTF-8 without a BOM;
- accept a single UTF-8 BOM only at the beginning of the file and ignore it;
- accept LF and CRLF line endings;
- reject invalid UTF-8;
- reject NUL bytes in the script;
- count physical lines starting from 1 for diagnostics.

## 3. Lexical rules

A script consists of statements and comments.
One statement normally occupies one physical line.

Leading and trailing ASCII spaces or tabs outside quoted strings are ignored.
Blank lines are ignored.

A comment begins with `#` outside a quoted string and continues to the physical line ending.
Comment bytes are discarded before line-continuation recognition.

Lexical processing order is normative:

1. recognize LF or CRLF physical line endings;
2. reject any bare CR that is not the CR byte of a CRLF physical line ending; use the quoted `\r` escape when a carriage return is needed as string data;
3. scan the physical line while honoring quoted strings and escapes;
4. discard a `#` comment that begins outside a quoted string;
5. after comment removal, trim trailing ASCII spaces/tabs for continuation detection;
6. if the final remaining byte is an unescaped backslash outside a quoted string, apply line continuation;
7. concatenate continued physical lines with one ASCII space replacing each continuation marker plus line ending;
8. tokenize the resulting logical statement.

A backslash that occurs only inside discarded comment text never continues a statement.
Under this processing order, `... \ # comment` **does** continue because the comment is discarded first and the backslash then becomes the final non-whitespace byte.
By contrast, `... # comment \` does not continue because that backslash is inside discarded comment text.

Example:

```text
LOAD "students.csv" FORMAT CSV HEADER YES   # source table
TYPE "score" INTEGER
WRITE "result.md" FORMAT MARKDOWN
```

## 4. Line continuation

A backslash (`\`) that remains as the final non-whitespace byte after comment removal, and is outside a quoted string, continues the statement on the following physical line.

The backslash and following line ending are replaced by one ASCII space before tokenization.

A diagnostic for a continued statement reports the first physical line of that statement.
A dangling continuation at end-of-file is a script syntax error.
A raw physical LF or CRLF is never permitted inside a quoted string; use `\n` or `\r` escapes instead.

## 5. Identifiers and keywords

Language keywords are ASCII and case-insensitive.
For example, `load`, `LOAD`, and `Load` are the same keyword.

Column names and string values are not keywords when quoted.
They are case-sensitive sequences after UTF-8 validation.

All column references in required commands are quoted strings, not bare identifiers.
This avoids locale and identifier-grammar ambiguity.

## 6. Quoted strings

A quoted string begins and ends with a double quote (`"`).

Supported escapes inside quoted strings are:

- `\\` — backslash;
- `\"` — double quote;
- `\n` — line feed U+000A;
- `\r` — carriage return U+000D;
- `\t` — tab U+0009;
- `\#` — literal `#`.

Because comments are recognized only outside quoted strings, an unescaped `#` inside quotes is also literal.

No `\xNN`, octal escape, or `\uNNNN` syntax is required.
Unknown escape sequences are syntax errors.

The decoded quoted string must be valid UTF-8 and must not contain NUL.

## 7. Numeric tokens

Row and column positions in script commands use unsigned decimal digit strings without a sign.
Positions are **1-based**.

`0` is never a valid row/column position.
Leading zeros are allowed and have no semantic effect.

A token made entirely of the required decimal digits is lexically valid even if its numeric magnitude exceeds an implementation integer type.
Range/domain validation must detect that condition without wraparound.
A syntactically valid numeric token whose value is outside the command's allowed domain is an operation/domain error (exit 5), not a script-syntax error.

The same distinction applies to `MODULE`, `HEIGHT`, and `GAP`: malformed digit grammar is syntax error 3; a valid decimal integer outside the specified parameter range is operation/domain error 5.

## 8. Value literals

A value literal is one of:

- unquoted `NULL` (keyword spelling is ASCII-case-insensitive);
- a quoted UTF-8 string;
- an unquoted INTEGER-form token;
- an unquoted DECIMAL-form token;
- unquoted `TRUE` or `FALSE` (ASCII-case-insensitive keyword recognition).

`NULL` is the **only** intrinsically typed/semantic value literal.
Every other value literal supplies text to the target column parser:

- a quoted literal supplies its decoded quoted-string text;
- an unquoted literal supplies its exact original token spelling, preserving sign, leading zeros, decimal zeros, and letter case.

The target column type then parses that text directly according to `03_table_model_and_types.md`.
There is no intermediate numeric or boolean coercion.

Consequences:

```text
SET CELL 1 "s" = TRUE       # STRING stores exactly "TRUE"
SET CELL 1 "s" = 001.2300   # STRING stores exactly "001.2300"
SET CELL 1 "i" = 1.0        # INTEGER conversion fails
SET CELL 1 "b" = 1          # BOOLEAN conversion succeeds and stores true
SET CELL 1 "d" = 001.2300   # DECIMAL stores canonical 1.23
SET CELL 1 "s" = NULL       # stores NULL, not the four letters
SET CELL 1 "s" = "NULL"     # stores the non-NULL STRING "NULL"
```

For lexical recognition of an unquoted value literal, INTEGER-form and DECIMAL-form refer to the textual grammars in `03_table_model_and_types.md`, but **range, precision, and scale are not checked by the lexer**.
Those checks belong to the eventual target type parser.
Therefore an arbitrarily large digit token may still be assigned to STRING and stored as text (subject to ordinary cell/resource limits), while assigning the same token to INTEGER or DECIMAL can fail target conversion.

Unquoted DATE, URL, EAN13, and CODE128 values are not required; quote them.

Examples of value literals:

```text
42
-17
3.1415
TRUE
NULL
"2026-08-30"
"https://example.com/a"
"471123456789"
```

## 9. Required statements

The required top-level statements are:

```text
LOAD
TYPE
ADD COLUMN
DROP COLUMN
RENAME COLUMN
MOVE COLUMN
SWAP COLUMNS
INSERT ROW
APPEND ROW
DELETE ROW
MOVE ROW
SWAP ROWS
SET CELL
SET NULL
SORT
FIND
WRITE
BARCODE-SHEET
```

A processing script must contain at least one executable statement, and its first executable statement must be `LOAD`.
An empty/comment-only script or a different first statement is a script-structure syntax error (exit 3), so no command executes.
A second `LOAD` is allowed later and replaces the active table only if the new load succeeds completely.

## 10. LOAD syntax

```text
LOAD <quoted-path> FORMAT CSV HEADER YES|NO [NULL-TOKEN <quoted-string>]
LOAD <quoted-path> FORMAT TSV HEADER YES|NO [NULL-TOKEN <quoted-string>]
LOAD <quoted-path> FORMAT MARKDOWN [NULL-TOKEN <quoted-string>]
```

Examples:

```text
LOAD "data.csv" FORMAT CSV HEADER YES
LOAD "raw.tsv" FORMAT TSV HEADER NO NULL-TOKEN "\\N"
LOAD "table.md" FORMAT MARKDOWN NULL-TOKEN "NULL"
```

If `HEADER NO` is used, generated names are `C1`, `C2`, ... in column order.
Markdown always contains a header row and therefore has no HEADER option.

All columns initially have type STRING after `LOAD`.

## 11. TYPE syntax

```text
TYPE <column-name> <type-name>
```

Required type names:

```text
STRING
INTEGER
DECIMAL
BOOLEAN
DATE
URL
EAN13
CODE128
```

Example:

```text
TYPE "points" INTEGER
TYPE "price" DECIMAL
TYPE "website" URL
TYPE "ean" EAN13
```

Conversion is atomic for the entire column.

## 12. Column mutation syntax

```text
ADD COLUMN <name> <type-name> [DEFAULT <value>] [AT <column-position>]
DROP COLUMN <name>
RENAME COLUMN <old-name> TO <new-name>
MOVE COLUMN <name> TO <column-position>
SWAP COLUMNS <name-a> <name-b>
```

Examples:

```text
ADD COLUMN "priority" INTEGER DEFAULT 1 AT 2
DROP COLUMN "unused"
RENAME COLUMN "web" TO "website"
MOVE COLUMN "name" TO 1
SWAP COLUMNS "first" "last"
```

## 13. Row mutation syntax

```text
INSERT ROW AT <row-position> VALUES ( <value> { , <value> } )
APPEND ROW VALUES ( <value> { , <value> } )
DELETE ROW <row-position>
MOVE ROW <row-position> TO <row-position>
SWAP ROWS <row-position-a> <row-position-b>
```

The number of values for INSERT/APPEND must equal the active column count exactly.
Values are converted according to current column types.
The whole row insertion is atomic.

## 14. Cell edit syntax

```text
SET CELL <row-position> <column-name> = <value>
SET NULL <row-position> <column-name>
```

`SET CELL ... = NULL` is also accepted and is equivalent to `SET NULL`.

## 15. SORT syntax

```text
SORT BY <column-name> ASC|DESC [ , <column-name> ASC|DESC ... ]
```

At least one key is required.
The same column must not appear twice in one SORT statement.

Example:

```text
SORT BY "points" DESC, "name" ASC
```

Sorting is type-aware and stable as defined in the table semantics specification.

## 16. FIND syntax

```text
FIND <quoted-query> IN ( <column-name> { , <column-name> } )
     MODE SENSITIVE|ASCII-INSENSITIVE
     WRITE <quoted-path> FORMAT CSV|TSV|MARKDOWN
     [NULL-TOKEN <quoted-string>]
```

The components above form one logical statement and may use the defined line-continuation syntax.

Example shown as one logical line:

```text
FIND "中文" IN ("title", "notes") MODE SENSITIVE WRITE "matches.md" FORMAT MARKDOWN
```

`FIND` writes matching rows and does not mutate the active table.
The output preserves the same columns and current row order.
The IN list must contain at least one column and must not name the same column more than once; a duplicate is an operation/domain error (exit 5).

## 17. WRITE syntax

```text
WRITE <quoted-path> FORMAT CSV HEADER YES|NO [NULL-TOKEN <quoted-string>]
WRITE <quoted-path> FORMAT TSV HEADER YES|NO [NULL-TOKEN <quoted-string>]
WRITE <quoted-path> FORMAT MARKDOWN [NULL-TOKEN <quoted-string>]
```

`WRITE` serializes the entire active table.

## 18. BARCODE-SHEET syntax

```text
BARCODE-SHEET <column-name> WRITE <quoted-path>
    MODULE <positive-integer>
    HEIGHT <positive-integer>
    GAP <nonnegative-integer>
    TEXT YES|NO
```

The components above form one logical statement.
The statement is valid only for an EAN13 or CODE128 column.

Example:

```text
BARCODE-SHEET "ean" WRITE "ean.svg" MODULE 2 HEIGHT 80 GAP 24 TEXT YES
```

`MODULE`, `HEIGHT`, and `GAP` are integer SVG user units.

## 19. Statement termination

A logical line contains exactly one statement.
A semicolon is not a statement separator and is not required.
Unexpected trailing tokens are syntax errors.

## 20. Parse-before-execute rule

The implementation must validate the complete batch script before executing the first product statement.

The syntax/structure phase must detect at least:

- invalid UTF-8;
- invalid escapes;
- raw physical newline inside a quoted string;
- unknown commands;
- malformed grammar;
- malformed numeric-token grammar;
- unknown type names;
- missing required syntax;
- duplicated syntactic options within a statement;
- an empty/comment-only script;
- a first executable statement other than LOAD.

A failure in this phase is exit code 3, executes zero product commands, creates zero script-requested outputs, and leaves the requested report path untouched as defined in the scope/error specifications.

After syntax succeeds, the mandatory preflight performs only the whole-script exact path-collision safety checks defined in `01_scope_and_constraints.md`.
Numeric command domains and all other statement-level domain/data checks are **not** hoisted into whole-script preflight; they are evaluated when their statement is reached in source execution order.

Errors such as an out-of-domain numeric parameter, unknown column, invalid row position for the current row count, or invalid DATE cell conversion are therefore detected during execution of that statement and use their normal defined error category.

This rule prevents half-execution caused by a purely syntactic error near end-of-file.
