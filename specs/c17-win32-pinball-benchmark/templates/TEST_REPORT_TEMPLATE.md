# Automated, Regression, Performance, and Reliability Test Report

Release candidate identifier: `<fill>`  
Build identifier/commit: `<fill>`  
Task package version: `1.0.0`

## Summary

| Domain | Passed | Failed | Skipped/Not Run | Total | Minimum |
|---|---:|---:|---:|---:|---:|
| Math / geometry / numeric | | | | | 35 |
| Physics / collision / CCD / invariants | | | | | 85 |
| Pinball mechanisms / Nudge / Tilt | | | | | 45 |
| Events / gameplay / scoring | | | | | 45 |
| Scene I/O / migration / recovery | | | | | 55 |
| Editor / history / clipboard / layers | | | | | 60 |
| Replay / headless / determinism / traces | | | | | 35 |
| UI focus / text / HiDPI / animation | | | | | 35 |
| Utilities / release evidence | | | | | 15 |
| Performance / resource / fault injection | | | | | 10 |
| **TOTAL** | | | | | **420** |

Mandatory meaningful case count: `<fill; must be >=420>`

## Canonical scenario results

| Scenario | Result | Golden/checkpoint/tolerance | Notes |
|---|---|---|---|
| Gravity Drop | NOT RUN | | |
| Perfect Bounce | NOT RUN | | |
| Friction Ramp | NOT RUN | | |
| High-Speed Thin Wall | NOT RUN | | |
| Flipper Strike | NOT RUN | | |
| Bumper Ring | NOT RUN | | |
| Sensor Crossing | NOT RUN | | |
| Eight-Ball Collision | NOT RUN | | |
| Drain Test | NOT RUN | | |
| Multiball Stress | NOT RUN | | |
| Stationary No Force | NOT RUN | | |
| Free Flight | NOT RUN | | |
| Elastic Head-On | NOT RUN | | |
| Official Reference Full Game | NOT RUN | | |

## Determinism

- identical-run repetitions: `<fill; >=10>`
- golden checkpoint result: `<fill>`
- GUI/headless equivalence: `<fill>`
- trace-on/trace-off equivalence: `<fill>`
- UI-scale equivalence: `<fill>`
- speed-multiplier same-step equivalence: `<fill>`
- Nudge/Tilt replay equivalence: `<fill>`
- `detcompare` deliberate-first-divergence test: `<fill>`

## Physics invariants

- stationary 10,000-step drift: `<fill>`
- free-flight velocity drift: `<fill>`
- equal-mass elastic momentum relative error: `<fill>`
- equal-mass elastic energy relative error: `<fill>`
- frictionless tangential preservation: `<fill>`

## Stress and numeric safety

- 16-ball / 30 simulated second result: `<fill>`
- 64-ball / 10 simulated second headless result: `<fill>`
- 1,000,000 fixed-step result: `<fill>`
- NaN/Inf count: `<fill>`
- out-of-world unexpected escapes: `<fill>`
- impact-budget diagnostics: `<fill>`
- event-budget diagnostics: `<fill>`

## Performance workloads

| Workload | Result | Wall time | Avg step | p95 step | Backlog drops | Notes |
|---|---|---:|---:|---:|---:|---|
| P1 Editor | NOT RUN | | | | N/A | |
| P2 Normal Play | NOT RUN | | | | | |
| P3 Headless Stress | NOT RUN | | | | N/A | |

## Resource stability

- cycle-10 stabilized RSS: `<fill>`
- cycle-100/final stabilized RSS: `<fill>`
- allowed bound: `<fill>`
- 1,000 file open/read/close descriptor result: `<fill>`
- trace-buffer cap result: `<fill>`
- Undo memory-cap result: `<fill>`
- Win32 USER/GDI transient-resource stability result: `<fill>`

## Persistence/recovery fault injection

| Fault stage / case | Result | Original bytes preserved | Dirty state preserved | Diagnostic |
|---|---|---|---|---|
| temp create | NOT RUN | | | |
| mid-write | NOT RUN | | | |
| flush/sync | NOT RUN | | | |
| temp close | NOT RUN | | | |
| rename/replace | NOT RUN | | | |
| no space | NOT RUN | | | |
| external change | NOT RUN | N/A | | |
| recovery crash/restart | NOT RUN | | | |

## Canonical E2E journey

J01–J24 result: `<fill>`  
Journey evidence index: `<path/reference>`

## Failures

For each failure include stable test ID, requirement ID(s), expected result, actual result, reproduction workflow, and affected Release Gate.

`<list>`


## Windows platform binding

- OS build/version: `<fill>`
- architecture: `<fill>`
- compiler/runtime linkage summary: `<fill>`
- imported non-system runtime files shipped: `<fill>`
- Per-Monitor v2 status: `<fill>`
- tested OS DPI values: `<fill>`
- Unicode path/clipboard/IME cases: `<fill>`
- headless HWND count/result: `<fill>`
- USER/GDI/HANDLE cycle result: `<fill>`
- prohibited native-control/rendering import audit: `<fill>`
