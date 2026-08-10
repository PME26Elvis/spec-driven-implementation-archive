# Mandatory Test and Acceptance ID Registry

Version: **1.0**
Status: **Normative final**

## 1. Purpose

This file is the canonical registry of mandatory named verification IDs for v1.0. It exists so an aggregate runner cannot silently omit an entire subsystem while still reporting a misleading green summary.

The implementation may add more tests. It may not remove, rename, skip, or reinterpret the IDs listed here.

## 2. Registry consumption rule

The final `releasecheck` must load or encode an expected registry equivalent to this document and compare it with actual discovered/executed result IDs.

For every mandatory ID:

- exactly one final result must exist;
- the test must have executed real production code/state where applicable;
- final status must be PASS;
- SKIP, XFAIL, TODO, NOT_RUN, FLAKY, TIMEOUT, ERROR, malformed result, duplicate result, or missing result blocks release.

Supplemental subcases may be reported beneath a registered ID.

## 3. Physics validation IDs — 40

Mandatory:

- `VAL-01` through `VAL-40` inclusive.

`VAL-01`–`VAL-12` are the base physics validations.

`VAL-13`–`VAL-40` are the advanced analytic/invariant/metamorphic/fuzz validations in `15_ADVANCED_PHYSICS_VALIDATION.md`.

Expected count: **40**.

## 4. Spatial query IDs — 15

Mandatory:

- `QRY-01` through `QRY-15` inclusive.

Expected count: **15**.

## 5. Sensor/event lifecycle IDs — 15

Mandatory:

- `SNS-01` through `SNS-15` inclusive.

Expected count: **15**.

## 6. Deterministic replay/checkpoint IDs — 18

Mandatory:

- `RPL-01` through `RPL-18` inclusive.

Expected count: **18**.

## 7. Solver Inspector IDs — 20

Mandatory:

- `SINSP-01` through `SINSP-20` inclusive.

Expected count: **20**.

## 8. CCD / TOI IDs — 30

Mandatory:

- `CCD-01` through `CCD-30` inclusive.

Expected count: **30**.

The named fixtures `CCD-THIN-WALL-01` and `CCD-ROT-01` are also mandatory acceptance inputs but are fixture IDs, not additional test-result IDs.

## 9. Shape Cast IDs — 18

Mandatory:

- `CAST-01` through `CAST-18` inclusive.

Expected count: **18**.

## 10. Collision filtering IDs — 24

Mandatory:

- `COLF-01` through `COLF-24` inclusive.

Expected count: **24**.

## 11. Replay Timeline IDs — 28

Mandatory:

- `TLN-01` through `TLN-28` inclusive.

Expected count: **28**.

## 12. Golden integrated scenario IDs — 12

Mandatory:

- `GOLD-01` through `GOLD-12` inclusive.

Expected count: **12** and aggregate requirement: **12 / 12 PASS**.

## 13. Force/impulse IDs — 8

Mandatory:

- `FRC-01` through `FRC-08` inclusive.

Expected count: **8**.

## 14. Motion recorder/graph IDs — 9

Mandatory:

- `REC-01` through `REC-09` inclusive.

Expected count: **9**.

## 15. Base full-application E2E IDs — 10

Mandatory:

- `E2E-01` through `E2E-10` inclusive.

Expected count: **10**.

## 16. Physics-focused E2E IDs — 4

Mandatory:

- `E2E-PHY-01` through `E2E-PHY-04` inclusive.

Expected count: **4**.

## 17. Query/sensor/replay/checkpoint E2E IDs — 4

Mandatory single IDs:

- `E2E-Q01`
- `E2E-S01`
- `E2E-R01`
- `E2E-C01`

Expected count: **4**.

## 18. Solver Inspector E2E IDs — 6

Mandatory:

- `E2E-SI-01` through `E2E-SI-06` inclusive.

Expected count: **6**.

## 19. Force/trajectory E2E IDs — 4

Mandatory:

- `E2E-FT-01` through `E2E-FT-04` inclusive.

Expected count: **4**.

## 20. CCD/Shape Cast E2E IDs — 6

Mandatory:

- `E2E-CCD-01` through `E2E-CCD-06` inclusive.

Expected count: **6**.

## 21. Collision filtering E2E IDs — 7

Mandatory:

- `E2E-COLF-01` through `E2E-COLF-07` inclusive.

Expected count: **7**.

## 22. Replay Timeline E2E IDs — 6

