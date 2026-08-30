# 08 — Testing, Acceptance, Deliverables, and Definition of Done

## 1. Verification philosophy

This assignment is intended for practical engineering comparison, not an academic benchmark.

Verification should be strong enough to catch incomplete or substituted implementations without creating a large evidence-production project.

Required verification consists of:

- implementation-supplied automated tests;
- the fixed acceptance fixtures in this task pack;
- a straightforward human checklist.

No coverage percentage, formal proof, performance paper, video evidence, screenshot archive, or CI badge is required.

## 2. Required implementation deliverables

The final implementation submission must contain at least:

```text
README.md
BUILDING.md
src/
tests/
testdata/
examples/
```

`src/` contains the C17 product source.
Headers may be placed in `src/` or a separate `include/` directory.

`tests/` contains automated tests for required behavior.
A tiny self-written C test harness is acceptable.

`testdata/` contains data used by the implementation tests.

`examples/` contains at least two usable example batch scripts and their small inputs.

## 3. BUILDING.md

BUILDING.md must state:

- that the product targets ISO C17;
- the source files that form the product;
- at least one example C17 compile/link command;
- how to build/run the tests;
- any optional convenience build files.

It must not claim a non-standard dependency is required for core behavior.

## 4. README.md

Implementation README must explain:

- product purpose;
- primary `--script` / `--report` invocation;
- supported formats;
- supported types;
- location of examples;
- how to run tests.

It need not repeat the full task specification.

## 5. Files that should not be delivered

Do not include large incidental development artifacts such as:

- compiler object files;
- executables unless explicitly useful as an optional convenience;
- coverage databases;
- IDE caches;
- editor history;
- core dumps;
- temporary barcode outputs;
- test logs from routine runs;
- dependency caches.

The source submission is the authoritative deliverable.

## 6. Automated test expectation

Tests must exercise actual product modules or the executable.
Tests that only reimplement the expected answer independently without calling product code do not count.

The tests may use only facilities reasonably available to the submitted project.
No third-party test framework is required.

## 7. Minimum unit-test areas

The implementation test suite must contain focused tests for at least:

1. UTF-8 valid ASCII;
2. valid 2/3/4-byte UTF-8;
3. invalid continuation byte;
4. overlong UTF-8;
5. CSV comma inside quoted field;
6. CSV embedded newline;
7. CSV doubled quote;
8. malformed CSV quote;
9. bare CR as CSV field data;
10. CRLF inside a quoted CSV cell round-trips as CR + LF data;
11. terminal CSV/TSV record ending does not manufacture an extra empty record;
12. Markdown rejects bare CR while `\r` escape represents semantic CR;
13. TSV escapes;
14. malformed TSV escape;
15. TSV NULL token after escape decoding;
16. output NULL token passes through ordinary CSV/TSV/Markdown field escaping and round-trips distinctly from equal-looking non-NULL data;
17. Markdown escaped pipe;
18. Markdown outer-pipe/empty-edge-cell tokenization;
19. Markdown row-width error;
20. Markdown output rejection for semantic leading/trailing ASCII space;
21. INTEGER minimum/maximum;
22. INTEGER overflow;
23. value-literal lexical-text conversion (`TRUE` to STRING, `1.0` rejected by INTEGER);
24. very large unquoted numeric-looking literal can remain STRING text while target numeric conversion rejects it;
25. exact DECIMAL comparison across scales;
26. DECIMAL precision-38 acceptance / precision-39 rejection;
27. DECIMAL scale-18 acceptance / scale-19 rejection;
28. Gregorian leap-year cases;
29. BOOLEAN accepted/rejected spellings;
30. URL normalization examples;
31. URL repeated-slash/dot-segment adversarial examples;
32. invalid URL examples;
33. EAN 12-digit check generation;
34. EAN 13-digit check validation;
35. EAN 95-module encoding;
36. Code128 B-only payload;
37. Code128 C-beneficial numeric payload;
38. Code128 mixed B/C payload;
39. Code128 checksum;
40. Code128 stop/module pattern;
41. stable single-key sort;
42. stable multi-key sort;
43. NULL-last ASC;
44. NULL-last DESC;
45. Chinese FIND;
46. ASCII-insensitive FIND;
47. duplicate FIND-column rejection;
48. atomic failed TYPE conversion;
49. atomic failed row insertion;
50. report dynamic-string escaping;
51. SVG width/height/viewBox agreement.

