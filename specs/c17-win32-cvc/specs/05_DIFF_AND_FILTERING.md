# 05 — Diff, Diffstat, and Path Filtering

## 1. Required Diff Algorithm

Line-oriented regular-file diff MUST use a handwritten implementation of the **Myers shortest edit script algorithm** for ordinary diff computation.

A naive whole-file replacement strategy is nonconforming.

The required production diff engine is Myers. A dynamic-programming LCS implementation, alternate shortest-edit algorithm, or whole-file fallback MUST NOT replace Myers for required diff computation. If resource exhaustion prevents Myers from completing, the command fails safely rather than silently switching to a different required-output algorithm.

External `diff`/`git diff` invocation is forbidden.

## 2. Line Model

A line is a byte sequence ending in `\n`, except that the final line may be unterminated.

The line terminator belongs to the line for reconstruction purposes.

Comparison is byte-exact.

CVC MUST NOT normalize:

- CRLF to LF;
- tabs;
- spaces;
- Unicode normalization;
- case.

Changing only line ending bytes therefore counts as a textual change.

## 3. Diff Correctness

The edit script MUST transform old file bytes into new file bytes when applied in order.

The reported insertion/deletion totals are line-operation counts:

- inserted line => +1 insertion;
- deleted line => +1 deletion;
- replacement => one or more deletions plus insertions as determined by shortest edit script.

Empty-to-empty yields zero edits.

## 4. Diff Output

Human output MUST identify:

- old/new path;
- added/deleted/modified state;
- old versus new lines with clear prefixes or markers;
- lack of final newline when necessary to avoid ambiguity.

Exact unified-diff hunk syntax is optional.

### 4.1 Byte-safe line rendering on Windows

Eligibility is deliberately **not** a UTF-8-content test. An eligible regular file may contain arbitrary non-NUL-prefix bytes, including malformed UTF-8 and NUL bytes after the first 8192-byte probe. Therefore human diff rendering MUST NOT write arbitrary blob bytes directly when doing so would violate the Windows UTF-8 stdout/stderr contract.

For displayed file-line payload, CVC MUST use this deterministic byte-safe rendering:

- valid non-ASCII UTF-8 scalar sequences are emitted as their original UTF-8 bytes;
- printable ASCII bytes `0x20` through `0x7e` are emitted literally **except** backslash, which is emitted as `\\`;
- tab is emitted as `\t`, carriage return as `\r`, and backspace/form-feed as `\b` / `\f`;
- every other ASCII control byte, NUL byte, DEL, and every byte that is not part of a valid UTF-8 scalar sequence is emitted as uppercase `\xHH`;
- the structural line-ending `\n` that terminates a line is not rendered as payload text; the existing no-final-newline indication distinguishes terminated from unterminated final lines.

This rendering is for human diff output only. It MUST NOT alter hashing, blob bytes, diff comparison, merge comparison, or insertion/deletion counts. The rendered stream is valid UTF-8 and unambiguously represents every underlying byte.

## 5. Save Diffstat

For each changed eligible regular text file selected by diffstat filters, `save` computes insertion/deletion counts between the parent snapshot and new snapshot.

For a newly added regular text file:

- insertions = number of lines in new file;
- deletions = 0.

For deletion:

- insertions = 0;
- deletions = number of lines in old file.

Empty file has zero lines for diffstat purposes.

A one-byte file with no newline has one line.

Windows symbolic-link target/kind changes do not add line counts. Directory containers do not contribute lines and are not independently reported merely for existing.

## 6. Tracking Filters

Tracking filters in `config.json` determine which otherwise eligible **versionable leaf paths** (regular files and supported Windows symbolic links) enter saved tree snapshots. Directories are structural and are not included merely because a pattern matches the directory name; directory patterns influence results through matching descendants and through safe traversal/pruning. They are repository policy, not transient display options.

Filter evaluation order:

1. built-in exclusions/safety rules;
2. tracking include list;
3. tracking exclude list.

A path is selected only when:

- it is eligible under built-in rules;
- it matches at least one include pattern;
- it matches no exclude pattern.

Exclude therefore has final precedence.

## 7. Diffstat Filters

Diffstat filters affect only presentation/statistics.

They MUST NOT change what is stored in a commit.

A file may be tracked and saved while excluded from diffstat output.

For `cvc save`, CLI `--include` and `--exclude` replace the configured diffstat lists for that invocation only. For `status` and `diff`, the same option spellings filter displayed paths only. No ordinary CLI include/exclude option changes repository tracking membership.

## 8. Command-Line Pattern Lists

CLI display/diffstat options use comma-separated patterns:

```text
--include=src/**,docs/**
--exclude=build/**,*.log
```

Comma is a separator and cannot be escaped in v1. Therefore filenames/patterns containing literal comma cannot be directly represented through CLI pattern options; they can still be matched through broader patterns in JSON config.

Whitespace is literal pattern data. The parser MUST NOT silently trim pattern elements. CLI pattern bytes MUST form valid UTF-8.

An empty element such as `a,,b` or trailing comma MUST be rejected.

## 9. Glob Grammar

Patterns operate on canonical repository-relative paths using `/`.

Required metacharacters:

- `*` matches zero or more bytes other than `/`;
- `?` matches exactly one byte other than `/`;
- `**` matches zero or more bytes including `/`;
- all other bytes match literally.

A run of three or more consecutive `*` bytes is invalid and MUST be rejected at configuration/CLI validation time rather than tokenized differently by different implementations. No character classes, braces, escaping, extglob, or regex semantics are required.

### 9.1 Examples

`*.c` matches:

- `main.c`

but not:

- `src/main.c`

`src/*.c` matches:

- `src/a.c`

but not:

- `src/lib/a.c`

`src/**` matches descendant path strings such as:

- `src/a.c`
- `src/lib/a.c`

Directories themselves are structural rather than independently tracked, so no separate snapshot entry for bare `src/` is implied.

`**/*.md` matches Markdown files at any depth, including root-level `README.md`.

The implementation MUST define `**/` so zero directory components are allowed.

## 10. Directory Traversal with Filters

An implementation MAY prune directory traversal for efficiency only when it can prove no include pattern can match any descendant.

Incorrect pruning that causes missed matches is a correctness failure.

## 11. UTF-8 and Pattern Matching

Repository paths used by acceptance are valid UTF-8, but glob matching is byte-oriented except that `/` retains separator semantics. `?` therefore consumes exactly one byte, not one Unicode scalar value or grapheme. Matching is case-sensitive on canonical UTF-8 bytes even though the native Windows namespace is commonly case-insensitive; platform collision rules are separate from glob semantics.

This is sufficient for exact Chinese filename matching and wildcard matching over UTF-8 byte sequences.

The implementation MUST NOT corrupt or truncate multibyte path bytes.

## 12. Rename Semantics

Rename detection heuristics are out of scope.

If `a.txt` disappears and identical content appears at `b.txt`, status/diff may report delete + add.

Content-addressed storage still deduplicates the blob bytes.

## 13. Type Changes

Changes among regular file, file symbolic link, directory symbolic link, and structural directory are type changes. A file-link <-> directory-link change is a type change even when the stored symlink target object ID is unchanged.

A type change is represented as deletion of the old entry plus addition of the new entry for diff purposes.

Line diff is performed only for the regular-file side(s) where meaningful; no attempt is made to diff a regular file against a symbolic-link target as if both were normal file contents.
