# DARC CLI Contract

## Global syntax

```text
darc [--repo PATH] [--config FILE] [--format text|json|ndjson]
     [--color auto|always|never] [--quiet|--verbose] COMMAND ...
```

`darc --help`, `darc --version`, and each canonical command/subcommand `--help` path work without network access.

## Commands

```text
darc init PATH

darc snapshot create SOURCE... [--name NAME] [--parent SNAPSHOT] [--timestamp NS]
darc snapshot list
darc snapshot show SNAPSHOT
darc snapshot delete SNAPSHOT --yes [--dry-run]
darc snapshot diff OLD NEW [--path PATH] [--format text|json|ndjson|svg]

darc restore SNAPSHOT --to PATH [--path PATH] [--overwrite never|files|all]
darc verify [--level quick|full|scrub] [--repair] [--format text|json|ndjson|svg]
darc gc [--dry-run] [--repack-parity]
darc stats [--format text|json|ndjson|svg]
darc index rebuild
darc repo inspect
darc config validate FILE
```

### Snapshot selectors

A selector may be:

- `HEAD`;
- a full lower-case 64-hex snapshot CID;
- a unique hexadecimal CID prefix of at least 8 characters;
- an unambiguous snapshot name stored in a published SNAPSHOT object.

Ambiguous prefixes/names are errors rather than arbitrary choices. Snapshot parent links are soft history links: deleting the parent ref does not invalidate a retained child.

### Output

Human text is the default. JSON/NDJSON are structured stdout with diagnostic/warning text kept on stderr. `snapshot diff`, `verify`, and `stats` support standalone SVG. Commands that do not support SVG reject it before mutating repository state.

Color is optional presentation only; `--color never` emits no terminal SGR sequences. `--quiet` suppresses chatter but does not suppress required command data.

## Exit codes

| Code | Meaning |
|---:|---|
| 0 | success / healthy |
| 2 | usage or configuration error |
| 3 | repository missing/invalid at repository boundary |
| 4 | requested snapshot/object selector not found or not uniquely resolvable |
| 5 | I/O or operational failure |
| 6 | repository/content corruption or degraded-repairable integrity state |
| 7 | unrecoverable integrity loss |
| 8 | restore conflict/safety rejection |
| 9 | writer lock held |
| 10 | unsupported repository/object version or codec |
| 11 | internal invariant failure |

Errors are emitted as stable symbolic codes such as `E_REPO_NOT_FOUND`, `E_CONFIG_PARSE`, `E_REPO_CORRUPT`, `E_UNRECOVERABLE`, `E_RESTORE_ESCAPE`, and `E_REPO_LOCKED` followed by a human-readable message.

## Restore overwrite modes

- `never`: any conflicting destination entry fails without replacing it.
- `files`: existing regular files may be atomically replaced; type changes are rejected.
- `all`: conflicting file/directory types may be replaced to reproduce the snapshot tree.

Restore walks destination path components with `openat`/`O_NOFOLLOW`, stages regular-file data in a temporary file, verifies the FILE full digest, then atomically publishes the final path. Partial restore may degrade a hardlink to an independent regular file when the link group's canonical primary is outside the selected subtree; a warning is emitted.