These may be grouped into fewer test functions.



## 8. Minimum integration-test areas

At least the following end-to-end behaviors must be tested by invoking the script execution layer or final executable:

1. load CSV -> type columns -> sort -> write CSV;
2. load TSV -> mutate rows/columns -> write Markdown;
3. load Markdown -> FIND Chinese -> write CSV;
4. URL column conversion -> canonical export;
5. EAN13 column -> SVG barcode sheet;
6. CODE128 column -> SVG barcode sheet;
7. malformed script causes zero product commands executed and leaves the requested report path untouched;
8. malformed input fails with nonzero exit and a report when the report was safely opened;
9. output requiring NULL-TOKEN fails when missing;
10. Markdown boundary-space output rejection occurs before destination open;
11. exact path collision preflight prevents input/report/output self-destruction;
12. complete successful run creates the expected report/output files and correct counters.

## 9. Fixed acceptance fixtures

The task pack's `acceptance/` directory supplies representative cases A through H.

They are deliberately small enough to inspect manually and are not an evidence-production burden.
An implementation must be run against them after its own tests pass.

Before each acceptance run, remove stale `*_actual.*`, `must_not_exist*`, `parse_guard_output.csv`, and ordinary report files created by prior runs unless the case explicitly tests preservation of a pre-existing file.
A stale file must never be counted as a newly produced successful result.

The fixtures are not a license to hard-code behavior.
Equivalent unseen inputs must work.

## 10. Human acceptance checklist — build and constraints

An evaluator should be able to mark all of these YES:

- [ ] Product implementation is C17.
- [ ] Product source does not require a third-party library.
- [ ] Product source does not require POSIX/Win32 APIs for mandatory behavior.
- [ ] No external interpreter or CLI is used to perform parsing/sorting/barcodes.
- [ ] Product can be built following BUILDING.md.
- [ ] `--help` works.
- [ ] `--version` works.
- [ ] A processing run accepts `--script` and `--report`.
- [ ] Product file I/O uses binary mode so TableTool, not host text translation, controls line endings.
- [ ] Normal transformed tables are files, not stdout dumps.

## 11. Human acceptance checklist — parsing and types

- [ ] CSV quoting and embedded newline work.
- [ ] TSV escaping works.
- [ ] Restricted Markdown input and output work with deterministic outer-pipe parsing.
- [ ] Markdown refuses boundary-space data it cannot round-trip rather than silently trimming it.
- [ ] Invalid UTF-8 is rejected.
- [ ] Chinese STRING data survives round-trip.
- [ ] INTEGER has signed 64-bit range checking.
- [ ] DECIMAL is exact and not merely a floating-point sort; precision/scale boundaries match the mechanical rules.
- [ ] Script value literals preserve lexical text until the target type parser consumes it.
- [ ] BOOLEAN canonicalizes to `true` / `false`.
- [ ] DATE validates real Gregorian dates.
- [ ] NULL remains distinct from empty STRING.
- [ ] Failed whole-column TYPE conversion does not partially convert the column.

## 12. Human acceptance checklist — table operations

- [ ] Add/drop/rename/move/swap column commands are implemented.
- [ ] Insert/append/delete/move/swap row commands are implemented.
- [ ] SET CELL and SET NULL are implemented.
- [ ] Invalid positions fail rather than clamp silently.
- [ ] Multi-key sorting works.
- [ ] Sorting is stable.
- [ ] NULL is last under both ASC and DESC.
- [ ] FIND does not mutate the active table.
- [ ] FIND can locate Chinese text.
- [ ] FIND rejects duplicate IN-list columns.
- [ ] ASCII-insensitive FIND folds ASCII letters only.

## 13. Human acceptance checklist — URL

- [ ] Only HTTP/HTTPS are accepted.
- [ ] Scheme and DNS host lowercase correctly.
- [ ] Default port is removed.
- [ ] Non-default port remains.
- [ ] Dot path segments normalize.
- [ ] Malformed percent escape is rejected.
- [ ] Userinfo and IPv6 are rejected as out of scope.
- [ ] Canonical URL is what gets exported.

