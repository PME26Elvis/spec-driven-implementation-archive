# Fixed Acceptance Inputs

These files are the normative A01-A25 inputs referenced by `specs/12_ACCEPTANCE_SCENARIOS.md`.

- Do not edit, downscale, regenerate, or substitute them when claiming release completion.
- Relative `traffic.trace_file` values assume the product is invoked with the project/task-pack root as process working directory.
- `SHA256SUMS.txt` hashes all normative JSON/YAML/CSV acceptance inputs except this README and the hash manifest itself.
- A16 and A25 intentionally reuse A09 inputs; A22/A23 use the fixed negative corpus in `fixtures/invalid/`.
- `parser_depth_128.json` and `parser_depth_128_yaml.yaml` are positive parser-depth fixtures used by A22; the paired depth-129 files are in the invalid corpus.

The 100,000-row A24 trace is intentionally shipped as explicit demand so every implementation receives exactly the same stress workload.