Mandatory:

- `E2E-TLN-01` through `E2E-TLN-06` inclusive.

Expected count: **6**.

## 23. Windows platform IDs — 30

Mandatory:

- `WIN-01` through `WIN-30` inclusive.

Expected count: **30**.

## 24. Windows full-application E2E IDs — 8

Mandatory:

- `E2E-WIN-01` through `E2E-WIN-08` inclusive.

Expected count: **8**.

## 25. Explicit named-case total

Sections 3–24 define **322 mandatory named functional/validation/E2E/platform result IDs**: the original 284 non-platform IDs plus 38 Windows-specific IDs.

This total excludes:

- performance workload IDs;
- stable-envelope fixture IDs;
- the minimum 60 deterministic regression fixtures;
- developer-tool self-test subcases;
- evidence-presence checks;
- package/dependency/traceability audits.

Those exclusions remain mandatory under their own sections and release gates.

## 26. Mandatory performance workload IDs

The final performance/scale report must include all of:

- `PERF-100`
- `PERF-1000`
- `PERF-BROAD-5000`
- `PERF-DENSE-500`
- `PERF-JOINT-200`
- `PERF-CHURN-5000`
- `PERF-QUERY-5000`
- `PERF-SENSOR-1000`
- `PERF-REPLAY-100K`
- `PERF-CCD-1000`
- `PERF-CCD-THIN-200`

Expected named performance workload count: **11**.

A workload is not allowed to claim PASS if it dropped mandatory physics/query/sensor/CCD work to improve timing.

## 27. Mandatory stable-envelope fixture IDs

The release acceptance system must include these named fixed-envelope scenes:

- `ENV-TOWER-01`
- `ENV-BRIDGE-01`
- `ENV-LINKED-01`
- `ENV-SLEEP-01`
- `ENV-SENSOR-01`

Expected count: **5**.

These are in addition to the Golden Suite.

## 28. Mandatory CCD fixture IDs

The following fixed CCD inputs must be present and integrity-checked:

- `CCD-THIN-WALL-01`
- `CCD-ROT-01`

Expected count: **2**.

## 29. Regression corpus requirement

The project must provide at least **60 deterministic non-Golden regression fixtures** satisfying `09_TEST_VERIFICATION.md` and `15_ADVANCED_PHYSICS_VALIDATION.md`.

Every fixture must have:

- stable unique fixture ID;
- documented purpose/oracle;
- deterministic input/seed where applicable;
- machine-readable result;
- no silent skip.

The 60-fixture minimum is a corpus-size requirement, not permission to use anonymous fixtures. IDs may use an implementation-defined stable prefix such as `REG-*`.

## 30. Developer-tool self-tests

Every required developer/verification tool must have self-tests proving at least:

- successful valid-input path;
- rejected invalid-input path;
- non-zero exit on failure;
- machine-readable report validity where required;
- no hard-coded PASS path;
- deterministic output where the tool contract requires determinism.

Tool self-test IDs may use implementation-defined names, but the aggregate `dev_tools` group must be PASS.

## 31. Required audit result IDs/groups

The final release report must contain one unambiguous PASS/BLOCKED result for each of:

- `audit_test_inventory`
- `audit_traceability`
- `audit_prohibited_dependencies`
- `audit_anti_placeholder`
- `audit_evidence_completeness`
- `audit_fixture_integrity`
- `audit_build_identity`

These are aggregate audit groups rather than individual executable test cases.

## 32. Registry self-check

The submission's expected-ID registry must itself be validated.

At minimum the registry checker must reject:

- duplicate expected IDs;
- malformed ranges;
- result ID not known to the registry unless explicitly classified supplemental;
- missing family;
- expected-count mismatch;
- duplicate final result for one mandatory ID.

## 33. Final registry acceptance

The Windows v1.0 sibling named-ID layer is complete only when:

- all **322** named functional/validation/E2E/platform IDs PASS;
- all **11** named performance workloads execute and satisfy their applicable gates;
- all **5** stable-envelope fixtures pass;
- both mandatory CCD fixtures pass;
- the >=60 regression corpus passes;
- developer-tool self-tests pass;
- all seven audit groups PASS;
- the `windows_platform` release group PASS;
- Windows evidence `WIN-EV-01` through `WIN-EV-12` is complete;
- Golden aggregate is 12/12 PASS.

This registry is consumed by the final release gate and does not weaken any more-specific requirement elsewhere in the package.
