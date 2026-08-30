# 01 — Scope and Engineering Constraints

## 1. Product Scope

The product is an offline deterministic elevator-group simulator and algorithm comparison tool.

It models one building containing one bank of elevators. A scenario describes building geometry, elevator properties, passenger traffic, simulation duration, random seed, algorithm settings, metrics, and reporting settings.

One invocation may execute one dispatch algorithm or the configured comparison set over the same passenger demand.

The executable consumes command-line arguments and ordinary files and writes ordinary files. It does not require a GUI, network service, database server, interactive TUI, daemon, real hardware, or runtime directory-management API.

## 2. Mandatory Functional Areas

The implementation MUST contain real working implementations of all of the following:

1. JSON configuration parser.
2. YAML configuration parser for the required YAML subset.
3. Shared typed configuration model.
4. Configuration semantic validator.
5. Deterministic PCG32 pseudorandom number generator.
6. Required probability samplers.
7. Passenger trace generator.
8. Passenger trace importer/exporter.
9. Fixed-step simulation clock.
10. Elevator kinematic state update.
11. Door state machine.
12. Passenger waiting queues.
13. Boarding and alighting transfer-lane model.
14. Capacity and overload enforcement.
15. Hall-call and destination-request handling.
16. Seven required dispatch algorithms.
17. Per-algorithm isolated simulation state.
18. Metrics aggregation.
19. Percentile computation.
20. SLA and fairness computation.
21. Simplified energy/cost accounting.
22. Event logging.
23. Summary reporting.
24. CSV output.
25. JSON machine-readable manifest/summary output.
26. ASCII histogram output.
27. Deterministic trace replay.
28. Runtime invariant checks.
29. Automated tests.
30. Fixed acceptance-corpus execution and evidence.

## 3. C17 Requirement

All product source code and mandatory test source code MUST compile as C17.

The implementation MUST NOT require C++, Rust, Go, Java, C#, Python, JavaScript, Lua, Perl, Ruby, shell scripting, or another language at runtime.

Build-system metadata and reviewer-side command scripts are not product source code, but no required product behavior or mandatory test assertion may be delegated to them.

Compiler-specific language extensions MUST NOT be necessary for correctness.

## 4. ISO C17 Dependency Boundary

Required product behavior and mandatory tests may use only ISO C17 standard-library facilities.

Typical allowed headers include:

- `<assert.h>`
- `<ctype.h>`
- `<errno.h>`
- `<float.h>`
- `<inttypes.h>`
- `<limits.h>`
- `<math.h>`
- `<stdbool.h>`
- `<stddef.h>`
- `<stdint.h>`
- `<stdio.h>`
- `<stdlib.h>`
- `<string.h>`
- `<time.h>`

The exact set of standard headers used is implementation-defined.

The following are outside the product dependency boundary and therefore MUST NOT be required by product behavior:

- POSIX APIs such as `mkdir`, `opendir`, `mmap`, `fork`, or pthreads;
- Win32 APIs;
- X11/Wayland APIs;
- compiler runtime extensions used as a feature dependency;
- dynamic loading of non-standard libraries.

The ordinary C process entry point and `argc`/`argv` are permitted.

## 5. File-Output Portability Rule

ISO C17 does not define directory creation or directory enumeration. Therefore the executable MUST NOT require either operation.

For `run`, `compare`, and `replay`, `--out` is an **output prefix**. The program creates a fixed set of regular files by appending normative suffixes to that prefix.

Example:

```text
--out results/run01
```

may create:

```text
results/run01.summary.json
results/run01.passengers.csv
```

The parent directory, if any, MUST already exist. Creating parent directories is reviewer/build-harness responsibility and is not product behavior.

The executable MUST NOT recursively delete directories or inspect unrelated directory contents.

This rule is normative and exists specifically to keep the product implementable with ISO C17 file I/O alone.

## 6. Prohibited Dependencies and Delegation

The implementation MUST NOT use or require:

- SQLite or another database engine;
- a JSON parsing library;
- a YAML parsing library;
- a CSV library;
- a statistics library;
- a random-distribution library;
- a simulation framework;
- an optimization library;
- an elevator-control library;
- a command-line parsing library;
- a logging framework;
- a third-party unit-test framework for mandatory tests;
- shelling out to `jq`, `yq`, `awk`, `sed`, Python, Node.js, or similar tools to implement product behavior;
- remote APIs or network calls;
- generated/prerecorded result tables standing in for runtime computation.

The project MAY contain optional developer convenience scripts, but product execution and mandatory tests MUST remain fully functional without them.

## 7. No Substitute Implementations

The following substitutions are explicitly non-compliant:

