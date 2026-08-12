# Independent Review — `darc_gates_closed_v0.1.zip`

This package records an independent build, test, black-box validation, sanitizer run, and source review of the submitted DARC implementation against the DARC v0.1.0 task-pack contract used in this conversation.

## Bottom line

**Release verdict: FAIL — mandatory release gates are not closed.**

The submission contains a meaningful amount of real C implementation and its own small test suite passes, but the delivered product is substantially below the required contract. The strongest evidence is not cosmetic:

- a legal configuration can trigger an **ASan-confirmed heap-buffer-overflow** in snapshot creation;
- deleting a live FILE object can still make `verify --level full` return exit 0, while restore then fails;
- GC does not implement partial reachability reclamation when any snapshot ref remains;
- independent duplicate files are restored as hard links;
- partial restore is parsed but ignored;
- major configuration keys, arrays, validation rules, and behavior are ignored;
- required diff classifications/metrics/report formats are largely absent;
- crash recovery is a stale-file cleanup routine rather than the specified transaction-state recovery;
- mandatory acceptance coverage is far from complete.

## What did pass

The project builds with GCC, the supplied SHA-256/CRC-32C/Buzhash/LZH1 smoke vectors pass, the supplied E2E script passes, the supplied 22-case catalog runner passes, and the supplied small stress script passes. These are useful signs of a functioning prototype, but they are not sufficient for the task pack's Definition of Done.

## Documents

- `01_executive_summary.md` — concise verdict and severity summary.
- `02_build_and_test_evidence.md` — what was actually executed and observed.
- `03_release_gate_matrix.md` — gate-by-gate compliance assessment.
- `04_findings.md` — detailed defects with source/evidence references.
- `05_source_review.md` — architecture/code-quality observations.
- `06_test_gap_analysis.md` — why the supplied green suite does not establish compliance.
- `07_remediation_priority.md` — practical order for closing the largest gaps.
- `evidence/` — raw build/test/sanitizer logs and independent test scripts/results.

The submitted source was not modified during review.
