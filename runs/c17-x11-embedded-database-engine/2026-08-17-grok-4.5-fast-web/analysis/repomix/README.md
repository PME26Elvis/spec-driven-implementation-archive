# Repomix authored-corpus review

Measured project: `project/`

Reviewed exclusions:

- `docs/evidence/EVIDENCE_MATRIX.md` — explicitly stamped `Generated:` and derived from executed verification.
- `docs/evidence/FULL_ID_MATRIX.md` — mechanically expanded 1,343-ID status matrix rather than authored prose/code.
- `docs/evidence/FULL_ID_MATRIX.csv` — machine-readable companion to the expanded matrix.

Retained intentionally: `docs/evidence/RG_MATRIX.md` and `PARTIAL_EVIDENCE.md` because they are concise authored gate/status documentation, plus all implementation source, headers, tests, tools and normal docs. The archive already removed the submitted `build/` outputs, so the config does not pretend such a directory exists.

Run from this recorded-run directory:

```bash
npx --yes repomix@1.18.0 project \
  --config analysis/repomix/repomix.config.json \
  --token-count-encoding o200k_base \
  --output /tmp/embedded-db-authored.xml
```
