# 09 — Windows Platform Contract

## 1. Purpose and Target

This document defines the Windows-native platform semantics for the CVC Windows task pack. It does not change the core assignment into a wrapper around platform services: JSON parsing, UTF-8 validation/encoding, SHA-256, glob matching, recursive repository traversal logic, object serialization, Myers diff, history traversal, merge-base selection, three-way merge, and repository verification remain project C code as required elsewhere.

The required product is a native Windows command-line executable named `cvc.exe` written in C17. The implementation MUST operate through the ordinary Win32 Unicode filesystem interface and MUST NOT require a POSIX compatibility runtime such as Cygwin or an MSYS runtime for normal product execution.

The target filesystem profile for mandatory acceptance is an ordinary local NTFS working tree on a supported desktop Windows release. The acceptance host MUST permit creation of ordinary Windows symbolic links (for example through Developer Mode or the applicable account privilege) so the required successful symlink round-trip cases can actually be exercised. CVC must still implement the specified safe-failure behavior when a particular creation attempt is denied. Network shares, UNC repository roots, FAT/exFAT-specific behavior, Windows containers, per-directory POSIX semantics, and cross-platform repository interchange are out of scope.

No specific compiler, IDE, build system, agent, shell, package manager, or development environment is prescribed by this task pack.

## 2. Permitted Win32 Surface

Production code MAY use Windows/CRT facilities needed to expose the local filesystem and process environment, including the low-level equivalents of:

- wide-character command-line acquisition;
- current-directory and absolute-path discovery;
- `FindFirstFileW` / `FindNextFileW` style directory enumeration;
- `CreateFileW`, `GetFileAttributesExW`, handle-based metadata queries, file sizing, reading, writing, seeking, truncation, and deletion;
- directory creation/removal;
- reparse-point inspection with `FILE_FLAG_OPEN_REPARSE_POINT` and `FSCTL_GET_REPARSE_POINT`;
- symbolic-link creation;
- `LockFileEx` / `UnlockFileEx`;
- `FlushFileBuffers`;
- same-volume rename/replacement primitives such as `MoveFileExW` / `ReplaceFileW`;
- wall-clock time and the `CVC_TEST_TIMESTAMP` environment-variable hook;
- ordinary error retrieval/formatting.

Equivalent lower-level Win32 calls are allowed. These APIs are permission to access the host, not permission to delegate the algorithms required by the assignment.

Production code MUST NOT use a high-level filesystem traversal library, shell command, Git library, JSON library, hashing library, diff library, merge library, regex/glob library, or database to replace required handwritten behavior.

## 3. Unicode Boundary

### 3.1 Internal text model

CVC's repository-visible strings remain canonical UTF-8 byte strings:

- repository-relative paths;
- branch names;
- commit messages;
- JSON text;
- stored Windows symbolic-link targets.

The native Win32 filesystem and command line use UTF-16. CVC MUST implement deterministic UTF-8 <-> UTF-16 conversion in its own C source, including surrogate-pair handling and rejection of malformed/unpaired surrogate sequences. `MultiByteToWideChar`, `WideCharToMultiByte`, locale/code-page conversion, `mbstowcs`, and equivalent conversion helpers MUST NOT define the required conversion semantics.

Using `wmain` or an equivalent wide-character command-line entry is permitted. The implementation MUST NOT depend on the active ANSI/OEM code page for Unicode correctness.

All repository-visible text written to redirected or piped `stdout`/`stderr` MUST be encoded as UTF-8 bytes and MUST NOT depend on the active console code page. An implementation MAY use a native wide-console output path when a handle is an interactive Windows console, but the observable text MUST be equivalent to the canonical UTF-8 representation and redirected output MUST remain UTF-8. Diagnostics that include repository paths, branch names, messages, or symbolic-link targets follow the same rule.

### 3.2 Unrepresentable native names

If directory enumeration returns a filename containing an unpaired UTF-16 surrogate, CVC cannot represent that path in its canonical UTF-8 model. The entry is unversionable: scanning MUST skip that entry/subtree without opening it as tracked content, emit a warning, and count it as ignored.

Valid UTF-16 filenames are converted to UTF-8 without Unicode normalization. CVC MUST NOT silently normalize NFC/NFD or change filename spelling.

## 4. Native and Canonical Path Forms

