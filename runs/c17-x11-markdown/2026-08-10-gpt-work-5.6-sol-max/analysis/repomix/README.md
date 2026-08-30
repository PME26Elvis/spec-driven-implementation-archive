# Repomix authored-corpus review

Measured project: `project/c17_x11_markdown_editor`

Reviewed exclusions:

- `bin/**` — compiled application, test, fixture-generator, evidence-generator, and analysis executables.
- `evidence/**` — generated evidence/fixture material, including binary corruption fixtures and evidence manifests; this measures verification corpus/output rather than authored source/docs.

Retained: `src/`, `include/`, `tools/`, `tests/`, `scripts/`, `config/`, `docs/`, `Makefile`, `README.md`, and any other non-excluded authored text in the snapshot.

Run from this recorded-run directory:

```bash
npx --yes repomix@1.18.0 project/c17_x11_markdown_editor \
  --config analysis/repomix/repomix.config.json \
  --token-count-encoding o200k_base \
  --output /tmp/markdown-authored.xml
```
