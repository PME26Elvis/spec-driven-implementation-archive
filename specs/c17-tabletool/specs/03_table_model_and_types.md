# 03 — Table Model and Typed Values

## 1. Active table model

At runtime the program maintains at most one active table.

A table consists of:

- an ordered list of columns;
- zero or more ordered rows;
- one cell for every row/column intersection.

Every column has:

- a non-empty UTF-8 name;
- one of the mandatory types;
- a 1-based position.

Every cell has either:

- the explicit state NULL; or
- one valid value of its column's type.

NULL is not the same as an empty STRING, numeric zero, `false`, or an empty URL.

## 2. Column-name rules

Column names:

- must be valid UTF-8;
- must not contain NUL;
- must not be empty;
- are compared exactly and case-sensitively;
- must be unique within a table.

Names may contain spaces, Chinese characters, punctuation, and ASCII digits.

The names `"Name"` and `"name"` are distinct.
The names `"名稱"` and `"名称"` are distinct.

The implementation must not apply Unicode normalization.

## 3. Initial type after import

CSV, TSV, and Markdown formats do not carry the TableTool schema.
Every imported column therefore begins as STRING.

The script uses `TYPE` statements to convert columns when stronger semantics are required.

This is intentional: type inference based on a few input rows is prohibited because it makes behavior data-dependent and difficult to reproduce.

## 4. Canonical text representation

Every non-NULL typed value has one canonical text representation.

Canonical text is used for:

- CSV/TSV/Markdown export;
- FIND on non-STRING columns;
- deterministic comparison for semantic string-like types where stated;
- report diagnostics when a value can safely be shown.

A successful conversion stores the semantic value, not merely the original spelling.
Export therefore uses canonical spelling and may differ from the source text.

## 5. STRING

### Validity

A STRING is an arbitrary valid UTF-8 sequence excluding NUL.

It may be empty.
It may contain:

- Chinese text;
- emoji;
- ASCII control characters other than NUL when the enclosing file format can represent them;
- line feeds;
- carriage returns;
- tabs;
- commas and quotes;
- Markdown punctuation.

### Canonical text

The canonical text is exactly the decoded UTF-8 string.
No normalization, trimming, or case conversion is applied.

### Ordering

STRING values are ordered lexicographically by Unicode scalar value.

The implementation must validate and traverse UTF-8 itself.
It must not depend on `strcmp` under a locale for semantic correctness.

For valid UTF-8 an implementation may use an equivalent bytewise optimization if tests demonstrate that it preserves the specified Unicode scalar-value order.

## 6. INTEGER

### Accepted syntax

An INTEGER text value must match the following logical grammar:

```text
[+|-] digit { digit }
```

There must be at least one decimal digit.
ASCII whitespace is not allowed inside or around the value.
Thousands separators are not allowed.
Hexadecimal, octal prefixes, and exponent notation are not allowed.

Examples accepted:

```text
0
+7
-7
00042
-00042
```

Examples rejected:

```text
1,000
0x10
3.0
1e3
 42
42 
+
```

### Range

The semantic range is exactly:

```text
-9223372036854775808 .. 9223372036854775807
```

Overflow and underflow must be detected before a wrapped value is stored.

### Canonical text

- no leading `+`;
- no unnecessary leading zeros;
- zero is `0`;
- negative zero canonicalizes to `0`.

Examples:

```text
+00042  -> 42
-00042  -> -42
-0      -> 0
```

### Ordering

Numeric signed 64-bit order.

## 7. DECIMAL

DECIMAL is an exact base-10 value.
Required behavior must not be implemented with binary floating-point comparison.

### Accepted syntax

```text
[+|-] digits [ "." digits ]
```

At least one digit is required before the decimal point.
If a decimal point appears, at least one digit must follow it.
No exponent notation is allowed.
No surrounding whitespace is allowed.

Examples accepted:

```text
0
1
-1
+1.25
00012.3400
-0.0001
```

Examples rejected:

```text
.5
5.
1e6
NaN
Infinity
1,234.5
```

### Precision limits

DECIMAL validation uses two independent limits.

**Input scale** is the number of digits physically present after the decimal point in the accepted input spelling.
Input scale must be at most 18, even if trailing fractional zeros would later disappear during canonicalization.

**Precision** is calculated mechanically as follows:

1. ignore the sign;
2. split the already-syntax-valid input into integer and fractional digit sequences;
3. remove trailing zeros from the fractional sequence for canonical-value purposes;
4. concatenate integer digits and the remaining fractional digits;
5. remove leading zeros from that combined sequence;
6. if no digit remains, the value is zero and its precision is defined as 1;
7. otherwise precision is the number of remaining digits.

Precision must be at most 38.

Examples:

```text
0.000000000000000001   # scale 18, precision 1: valid
0000.0012300            # scale 7,  precision 3: valid
0.000000000000000000   # scale 18, precision 1: valid zero
1.0000000000000000000  # scale 19: invalid even though canonical value would be 1
99999999999999999999999999999999999999  # precision 38: valid
100000000000000000000000000000000000000 # precision 39: invalid
```

An implementation may use a sign plus digit arrays or another exact representation.
Using `double`, `float`, or `long double` as the authoritative stored/comparison value does not satisfy this requirement.

### Canonical text

