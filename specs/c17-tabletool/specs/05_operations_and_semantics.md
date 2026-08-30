# 05 — Operations and Behavioral Semantics

## 1. General execution model

After the whole script passes syntax parsing, statements execute in source order.

There is one active table.
Commands either:

- replace the active table (`LOAD`);
- mutate it (`TYPE`, row/column/cell operations, SORT);
- inspect it and write a result without mutating it (`FIND`);
- serialize it (`WRITE`);
- render a barcode document (`BARCODE-SHEET`).

A statement is atomic with respect to the in-memory table:
if that statement fails, it must not leave a partial table mutation.
File outputs are governed separately by the output/open rules and are not promised to be filesystem-transactional.

The entire script is **not** one global transaction.
Outputs successfully written by earlier statements remain if a later statement fails.

## 2. LOAD

LOAD parses the complete source into a temporary table first.

Only after all of the following succeed may it replace the current active table:

- file open/read;
- UTF-8 validation;
- format parsing;
- header validation;
- row-width validation;
- NULL-TOKEN interpretation;
- memory allocation.

If a second LOAD fails, the previously active table remains intact, but script execution stops because the statement failed.

## 3. TYPE

TYPE follows the atomic whole-column conversion described in the type specification.

Column lookup is exact and case-sensitive.

Unknown column => operation error.
Invalid data in any non-NULL cell => data/type error identifying the first failing row.

## 4. ADD COLUMN

Syntax:

```text
ADD COLUMN "name" <type-name> [DEFAULT <value>] [AT <column-position>]
```

Behavior:

- the name must be unique and valid;
- if AT is omitted, append after the current last column;
- valid AT range is `1 .. column_count + 1`;
- if DEFAULT is omitted, every existing row receives NULL;
- if DEFAULT is provided, convert it once according to the new type and copy the semantic value to every existing row;
- adding a column to a zero-row table still creates the schema entry.

A failed default conversion or allocation leaves the table unchanged.

## 5. DROP COLUMN

The named column is removed from every row and from the schema.

Dropping the only remaining column is prohibited.
The active table always has at least one column.

Unknown column is an operation error.

## 6. RENAME COLUMN

The old name must exist.
The new name must satisfy all name rules and must not collide with any other existing column.

Renaming to exactly the same name succeeds as a no-op.

## 7. MOVE COLUMN

`MOVE COLUMN "x" TO N` removes the named column from its current position and reinserts it so that its **final** 1-based position is N.

Valid destination range is `1 .. column_count`.

Examples for columns `[A,B,C,D]`:

```text
MOVE COLUMN "B" TO 4  => [A,C,D,B]
MOVE COLUMN "D" TO 2  => [A,D,B,C]
```

All row cells move with their column.

Moving a column to its existing position succeeds as a no-op.

## 8. SWAP COLUMNS

Both names must exist.
Their complete schema entries and every corresponding cell are exchanged.

Swapping a column with itself succeeds as a no-op.

## 9. INSERT ROW

Valid insertion position is:

```text
1 .. row_count + 1
```

Position `row_count + 1` is equivalent to append.

The provided value count must equal column count exactly.
Every value must convert to its corresponding current column type before insertion begins.

The new row becomes exactly the requested final position.

## 10. APPEND ROW

APPEND ROW is equivalent to inserting at `row_count + 1`.

It is provided as a convenience and remains mandatory.

## 11. DELETE ROW

Valid position is `1 .. row_count`.

Deleting from a zero-row table is an error.
Rows following the deleted row shift up by one.

## 12. MOVE ROW

`MOVE ROW A TO B` removes current row A and reinserts it so its **final** row position is B.

Both A and B must be within `1 .. row_count` before the move.

Example:

Initial row labels:

```text
[A,B,C,D,E]
```

Then:

```text
MOVE ROW 2 TO 5 => [A,C,D,E,B]
MOVE ROW 5 TO 2 => [A,E,B,C,D]   # if applied to the original list
```

Moving a row to its current position succeeds as a no-op.

## 13. SWAP ROWS

Both row positions must exist.
Entire rows are exchanged.

Swapping a row with itself succeeds as a no-op.

## 14. SET CELL and SET NULL

`SET CELL` validates and converts the new value before discarding the old value.

If conversion/allocation fails, the old cell remains unchanged.

`SET NULL` releases the prior non-NULL value if needed and marks the cell NULL.

Unknown column or invalid row position is an operation error.

## 15. Stable SORT

SORT must be stable.

For two rows whose values compare equal on **all listed sort keys**, their relative order before SORT must be preserved afterward.

An implementation may use mergesort, indexed stable sorting, or another algorithm.
A plain `qsort` call without an explicit stability mechanism does not meet the requirement.

## 16. Multi-key SORT

Keys are evaluated left to right.

Example:

```text
SORT BY "points" DESC, "name" ASC
```

means:

1. higher points before lower points;
2. where points are equal, lower name according to STRING order first;
3. where both keys are equal, preserve previous row order.

## 17. NULL sort behavior

NULL sorts **after every non-NULL value** regardless of ASC or DESC.

Thus:

```text
ASC  => values ascending, then NULL
DESC => values descending, then NULL
```

Two NULLs compare equal for that key.

