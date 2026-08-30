# C17 Elevator Group Control Simulator — Task Pack v1.0.2

## Purpose

This task pack defines one complete software implementation assignment: a deterministic multi-elevator simulation and dispatch-comparison system written in C17.

The package is intentionally independent of agent frameworks, tool-calling systems, IDEs, build harnesses, operating-system distributions, and development workflows. It defines only the product behavior, engineering constraints, data contracts, simulation semantics, required algorithms, tests, evidence, Definition of Done, and release gates.

The intended use is to give the same assignment to different implementers and compare how completely and correctly they satisfy the same fixed requirements.

## Product Summary

The deliverable is a non-interactive command-line program that simulates passenger traffic in a multi-floor building served by a bank of elevators. It MUST:

- parse equivalent JSON and YAML scenario files using parsers implemented in C17;
- generate deterministic passenger demand from a specified seed or load an explicit trace;
- replay one fixed passenger trace against several dispatch algorithms;
- model fixed-step elevator kinematics, capacity, doors, boarding, alighting, and concurrent transfer lanes;
- implement seven required dispatch strategies from first principles;
- collect passenger, elevator, fairness, SLA, movement, and simplified energy metrics;
- emit deterministic machine-readable and human-readable files;
- validate input and enforce runtime invariants;
- include automated tests and execute the fixed acceptance corpus shipped with this task pack.

There is no graphical UI and no interactive terminal UI requirement.

## Language and Dependency Boundary

Product source code and mandatory test source code MUST use **C17** and the **ISO C17 standard library only**.

No POSIX, Win32, X11, filesystem-directory API, third-party parser, test framework, simulation framework, database, scripting runtime, external command, or network service may implement required product behavior.

Because ISO C17 has no portable directory-creation API, runtime output deliberately uses a **flat output-prefix contract** rather than requiring the executable to create directories. Parent directories, if desired by the reviewer, are created outside the product.

See `specs/01_SCOPE_CONSTRAINTS.md` for the normative dependency rules.

## Required Commands

The executable name used throughout the specification is `elevsim`. A platform-specific executable suffix is acceptable where unavoidable.

```text
elevsim validate <scenario.json|scenario.yaml>
elevsim generate <scenario.json|scenario.yaml> --out <trace.csv>
elevsim run <scenario.json|scenario.yaml> --algorithm <algorithm-id> --out <output-prefix>
elevsim compare <scenario.json|scenario.yaml> --out <output-prefix>
elevsim replay <scenario.json|scenario.yaml> --trace <trace.csv> --algorithm <algorithm-id> --out <output-prefix>
```

`--out` for `run`, `compare`, and `replay` is a filename prefix, not a directory. Exact file naming and overwrite rules are defined in `specs/08_OUTPUT_FORMATS.md` and `specs/09_CLI_AND_FILES.md`.

## Reading Order

1. `specs/01_SCOPE_CONSTRAINTS.md`
2. `specs/02_DOMAIN_MODEL.md`
3. `specs/03_SIMULATION_ENGINE.md`
4. `specs/04_TRAFFIC_GENERATION.md`
5. `specs/05_DISPATCH_ALGORITHMS.md`
6. `specs/06_CONFIG_JSON_YAML.md`
7. `specs/07_METRICS_AND_ENERGY.md`
8. `specs/08_OUTPUT_FORMATS.md`
9. `specs/09_CLI_AND_FILES.md`
10. `specs/10_ERRORS_EDGE_CASES.md`
11. `specs/11_TEST_PLAN.md`
12. `specs/12_ACCEPTANCE_SCENARIOS.md`
13. `specs/13_DELIVERABLES_DOD_GATES.md`

## Fixed Acceptance Inputs

`fixtures/acceptance/` contains the normative acceptance inputs referenced by `12_ACCEPTANCE_SCENARIOS.md`.

An implementer MAY add its own tests and fixtures, but MUST NOT replace, weaken, edit, or omit the task-pack acceptance inputs when claiming completion. Evidence must record the SHA-256 hashes of the acceptance input files used, as described in the release-gate specification.

`fixtures/invalid/` contains a fixed negative corpus with its own `SHA256SUMS.txt`. Additional negative tests are encouraged but do not replace the supplied corpus.

## Normative Language

The words **MUST**, **MUST NOT**, **REQUIRED**, **SHALL**, and **SHALL NOT** describe mandatory requirements.

The words **SHOULD** and **RECOMMENDED** describe strong recommendations that may be deviated from only when mandatory behavior remains compliant.

Where this package fixes an exact ordering, formula, identifier, unit, file schema, or test input, an equivalent-looking alternative is not automatically acceptable.

## Scope Principle

This is one project. There is no separate dev-tools project.

All required engineering effort is directed toward the elevator simulator, physical state model, dispatch algorithms, configuration parsing, deterministic traffic generation, trace/replay behavior, reporting, validation, and tests.


## Package Integrity

Root `SHA256SUMS.txt` hashes every distributed task-pack file except itself. The two fixture-level manifests remain the normative acceptance-input integrity lists used by release gate G14; the root manifest is an additional packaging-integrity aid and is not a required feature of the C product.
