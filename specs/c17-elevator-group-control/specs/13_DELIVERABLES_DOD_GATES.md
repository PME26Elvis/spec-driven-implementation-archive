# 13 — Deliverables, Definition of Done, and Release Gates

## 1. Required Submission Contents

Final submission MUST contain:

- all C17 product source files;
- all C17 mandatory test source files;
- build metadata/instructions;
- implementation README;
- any implementation-authored additional test fixtures;
- the task-pack fixed acceptance/invalid inputs used, unmodified;
- a test runner executable target or documented test build/run command;
- acceptance/release evidence;
- any small standard build-system metadata required to compile.

Compiled binaries/object files are optional unless the reviewer specifically requests them; source is authoritative.

The submission MUST NOT require generated caches, IDE state, external databases, downloaded packages, or network access for required behavior.

## 2. Implementation README

Must document:

- implementation version;
- how to build product and tests;
- how to run mandatory tests;
- how to invoke every required CLI command;
- flat output-prefix semantics;
- source/module overview;
- numeric representation and floating comparison tolerance;
- analytic motion-profile implementation confirmation;
- PCG32 initialization/Box-Muller cache behavior;
- supported YAML subset/limits;
- algorithm parameter defaults;
- output file list;
- optional extensions, if any;
- known limitations that do not violate mandatory requirements;
- final release status COMPLETE or INCOMPLETE.

## 3. Fixed Task-Pack Inputs

The submission MUST retain or otherwise run against the exact normative files under:

```text
fixtures/acceptance/
fixtures/invalid/
fixtures/equivalence/
```

Task-pack acceptance files MUST NOT be edited to fit the implementation.

`fixtures/acceptance/SHA256SUMS.txt` is the integrity manifest for normative positive acceptance configs/traces. `fixtures/invalid/SHA256SUMS.txt` is the integrity manifest for the fixed A22/A23 negative corpus.

If repository layout moves the files, evidence MUST map each original task-pack path/hash to the actual tested copy.

## 4. Additional Implementation Fixtures

The implementer MUST additionally include whatever small direct unit fixtures are needed to prove internal behavior that cannot be expressed through public CLI state alone, including focused:

- parser tokens/trees;
- kinematic profile values;
- route/algorithm states;
- metric histories;
- allocation/error paths where practical.

These supplement but never replace the fixed task-pack acceptance corpus.

## 5. Evidence

Evidence exists to make human review easy; it does not replace tests.

Required evidence artifacts:

- mandatory test summary with pass/fail counts;
- positive acceptance and invalid-corpus SHA-256 verification statement/result;
- A01-A25 acceptance table;
- release-gate table G1-G15;
- representative A09 comparison text and CSV;
- common A09 trace fingerprint;
- A16 replay identity evidence;
- A24 stress summary;
- A25 determinism comparison result;
- list of optional extensions used;
- list of every mandatory gap/unexecuted item, if any.

Evidence MUST be produced from or verifiably derived from the delivered implementation and fixed inputs. Hand-writing PASS without underlying execution does not satisfy a gate.

Large raw A24 logs/CSV may be omitted from the final archive if size is impractical, provided exact fixed input, invocation, fingerprint, summary, and result evidence remain reproducible.

## 6. Definition of Done

The task is DONE only when:

- every mandatory normative requirement is implemented;
- every mandatory automated test passes;
- A01-A25 all pass;
- every release gate G1-G15 passes;
- fixed acceptance inputs are unmodified;
- no required behavior is a mock, placeholder, hard-coded result, disabled path, or external delegation.

Anything less is INCOMPLETE.

## 7. Release Gate G1 — Build and Dependency Boundary

PASS only if:

- product builds from delivered source as C17;
- mandatory tests build from delivered C17 source;
- required runtime behavior uses ISO C17 standard library only;
- no POSIX/Win32/X11/third-party/runtime parser/scripting dependency implements required behavior;
- product does not require directory creation/enumeration API;
- ordinary execution does not require network access.

Human review SHOULD inspect includes/link flags/source for prohibited dependencies in addition to running the program.

## 8. Release Gate G2 — Configuration and Text Parsers

PASS only if:

