# Lattice Markdown 1.0.0 — Release Evidence Report

- Build identifier: `32fd04b1e1dfac24c4d80a7946aa0c35d35af14d1b91dfd6e6bba5d2bc6c97e1`
- Source-tree identity: `9fa0443a5398adc187aa710d5ae544a3a670519acc42d23443111d4c803830ea`
- Evidence generated: `2026-08-09T19:35:44Z`
- Mandatory test result: **119 passed, 0 failed, 0 skipped**

| Category | Passed | Failed | Skipped | Evidence |
|---|---:|---:|---:|---|
| Unit | 30 | 0 | 0 | `evidence/logs/unit.log` |
| Integration | 27 | 0 | 0 | `evidence/logs/integration.log` |
| X11 end-to-end | 10 | 0 | 0 | `evidence/logs/e2e.log` |
| Performance | 10 | 0 | 0 | `evidence/results/performance.json` |
| Failure injection | 15 | 0 | 0 | `evidence/results/failure.json` |
| Regression / rendered acceptance / validator mutation | 27 | 0 | 0 | `evidence/logs/regression.log` |

## Release Gates

- RG-BUILD-C17-X11: PASS
- RG-UNIT-INTEGRATION-REGRESSION: PASS
- RG-X11-E2E-CLIPBOARD-IME-XDND: PASS
- RG-WORKSPACE-SESSION-EXTERNAL-RECOVERY: PASS
- RG-PERFORMANCE: PASS
- RG-FAILURE-SAFETY: PASS
- RG-SCREENSHOTS-ACTUAL-APPLICATION: PASS (29 PNG checkpoints)
- RG-EVIDENCE-INTEGRITY: PASS (`evidence/logs/evidencecheck.log`)

## Artifact Paths

- Manifest: `evidence/manifest.json`
- Screenshots: `evidence/screenshots/`
- Deterministic fixtures: `evidence/fixtures/`
- Test logs: `evidence/logs/`
- LOC report: `evidence/loc-report.json`

## Human Review

The manifest validator verifies completeness, paths, dimensions, byte sizes, and SHA-256 digests. Human screenshot review remains required; use `docs/HUMAN_ACCEPTANCE_CHECKLIST.md`.

## Known Non-mandatory Limitations

- The Markdown renderer intentionally targets the frozen v1.0 construct set; extension syntax outside that set remains preserved as source text.
