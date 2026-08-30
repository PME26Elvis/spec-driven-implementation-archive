# Repomix authored-corpus review

Measured project: `project/physics_sandbox_v1.0_wip/physics_sandbox`

Reviewed exclusion:

- `evidence/**` — release/test evidence outputs (`releasecheck` results, test report/status files, golden report, registry report and evidence index). These are execution/evidence material rather than implementation source.

Retained: `src/**`, `tests/**`, the authored starter scene, Makefile and README. No compiled `bin/` or `build/` directory is present in this archived project snapshot, so none is invented in the config.

Run from this recorded-run directory:

```bash
npx --yes repomix@1.18.0 project/physics_sandbox_v1.0_wip/physics_sandbox \
  --config analysis/repomix/repomix.config.json \
  --token-count-encoding o200k_base \
  --output /tmp/physics-authored.xml
```
