# DARC 0.1.0 Release Verification Summary

- Strict default clean build: PASS, 0 compiler warnings (`build-and-test-final.log`).
- Mandatory quick catalog: 242/242 PASS, 0 FAIL, 0 SKIP (`build-and-test-final.log`).
- Mandatory stress catalog: 10/10 PASS at specified fixture sizes (`stress-final/`).
- Overall mandatory catalog: 252/252 PASS.
- Traceability: 252 unique catalog IDs, 252 unique PASS rows, no missing/extra/duplicate IDs (`traceability-validation.log`).
- ASan + UBSan + LeakSanitizer quick: 242/242 PASS, RC 0 (`test-sanitize-final-source.log`).
- Final E2E: PASS (`release-e2e-final-source.log`).
- Final repository scrub: HEALTHY (`final-scrub.log`).
- SVG human render inspection: PASS (`svg-human-check.log`).
- Production marker/subprocess/dependency audit: PASS (`source-audit.log`, `runtime-deps.log`).
- Install/uninstall smoke: PASS (`install-smoke.log`).
- Candidate source archive reproducibility: PASS — clean unzip/build, 242/242 quick, release E2E, 0 warnings (`reproducibility.log`).
