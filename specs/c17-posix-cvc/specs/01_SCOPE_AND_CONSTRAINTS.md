# 01 — Scope and Engineering Constraints

## 1. Product Identity

Executable name: `cvc`.

CVC is a local, single-repository, command-line version-control system. A repository manages a working tree rooted at the directory containing `.cvc/`.

## 2. Language and Dependency Rules

### 2.1 Required language

Production implementation MUST be C17.

### 2.2 Permitted runtime/API surface

The implementation MAY use:

- the ISO C17 standard library;
- a minimal POSIX filesystem/process-adjacent surface required for ordinary local repository operation, including directory enumeration, file metadata, symbolic-link inspection/readback, filesystem locking primitives, fsync-style durability primitives, secure creation of temporary files, and atomic rename within a filesystem.

This POSIX allowance exists only to access the host filesystem and process environment. It MUST NOT be used to delegate required algorithms. Recursive repository walking MUST be implemented from ordinary directory enumeration/metadata primitives rather than delegated to high-level walkers such as `ftw()`/`nftw()`/`fts`. Likewise, `glob()`, `fnmatch()`, `wordexp()`, cryptographic hash helpers, or subprocess execution MUST NOT replace the handwritten glob matcher, SHA-256, parser, diff, merge, or serialization required by this pack.

UTF-8 validation and UTF-16-surrogate-to-UTF-8 emission required by JSON/path/message rules MUST also be implemented in project C source using deterministic byte logic. Locale-dependent multibyte conversion, ICU, iconv, or equivalent Unicode libraries MUST NOT define required semantics.

### 2.3 Forbidden dependencies

Production code MUST NOT depend on third-party libraries for:

- JSON parsing;
- cryptographic hashing;
- glob/path matching;
- diff;
- merge;
- repository/object storage;
- command parsing beyond basic C/POSIX facilities;
- serialization;
- compression;
- embedded databases;
- Git interoperability.

The required algorithms and parsers listed by this pack MUST be implemented in the submission's own C source.

### 2.4 External executables

The product MUST NOT invoke external commands to implement required functionality. In particular, it MUST NOT shell out to `git`, `diff`, `patch`, `rsync`, `find`, `jq`, `sha256sum`, Python, Perl, or equivalent substitutes.

Test automation MAY invoke the built `cvc` executable and ordinary test harness utilities, but tests cannot compensate for missing production functionality.

## 3. Repository Scope

### 3.1 One repository root

CVC discovers the repository by walking from the current directory upward until an actual filesystem directory named `.cvc` is found. A symlink named `.cvc` MUST NOT be accepted as repository metadata. Once discovered, commands operate on the **entire working tree rooted at that repository root**, not merely the caller's current subdirectory.

If none is found, repository-required commands MUST fail with a nonzero exit status and a concise diagnostic. `cvc init` MUST fail if any filesystem entry named `.cvc` already exists in the current directory, regardless of its type.

### 3.2 Nested repositories

When scanning a working tree, CVC MUST NOT descend into any nested directory that contains an actual child directory named `.cvc`. The boundary is triggered by the presence/type of that child directory; the parent scanner does not parse/validate the nested repository before deciding to stop. Thus even a partially initialized or corrupt nested `.cvc` directory remains an opaque safety boundary. The nested repository root and its contents are outside the parent repository's tracked universe.

The root repository's own `.cvc` directory is always excluded.

### 3.3 No staging area

There is no index/staging area.

`cvc save` computes a new snapshot directly from the eligible current working tree using the repository `tracking` configuration. Command-line `--include` / `--exclude` options do **not** change snapshot membership; on `save` they filter diffstat presentation only, as defined in the CLI and filtering specifications.

## 4. Supported Filesystem Entry Types

CVC MUST support:

- regular files;
- directories as structural containers;
- symbolic links.

Directories are not independently versioned entries. Empty working-tree directories are not stored in snapshots. A tree entry for a directory exists only when at least one versioned descendant requires that subtree; the commit root tree itself may be empty.

Other entry types, including sockets, FIFOs, block devices, and character devices, MUST be ignored and reported in the status ignored summary when encountered.

### 4.1 Symbolic links

A symbolic link is versioned as the textual byte sequence of its link target as returned by the operating system.

CVC MUST NOT dereference a symlink while scanning, saving, diffing, restoring, switching, merging, or verifying the repository.

A dangling symlink is valid and versionable.

Symlink loops therefore MUST NOT cause recursion.

## 5. Text-File Eligibility

Regular files are versionable only if they are eligible text files.

A regular file is ineligible if either rule is true:

1. file size is strictly greater than **8 MiB = 8,388,608 bytes**;
2. the first `min(file_size, 8192)` bytes contain `0x00`.

An empty regular file is eligible.

No additional MIME detection, entropy test, extension list, or encoding detector is required or permitted to change this normative decision.

### 5.1 Ineligible files

Ineligible regular files:

- MUST NOT be stored as blobs;
- MUST NOT appear in new tree snapshots;
- MUST NOT participate in text diff or merge;
- MUST NOT require dedicated large-file test fixtures;
- MUST contribute to the status ignored summary when encountered during the relevant scan.

If a path was tracked in the current commit but the working-tree regular file at that path becomes ineligible, the next successful `save` records that tracked path as deleted from the new snapshot. The ineligible working-tree file itself is not deleted by `save`.

## 6. No Compression or Packing

All repository objects MUST remain loose objects.

CVC MUST NOT implement or require:

- compression;
- delta compression;
- pack files;
- pack indexes;
- garbage collection;
- repacking.

Unreachable loose objects MAY remain on disk indefinitely and are not a correctness failure.

## 7. Determinism

Given identical:

- eligible working-tree bytes and symlink targets;
- path set;
- parent commit IDs;
- commit message bytes;
- `CVC_TEST_TIMESTAMP` value when the deterministic test hook is used;
- configuration and command arguments;

conforming implementations MUST serialize the same v1 tree/commit bytes and therefore derive identical object IDs. Repository-format implementation freedom MUST NOT change canonical v1 object bytes.

Directory enumeration order MUST NOT affect tree IDs.

## 8. Path Model

Repository paths are relative to repository root and use `/` as the canonical internal separator.

Stored paths MUST use components that are valid UTF-8 and contain no ASCII control bytes (`0x01`-`0x1f`, `0x7f`). Stored paths also MUST NOT:

- begin with `/`;
- contain empty path segments;
- contain `.` or `..` segments;
- escape the repository root.

The `.cvc` path component at repository root is reserved and never versioned.

## 9. No Network Features

The product MUST NOT require network access.

The following are out of scope:

- clone;
- fetch;
- pull;
- push;
- remote repositories;
- hosted service integration.

## 10. Security Boundary

CVC is not required to encrypt repository data in this task. Stored objects may be plaintext.

Integrity is provided by content hashing and structural verification, not confidentiality.
