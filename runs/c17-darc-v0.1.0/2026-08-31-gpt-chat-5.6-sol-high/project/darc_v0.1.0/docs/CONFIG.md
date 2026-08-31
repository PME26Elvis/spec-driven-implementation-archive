# Configuration

DARC accepts `.json`, `.yaml`, and `.yml` configuration files through `--config FILE`. JSON and the supported YAML subset are parsed by project-owned C17 parsers. Unknown keys, duplicate keys, invalid UTF-8/escapes, malformed numbers, excessive nesting, wrong types, and unsupported syntax are rejected.

Validate and normalize a file with:

```sh
./darc config validate examples/config.json
./darc config validate examples/config.yaml
```

The checked-in examples normalize to the same semantic configuration and the fixed task-pack configuration hash.

## Precedence

Configuration precedence is:

1. explicit CLI option;
2. explicit `--config` file;
3. repository defaults stored at `state/defaults.json`;
4. built-in defaults.

`darc init` writes normalized repository defaults into the repository. Canonical configuration/profile hashes are SHA-256 over normalized JSON; snapshot semantic profile data excludes presentation-only/runtime fields.

## Top-level schema

The only accepted top-level sections are:

- `repository`: `format_version`, `parity_enabled`, `parity_data_members`;
- `scan`: `follow_symlinks`, `cross_filesystems`, `include`, `exclude`, `exclude_hidden`, `on_special_file`, `on_permission_error`;
- `chunking`: `algorithm`, `window_bytes`, `min_bytes`, `avg_bytes`, `max_bytes`;
- `compression`: `algorithm`, `enabled`, `min_savings_bytes`;
- `snapshot`: `name`, `parent`, `timestamp_ns`, `trust_unchanged_identity`;
- `restore`: `overwrite`, `preserve_mode`, `preserve_mtime`, `create_hardlinks`, `on_unrecoverable`;
- `diff`: `path`, `top_n_svg`, `show_chunk_metrics`;
- `verify`: `level`, `repair`;
- `gc`: `dry_run`, `repack_parity`;
- `output`: `format`, `color`, `quiet`, `verbose`.

## Built-in defaults and fixed constraints

- repository format: `1`;
- parity: enabled, exactly 8 data members per full stripe;
- CDC: `buzhash64`, 64-byte window, min 16 KiB, average 64 KiB, max 256 KiB;
- compression: `lzh1`, enabled, minimum savings 1 byte;
- include: `**`;
- symlinks are archived as symlinks, not followed;
- cross-filesystem scan: disabled;
- special/permission errors: `error` unless configured to `skip`;
- incremental identity fast path: enabled;
- restore: `overwrite=never`, preserve mode/mtime, create hardlinks;
- diff chunk metrics: enabled, SVG top-N 50;
- verify: `full`, no repair;
- GC parity repack: enabled;
- output: text, color auto.

Validation enforces `format_version=1`, `parity_data_members=8`, a 64-byte CDC window, `4096 <= min < avg < max`, power-of-two average size, and maximum chunk size no greater than 16 MiB.

## YAML subset

The parser intentionally supports the configuration-oriented YAML subset required by the task pack rather than arbitrary YAML features. JSON-style scalar types, mappings, sequences, comments, quoted/plain scalar use cases from the examples, and required edge cases are covered by the acceptance suite. Unsupported YAML constructs fail rather than being silently reinterpreted.
