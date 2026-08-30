# Repomix authored-corpus review

Measured project: `project/darc_gates_closed_v0.1/darc`

Reviewed exclusion:

- `bin/**` — contains the compiled `darc` executable. The archived tree has no other build/output directory that needs a speculative exclusion.

Retained: source, headers, tests, test scripts, documentation, Makefile, and the authored JSON/YAML example configs.

Run from this recorded-run directory:

```bash
npx --yes repomix@1.18.0 project/darc_gates_closed_v0.1/darc \
  --config analysis/repomix/repomix.config.json \
  --token-count-encoding o200k_base \
  --output /tmp/darc-authored.xml
```
