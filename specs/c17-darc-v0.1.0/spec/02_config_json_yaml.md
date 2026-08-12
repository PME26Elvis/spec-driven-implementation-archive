# 02 — JSON and YAML Parameter Files

## 1. Objective

DARC MUST accept equivalent configuration in JSON and YAML so that scanning, snapshot creation, diff/reporting, restore, verification, garbage collection, and output behavior can be parameterized without changing command syntax.

Both formats map to one internal typed configuration model.

## 2. Selection and precedence

Configuration may be provided explicitly with:

```text
--config PATH
```

Precedence, highest first:

1. command-line option;
2. explicit configuration file value;
3. repository-local defaults stored at initialization;
4. built-in default.

No environment-variable override is required.

## 3. Format detection

- `.json` selects JSON.
- `.yaml` or `.yml` selects YAML.
- other extensions MUST be rejected with `E_CONFIG_FORMAT` rather than guessed.
- UTF-8 is required for configuration files.
- a UTF-8 BOM MAY be accepted but must not be required.

## 4. Required JSON support

The parser MUST support RFC 8259 JSON data types needed by this configuration model:

- object;
- array;
- string with standard escapes and `\uXXXX` escapes;
- number;
- `true`;
- `false`;
- `null`.

Additional requirements:

- reject trailing garbage;
- reject comments;
- reject trailing commas;
- reject duplicate object keys as a validation error;
- reject malformed UTF-8;
- correctly combine UTF-16 surrogate-pair escapes into Unicode scalar values;
- reject lone surrogate escapes;
- enforce nesting-depth protection of at least 64 levels and reject beyond the implementation limit safely.

## 5. Required YAML subset

Full YAML 1.2 is intentionally not required. DARC MUST implement and document a deterministic YAML 1.2-style configuration subset containing:

- indentation-based mappings;
- indentation-based sequences using `-`;
- plain scalar keys without flow punctuation;
- quoted strings using single or double quotes;
- double-quoted escapes needed to represent JSON-equivalent strings;
- booleans `true` and `false`;
- `null` and `~`;
- decimal integers;
- decimal floating-point numbers;
- comments beginning with `#` outside quoted strings;
- empty arrays/maps only through `[]` and `{}`;
- literal block strings using `|` are optional and not needed by the canonical config.

The following MUST be rejected rather than partially interpreted:

- anchors and aliases;
- merge keys;
- tags;
- directives;
- multi-document streams;
- complex keys;
- flow collections other than exactly empty `[]` and `{}`;
- implicit timestamp/date coercion;
- hexadecimal/octal numeric coercion;
- duplicate mapping keys;
- tab indentation.

YAML scalar typing MUST follow the restricted rules above, not YAML 1.1 legacy booleans such as `yes/no/on/off`.

## 6. Required configuration model

Top-level keys:

```text
repository
scan
chunking
compression
snapshot
restore
diff
verify
gc
output
```

Unknown top-level or nested keys MUST be rejected by default to catch misspellings. A future format version may add keys only through explicit schema/version evolution.

## 7. Repository section

```text
repository.format_version          integer, currently 1
repository.parity_enabled          boolean, default true
repository.parity_data_members     integer, exactly 8 in format v1
```

`parity_data_members` is fixed to 8 for format v1; a different value is a validation error rather than an alternate encoding.

## 8. Scan section

```text
scan.follow_symlinks               boolean, default false
scan.cross_filesystems             boolean, default false
scan.include                       array<string>, default ["**"]
scan.exclude                       array<string>, default []
scan.exclude_hidden                boolean, default false
scan.on_special_file               "error"|"skip", default "error"
scan.on_permission_error           "error"|"skip", default "error"
```

Patterns use the glob language defined in `05_scan_snapshot_incremental.md`.

## 9. Chunking section

```text
chunking.algorithm                 "buzhash64", fixed
chunking.window_bytes              64, fixed in format v1
chunking.min_bytes                 integer, default 16384
chunking.avg_bytes                 integer, default 65536
chunking.max_bytes                 integer, default 262144
```

