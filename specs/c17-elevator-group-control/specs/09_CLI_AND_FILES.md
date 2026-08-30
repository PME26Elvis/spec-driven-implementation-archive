# 09 — CLI and File Behavior

## 1. Philosophy

The CLI is intentionally small and fully non-interactive.

It exists to select an operation, scenario, algorithm, trace, and output prefix. Simulation data belongs in files.

No ANSI control sequences, terminal cursor manipulation, menus, prompts, or manual simulation stepping are required.

## 2. Exit Codes

Normative process exit codes:

- `0`: success;
- `2`: command-line usage error;
- `3`: configuration syntax / unsupported YAML feature / malformed UTF-8 syntax input;
- `4`: configuration semantic validation error;
- `5`: trace CSV syntax or trace semantic error;
- `6`: simulation invariant or deadlock error;
- `7`: output I/O or output-collision error;
- `8`: internal/resource/allocation/representational error.

A single failure is reported using the most direct category encountered before simulation starts. The program need not combine multiple category codes.

## 3. Common CLI Parsing Rules

Command/option names are case-sensitive.

Required option values are the next argv element and may contain spaces when the invoking environment passes them as one argument.

For a command:

- required options may appear in any order after the scenario positional argument;
- an option may appear at most once;
- unknown option -> exit 2;
- missing option value -> exit 2;
- unexpected extra positional argument -> exit 2.

`--force` is a flag with no value.

The program MUST NOT parse a single argv string by re-invoking a shell.

## 4. Standard Streams

Success stdout MUST be concise and MUST NOT contain bulk simulation CSV/log data.

At minimum it identifies success and the exact output file/prefix where applicable.

Failures write at least one concise diagnostic to stderr.

Diagnostics may also list additional independent validation errors.

## 5. `validate`

```text
elevsim validate <scenario>
```

No other options are required or permitted except command help.

Behavior:

1. select parser by extension;
2. parse syntax/UTF-8;
3. map typed schema/defaults;
4. perform all semantic/cross-field validation possible without simulation;
5. if traffic mode is `trace`, open and fully validate/canonicalize the referenced trace in memory/streaming validation without writing output;
6. print concise success and exit 0.

It writes no product output files.

## 6. `generate`

```text
elevsim generate <scenario> --out <trace.csv> [--force]
```

Valid only when `traffic.mode=generated`; using it with trace mode is semantic error 4.

Success MUST create exactly:

```text
<trace.csv>
<trace.csv>.info.json
```

plus no other required files.

Without `--force`, reserve both targets with the normative `fopen(..., "wbx")` multi-target procedure in `08_OUTPUT_FORMATS.md`; any collision/reservation failure is exit 7 before generation begins.

With `--force`, overwrite only those two exact targets.

## 7. `run`

```text
elevsim run <scenario> --algorithm <algorithm-id> --out <output-prefix> [--force]
```

Behavior:

1. validate config/trace mode;
2. validate required algorithm ID;
3. derive all enabled output filenames from prefix;
4. perform collision checks;
5. generate/import canonical trace once;
6. initialize fresh simulation/policy state;
7. run exactly one selected algorithm;
8. emit files from `08_OUTPUT_FORMATS.md`.

`--out` MUST be non-empty and is never treated as a directory.

## 8. `compare`

```text
elevsim compare <scenario> --out <output-prefix> [--force]
```

Runs each unique algorithm in `algorithms.default_compare` sequentially in listed order.

It MUST prepare one canonical trace before first algorithm.

Before starting first algorithm it MUST derive/check all enabled common and algorithm-specific output filenames.

If an algorithm fails:

- stop before starting later algorithms;
- overall exit nonzero using underlying failure class;
- successful earlier output groups may remain diagnostic;
- any written comparison text/JSON-like diagnostic MUST visibly indicate incomplete/failed status;
- no missing algorithm may be represented as successful zero metrics.

Task-pack acceptance compare contains all seven required algorithms.

## 9. `replay`

```text
elevsim replay <scenario> --trace <trace.csv> --algorithm <algorithm-id> --out <output-prefix> [--force]
```

Replay demand comes exclusively from explicit `--trace`, regardless of scenario's generated segment definitions.

Building, cars, simulation, algorithms, metrics, and output settings still come from scenario.

The supplied trace is validated against that scenario's building/service/tick/duration rules.

A replay of a canonical generated trace under identical scenario/algorithm MUST reproduce run simulation outputs. `run_kind` in manifest is expected to differ, so manifest byte identity is not required between `run` and `replay`; all simulation-derived child data/metrics/events must match.

## 10. `--help`

Required:

```text
elevsim --help
elevsim validate --help
elevsim generate --help
elevsim run --help
elevsim compare --help
elevsim replay --help
```

Help exits 0 and lists the exact supported command form/options.

`--help` does not require localization.

## 11. `--version`

```text
elevsim --version
```

Exits 0 and prints a non-empty implementation version string on one line.

Version content is implementation-owned and is not part of canonical simulation files.

## 12. Paths

All scenario, trace, and output path strings are opaque argv/config byte strings passed to standard C file functions.

The product does not normalize separators, expand `~`, expand environment variables, glob wildcards, resolve symlinks, or construct directories.

Relative paths are relative to process working directory.

Task-pack acceptance commands are defined from the task-pack/repository root so their fixture paths are unambiguous.

## 13. Output Prefix

For run/compare/replay, the exact prefix is concatenated with the exact suffixes in `08_OUTPUT_FORMATS.md`.

The product MUST NOT automatically append a path separator or create a child directory.

Example:

```text
--out evidence/a09
```

requires parent `evidence` to have been created outside the product and produces `evidence/a09.trace.csv`, `evidence/a09.eta_cost.summary.json`, etc.

## 14. Locale Independence

Configuration/trace numeric grammar is defined by the task pack, not locale.

Output uses `.` decimal point.

If the implementation calls `setlocale`, it MUST ensure numeric parsing/formatting remains C-locale compatible. The safest compliant behavior is not to enable locale-sensitive numeric formatting.

UTF-8 validation/manipulation is byte/code-point logic and MUST NOT rely on locale-specific multibyte conversion state.

## 15. No Interactive Runtime Input

`validate`, `generate`, `run`, `compare`, and `replay` MUST NOT ask the user to:

- choose a car;
- approve an overwrite;
- press a key;
- provide additional traffic;
- manually advance time;
- select an algorithm interactively.

Overwrite behavior is controlled only by `--force`.
