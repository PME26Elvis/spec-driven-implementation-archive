# Repomix authored-corpus review

Measured project: `project/cvc`

This config measures the normalized **implementer-authored corpus**, not
conversation text or generated validation output.

Archive curation already removes the 16 derived CPython bytecode-cache files
that were present under `tests/run/__pycache__/` in the raw submission.

Reviewed Repomix exclusion:

- `docs/full_run.txt` — mechanically generated full acceptance-test transcript.

Implementation source, headers, Python test source, build scripts, README,
`docs/IMPLEMENTATION.md`, and the authored `docs/TEST_EVIDENCE.md` remain
included.

Validated locally with Repomix 1.18.0 and `o200k_base`:

- **54 authored files**
- **152,176 tokens**
