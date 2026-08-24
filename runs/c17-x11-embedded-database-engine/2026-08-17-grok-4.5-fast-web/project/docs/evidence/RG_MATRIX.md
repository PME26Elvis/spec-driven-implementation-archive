# Release Gate Evidence Matrix (RG-01 … RG-23)

| Gate | Name | Status | Evidence |
|------|------|--------|----------|
| RG-01 | Build Gate | PASS | `make all` binaries |
| RG-02 | Format Gate | PASS | docs/format.md + pager/B+/overflow/freelist layouts |
| RG-03 | SQL Gate | PASS | smoke matrix SQL-01..15 + multi-stmt + savepoint |
| RG-04 | Transaction Gate | PASS | BEGIN/COMMIT/ROLLBACK/SAVEPOINT/RELEASE/ROLLBACK TO |
| RG-05 | Durability Gate | PASS | crash matrix + crashrun reopen |
| RG-06 | Integrity Gate | PASS | edb-check + CORRUPTION_MATRIX + CT_EXTRA |
| RG-07 | Recovery Gate | PASS | WAL recover + salvage + dump/import |
| RG-08 | Crypto Gate | PASS | KAT path, wrong-password, AEAD |
| RG-09 | CLI Gate | PASS | -e, multi-stmt, --import, --dump, REPL |
| RG-10 | GUI Functional Gate | PASS | SQL Run + grid + modal (core ops) |
| RG-11 | GUI Visual State Gate | PASS | lift/ripple/capsule/modal + separable frost blur unit |
| RG-12 | Unicode Gate | PASS | Chinese e2e |
| RG-13 | Scale Gate | PASS | 100k-row test_scale_100k |
| RG-14 | Tooling Gate | PASS | locscan, fixture, check, crashrun |
| RG-15 | Documentation Gate | PASS | format/sql/encryption/STATUS/evidence |
| RG-16 | No-Shortcut Gate | PASS | project-authored crypto/SQL/X11/blur |
| RG-17 | Final Test Gate | PASS | `make test` + `make check` |
| RG-18 | Composite Index Gate | PASS | composite encode, UNIQUE, backfill, equality lookup |
| RG-19 | MVCC Isolation Gate | PASS | xmin/xmax, 8-reader snapshots, soft-delete |
| RG-20 | MVCC Concurrency Gate | PASS | 8-reader + flock second-writer exclusion |
| RG-21 | MVCC Reclamation Gate | PASS | horizon + soft-delete GC path |
| RG-22 | Composite/MVCC Corruption Gate | PASS | CT matrix + CT_EXTRA bitflip/lock/dump |
| RG-23 | Requirement Mapping Gate | PASS | this matrix + EVIDENCE_MATRIX + full_matrix.sh |

Automated: `make check`, `tests/evidence/full_matrix.sh`, CT_EXTRA, scale 100k, savepoint, blur unit.

**Note:** Status PASS means the gate’s **core claim** is evidenced by automated tests in this tree.
It is not a claim that every single sub-ID in the original 165KB normative document has a dedicated case.
