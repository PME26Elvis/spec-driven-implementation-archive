# Workstream A Utility Contracts

## Exit Codes

All three required utilities use: 0 success, 2 usage, 3 configuration/manifest schema, 4 input/read, 5 output/write, 6 verification mismatch, 7 internal consistency. A diagnostic is written to stderr; selected machine output is written only to the requested file/stdout.

## `locscan`

```text
locscan --root DIR --config CONFIG.(json|yaml) [--json OUTPUT] [--details]
```

The supported configuration keys are `include_extensions`, `source_extensions`, `test_extensions`, `documentation_extensions`, `config_build_extensions`, `exclude_dirs`, `exclude_paths`, `generated_paths`, `include_overrides`, and boolean `follow_directory_symlinks` (default false). String-list JSON and the documented YAML inline/block subset are authored parsers; an invalid supplied file never falls back to defaults.

Patterns are case-sensitive on Linux. `*` and `?` are simple wildcards; paths are normalized relative to the selected root. Excluded directories, excluded paths, and generated paths do not contribute to authored totals; explicit include overrides can bring a named file back. Binary/NUL files are excluded. Directory symlinks are not followed by default, preventing cycles and out-of-root traversal.

## `fixturegen`

```text
fixturegen --list-profiles
fixturegen --profile NAME --output DIR [--seed UINT64]
fixturegen --verify DIR
```

Profiles are `small`, `unicode`, `markdown-all`, `workspace`, `medium`, `large`, `stress-long-line`, and `failure`. Generation uses fixed xorshift64* and fixed content timestamps; identical profile/seed/version produces identical regular-file bytes and ordered `fixture-manifest.json` entries. The manifest lists schema/generator/profile/seed plus every generated regular file's path, size, C17 SHA-256, and role.

Verification rejects malformed manifests, missing files, size changes, and digest changes. Extra unlisted files are ignored by `--verify`; release generation avoids them by building all profiles in a fresh staging root and replacing each generated profile only after every profile verifies. The clean `workspace` profile is additionally checked to contain no `.mdeditor/` state.

## `evidencecheck`

```text
evidencecheck --root DIR --manifest RELATIVE_PATH
```

The validator requires the full v1.0 top-level schema, 119 passing and zero failed/skipped mandatory cases across all six categories, all stable screenshot/performance/failure IDs, eight exact fixture manifests, and required artifacts. Every reference must be normalized relative, exist within the selected real root after symlink resolution, and match C17 SHA-256 (plus size where required). Screenshot PNG/JPEG/BMP headers are inspected for recorded dimensions. Performance entries must bind to the exact named fixture-manifest digest and include measurements/environment; fault entries must identify fault/fixture and verification.

The tool never judges screenshot meaning and never fabricates results. `scripts/test_evidence_mutations.sh` demonstrates rejection of a missing file, digest mismatch, screenshot omission, and failed test count; utility integration also covers `../` escape.