- remove leading `+`;
- remove unnecessary leading integer zeros;
- remove trailing fractional zeros;
- remove the decimal point if the fractional part becomes empty;
- canonicalize all spellings of signed zero to `0`.

Examples:

```text
+0012.3400 -> 12.34
000.5000   -> 0.5
-0.000     -> 0
10.000     -> 10
```

### Ordering

Exact mathematical decimal order.

Values with different scales but equal numeric value compare equal.
For example, `1.0`, `1.00`, and `01.000` are the same semantic value after conversion.

## 8. BOOLEAN

### Accepted syntax

The following ASCII spellings are accepted case-insensitively:

```text
true
false
```

The exact tokens `1` and `0` are also accepted.

No other words are accepted.

### Canonical text

```text
true
false
```

### Ordering

`false < true`.

## 9. DATE

DATE is a Gregorian calendar date without a time or timezone.

### Accepted syntax

Exactly:

```text
YYYY-MM-DD
```

where all components use ASCII digits and the year is from `0001` through `9999`.

Examples:

```text
2026-08-30
2000-02-29
0001-01-01
9999-12-31
```

### Calendar validity

Month must be 01 through 12.
Day must exist in that month.

Leap years follow the proleptic Gregorian rule:

- divisible by 4 => leap year;
- divisible by 100 => not leap year;
- divisible by 400 => leap year.

Therefore:

- `2000-02-29` is valid;
- `1900-02-29` is invalid;
- `2024-02-29` is valid;
- `2023-02-29` is invalid.

### Canonical text

The accepted `YYYY-MM-DD` spelling is already canonical.

### Ordering

Chronological order by year, month, then day.

No call to host timezone conversion is needed or expected.

## 10. URL

URL is a semantic HTTP/HTTPS URL type.

Its syntax, normalization, and comparison are defined fully in `06_url_and_barcode.md`.

A URL cell must be fully parsed and validated during conversion.
Checking only a prefix or using a permissive substring test does not satisfy the type.

Canonical text is the normalized URL.

## 11. EAN13

EAN13 is a semantic barcode type.

The input, check-digit behavior, canonical representation, symbol encoding, and rendering requirements are defined in `06_url_and_barcode.md`.

Canonical text is exactly 13 ASCII digits.

## 12. CODE128

CODE128 is a semantic barcode payload type for this assignment's required Code 128 B/C encoder.

The accepted character repertoire and encoding algorithm are defined in `06_url_and_barcode.md`.

Canonical text is the accepted payload text itself.

## 13. NULL

NULL is a state outside the non-NULL value domain of every type.

Rules:

- NULL converts to NULL under `TYPE`;
- NULL compares equal to NULL for sort-key equality;
- NULL ordering is defined by SORT semantics;
- FIND does not search inside a NULL cell;
- barcode rendering skips NULL cells;
- file import/export requires the explicit NULL-TOKEN behavior in the file-format specification.

## 14. TYPE conversion

`TYPE "column" TARGETTYPE` converts every non-NULL cell in the named column.

Conversion procedure:

1. preserve all NULL cells as NULL;
2. parse every non-NULL cell's current canonical text using TARGETTYPE rules;
3. if every parse succeeds, replace the column type and values;
4. if any parse fails, change nothing in that column and fail the statement.

The first failing row in current row order must be identified in the report.

Converting a column to its current type is permitted and succeeds without changing values.

Converting any non-NULL type to STRING stores that value's current canonical text.

## 15. Insertion and SET conversion

A value literal supplied to INSERT ROW, APPEND ROW, ADD COLUMN DEFAULT, or SET CELL is converted directly to the target column type.

Conversion uses the literal-text rule from `02_cli_and_script_language.md`:

- unquoted `NULL` becomes NULL directly;
- quoted literals contribute decoded string text;
- all other unquoted literals contribute their exact source token spelling;
- the target type parser consumes that text without hidden cross-type coercion.

The entire command must validate all values before mutating table state.

For example, if the seventh value of an APPEND ROW is an invalid DATE, no part of the new row is appended.

## 16. Equality used by operations

The required command set does not expose a general equality filter.
Where the implementation needs equality internally:

- INTEGER uses numeric equality;
- DECIMAL uses exact numeric equality;
- BOOLEAN uses boolean equality;
- DATE uses same calendar date;
- STRING uses exact code-point sequence;
- URL uses canonical URL text;
- EAN13 uses canonical 13 digits;
- CODE128 uses exact payload text;
- NULL equals NULL only for internal key-equivalence purposes.

## 17. Ownership and copying

The specification does not mandate row-major or column-major storage.

However:

- mutations must not leave dangling references;
- moving/swapping rows or columns must preserve complete values;
- strings and variable-length semantic values must have clear ownership;
- a failed operation must not corrupt the active table;
- freeing the active table must release all memory owned by it.

## 18. No hidden coercion

Outside the explicitly defined parsing/conversion rules, the implementation must not silently coerce values.

Examples of prohibited behavior:

- invalid INTEGER becoming `0`;
- invalid BOOLEAN becoming `false`;
- invalid DATE being stored as STRING in a DATE column;
- invalid EAN13 retaining an unchecked payload;
- trimming STRING values automatically;
- interpreting empty STRING as NULL without a NULL-TOKEN rule.