## 14. Human acceptance checklist — EAN13

- [ ] 12 digits generate a check digit.
- [ ] 13 digits verify check digit.
- [ ] Bad check digit is rejected.
- [ ] First-digit parity affects left encoding.
- [ ] Encoded EAN data/guards total 95 modules.
- [ ] SVG includes the defined quiet zones.
- [ ] SVG contains actual computed bar rectangles/path, not a placeholder.

## 15. Human acceptance checklist — Code128

- [ ] Printable ASCII payload validates.
- [ ] Unsupported bytes reject.
- [ ] Set B works.
- [ ] Numeric runs can use Set C.
- [ ] Mixed payload can switch B/C.
- [ ] Encoder selects a globally shortest B/C sequence.
- [ ] Tie-breaking is deterministic.
- [ ] Checksum uses modulo 103 weighted rule.
- [ ] Stop pattern is present.
- [ ] SVG quiet zones and module widths are correct.
- [ ] XML-special payload text is escaped when TEXT YES.

## 16. Human acceptance checklist — errors/report

- [ ] Success report contains required summary and exact counters.
- [ ] Failure report identifies useful line/row/column context where applicable.
- [ ] Dynamic report strings escape LF/CR/tab/control bytes and cannot inject report records.
- [ ] Syntax error near script end produces no earlier data outputs because parsing happens first.
- [ ] Syntax/encoding error does not open or modify the requested report path.
- [ ] A failed mutating command leaves active table uncorrupted.
- [ ] Missing required NULL-TOKEN fails before output open.
- [ ] Ambiguous NULL token is rejected.
- [ ] Exact path collisions are rejected before unsafe files are opened.
- [ ] Unknown commands/options are rejected.
- [ ] Nonzero failures use the defined exit-code category.
- [ ] Out-of-range numeric command parameters are domain errors, while protective resource ceilings are resource errors.

## 17. Acceptance case A — typed sort and Markdown

Use the supplied people CSV/script.

Check that:

- `points` becomes INTEGER;
- `price` becomes DECIMAL;
- Chinese names remain correct;
- sort is points DESC then name ASC;
- exact expected Markdown rows appear.

## 18. Acceptance case B — URL normalization

Use the supplied URLs fixture.

Check that successful URLs export in canonical form.

A separate invalid URL case must fail and identify the URL column/row.

## 19. Acceptance case C — EAN13

Use the supplied EAN fixture.

Check:

- 12-digit payload becomes canonical 13 digits;
- known 13-digit payload stays valid;
- SVG is created;
- SVG root width/height/viewBox agree with computed geometry;
- report records the SVG output.

A bad-check fixture must fail during TYPE conversion.

## 20. Acceptance case D — Code128

Use the supplied Code128 fixture.

Check:

- alphabetic data uses valid B encoding;
- long numeric data uses C where optimal;
- mixed payload has deterministic codeword/checksum behavior;
- SVG contains separate barcode blocks in row order;
- SVG root width/height/viewBox agree with computed geometry.

## 21. Acceptance case E — Chinese search

Use the supplied notes fixture.

FIND query `資料` must return only rows whose searched columns contain that exact Chinese sequence.

The source active table must remain unchanged for a subsequent WRITE.

## 22. Acceptance case F — TSV and complete mutation families

Use `acceptance/scripts/case_f_mutations.tts`.

Check that:

- TSV `\\N` decodes to semantic `\N` and becomes NULL under the script NULL token;
- INTEGER, DECIMAL, BOOLEAN, and DATE conversions work;
- ADD/DROP/RENAME/MOVE/SWAP COLUMN all affect real table state;
- INSERT/APPEND/DELETE/MOVE/SWAP ROW all affect real table state;
- SET CELL and SET NULL work;
- final Markdown bytes match `acceptance/expected/mutations_expected.md`.

## 23. Acceptance case G — Markdown typed/NULL behavior

Use `case_g_markdown_types.tts` and compare with `markdown_types_expected.csv`.

Check that:

- Markdown import handles escaped pipe text;
- BOOLEAN canonicalization is correct;
- valid leap-day DATE values survive;
- NULL remains distinct from an empty STRING.

