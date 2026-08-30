# Reference and Acceptance Fixtures

The task pack ships fixed input data so different implementers are evaluated against the same cases.

- `equivalence/office_small.json` and `.yaml` are normative parser-equivalence fixtures.
- `acceptance/` contains the fixed A01-A25 acceptance configurations and explicit traces referenced by `specs/12_ACCEPTANCE_SCENARIOS.md`.
- `acceptance/SHA256SUMS.txt` records hashes of normative acceptance config/trace inputs; implementers must not alter them when claiming completion.
- `invalid/expected.csv` defines the fixed negative-input corpus and expected broad exit class.
- `traces/` contains a few additional small reference traces useful for unit tests.

Configuration/CSV/YAML fixture files are test data, not human documentation, and are excluded from the task-pack human-document line count. Markdown README files are included in that count.

Integrity manifests are provided separately for `acceptance/` and `invalid/`; both are normative for release evidence.