- JSON required grammar/escapes/surrogates work;
- uint64 integer precision rules work;
- YAML required subset works;
- unsupported YAML features reject;
- nesting limits work;
- duplicate/unknown/type/range rules work;
- UTF-8 and identifier restrictions pass, including the generic >=1 MiB decoded string/key capability and exact typed UTF-8 byte-length boundaries;
- equivalent JSON/YAML map identically.

## 9. Release Gate G3 — Deterministic Traffic and Trace

PASS only if:

- PCG32 exact vectors pass;
- uniform/sampler tests pass;
- all five generated traffic profiles pass focused tests;
- burst exact-count/bounds pass;
- scenario-wide 2,000,000-passenger ceiling and no-partial-output overflow behavior pass;
- same seed/config repeats trace within build;
- trace import/canonicalization/fingerprint pass;
- CRLF canonicalization and invalid-trace cases pass.

## 10. Release Gate G4 — Physical Simulation

PASS only if:

- fixed-step authoritative clock is used;
- analytic triangular/trapezoidal motion formulas pass;
- asymmetric accel/decel pass;
- speed/position/arrival quantization rules pass;
- active leg target is immutable;
- doors prohibit motion;
- door/dwell/transfer quantization pass;
- zero-duration closure passes;
- concurrent transfer lanes pass;
- capacity reservation never exceeds limit.

A constant floor-time or generic tick-position hack is FAIL.

## 11. Release Gate G5 — Passenger and Request Integrity

PASS only if:

- passenger state/ownership invariants pass;
- FIFO rules pass;
- hall activation/clear lifecycle passes;
- conventional owner stickiness, exact residual release at `DOOR_CLOSE_START`, and only-permitted reassignment/release conditions pass;
- residual demand after partial/full pickup persists;
- full-bypass once-per-episode rule passes;
- service-range mixed queue passes;
- completed/unserved accounting balances;
- no passenger loss/duplication/teleporting occurs.

## 12. Release Gate G6 — Seven Dispatch Algorithms

PASS only if all seven required algorithm IDs are real distinct implementations and all algorithm-specific tests pass.

Required observable focused cases include:

- nearest-car scoring/ties;
- directional collective turnaround behavior;
- LOOK endpoint/incremental-route behavior, including opposite-direction hall calls being deferred to the reverse sweep;
- ETA workload-vs-distance divergence using the normative frozen-demand predictor and conventional pre-boarding destination-visibility rule;
- zoning partition/fallback/overflow;
- adaptive exact window/mode/staging;
- destination-control preboarding grouping/multi-car same-hall behavior, sticky group assignment, and reservation-aware later grouping.

Seven names calling one shared baseline is FAIL.

## 13. Release Gate G7 — Starvation and Fairness

PASS only if:

- exact urgent boundary semantics pass;
- urgent activation logged once per episode;
- urgent work uses the fixed forced-first-uncommitted frozen-predictor selection/promotion rule;
- A14 completes old demand under sufficient drain;
- required fairness metrics, max hall age, P99, 2x-SLA counts, Gini are reported correctly.

## 14. Release Gate G8 — Metrics and Energy

PASS only if exact/synthetic metric tests prove:

- nearest-rank percentiles;
- population standard deviation;
- empty/one-observation rules;
- hard-stop/unserved SLA rule;
- Gini;
- histogram boundaries;
- utilization partition;
- load/movement/empty-loaded partition;
- startup/reversal/door/staging counters;
- passenger-meter;
- all energy component identities.

## 15. Release Gate G9 — CLI and Outputs

PASS only if:

- required commands/help/version work;
- exit-code classes match;
- output uses flat prefix and no runtime directory creation;
- exact required filenames and CSV headers match;
- JSON valid/complete;
- event log required classes/identifiers exist;
- destination assignments precede boarding;
- ISO C17 `wbx` multi-target reservation, cleanup, and `--force` semantics pass;
- output failure cannot masquerade as success;
- LF/UTF-8/numeric-format requirements pass.

## 16. Release Gate G10 — Replay and Compare Integrity

PASS only if:

- replay reproduces simulation-derived output from same canonical trace;
- compare prepares one common trace;
- algorithm state is fresh/isolated each run;
- child manifests share fingerprint/passenger count;
- comparison rows equal child summaries;
- compare stops/marks failure correctly if a child fails.

