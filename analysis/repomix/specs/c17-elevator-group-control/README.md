# Elevator Group Control spec — Repomix review

The previous all-files count was inflated by normative **data corpora**, not specification prose. In particular, `fixtures/acceptance/traces/a24_office_100k.csv` is a fixed 100,000-passenger stress input and is about 3.47 MB by itself.

The task pack's own `fixtures/README.md` states that fixture JSON/CSV/YAML files are test data rather than human documentation. This config therefore excludes the acceptance/negative data files and their generated integrity manifests while retaining the fixture `README.md` files and all normative Markdown specification documents.

The exclusion is deliberately scoped to this spec's inspected fixture layout; it is not a repository-wide rule for JSON/CSV/YAML.

From the repository root:

```bash
npx --yes repomix@1.18.0 specs/c17-elevator-group-control \
  --config analysis/repomix/specs/c17-elevator-group-control/repomix.config.json \
  --token-count-encoding o200k_base \
  --output /tmp/elevator-authored.xml
```
