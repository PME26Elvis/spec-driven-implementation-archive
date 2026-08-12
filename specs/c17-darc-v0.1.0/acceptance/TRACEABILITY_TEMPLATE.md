# Acceptance Traceability Template

The implementer must copy/adapt this file into the delivered project and map every mandatory `TEST_CATALOG.md` ID.

| Catalog ID | Test file | Test function/case | Status | Notes |
|---|---|---|---|---|
| ALG-001 |  |  | TODO |  |

Rules:

- one catalog ID may map to multiple concrete tests;
- one concrete test may cover multiple IDs only when assertions independently prove each behavior;
- every mandatory ID must end in PASS before completion;
- `SKIP`, `XFAIL`, `TODO`, and blank are not PASS;
- failures discovered outside the catalog must also be tracked and fixed before completion if they violate a MUST/MUST NOT requirement.

The final matrix must include all current catalog IDs, including `DEL-001` through `DEL-006`.
