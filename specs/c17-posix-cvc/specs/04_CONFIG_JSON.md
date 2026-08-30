# 04 — Handwritten JSON Configuration

## 1. File

Repository configuration path:

```text
.cvc/config.json
```

Only JSON is supported. YAML is not part of this task.

## 2. Parser Requirement

The JSON lexer/parser MUST be implemented in the submission's own C17 source.

It MUST NOT use third-party or external JSON implementations.

The parser MUST build enough structured representation to validate and consume the schema defined here. A full generic DOM is permitted but not required.

## 3. JSON Syntax Support

The parser MUST correctly support the RFC 8259 data model used by this assignment:

- objects;
- arrays;
- strings;
- numbers;
- `true`;
- `false`;
- `null`.

String escapes MUST support:

- `\"`;
- `\\`;
- `\/`;
- `\b`;
- `\f`;
- `\n`;
- `\r`;
- `\t`;
- `\uXXXX`.

Surrogate-pair handling MUST correctly combine valid UTF-16 surrogate pairs in `\u` escapes into UTF-8.

Unpaired surrogate escapes MUST be rejected.

Malformed UTF-8 in literal non-escaped string bytes MUST be rejected. A UTF-8 BOM at the beginning of `config.json` is rejected in v1 rather than silently ignored.

The parser must remain length-aware: a syntactically valid JSON string may contain `\u0000`. Such a value MUST be parsed correctly at the JSON layer rather than truncated as a C string. The configuration schema then rejects decoded NUL in keys and in path-pattern strings because repository paths/patterns cannot contain NUL.

## 4. Number Parsing

JSON numbers MUST obey JSON grammar, including optional minus, integer component, fraction, and exponent.

The parser must lex and validate the complete JSON number grammar even though v1 schema has only one numeric field. `format_version` specifically MUST use an integer-form JSON token with no fraction or exponent and its value MUST equal `1`; therefore `1.0` and `1e0` are syntactically valid JSON numbers but semantically invalid format versions.

Overflow beyond the implementation's supported numeric range MUST produce a validation error, not wraparound.

NaN and Infinity are invalid JSON and MUST be rejected.

## 5. Duplicate Object Keys

Duplicate keys within the same JSON object MUST be rejected **after JSON string escape decoding**. For example, keys `"a"` and `"\u0061"` are duplicates.

This removes ambiguity and prevents raw-token spelling from changing object-key identity.

## 6. Trailing Data

After the top-level JSON value and permitted JSON whitespace, any additional byte is an error.

Comments and trailing commas are not JSON and MUST be rejected.

## 7. Required Top-Level Schema

Top-level value MUST be an object.

Supported keys:

```json
{
  "format_version": 1,
  "save": {
    "show_diffstat": true
  },
  "tracking": {
    "include": ["**"],
    "exclude": [".cvc/**"]
  },
  "diffstat": {
    "include": ["**"],
    "exclude": []
  }
}
```

`format_version` is required and MUST satisfy the integer-token rule above.

Other sections MAY be omitted and receive defaults.

Unknown keys MUST be rejected at every schema object level, not only the top level. For example, `save.foo`, `tracking.foo`, and `diffstat.foo` are errors. Silent ignoring is forbidden because it hides misspelled configuration.

## 8. Defaults

When optional sections are absent:

- `save.show_diffstat = true`;
- `tracking.include = ["**"]`;
- `tracking.exclude = []` plus the built-in immutable exclusions;
- `diffstat.include = ["**"]`;
- `diffstat.exclude = []`.

Built-in exclusions such as root `.cvc/**`, nested repositories, unsupported special files, and text-eligibility exclusions cannot be disabled by config.

## 9. Pattern Arrays

Every include/exclude value MUST be an array of JSON strings. Decoded pattern strings MUST be nonempty and MUST NOT contain NUL.

An empty include array means **match nothing**.

An empty exclude array excludes nothing beyond built-in exclusions.

Empty pattern strings MUST be rejected.

## 10. Effective Configuration

Precedence for settings is:

1. built-in hard safety rules;
2. repository `config.json`;
3. command-line options for the current command.

Command-line include/exclude values replace the corresponding configured list for that command; they do not append.

The CLI comma-list grammar is defined in `05_DIFF_AND_FILTERING.md`.

`--no-diffstat` overrides `save.show_diffstat` for that invocation. `status` and `diff` display filters are separate command-local filters and do not inherit `diffstat.include`/`diffstat.exclude`.

Because `format_version` lives in this file, every command that operates on an **existing repository** (other than help) MUST parse and semantically validate `config.json` before relying on repository contents. `cvc init` is the exception because it creates the initial config rather than consuming a preexisting one. `cvc config validate` is the diagnostic path for an invalid file; other existing-repository commands fail safely rather than proceeding with partial/default configuration.

## 11. Error Diagnostics

JSON syntax errors MUST identify at least:

- byte offset or line/column;
- a short reason.

Semantic validation errors MUST identify the offending key/path when practical.

Example acceptable diagnostic:

```text
config.json:4:19: duplicate key "include"
```

Exact wording is not prescribed.

## 12. Parser Robustness Cases

Tests MUST cover at least:

- empty input;
- top-level non-object;
- nested objects/arrays;
- escaped quote and backslash;
- BMP Unicode escape;
- surrogate pair;
- unpaired high surrogate;
- unpaired low surrogate;
- invalid hex escape;
- invalid UTF-8;
- duplicate key;
- trailing comma;
- comment syntax;
- number overflow;
- unknown schema key;
- wrong schema type;
- UTF-8 BOM rejection;
- syntactically valid `\u0000` string handling followed by schema rejection where used as a pattern/key;
- `format_version` written as `1.0` or `1e0` rejected semantically.
