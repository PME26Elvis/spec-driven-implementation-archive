# 25 — Performance, Memory, and Resource Stability Requirements

These requirements establish a practical quality floor rather than a hardware leaderboard. Implementations may be faster, but MAY NOT trade correctness/determinism for benchmark numbers.

## 1. Reference workload classes

Three normative workload classes are defined.

### P1 — Interactive editor

- 500 authored objects;
- 50 events;
- 200 total event actions;
- 8 layers;
- representative mixture of all 15 object types;
- no active simulation.

### P2 — Normal play

- 200 static/active authored objects;
- 30 Sensors/Rollovers;
- 16 active balls;
- 2 flippers;
- event rate under 200 actions/s simulated;
- debug overlays off.

### P3 — Headless stress

- 500 authored collidable objects;
- 64 active balls;
- 100 Sensors/Rollovers;
- 30 simulated seconds;
- event rate under action budget.

## 2. Correctness priority

Performance tests are run only after correctness gates pass. A result produced by skipped collision candidates, reduced solver iterations, disabled ball-ball collision, altered fixed timestep, or disabled events is invalid.

## 3. Editor responsiveness

On the evaluation machine, P1 SHALL:

- complete selection click feedback within 100 ms wall time at least 95% of sampled interactions;
- update move drag visuals without multi-second stalls;
- complete Undo/Redo of a single normal transform within 250 ms;
- validate scene within 2 s;
- canonical Save within 2 s excluding deliberately slow/failing storage.

These thresholds are loose quality floors, not comparative score normalization.

## 4. Simulation throughput

P2 SHALL sustain production fixed-step processing fast enough that 1x real-time simulation does not accumulate persistent backlog on the evaluation machine during a 30-second run.

Evidence SHALL report:

- simulated fixed steps;
- wall duration;
- dropped backlog counter;
- average and p95 fixed-step compute time.

Required PASS: dropped-backlog counter remains zero under P2 after initial warmup.

## 5. Render responsiveness

Under P2 in a visible normal-size window:

- UI remains responsive to Pause within 250 ms wall time;
- no input starvation longer than 500 ms;
- rendering may drop frames but SHALL not change simulation result;
- simulation controls remain visually updated.

## 6. Headless throughput sanity

P3 has no fixed universal steps/s threshold because evaluation hardware varies. However, completion wall time SHALL be reported and run SHALL complete without pathological superlinear behavior such as effectively hanging at valid counts.

A watchdog limit of 120 s on the evaluation environment MAY be used by acceptance harness. Timeout is FAIL unless environment issue is demonstrated.

## 7. Broad-phase expectation

The implementation MAY use brute-force collision candidate generation for small scenes but SHALL remain within P2/P3 usability gates. If brute force causes gate failure, a broad-phase acceleration structure is required by outcome.

No specific tree/grid algorithm is mandated.

## 8. Memory baseline measurement

Release evidence SHALL report resident memory or a platform-available equivalent at:

- after application startup/new scene idle;
- after opening P1 scene;
- after 30 s P2 play;
- after returning to Edit;
- after repeated-cycle test.

Exact allocator overhead is implementation-defined.

## 9. Repeated-cycle leak test

Perform 100 cycles:

1. open canonical full reference table;
2. Play for 1 simulated second;
3. Stop to Edit;
4. perform 10 Undoable edits then Undo all;
5. open and close required modal 10 times;
6. close scene/new scene as applicable.

After warmup cycle 10, resident memory sampled after GC-equivalent is not applicable in C; therefore raw process memory after idle stabilization SHALL not exhibit monotonic unbounded growth.

PASS criterion: final stabilized resident memory <= max(1.20 * cycle-10 baseline, cycle-10 baseline + 64 MiB), excluding OS/file-cache effects explicitly measured outside process RSS.

## 10. Trace-buffer bounds

Event/collision trace buffers SHALL obey documented caps. Clearing or rotating them must release/reuse storage so event-heavy runs do not grow memory indefinitely.

## 11. Undo memory bounds

Undo history SHALL have an explicit memory/count cap.

At minimum:

- retain 100 normal commands;
- default maximum history memory 128 MiB;
- if memory cap is reached, discard oldest complete transactions only;
- never discard the current command halfway;
- saved-checkpoint dirty-state logic remains correct even when its historic command is evicted; if exact semantic equivalence to save checkpoint can no longer be inferred from history marker, use semantic fingerprint comparison.

## 12. Clipboard bounds

Clipboard payload may contain at most the same object/event limits as a scene but paste still enforces destination limits atomically.

Replacing clipboard releases prior payload ownership safely.

## 13. File descriptor/resource stability

Repeated open/save/replay/trace exports SHALL not leak file descriptors.

Mandatory test runs 1,000 open/read/close cycles on a small fixture and confirms open descriptor count returns to baseline within a bounded tolerance for unrelated runtime descriptors.

## 14. X11 resource stability

Repeated modal/popup/resize actions SHALL not create unbounded X11 resources. Since required UI is software-drawn in the application window, creation of a new native child window per transient frame/animation is prohibited.

## 15. Thread policy

Multithreading is optional. If used:

- deterministic simulation state mutation SHALL have a single deterministic commit order;
- races may not affect replay fingerprints;
- UI thread must not consume partially mutated physics state;
- shutdown joins/stops worker threads safely.

Performance gates do not require threads.

## 16. Performance report

`TEST_REPORT.md` or a referenced machine-readable report SHALL record for P1/P2/P3:

- application version;
- scene fingerprint;
- machine CPU count/model when available;
- fixed timestep;
- object/ball/event counts;
- wall duration;
- simulated duration;
- average/p95 step time for P2/P3;
- memory samples;
- backlog drops;
- pass/fail.

This environment information is evidence metadata, not an implementation-environment requirement.