- hard-coded result tables instead of simulation;
- prerecorded passenger traces only, with no generated-traffic implementation;
- using one dispatch policy under several algorithm names;
- changing only constant weights while claiming fundamentally distinct required algorithms unless this specification explicitly defines the policy as a weighted variant;
- treating every trip as constant-time regardless of distance, acceleration, speed, and timestep;
- ignoring door timing;
- ignoring passenger transfer timing;
- ignoring capacity;
- allowing passengers to teleport into or out of cars;
- calculating only mean waiting time while omitting required metrics;
- parsing JSON while treating YAML as JSON with another extension;
- parsing YAML/JSON/CSV by external utility;
- using `rand()` as the required simulation PRNG;
- regenerating passenger demand separately for algorithms during one comparison;
- reporting metrics not derived from actual simulation state;
- disabling invariant checks in normal acceptance runs;
- dropping failed/unserved passengers from accounting;
- editing supplied acceptance inputs to make tests easier;
- satisfying output requirements with hand-created evidence files not produced by the delivered program.

## 8. Determinism Requirement

For the same:

- executable build;
- semantic scenario content;
- seed or explicit passenger trace;
- algorithm;
- algorithm parameters;

all outputs designated deterministic MUST reproduce under repeated execution.

Wall-clock time, process ID, memory address, locale, filesystem enumeration order, pointer ordering, allocation order, or unrelated previous runs MUST NOT influence simulation decisions.

Equivalent JSON and YAML configurations MUST lead to the same parsed semantic model and, within one implementation build, the same generated trace and simulation outputs.

Cross-implementation byte identity is required only where the task pack supplies exact vectors, exact explicit traces, exact schemas, or exact formulas. Floating-point sampler results need not be bit-identical across different standard-library implementations unless an explicit acceptance fixture fixes the trace.

## 9. Time and Numeric Representation

Simulation time MUST be represented internally as integer microseconds or integer simulation ticks. Floating-point time accumulation is prohibited for the authoritative clock.

All timestamps written to trace/event/passenger outputs are integer microseconds.

Position, speed, acceleration, statistical aggregates, and algorithm cost calculations MAY use IEEE 754 floating point. An implementation MAY instead use fixed-point arithmetic.

Regardless of representation:

- comparison/tie decisions MUST use stable documented rules;
- NaN or Infinity MUST never be accepted from configuration or emitted as successful numeric output;
- output rounding follows `08_OUTPUT_FORMATS.md`;
- integer conversions and size calculations MUST be overflow-checked.

## 10. Required Capacity

The implementation MUST support at minimum:

- 2 to 200 floors;
- 1 to 32 elevators;
- 0 to 2,000,000 passengers in one generated or imported trace;
- configured simulation duration from 1 second through 7 days;
- required timestep values defined in `03_SIMULATION_ENGINE.md`;
- per-car capacity from 1 to 100 persons;
- at least 64 generated traffic segments in one scenario;
- generic JSON/YAML decoded strings and keys of at least 1,048,576 UTF-8 bytes when allocation succeeds;
- schema-level UTF-8 scenario, description, segment, elevator-ID, and configured-path fields up to their exact limits in `06_CONFIG_JSON_YAML.md`.

The supported canonical passenger-trace ceiling is exactly **2,000,000 passengers per scenario trace**, regardless of whether demand is imported or generated from one or many segments. The implementation MUST NOT silently truncate, wrap passenger IDs, emit a partial successful trace, or continue simulation with only a prefix of the demand. Exact pre-validation and runtime-overflow behavior is defined in `04_TRAFFIC_GENERATION.md`, `06_CONFIG_JSON_YAML.md`, and `10_ERRORS_EDGE_CASES.md`.

The program MUST fail cleanly if allocation or representational limits prevent a requested run.

No real-time performance requirement exists. The simulator may run faster or slower than simulated time.

## 11. Execution Model

Required behavior is single-process and single-threaded.

`compare` MUST execute algorithm runs sequentially in `algorithms.default_compare` order.

Optional parallel execution is outside v1.0.2 scope because it would introduce additional platform/library dependencies and nondeterminism risks.

## 12. Source Organization

The specification does not mandate exact filenames, but the implementation SHOULD separate at least these concerns:

- configuration model/validation;
- JSON lexer/parser;
- YAML lexer/parser;
- UTF-8 validation;
- PRNG/sampling;
- passenger trace CSV;
- simulation engine;
- elevator/door/transfer model;
- dispatch-policy interface;
- individual algorithms;
- metrics;
- reporting;
- CLI;
- tests.

A monolithic source file is not automatically non-compliant, but architecture/readability may be considered during human review.

## 13. Compatibility Goal

The grading target is observable behavior and conformance, not a particular operating system.

The product MUST use ordinary C standard file I/O and MUST NOT assume ANSI terminal capabilities, GUI availability, network access, directory APIs, or a particular path separator.

Input and output paths are opaque byte strings supplied by the process. Relative paths are interpreted by the C runtime relative to the process working directory.
