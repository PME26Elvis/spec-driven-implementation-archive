# Elevator Group Control Simulator — C17 Implementation

Implementation version: **1.0.2-impl.2**  
Task-pack contract: **elevator_group_control_taskpack_v1.0.2**

## Build

Requirements: an ISO C17 compiler and the ISO C math library. Required product/test behavior has no third-party, POSIX, Win32, X11, network, database, parser-library, or scripting-runtime dependency.

```sh
make
make test
```

The default build is deliberately strict:

```text
-O2 -std=c17 -Wall -Wextra -Wpedantic -Werror
```

`make test` builds `tests/test_elevsim` from C17 source and runs the mandatory implementation-authored unit/integration harness.

## Required CLI

```text
./elevsim validate <scenario.json|scenario.yaml>
./elevsim generate <scenario.json|scenario.yaml> --out <trace.csv> [--force]
./elevsim run <scenario.json|scenario.yaml> --algorithm <algorithm-id> --out <prefix> [--force]
./elevsim compare <scenario.json|scenario.yaml> --out <prefix> [--force]
./elevsim replay <scenario.json|scenario.yaml> --trace <trace.csv> --algorithm <algorithm-id> --out <prefix> [--force]
./elevsim --help
./elevsim --version
```

Algorithm IDs:

```text
nearest_car
directional_collective
scan_look
eta_cost
zoning
adaptive_peak
destination_control
```

## Flat output-prefix semantics

`--out` is a filename prefix, never a directory request. The program does not create directories. Parent directories must already exist.

Before a multi-file operation starts, required targets are reserved using ISO C17 `fopen(path, "wbx")`. On reservation failure the operation fails rather than partially overwriting unrelated output. `--force` permits replacement according to the task-pack contract.

Depending on command/configuration, canonical files use the prefix with these suffixes:

```text
.trace.csv
.trace_info.json
.manifest.json
.summary.json
.summary.txt
.passengers.csv
.elevator_samples.csv
.events.log
.wait_histogram.txt
.comparison.csv
.comparison.txt
```

`compare` emits one common trace/trace-info pair plus isolated per-algorithm child outputs.

## Source layout

- `include/elevsim.h` — shared typed domain/config/simulation/output interfaces.
- `src/parse.c` — from-scratch JSON parser and required YAML subset, common value tree, UTF-8 handling, duplicate-key/nesting/resource checks.
- `src/config.c` — typed schema/default/range/serviceability validation.
- `src/trace.c` — PCG32, deterministic samplers/generation, strict CSV importer, canonicalization, FNV-1a fingerprint.
- `src/sim.c` — authoritative fixed-step state engine, analytic motion, transfer lanes, queues, seven dispatch algorithms, starvation, metrics state.
- `src/output.c` — deterministic canonical output writers and comparison reports.
- `src/main.c` — CLI parsing, command orchestration, output reservation/error classes.
- `src/util.c` — allocation/error/string helpers, UTF-8 utilities, analytic motion helper, numeric utilities.
- `tests/test_main.c` — C17 mandatory test harness.
- `fixtures/acceptance`, `fixtures/invalid`, `fixtures/equivalence` — unmodified task-pack normative fixtures.
- `evidence/final` — reproducible test/acceptance/release evidence.

## Numeric representation and comparison

Authoritative simulation time is integer microseconds on the configured fixed tick. Duration/timing quantities are quantized by integer ceiling to tick boundaries where required. Counts/IDs use fixed-width integer types. Physical position/speed, scores, metrics and energy use `double`; deterministic score/tie comparisons use the implementation's small relative/absolute `es_nearly_equal` tolerance rather than arbitrary formatted-string comparison. Canonical output applies the task-pack fixed numeric formatting/rounding rules.

## Analytic motion profile

Movement is not constant floor-time and does not advance position by a generic per-tick approximation. Each leg uses the task-pack analytic asymmetric acceleration/deceleration profile, selecting triangular or trapezoidal motion from distance, `vmax`, acceleration and deceleration. The analytic leg duration is ceiling-quantized to the fixed tick; samples evaluate the closed-form trajectory and arrival snaps exactly to the target floor. The active leg target is immutable.

## PCG32 and normal sampling

Traffic generation uses the specified PCG32 state transition/output and deterministic initialization. Uniform bounded draws use rejection rather than modulo bias. Exponential/Poisson/weighted/truncated-normal generation is implemented in C17. Box–Muller normal generation keeps the second variate in an explicit deterministic cache; cache state is part of one generation stream and is reset with generator initialization. Exact PCG vectors and sampler properties are covered by mandatory tests.

## JSON/YAML support and limits

JSON supports the task-pack required grammar, escapes, Unicode `\u` decoding including surrogate pairs, lexical integer precision and duplicate decoded-key rejection. Invalid UTF-8, decoded NUL, malformed numbers/escapes and unsupported syntax reject before simulation.

YAML intentionally implements only the task-pack schema-1.0 subset: indentation maps/sequences, required quoted/plain scalars, comments and allowed flow forms. Anchors/aliases, tags, document markers, block scalars, tab indentation and other unsupported features reject. YAML core-looking tokens such as `yes/no/on/off` and dates follow the frozen task-pack resolution rules rather than delegating to a general YAML library.

Maximum nesting depth is 128. Generic decoded strings/keys support at least 1 MiB; typed identifiers then apply their exact UTF-8 byte limits in schema validation.

## Default algorithm parameters

Unless overridden by the scenario:

- nearest car: away penalty 100 m; opposite-direction penalty 50 m.
- ETA: `w_wait=1`, `w_detour=.35`, `w_stops=4`, `w_load=20`, `w_reverse=8`, `w_fairness=.15`.
- zoning: static-equal zones; overflow wait 45 s.
- adaptive: window 300 s, minimum hold 120 s, up/down/interfloor fractions `.55`, minimum rate 5/min, lobby reserve `.5`.
- destination control: max group 16, route-similarity floor span 5, `w_pickup=1`, `w_route=.35`, `w_dest_stops=4`, `w_load=20`, `w_age=.15`.
- default starvation threshold 120 s; default max drain 3600 s; default deadlock window 300 s when omitted by a valid schema location.

## Determinism/replay

Generated/imported traces are canonicalized before simulation and fingerprinted with 64-bit FNV-1a. Replay consumes the canonical explicit trace. Compare clones fresh simulation state for each algorithm and uses the one common canonical trace. No pointer address, hash-table iteration order, wall clock, locale, network state or filesystem enumeration participates in deterministic outputs.

## Known limitations / optional extensions

No optional extensions are required or used. The implementation intentionally does not support YAML features outside the frozen mandatory subset and does not create output directories, both by specification.

## Release status

**COMPLETE.**

Final executed evidence records: strict C17 build PASS; mandatory C17 tests **92/92 PASS**; fixed negative corpus **44/44 PASS**; positive and invalid fixture SHA-256 integrity **45/45 + 45/45**; acceptance **A01–A25 ALL PASS**; Release Gates **G1–G15 ALL PASS**; official A24 100,000-passenger, 100-floor, 16-car, seven-algorithm compare PASS; and A25 byte determinism PASS.

See `evidence/final/RELEASE_STATUS.md` and the final evidence files referenced there.
