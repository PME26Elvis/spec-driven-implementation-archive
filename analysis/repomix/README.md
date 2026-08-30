# Repomix authored-corpus policy

This directory contains **archive-maintainer analysis metadata**. It is intentionally kept outside the archived `project/` snapshots so that measurement rules never modify or masquerade as part of an implementer's submission.

## Metric

The configs in this directory define an **implementer-authored corpus** for Repomix size/token comparisons. The intended corpus includes implementation source, headers, test code, build/test scripts, human-authored documentation, human-authored configuration, and small authored examples where they are part of the implementation surface.

It excludes material whose size mostly measures execution or corpus volume rather than authored implementation effort: compiled binaries, build/intermediate outputs, generated evidence/reports, logs/diffs/traces, and data-only benchmark/negative-test corpora when those corpora are generated or mechanically expanded.

## Reproducibility rules

- Every run has its own reviewed config under `<run>/analysis/repomix/repomix.config.json`.
- Spec-specific measurement overrides live under `analysis/repomix/specs/<spec>/`.
- Archived `project/` trees are not edited to add measurement config.
- Configs explicitly disable `.gitignore`, `.ignore`, and Repomix default ignore patterns so corpus selection does not silently change with unrelated ignore-file/default changes.
- `ignore.customPatterns` contains only paths/patterns justified by inspection of that specific snapshot; there is no generic guessed list of `*.zip`, `dist/`, caches, etc.
- Repomix binary-content detection may still avoid embedding binary bytes, but known compiled-output paths are explicitly excluded where present.
- The default tokenizer for comparisons is `o200k_base`.

Repomix supports external configs with `-c/--config <path>`, so these configs can be kept outside the measured project while remaining directly executable.

## CI

`Count authored run tokens` validates and measures all recorded-run configs. `Count spec tokens` uses a spec override automatically when `analysis/repomix/specs/<spec>/repomix.config.json` exists.