This rule prevents missing values from unexpectedly moving to the top under DESC.

## 18. Type-specific sort order

- STRING: lexicographic Unicode scalar-value order.
- INTEGER: signed numeric order.
- DECIMAL: exact mathematical order.
- BOOLEAN: `false < true`.
- DATE: chronological order.
- URL: lexicographic order of canonical normalized URL text by Unicode/ASCII scalar value.
- EAN13: lexicographic order of canonical 13 digits; because lengths are equal this also matches unsigned decimal order.
- CODE128: lexicographic order of payload Unicode/ASCII scalar values.
- NULL: as defined above.

## 19. Sorting must not mutate cell values

SORT changes row order only.
It must not reparse, recanonicalize, or otherwise alter cells as a side effect.

## 20. FIND overview

FIND is the product's search operation.

It searches the current table and writes a table containing matching rows.
It never changes active-table row order or contents.

The FIND output schema and column order are identical to the active table.
Only rows are filtered.

The IN list must contain at least one existing column.
The same column name must not appear more than once in one FIND statement.
A duplicate IN-list column is an operation/domain error detected before the destination is opened.

## 21. Searchable text

For each named column:

- NULL contributes no searchable text;
- STRING contributes exact stored text;
- every other type contributes canonical text.

The query is the decoded quoted UTF-8 string.

## 22. Chinese and UTF-8 search requirement

Search must operate on valid UTF-8 sequences without assuming one byte equals one character.

A query containing Chinese text such as:

```text
"資料"
```

must match the same consecutive Unicode scalar values inside a cell such as:

```text
"資料結構"
```

No Unicode normalization is required.
Thus canonically equivalent but byte/code-point-distinct spellings are not required to match.

## 23. FIND matching rule

A row matches if the query occurs as a consecutive sequence in at least one of the listed columns.

A row is written at most once even if:

- the query appears multiple times in one cell;
- multiple listed columns match.

Matching rows preserve current active-table order.

An empty query matches **every row**, including a row whose searched cells are all NULL.

## 24. SENSITIVE mode

SENSITIVE compares Unicode scalar values exactly.

ASCII uppercase/lowercase differ.
Chinese text is naturally supported.

Examples:

```text
"Data" does not match "data"
"中文" matches "含中文內容"
```

## 25. ASCII-INSENSITIVE mode

Only ASCII letters A-Z are folded to a-z for comparison.

All non-ASCII code points are compared exactly.

Therefore:

- `"DATA"` matches `"data"`;
- `"中文"` still matches `"中文"`;
- full Unicode case folding such as German ß or Turkish I behavior is out of scope.

The implementation must not call locale-sensitive `tolower` on arbitrary negative/non-ASCII bytes and assume Unicode correctness.

## 26. FIND output

FIND serializes matching rows according to the specified output format.

Header behavior:

- CSV/TSV FIND output always includes a header record;
- Markdown naturally includes its header and separator.

This intentionally differs from WRITE, where CSV/TSV HEADER is caller-selectable.

FIND must enforce the same NULL-token ambiguity rules, Markdown boundary-space validation, and output escaping rules as WRITE.

## 27. WRITE

WRITE serializes the current active table without changing it.

If output validation fails, such as a missing required NULL-TOKEN, ambiguous NULL token, or non-round-trippable Markdown boundary space, the destination must not be opened.

The destination path must have passed the pre-execution path-collision rules.

Writing an output does not alter the active table or the LOAD-source bookkeeping used for diagnostics.

## 28. BARCODE-SHEET

BARCODE-SHEET:

- requires the named column to be EAN13 or CODE128;
- skips NULL cells;
- renders one barcode block for every non-NULL row in current row order;
- does not deduplicate equal values;
- does not mutate the active table.

If every cell in the selected column is NULL, it still writes a valid empty SVG sheet containing no barcode blocks.

Detailed SVG geometry is defined in the barcode specification.

## 29. Output command sequencing

Multiple output commands are allowed.

For example:

```text
WRITE "clean.csv" FORMAT CSV HEADER YES
FIND "urgent" IN ("notes") MODE ASCII-INSENSITIVE WRITE "urgent.md" FORMAT MARKDOWN
BARCODE-SHEET "ean" WRITE "ean.svg" MODULE 2 HEIGHT 80 GAP 20 TEXT YES
```

All three outputs are independently produced if their statements succeed.

If the third statement fails, the first two files remain.
The report identifies which command failed.

## 30. Destination collision inside one script

Two output statements may use the same path.
Later successful output replaces the earlier output using ordinary file-opening semantics.

This is allowed but **must** produce a warning in the report because it is often accidental.
If the later output truncates the reused destination and then fails during I/O, the earlier bytes at that same path are not guaranteed to remain.

## 31. Resource failure

If an allocation fails during a mutating operation:

- the current operation fails;
- the active table remains in its pre-command state;
- execution stops;
- the report records resource failure.

It is not sufficient to continue with a partially inserted row or truncated cell.

## 32. No silent skipping

Malformed rows, invalid typed values, unknown columns, and unsupported statement syntax are errors.

The implementation must not silently:

- skip bad records;
- drop extra fields;
- fill missing fields;
- keep only rows it can parse;
- ignore an unknown command;
- ignore an unsupported option.