## 17. Release Gate G11 — Byte Determinism

PASS only if A25 triple-run comparison shows byte-identical canonical files for corresponding repeated operations.

Nondeterministic output order, metadata, pointer/hash iteration, or allocation-dependent tiebreak is FAIL.

## 18. Release Gate G12 — Error Handling

PASS only if:

- every fixed invalid corpus entry fails in expected broad class;
- malformed input never starts simulation;
- allocation/resource failures handled cleanly where reasonably testable;
- no crash/hang/NaN success;
- impossible state reaches invariant/deadlock failure rather than fabricated UNSERVED outcome.

## 19. Release Gate G13 — Stress and Scale

PASS only if A24 fixed 100,000-passenger, 100-floor, 16-car, seven-algorithm compare completes without:

- invariant/deadlock failure;
- crash/memory corruption;
- passenger-accounting mismatch;
- malformed/missing required output;
- NaN/Infinity;
- fingerprint mismatch between algorithms.

The 1,000,000-row importer test must also pass for parsing/canonicalization capacity.

No wall-clock speed threshold is imposed.

## 20. Release Gate G14 — Acceptance Input Integrity

PASS only if evidence verifies both:

- supplied normative positive acceptance config/trace files match `fixtures/acceptance/SHA256SUMS.txt`;
- supplied fixed negative-corpus files match `fixtures/invalid/SHA256SUMS.txt`.

Any edited, regenerated, downscaled, substituted, or omitted normative acceptance input is FAIL unless the task-pack maintainer issued a newer official task-pack revision.

Implementation-authored additional scenarios do not affect this gate.

## 21. Release Gate G15 — Full Acceptance Checklist

PASS only if:

- A01 through A25 all execute/pass as specified;
- every item in `12_ACCEPTANCE_SCENARIOS.md` manual checklist is PASS with supporting outputs/tests;
- mandatory automated test runner returns PASS;
- no gate relies solely on a completion claim without evidence.

## 22. Stop Conditions During Implementation

An implementer may stop and declare COMPLETE only after G1-G15 are PASS.

If an external constraint prevents completion, submission may stop as INCOMPLETE but MUST:

- identify the blocker;
- list every failed/unexecuted mandatory test/scenario/gate;
- avoid wording that implies full completion.

Partial work is acceptable as partial work; it is not DONE.

## 23. Prohibited Completion Claims

Examples that do not satisfy completion:

- "core features implemented" while policies/parsers/metrics are missing;
- "YAML conceptually supported" while only JSON parses;
- "ETA implemented" when it is geometric distance;
- "destination control" that assigns only after boarding;
- "physics simulated" with constant per-floor travel time;
- "tests pass" with supplied acceptance fixtures edited/downscaled;
- "stress skipped" without a real external blocker;
- "would pass on another machine" without execution evidence;
- manual report files substituted for runtime output;
- hidden POSIX/third-party dependency despite C source façade.

## 24. Optional Enhancements

Allowed only after mandatory behavior remains intact, examples:

- additional dispatch algorithms;
- extra traffic profiles;
- accessibility/passenger weights;
- maintenance/out-of-service cars;
- richer calibrated energy model;
- express/transfer elevator systems;
- optional visualization outside core product;
- optional platform-specific wrapper around the standard-C core.

Optional work MUST NOT change schema 1.0 acceptance semantics or replace any release gate.

## 25. Human Evaluation Guidance

Beyond binary conformance, a human may compare implementations by:

- release gates passed;
- failure diagnostics;
- architecture/readability;
- test depth beyond minimum;
- memory/performance behavior;
- number of revisions required;
- implementation time;
- token usage or other external process metrics.

These are reviewer observations, not product requirements.

## 26. Final Acceptance Statement

Completed evidence SHOULD end with a clear table:

```text
Gate  Status
G1    PASS
G2    PASS
...
G15   PASS

Mandatory tests: PASS
Acceptance A01-A25: PASS
Acceptance input hashes: PASS
Known mandatory gaps: NONE
Release status: COMPLETE
```

If any mandatory gap/gate/scenario remains, `Release status` MUST be `INCOMPLETE`.