Validation:

- `min_bytes >= 4096`;
- `avg_bytes` is a power of two;
- `min_bytes < avg_bytes < max_bytes`;
- `max_bytes <= 16777216`;
- values are stored in snapshot profile metadata.

## 10. Compression section

```text
compression.algorithm              "lzh1", fixed in format v1
compression.enabled                boolean, default true
compression.min_savings_bytes      integer, default 1
```

A chunk is stored compressed only if the canonical compressed representation saves at least `min_savings_bytes` compared with canonical raw storage payload size.

## 11. Snapshot section

```text
snapshot.name                      string|null
snapshot.parent                    string|null
snapshot.timestamp_ns              integer|null
snapshot.trust_unchanged_identity  boolean, default true
```

`timestamp_ns=null` means current UTC time. A fixed integer is required for reproducibility tests.

## 12. Restore section

```text
restore.overwrite                  "never"|"files"|"all", default "never"
restore.preserve_mode              boolean, default true
restore.preserve_mtime             boolean, default true
restore.create_hardlinks           boolean, default true
restore.on_unrecoverable           "error", fixed in v1
```

DARC MUST NOT silently emit placeholder bytes for unrecoverable content.

## 13. Diff section

```text
diff.path                          string|null
diff.top_n_svg                     integer, default 50
diff.show_chunk_metrics            boolean, default true
```

## 14. Verify section

```text
verify.level                       "quick"|"full"|"scrub", default "full"
verify.repair                      boolean, default false
```

## 15. GC section

```text
gc.dry_run                         boolean, default false
gc.repack_parity                   boolean, default true
```

## 16. Output section

```text
output.format                      "text"|"json"|"ndjson"|"svg", default "text"
output.color                       "auto"|"always"|"never", default "auto"
output.quiet                       boolean, default false
output.verbose                     boolean, default false
```

SVG is valid only for commands explicitly supporting it. Invalid command/format combinations MUST fail validation.

## 17. Canonical semantic equivalence requirement

The provided example JSON and YAML files represent the same semantic configuration. Parsing either MUST produce an identical internal normalized configuration and identical normalized configuration hash.

The normalized configuration hash is:

```text
SHA-256(canonical_config_json_v1)
```

`canonical_config_json_v1` is the UTF-8 JSON serialization of the fully default-expanded typed configuration with these exact rules:

- object keys sorted recursively by unsigned UTF-8 byte order;
- no insignificant whitespace;
- arrays preserve semantic order;
- integers are base-10 with no leading zero except `0`;
- booleans are `true`/`false`; null is `null`;
- strings use `"` and `\` escapes and `\u00XX` for control bytes U+0000..U+001F; other valid non-ASCII Unicode is emitted directly as UTF-8;
- floating-point values are not present in the v1 typed schema even though the parser recognizes JSON numbers.

The normalization MUST NOT depend on whitespace, key order in the source file, YAML comments, or quote style.

For snapshot metadata, `profile_hash` is a second SHA-256 over canonical JSON containing only the effective `repository`, `scan`, `chunking`, and `compression` sections plus `snapshot.trust_unchanged_identity`. It excludes output/reporting/restore/verify/gc options and excludes snapshot name, parent, and timestamp.

## 18. Config validation command

`darc config validate FILE` MUST:

- parse the file;
- validate all semantic constraints;
- print normalized effective values in text mode;
- emit normalized JSON in JSON mode;
- report the normalized config hash;
- return exit 0 only when valid.

## 19. Security and resource limits

Parsers MUST reject pathological input safely:

- deeply nested structures beyond the documented limit;
- integer overflow;
- strings exceeding implementation limits without buffer overflow;
- malformed UTF-8;
- unterminated strings;
- YAML indentation jumps that cannot be interpreted unambiguously.

Parser errors MUST include line and column where possible.
