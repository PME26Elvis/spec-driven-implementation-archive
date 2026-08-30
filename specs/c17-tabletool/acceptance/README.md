# Acceptance Fixtures

These files provide small fixed examples for final manual verification.
They are intentionally not an exhaustive test suite.

Run the supplied `.tts` scripts with the working directory set to the root of this task pack, or copy the fixture paths into an equivalent local test setup.

The scripts write `*_actual.*` outputs into the current working directory.
Compare them with files under `acceptance/expected/`.

Before each case, remove stale outputs/report files from an earlier run unless that case explicitly checks preservation of a pre-existing sentinel file. A stale file is never evidence that the current run produced it.

## Case A — people

Script:

```text
acceptance/scripts/case_a_people.tts
```

Checks CSV loading, INTEGER/DECIMAL conversion, deterministic multi-key sorting, Chinese UTF-8 preservation, and Markdown output.

Expected table:

```text
acceptance/expected/people_expected.md
```

## Case B — URLs

`case_b_urls.tts` must succeed and match `urls_expected.csv`.

`case_b_urls_bad.tts` must fail during URL conversion.
Before running it, remove any stale `must_not_exist.csv`; after failure that path must still not exist.

## Case C — EAN-13

`case_c_ean.tts` must:

- canonicalize 12-digit input to 13 digits;
- preserve a valid 13-digit input;
- create `ean_actual.svg`.

`ean_reference_modules.txt` supplies one 95-module reference bit string for inspection.

`case_c_ean_bad.tts` must fail on a wrong supplied check digit.
Before failure cases, remove stale `*_actual.*` or `must_not_exist.*` files named by that case so a pre-existing file cannot be mistaken for newly created output.

## Case D — Code 128

`case_d_code128.tts` must create a canonical CSV and SVG sheet.

`code128_reference_codewords.txt` gives deterministic reference codeword/checksum sequences for the four fixture payloads.

These references are particularly useful for checking B/C optimization separately from SVG rendering.

## Case E — Chinese FIND

`case_e_chinese_find.tts` must produce only rows containing `資料` in the selected fields.

It then writes the entire active table again.
The full-table output must match `notes_after_find_expected.csv`, confirming that FIND did not mutate the table.

## Case F — TSV + mutation + Markdown

`case_f_mutations.tts` exercises TSV escape/NULL decoding, typed conversion, all row/column mutation families, SET/SET NULL, and Markdown output.
Its result must match `mutations_expected.md`.

## Case G — Markdown + BOOLEAN/DATE/NULL + CSV

`case_g_markdown_types.tts` loads restricted Markdown, converts BOOLEAN and DATE, preserves NULL distinct from empty STRING, and writes CSV.
Its result must match `markdown_types_expected.csv`.

`case_g_markdown_null_space_bad.tts` intentionally uses a Markdown NULL token with boundary ASCII space and must fail before creating its destination.

## Case H — parse-before-execute and report safety

`case_h_parse_guard.tts` contains a valid LOAD and WRITE followed by a syntax error.
It must exit 3 with zero commands executed.
The earlier WRITE destination must not be created or modified.

For the report-safety variant, invoke the same malformed script with `--report acceptance/fixtures/parse_guard.csv`.
That existing fixture file must remain byte-identical because syntax errors never open/modify the requested report path.

## Precedence

If an accidental discrepancy is ever found between a fixture and the normative specification, the normative specification controls.
The fixture must then be corrected rather than working around the specification.