Repository paths, config glob patterns, status/diff output, and required CLI path operands use `/` as the canonical separator even on Windows. `\` is not a repository separator and is forbidden in stored path components and required repository-relative CLI path operands.

For Win32 calls, CVC converts canonical paths to absolute native wide-character paths rooted at the discovered repository root. The implementation MUST dynamically allocate path buffers and MUST NOT assume `MAX_PATH == 260` as a repository limit. Native filesystem paths use `\` separators. In particular, after applying an extended-length `\\?\` prefix CVC MUST NOT rely on Win32 to translate canonical `/`, `.` or `..` syntax; canonical components are validated first and explicitly joined into a native absolute path.

When necessary, CVC MUST use extended-length absolute Win32 paths (`\\?\` form) so that an otherwise valid repository path longer than the legacy `MAX_PATH` boundary is not rejected merely because of a fixed 260-character buffer. Acceptance may include a path whose native absolute spelling exceeds 260 UTF-16 code units while remaining comfortably within NTFS/extended-path limits.

Each canonical repository component, after handwritten UTF-8-to-UTF-16 conversion, MUST also fit the filesystem's component limit returned for the repository volume by `GetVolumeInformationW`/equivalent low-level volume information. The mandatory NTFS profile normally reports 255 TCHARs. A scanner skips an unversionable over-limit native component if encountered through lower-level means; verification rejects a committed tree component that exceeds the active repository volume's reported component limit.

The mandatory profile uses local drive-letter roots. CVC MUST reject an operation cleanly if it cannot construct a safe native path within the host filesystem's actual limits.

## 5. Windows-Safe Path Components

In addition to the generic UTF-8/control/`.`/`..` rules, a versioned path component MUST be materializable through the Win32 namespace. Writers therefore MUST reject/ignore a component that:

- contains any of `<`, `>`, `:`, `"`, `\`, `|`, `?`, or `*`;
- ends in ASCII space or `.`;
- is a reserved DOS device basename, case-insensitively, before the first dot: `CON`, `PRN`, `AUX`, `NUL`, `COM1` through `COM9`, `LPT1` through `LPT9`, or the Windows-recognized superscript-digit forms `COM¹`, `COM²`, `COM³`, `LPT¹`, `LPT²`, and `LPT³`.

The same component restrictions apply to branch-name segments because branch refs are materialized as files/directories below `.cvc/refs/heads/`.

The root metadata component `.cvc` is reserved under Windows ASCII case-insensitive filename semantics: case variants such as `.CVC` alias the reserved metadata name and MUST NOT be tracked as ordinary content. NUL is forbidden by the generic path model even though it cannot be passed through ordinary Win32 path strings.

Alternate data stream syntax is therefore impossible in canonical repository paths because `:` is forbidden. CVC versions only the unnamed regular-file data stream; NTFS alternate data streams are not versioned.

## 6. Case and Name-Collision Rules

Canonical object/tree names preserve exact UTF-8 bytes and are serialized/sorted by the unsigned-byte rules in the repository model. CVC does not lowercase user filenames.

However, Windows commonly provides a case-insensitive namespace. A single tree MUST NOT contain sibling names that Windows ordinal case-insensitive comparison considers equal. CVC MUST use locale-independent ordinal Windows comparison semantics equivalent to `CompareStringOrdinal(..., TRUE)` for this **collision check only**. Exact canonical byte spelling remains the stored name and output spelling.

Consequences:

- `Readme.txt` and `README.TXT` cannot coexist as sibling entries in one committed tree;
- if a case-sensitive NTFS directory exposes both names simultaneously, scanning MUST fail that selected snapshot with a clear collision diagnostic rather than choose one nondeterministically;
- a case-only filename rename is observable as deletion of the old canonical path plus addition of the new canonical path; no rename detection is required;
- materialization between commits that differ only by case MUST succeed safely, using a temporary intermediate name if the native rename/replace operation requires it;
- before creating/replacing a child, CVC MUST enumerate/inspect the actual existing native sibling spelling rather than trusting a case-insensitive direct open. An existing sibling that compares ordinal-case-insensitively equal but has different canonical spelling is an untracked collision unless it is the currently tracked entry being intentionally transformed by that operation's case-only transition; it MUST NOT be accidentally overwritten as though the spelling matched exactly;
- Windows 8.3 short names are namespace aliases, **not** additional canonical repository names. When the host exposes an alternate short name (for example through `WIN32_FIND_DATAW.cAlternateFileName`), CVC MUST ensure a create/open/replace target cannot resolve through that alias to a different native sibling. An alias collision is treated as an ordinary existing-path/namespace collision unless the resolved entry is precisely the tracked native entry that the current operation is already authorized to replace. CVC MUST NOT track the short alias as a second path;
- tree verification rejects case-insensitive sibling collisions even when canonical UTF-8 bytes differ.

