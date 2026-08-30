# 01 — Scope and Engineering Constraints

## 1. Product Identity

Executable name: `cvc`.

CVC is a local, single-repository, command-line version-control system. A repository manages a working tree rooted at the directory containing `.cvc/`.

## 2. Language and Dependency Rules

### 2.1 Required language

Production implementation MUST be C17.

### 2.2 Required platform profile

The required product is a native Windows command-line executable. Production code MAY use:

- the ISO C17 standard library;
- the minimal Windows/CRT surface needed for ordinary local repository operation, as normatively constrained by `09_WINDOWS_PLATFORM_CONTRACT.md`.

The platform allowance exists only to access the local Windows filesystem, Unicode command line, time/environment, native symbolic-link/reparse metadata, file flushing/replacement, and interprocess file locking. It MUST NOT be used to delegate required algorithms.

Recursive repository walking MUST be implemented in project C logic over low-level directory enumeration/metadata primitives. The product MUST NOT delegate recursive traversal to a high-level walker or shell. Likewise, platform/path-matching helpers, cryptographic hash helpers, Unicode code-page conversion helpers, or subprocess execution MUST NOT replace the handwritten glob matcher, SHA-256, UTF-8/UTF-16 conversion, JSON parser, diff, merge, or serialization required by this pack.

UTF-8 validation and UTF-8 <-> UTF-16 conversion required at the Windows boundary MUST be implemented in project C source using deterministic scalar/surrogate logic. Locale-dependent multibyte conversion and Windows code-page conversion are not the normative implementation.

### 2.3 Forbidden dependencies

Production code MUST NOT depend on third-party libraries for:

- JSON parsing;
- cryptographic hashing;
- Unicode conversion;
- glob/path matching;
- diff;
- merge;
- repository/object storage;
- command parsing beyond basic C/Windows runtime facilities;
- serialization;
- compression;
- embedded databases;
- Git interoperability.

The required algorithms and parsers listed by this pack MUST be implemented in the submission's own C source.

A compiler/runtime is not prescribed, but the produced `cvc.exe` MUST NOT require Cygwin, MSYS, WSL, or another POSIX-compatibility environment for normal product execution.

### 2.4 External executables

The product MUST NOT invoke external commands to implement required functionality. In particular, it MUST NOT shell out to Git, `diff`, `patch`, `robocopy`, `xcopy`, PowerShell, `cmd.exe`, WSL tools, Python, Perl, or equivalent substitutes.

Test automation MAY invoke the built `cvc.exe` and ordinary test harness utilities, but tests cannot compensate for missing production functionality.

## 3. Repository Scope

### 3.1 One repository root

CVC discovers the repository by walking from the current directory upward until an actual non-reparse filesystem directory logically named `.cvc` is found. Under Windows filename semantics, ASCII case variants alias the reserved name. A symbolic link, junction, mount point, or other reparse point in place of `.cvc` MUST NOT be accepted as repository metadata. Once discovered, commands operate on the **entire working tree rooted at that repository root**, not merely the caller's current subdirectory.

If none is found, repository-required commands MUST fail with a nonzero exit status and a concise diagnostic. `cvc init` MUST fail if any filesystem entry that aliases the reserved `.cvc` name already exists in the current directory, regardless of its type.

### 3.2 Nested repositories

When scanning a working tree, CVC MUST NOT descend into any nested directory that contains an actual non-reparse child directory that aliases the reserved `.cvc` name. The boundary is triggered by the presence/type of that child directory; the parent scanner does not parse/validate the nested repository before deciding to stop. Thus even a partially initialized or corrupt nested `.cvc` directory remains an opaque safety boundary. The nested repository root and its contents are outside the parent repository's tracked universe.

The root repository's own `.cvc` directory is always excluded.

### 3.3 No staging area

There is no index/staging area.

`cvc save` computes a new snapshot directly from the eligible current working tree using the repository `tracking` configuration. Command-line `--include` / `--exclude` options do **not** change snapshot membership; on `save` they filter diffstat presentation only, as defined in the CLI and filtering specifications.

## 4. Supported Filesystem Entry Types

CVC MUST support:

- ordinary regular files using the unnamed data stream;
- ordinary non-reparse directories as structural containers;
- Windows symbolic links with reparse tag `IO_REPARSE_TAG_SYMLINK`, including both file-link and directory-link kinds.

Directories are not independently versioned entries. Empty working-tree directories are not stored in snapshots. A tree entry for a directory exists only when at least one versioned descendant requires that subtree; the commit root tree itself may be empty.

Any reparse point that is not a Windows symbolic link is unversionable in v1. Junctions, mount points, cloud placeholders, and unknown reparse tags MUST be ignored, reported in the status ignored summary when encountered, and MUST NOT be traversed as directories.

### 4.1 Windows symbolic links

A supported symbolic link is versioned without dereferencing its target. Its canonical target and file-link/directory-link kind are defined by `09_WINDOWS_PLATFORM_CONTRACT.md` and the repository model.

A dangling symbolic link is valid and versionable. A symbolic-link loop MUST NOT cause recursion because target traversal is forbidden.

If Windows refuses symbolic-link creation during a materializing command, CVC MUST fail safely under the ordinary rollback rules; it MUST NOT substitute a copied target, junction, or regular file.

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

- eligible working-tree bytes and Windows symbolic-link target/kind data;
- path set;
- parent commit IDs;
- commit message bytes;
- `CVC_TEST_TIMESTAMP` value when the deterministic test hook is used;
- configuration and command arguments;

conforming implementations MUST serialize the same v1 tree/commit bytes and therefore derive identical object IDs. Repository-format implementation freedom MUST NOT change canonical v1 object bytes.

Directory enumeration order MUST NOT affect tree IDs.

## 8. Path Model

Repository paths are relative to repository root and use `/` as the canonical internal separator even on Windows.

Stored path components MUST be valid UTF-8 and satisfy both the generic structural rules here and the Windows-materializability rules in `09_WINDOWS_PLATFORM_CONTRACT.md`. Stored paths MUST NOT:

- begin with `/`;
- contain empty path segments;
- contain `.` or `..` segments;
- contain NUL (`0x00`) or ASCII control bytes (`0x01`-`0x1f`, `0x7f`);
- contain `\`;
- escape the repository root.

The metadata name `.cvc` is reserved under Windows ASCII case-insensitive filename semantics and is never versioned as ordinary content.

Native Windows paths are an implementation boundary only. CVC MUST dynamically construct safe absolute wide-character paths and MUST NOT impose the legacy 260-character `MAX_PATH` limit on otherwise valid repository paths.

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
