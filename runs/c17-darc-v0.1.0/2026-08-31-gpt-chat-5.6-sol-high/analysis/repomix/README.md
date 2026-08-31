# Repomix authored-corpus review

Measured project: `project/darc_v0.1.0`

This config measures the normalized **implementer-authored corpus**, not conversation text or generated validation evidence.

Reviewed exclusions:

- `evidence/**` — generated release/build/test/stress/sanitizer/E2E evidence.
- `examples/**` — byte-identical benchmark input already preserved in canonical `specs/c17-darc-v0.1.0/`.
- `acceptance/CHECKLIST.md`
- `acceptance/GOLDEN_VECTORS.md`
- `acceptance/TEST_CATALOG.md`
- `acceptance/TRACEABILITY_TEMPLATE.md`

The four acceptance files above were verified byte-identical to the canonical task-pack copies. Generated/filled `acceptance/CHECKLIST_COMPLETED.md` and `acceptance/TRACEABILITY.md` remain included because they were produced by this implementation run.

Validated by `Count authored run tokens` workflow run 12 with Repomix 1.18.0 and `o200k_base`:

- **52 authored files**
- **160,382 tokens**
