# Release Gate Matrix

| Gate | Verdict | Primary reason |
|---|---|---|
| Build | **FAIL** | Builds, but strict quality gate is undermined by 17 warnings and an ASan-confirmed heap overflow. |
| Algorithms | **PARTIAL / FAIL gate** | Core smoke vectors pass, but mandatory algorithm golden breadth, malformed cases, CDC boundary vectors, Robin Hood collision tests, etc. are missing. |
| Repository format | **FAIL** | Canonical format diverges from specification in multiple fields; persistent index is unsorted/no checksum; authoritative/derived semantics incomplete. |
| Snapshot | **FAIL** | No default HEAD parent, no race retry, no include/exclude behavior, special files silently ignored, root metadata/profile handling incomplete, no scan-cache fast path. |
| Incremental/dedup | **FAIL** | Storage dedup exists, but fast-path incrementality is absent and hardlink/duplicate semantics are incorrect. |
| Diff/reporting | **FAIL** | Minimal A/D/M only; no P/T/H, metadata deltas, chunk reuse, subtree filter, required NDJSON/SVG diff, escaping guarantees. |
| Restore | **FAIL** | Partial restore ignored; overwrite semantics wrong; full-file hash not verified; unsafe/fixed path handling; independent duplicates become hardlinks. |
| Integrity/recovery | **FAIL** | Full verify does not walk reachable graph; live missing FILE may go undetected; parity/repair semantics incomplete. |
| Crash safety | **FAIL** | Journal lacks required stage/intended-state data; recovery is cleanup-only; required crash checkpoints absent. |
| Garbage collection | **FAIL** | Source explicitly states full recursive mark-and-sweep is not implemented when refs remain. |
| Configuration | **FAIL** | Parser/schema/precedence/normalization requirements are largely unmet; important runtime config is ignored. |
| CLI/output | **FAIL** | Stable errors/help/output formats differ; verify lacks JSON/NDJSON/SVG; diff lacks required formats and presentation. |
| Tests | **FAIL** | Mandatory acceptance catalog not mapped/implemented; stress sizes far below contract; `make test` omits catalog/stress. |
| Documentation | **FAIL** | README claims many gates PASS despite source/test evidence and simultaneously admits mandatory partials; repository-format documentation is too small to match the normative contract. |
| Overall DoD / Stop Condition | **FAIL** | Multiple MUST requirements and release gates remain open. |

## Important interpretation

A gate marked FAIL does not mean every implementation detail in that area is absent. It means at least one mandatory condition for that gate is not satisfied, so the gate cannot legally be closed under the task pack.
