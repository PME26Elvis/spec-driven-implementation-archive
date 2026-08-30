# Prohibited Substitute Implementations

The task exists to evaluate real implementation work. The following substitutions are explicitly nonconforming.

## 1. Existing VCS Wrapper

Forbidden:

- invoking Git, Mercurial, Fossil, SVN, or another VCS for required behavior;
- storing a hidden Git repository and wrapping it with `cvc` commands;
- translating CVC commands into another VCS CLI.

## 2. Snapshot Directory Copier

Forbidden as the primary repository model:

```text
snapshots/0001/full-project-copy/
snapshots/0002/full-project-copy/
```

CVC MUST use required content-addressed blob/tree/commit objects.

## 3. Fake Branches

Forbidden:

- one copied working directory per branch;
- branch names mapped only to snapshot directories without commit refs/graph.

## 4. Fake Hashing or Noncanonical Objects

Forbidden:

- non-cryptographic ad-hoc hash represented as SHA-256;
- external `sha256sum` invocation;
- third-party SHA implementation linked into production.

SHA-256 must exist in project C source and pass vectors. Committed loose objects must use the fixed v1 envelope/tree/commit serialization; an implementation-private alternative object encoding is not a substitute.

## 5. Fake JSON

Forbidden:

- searching config with `strstr`/regex-like assumptions instead of parsing JSON grammar;
- accepting only one hard-coded key order;
- using jq/Python/third-party parser;
- silently treating malformed config as defaults.

## 6. Fake Diff

Forbidden:

- reporting entire old file deleted and entire new file added for every modification;
- comparing only line counts;
- external `diff` or Git invocation;
- hard-coded expected fixture output.

Required ordinary diff must use the specified handwritten algorithmic approach. Win32 wildcard helpers, CRT wildcard expansion, POSIX `glob()`/`fnmatch()` through a compatibility layer, or equivalent library matching are forbidden as replacements for the handwritten path matcher. High-level walkers or shell recursion cannot replace handwritten traversal over low-level Win32 enumeration. Required UTF-8 validation and UTF-8/UTF-16 conversion cannot be delegated to ICU/iconv/locale/code-page conversion helpers.

## 7. Fake Merge

Forbidden:

- always choose ours;
- always choose theirs;
- concatenate both files;
- always report conflict when both sides changed;
- invoke Git merge/diff3/patch;
- merge only by comparing final file hashes without actual three-way text handling.

## 8. Fake Rollback

Forbidden:

- setting branch ref directly to the target revision and calling that rollback;
- deleting later commits;
- rewriting parent pointers.

## 9. Binary/Large Feature Inflation

Binary and large files are intentionally out of scope.

A submission is not rewarded for introducing binary diff, LFS, compression, packs, or media handling in place of missing mandatory features.

## 10. Placeholder Commands

Forbidden:

- commands that print success without performing required work;
- TODO paths hidden behind success status;
- UI/demo text that implies functionality not implemented;
- generated canned log/diff/status output.

## 11. Test-Only Backdoor

Forbidden:

- detecting fixture filenames and returning expected outputs;
- bypass environment variables that skip core algorithms;
- special test mode that uses a different simpler repository implementation.

The specified `CVC_TEST_TIMESTAMP` deterministic timestamp hook is explicitly permitted and is not a backdoor.

## 12. Silent Data Loss

Forbidden:

- overwriting dirty tracked files during switch/merge/rollback;
- following an untracked symbolic link, junction, mount point, or other reparse point to write outside repository;
- ignoring hash mismatch and continuing as if repository were valid;
- moving a ref to a commit before required objects are safely installed.

## 13. Hidden Required Dependency

Forbidden:

- requiring Cygwin/MSYS/WSL or another POSIX compatibility layer for normal product execution;
- requiring a third-party runtime library not permitted by scope;
- requiring a locally running service/database;
- requiring network access for normal repository operation.

## 14. Windows Platform Substitutions

Forbidden:

- using ANSI/OEM narrow Win32 file APIs as the normative Unicode path implementation;
- imposing legacy `MAX_PATH` because of fixed 260-character buffers when extended-length Win32 paths are required;
- treating Windows junctions/mount points as ordinary directories during repository traversal;
- using exclusive `CreateFile` share mode on `.cvc/lock` as a substitute for the required shared/exclusive `LockFileEx` protocol;
- storing lock ownership/PID bytes in `.cvc/lock` instead of keeping the required zero-byte lock file;
- recreating a directory symbolic link as a file symbolic link or vice versa;
- copying/dereferencing a symbolic-link target when native symbolic-link creation fails;
- silently allowing two committed siblings/refs that collide under the required Windows ordinal case-insensitive rule.

## 15. Scope Substitution

An implementation that replaces difficult required functionality with unrelated extra features remains incomplete.

Examples:

- adding colored output instead of merge conflict continuation;
- adding tags instead of repository verification;
- adding compression instead of correct object durability;
- adding interactive menus instead of required CLI contracts.