Then run `case_g_markdown_null_space_bad.tts` after removing stale `must_not_exist_g.csv`.
It must fail because a Markdown NULL token has leading/trailing ASCII space, and it must not create that destination.

## 24. Acceptance case H — parse-before-execute and report safety

Use `case_h_parse_guard.tts`.

Normal malformed-script run:

- exit code is 3;
- `parse_guard_output.csv` is not created or modified;
- requested report path is not created or modified;
- stderr contains a concise syntax diagnostic.

Report-safety variant:

```text
tabletool --script acceptance/scripts/case_h_parse_guard.tts --report acceptance/fixtures/parse_guard.csv
```

The existing `parse_guard.csv` fixture must remain byte-identical to `acceptance/expected/parse_guard_expected.csv`.
This is intentionally a syntax-error case: the report path is never opened, even though a syntactically valid earlier LOAD happens to name the same file.

## 25. No performance benchmark gate

There is no strict wall-clock benchmark.

However, the program must not be intentionally quadratic for obvious core operations when a standard practical design is straightforward.

Expected engineering choices include:

- amortized dynamic buffers/arrays rather than reallocating one byte at a time;
- a stable `O(n log n)` sorting approach for ordinary table sizes;
- linear parsing in input length;
- Code128 optimization polynomial/linear in the bounded payload rather than enumerating every segmentation exponentially.

Performance that makes the supplied 100,000-row scale impractical under ordinary conditions is a defect even though no exact seconds are specified.

## 26. No coverage gate

No line/branch coverage percentage is required.

A smaller meaningful test suite that covers the required contracts is preferable to generated low-value tests.

## 27. No evidence bundle gate

The implementation does **not** need to produce:

- screenshots;
- test videos;
- provenance logs;
- command transcripts;
- coverage HTML;
- benchmark charts;
- formal verification artifacts.

The evaluator may simply build, run tests, run acceptance scripts, and inspect outputs/source.

## 28. Definition of Done

The assignment is DONE only when all of the following are true:

1. all mandatory source/delivery files exist;
2. the product builds as C17 without mandatory non-standard dependencies;
3. all mandatory script statements are implemented and connected;
4. CSV, TSV, and Markdown behavior matches the specification;
5. all eight required types are real validated semantic types;
6. stable sorting and UTF-8 search work;
7. URL parsing/normalization works;
8. EAN13 encoding and SVG output work;
9. Code128 B/C optimal encoding, checksum, patterns, and SVG output work;
10. defined errors/exit codes/report escaping/counter and no-truncation behavior work;
11. implementation-supplied automated tests pass;
12. supplied acceptance cases A through H pass;
13. the human checklist has no known mandatory NO item;
14. no prohibited substitute implementation is used;
15. no known mandatory function is a placeholder, mock, or hard-coded fixture solution.

## 29. Stop condition

Once the Definition of Done is satisfied, the implementer may stop.

The implementer is **not** expected to add optional features to make the project appear more complete.

In particular, completion does not require:

- adding a GUI;
- implementing JSON/YAML;
- adding a database;
- adding more barcode families;
- making URL handling fully RFC-complete;
- implementing Unicode normalization;
- adding networking;
- adding a plugin architecture.

This stop condition is part of the assignment so different implementations are judged against the same product scope rather than against optional embellishment.

## 30. Release blocker rule

A known failure of any mandatory item is a release blocker.

The final submission must not claim completion if, for example:

- Code128 rendering exists but optimal set switching is missing;
- Markdown output works but Markdown import is placeholder-only;
- FIND works only for ASCII because UTF-8 is processed incorrectly;
- DECIMAL is secretly `double` and misorders exact values;
- tests are present but mandatory commands are not wired;
- barcode SVG is static fixture content.

If such a gap remains, it should be stated plainly as incomplete rather than hidden.

## 31. Evaluation priority

When comparing implementations, evaluate in this order:

1. correctness and completeness;
2. compliance with prohibited-substitute rules;
3. robustness and edge-case handling;
4. code organization/readability;
5. test quality;
6. efficiency;
7. optional polish.

Optional features must never compensate for a missing mandatory requirement.