Branch names preserve exact UTF-8 spelling. Creation MUST reject a branch whose ref pathname would case-insensitively collide with an existing branch/ref namespace component. Command lookup still uses exact stored branch spelling; an implementation MUST NOT accidentally accept a wrong-case branch merely because a direct filesystem open would succeed case-insensitively.

## 7. Reparse Points and Symbolic Links

### 7.1 Classification first

Any working-tree entry carrying `FILE_ATTRIBUTE_REPARSE_POINT` MUST be classified before it is opened as ordinary file/directory content.

Only reparse points with tag `IO_REPARSE_TAG_SYMLINK` are versionable symbolic links in this assignment. Junctions, mount points, cloud placeholders, and all other reparse tags are unversionable, ignored, reported in the ignored summary, and MUST NOT be traversed as directories.

Repository metadata itself is stricter: no authoritative `.cvc` metadata component may be a reparse point of any tag.

### 7.2 Reading a Windows symbolic link

A supported symbolic link MUST be opened as the reparse point itself, without target traversal, and inspected from `FSCTL_GET_REPARSE_POINT` data.

The canonical stored target is the symbolic-link reparse buffer's **PrintName** converted from UTF-16 to UTF-8. A symbolic link with an empty PrintName, an embedded U+0000 in the length-delimited PrintName, or malformed/unpaired UTF-16 PrintName is unversionable and MUST be ignored with warning rather than dereferenced to guess a target.

The link's own directory/file kind is part of the committed tree entry, because Windows requires that distinction to recreate a dangling link. CVC determines this from the symbolic-link entry's own Win32 attributes (for example `WIN32_FIND_DATAW.dwFileAttributes` / `GetFileAttributesW`, which describe the symbolic link rather than its target): `FILE_ATTRIBUTE_DIRECTORY` set means directory symbolic link; clear means file symbolic link. This determination MUST NOT open the target and works for dangling links. The repository model defines separate file-symbolic-link and directory-symbolic-link tree entry types while both reference the same `symlink` object payload format.

CVC MUST NOT call a target-resolving API to replace this readback rule. Relative, absolute, dangling, and looping symbolic links remain valid without target access.

### 7.3 Restoring a Windows symbolic link

Restoration recreates a Win32 symbolic link with:

- the stored PrintName converted back to UTF-16;
- the tree entry's stored file-link or directory-link kind;
- no attempt to open the target to infer type.

When calling the native symbolic-link creation API on a host that recognizes `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE`, CVC MUST request that flag, combined with `SYMBOLIC_LINK_FLAG_DIRECTORY` for a stored directory link. If the host rejects the unprivileged-create flag itself as unsupported, CVC MAY retry the same requested link without that flag; it MUST NOT change the stored link kind or target.

If Windows still denies symbolic-link creation because of account/host policy, the operation MUST fail nonzero under the ordinary detected-failure/rollback rules. CVC MUST NOT silently substitute a regular text file, junction, copied target, or dereferenced content.

## 8. Repository Locking with `LockFileEx`

`.cvc/lock` remains a persistent zero-byte ordinary file. Lock state is kernel state, never payload bytes.

CVC MUST open the existing lock file without truncation and with sharing that permits competing CVC processes to open the same lock file, then use `LockFileEx` nonblocking locks over exactly one byte beginning at offset zero. Windows permits locking a byte range beyond current EOF, so the lock file remains zero bytes.

Required modes:

- read-only repository command: `LOCKFILE_FAIL_IMMEDIATELY`, without `LOCKFILE_EXCLUSIVE_LOCK`;
- mutating repository command: `LOCKFILE_FAIL_IMMEDIATELY | LOCKFILE_EXCLUSIVE_LOCK`.

The range is offset `0`, length `1`. The lock is held for the same command scopes defined by the error/recovery specification. Multiple readers may coexist; any writer conflicts with readers/writers. Lock acquisition failure returns repository-busy nonzero status without waiting indefinitely.

`UnlockFileEx` is used on normal release; closing the handle/process termination releases the lock as well. There is no PID stealing, timestamp-based stale-lock recovery, or deletion/recreation of `.cvc/lock`.

## 9. Durable Object and Metadata Installation

All repository-controlled temporary files used to install an object/ref/HEAD/state file MUST be created inside the repository on the same volume as the destination, with collision-resistant names and create-new semantics so an existing pathname is never truncated accidentally.

Before publication, CVC writes the complete bytes and successfully calls `FlushFileBuffers` on the temporary file handle.

For a new immutable object, publication MUST install the temporary file at the final object pathname without exposing partial bytes. If the final object appeared concurrently, CVC validates and reuses it only when it is the identical valid canonical object.

For mutable metadata/ref replacement, publication MUST use a same-volume Win32 rename/replacement primitive that provides old-or-new pathname visibility rather than in-place truncation. `MoveFileExW` with `MOVEFILE_REPLACE_EXISTING`/`MOVEFILE_WRITE_THROUGH` where applicable, or `ReplaceFileW` after the replacement file itself has already been successfully flushed, are acceptable when used consistently with the repository writer lock and rollback protocol. CVC MUST NOT rely on `REPLACEFILE_WRITE_THROUGH` for durability because that flag is not supported; durability comes from flushing the complete replacement file before publication plus the required same-volume publication ordering. A ref MUST NOT move until every newly referenced object has already been fully written, flushed, installed, and hash-validated.

Windows does not provide a single portable equivalent of POSIX directory `fsync` with identical semantics. This task therefore requires file-handle flushing plus same-volume write-through/replacement ordering; it does not require an undocumented directory-flush trick.

## 10. Working-Tree Replacement and Hard Links

Regular-file replacement MUST use a new temporary file and rename/replacement of the directory entry. It MUST NOT open an existing tracked pathname and truncate/write it in place because the pathname may be one of several NTFS hard links.

Deleting a tracked regular file removes only that pathname. Replacing one tracked hard-linked pathname intentionally breaks that link relationship at the replaced path while preserving the bytes observable through the other hard-link name.

Hard-link identity is never stored or restored.

## 11. Windows Metadata Not Versioned

CVC snapshots do not version or promise restoration of:

- ACL/security descriptors;
- owner information;
- DOS read-only/hidden/system/archive attributes;
- creation/access/modify timestamps;
- NTFS compression/encryption/sparse flags;
- alternate data streams;
- short 8.3 names;
- hard-link identity;
- object IDs/file IDs.

Content/type/path are authoritative. A native permission, sharing violation, or read-only condition that prevents a requested materialization is an ordinary detected filesystem failure and is subject to the all-or-rollback behavior defined elsewhere; CVC MUST NOT report success after a partial update.

## 12. Metadata Reparse-Point Defense

Repository discovery and every authoritative metadata open MUST ensure that `.cvc` and the relevant metadata path components are real non-reparse directories/files of the required type. A junction/symlink inserted into `.cvc`, `.cvc/refs`, `.cvc/objects`, `.cvc/state`, a fan-out directory, or an authoritative file path MUST be rejected rather than followed.

Fixed metadata names created by CVC use the exact spelling shown by the repository layout (`.cvc`, `HEAD`, `config.json`, `refs`, `heads`, `objects`, `state`, `lock`). Because Windows can open a wrong-case alias, verification MUST enumerate/check actual stored spelling and treat a case-only spelling change of a fixed metadata name as noncanonical metadata. Repository discovery may notice a case-aliased `.cvc`, but normal validation then rejects it unless the actual directory spelling is exactly `.cvc`.

Likewise, object fan-out directory names and object filenames MUST be actual lowercase hexadecimal spellings. An uppercase or mixed-case pathname that would alias a canonical object path on Windows is an integrity error, not a valid alternate object spelling or ignorable temporary file.

Component-by-component validation is required where a single final-path open could otherwise traverse an intermediate reparse point.

The same safety principle applies during working-tree materialization: CVC MUST NOT traverse an existing reparse point in a way that causes a write/delete outside the repository root.

## 13. Native Sharing and Busy Files

Windows may reject rename/delete/replacement of a file that another process has open without compatible sharing. Such a sharing violation is a normal detected failure. CVC MUST preserve/restore the pre-command tracked state and controlling ref/HEAD according to the operation's rollback rules; it MUST NOT weaken correctness by truncating an already-open destination in place.

Acceptance may deliberately hold a destination file open with restrictive sharing to test this failure path.

## 14. Cross-Platform Compatibility

The Windows and POSIX task packs describe the same product concept but are separate assignments. Cross-platform repository interchange is not a required feature.

In particular, the Windows v1 tree format distinguishes file symbolic links from directory symbolic links. Implementations MUST follow this Windows pack's canonical format and MUST NOT claim compatibility with repositories produced by the POSIX task pack unless they implement an optional converter outside the required scope.
