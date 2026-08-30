# 01 — Scope and Engineering Constraints

## 1. Product boundary

`tabletool` is a non-interactive command-line table transformation program.
A run is driven by a UTF-8 batch script.
The program loads one table into an active in-memory table, executes commands in script order, and writes requested outputs to files.

The program is not required to provide an interactive terminal UI.
There is no prompt loop and no full-screen terminal interface.

## 2. Required language level

The implementation must be valid ISO C17 source code and target a hosted C17 implementation.
Product source may use headers and facilities from the ISO C17 standard library.

Examples of allowed standard headers include, but are not limited to:

- `<assert.h>`
- `<ctype.h>`
- `<errno.h>`
- `<float.h>`
- `<inttypes.h>`
- `<limits.h>`
- `<math.h>` when needed for non-core calculations
- `<stdbool.h>`
- `<stddef.h>`
- `<stdint.h>`
- `<stdio.h>`
- `<stdlib.h>`
- `<string.h>`
- `<time.h>` when used without relying on host timezone/locale semantics

Implementation source must not require non-standard headers for correct product behavior.

### Practical C17 platform profile

This assignment intentionally targets ordinary modern C implementations rather than every theoretical C17 execution environment.
Mandatory behavior assumes:

- `CHAR_BIT == 8`;
- byte values for ASCII characters `0x00` through `0x7F` have their conventional ASCII meanings;
- `uint8_t`, `uint32_t`, and `uint64_t` are available through `<stdint.h>`;
- ordinary binary files can be opened with the C standard I/O API.

An implementation may reject compilation on a target that does not satisfy this profile.
Doing so is not a portability defect for this assignment.

### Binary file access

Whenever TableTool itself opens a script, table file, report, or generated SVG, it must use binary-mode standard C file access (`rb`, `wb`, or the equivalent mode containing `b`).
On systems where text and binary modes are identical this has no observable effect.
On systems where text mode translates line endings, TableTool must still read and write the exact bytes required by this specification.

LF/CRLF interpretation and output-LF generation are therefore implemented by TableTool, not delegated to host text-mode translation.

## 3. Portability rule

The product must not depend on:

- POSIX APIs such as `unistd.h`, `dirent.h`, `mmap`, `fork`, `exec`, `popen`, or POSIX regex;
- Windows API headers or Win32 calls;
- Linux-specific `/proc`, `/sys`, inotify, epoll, or similar facilities;
- compiler intrinsics as the only implementation of required behavior;
- platform-specific path discovery;
- shell commands.

A build file may be included for convenience, but the program architecture and functionality must not depend on a particular build system.

## 4. Third-party code restriction

No third-party runtime or source library may provide required functionality.
This includes both linked dependencies and copied/vendorized implementations.

The following substitutions are specifically prohibited:

- SQLite or another database engine as the table model;
- a CSV/TSV parsing library;
- a JSON/YAML parser used to replace the required script parser;
- ICU or another Unicode library;
- `libcurl` or another URL parser;
- barcode libraries such as Zint, ZXing, bwip-js, or equivalent;
- regex libraries for implementing search or validation;
- external `sort`, `awk`, `sed`, `cut`, `csvkit`, `pandoc`, or similar utilities;
- invoking Python, Node.js, Ruby, Perl, Java, PowerShell, Bash, or another interpreter to implement product behavior.

The implementation may of course be developed or tested using any tools available to the implementer; this restriction applies to the submitted product behavior and implementation.

## 5. No disguised prototype

The following do not satisfy the assignment:

- hard-coded output for the supplied acceptance fixtures;
- parsers that only accept the exact example data;
- barcode SVG containing precomputed bars for known inputs;
- UI/mock output standing in for actual transformation logic;
- TODO branches for mandatory commands;
- placeholder functions returning success without applying the operation;
- silently ignoring unsupported or malformed input;
- treating every column as an unvalidated string while only printing a type name.

## 6. Executable behavior

The primary executable is named `tabletool` or the platform-equivalent executable filename.

The normal workflow is:

1. parse command-line options;
2. reject identical decoded/runtime `--script` and `--report` path values;
3. open/read the complete batch script in binary mode and close the script file;
4. validate script encoding and parse the entire batch script before execution;
5. if parsing fails, execute no product command, do not open or modify the requested report path, emit one concise stderr diagnostic, and exit with code 3;
6. for a syntactically valid script, perform the mandatory whole-script exact path-collision safety preflight;
7. if that preflight finds a collision involving the requested report path, do not open/modify that path, emit one concise stderr diagnostic, and exit with operation/domain code 5;
8. if that preflight finds a collision not involving the report path, open the safe report, record the failed run with zero commands executed, close it, and exit with operation/domain code 5;
9. if the path-collision preflight succeeds, open the run-report file in binary write mode;
10. execute commands in source order;
11. stop at the first command failure;
12. finalize and close the report, preserving the defined first-error/report-I/O rules;
13. release all owned resources;
14. exit with the defined status code.

The script parse phase has **no data-file side effects**.
In particular, a malformed script never truncates a pre-existing report, LOAD source, or output destination merely to describe the syntax error.

For a syntactically valid script, the mandatory whole-script preflight is limited to the exact path-collision safety rules in this specification.
Do **not** hoist later statement-level numeric/domain/data validation into this phase: those errors must be encountered in source execution order so two conforming implementations cannot disagree about which earlier commands completed.
A path-collision failure not involving the report path is recorded in the safe report with zero commands executed; a collision involving the report path uses stderr and leaves that path untouched.

## 7. Terminal-output policy

Normal data results must be written to files.
The program must not emit transformed tables or barcode graphics to stdout.

The following terminal output is allowed:

- `--help` text to stdout;
- `--version` text to stdout;
- one concise diagnostic to stderr for any failure that occurs before a safe report file has been opened, including invocation errors, script syntax/encoding errors, unsafe preflight path collisions, and report-open failure.

Once the report file is open, ordinary script/data diagnostics belong in that report.
The implementation may additionally emit no terminal text at all for such reported failures.

## 8. Memory-management expectations

The implementation must support dynamically sized tables and fields.
It must not rely on tiny fixed buffers such as 256-byte rows or 4 KiB CSV lines.

At minimum:

- a single STRING cell of at least 1 MiB must be representable if memory is available;
- CSV quoted records may contain embedded newlines;
- the number of rows is not fixed at compile time;
- the number of columns is not fixed at compile time;
- allocations must be overflow-checked before multiplication/addition used for sizes;
- allocation failure must be reported cleanly rather than causing undefined behavior.

The program may enforce documented protective limits, but acceptance limits must be at least:

- 1,024 columns;
- 100,000 rows;
- 1 MiB decoded text per cell;
- 8 MiB per script statement after line continuations are resolved.

## 9. Input immutability

Input files are read-only from the program's logical perspective.
No command may edit a loaded input file in place.

To avoid accidental self-destruction, path collisions are checked by exact equality of the **decoded runtime path values** that would be supplied to the C standard file API.
For script path operands this means quoted-string escape decoding has already occurred.
Different source spellings that decode to the same path string are therefore equal for this rule.

Before execution of a syntactically valid script:

- the report path must differ from the script path;
- the report path must differ from every LOAD source path;
- the report path must differ from every WRITE, FIND, and BARCODE-SHEET destination path;
- every WRITE, FIND, and BARCODE-SHEET destination must differ from the script path;
- every output destination must differ from every LOAD source path appearing anywhere in the script.

The preflight evaluates the complete parsed script as one safety set.
If **any** prohibited collision involves the requested report path, report-path safety takes precedence over other collision diagnostics: the report path is rejected **before it is opened**, must remain unmodified, and the failure is diagnosed on stderr with operation/domain exit code 5.
Only when no prohibited collision involves the report path may a different prohibited collision be recorded in that safely opened report; that run has zero commands executed and exit code 5.
When several same-class collisions exist, exact diagnostic wording/pair selection is not graded, but the report-safe versus report-unsafe behavior and exit code are mandatory.

Two output statements may intentionally use the same destination path; this is allowed and produces the mandatory warning defined elsewhere.

Resolving aliases, `.`/`..`, hard links, symlinks, or case-insensitive filesystem equivalence is out of scope.
Only exact decoded runtime path equality is required.

## 10. Determinism

For identical input bytes, identical script bytes, and identical command-line options, successful output bytes must be identical across runs, excluding the human-readable report timestamp if an implementation chooses to include one.

Required data behavior must not depend on:

- current locale;
- current timezone;
- hash-table iteration order;
- address layout;
- unspecified C sort stability;
- filesystem directory ordering.

## 11. Algorithms that must be owned by the implementation

The implementation must contain its own logic for:

- UTF-8 validation and code-point traversal needed by search;
- CSV state-machine parsing;
- TSV escape parsing;
- Markdown pipe-table parsing;
- exact DECIMAL comparison;
- Gregorian DATE validation/comparison;
- stable multi-key sorting;
- HTTP/HTTPS URL parsing and normalization;
- EAN-13 checksum and symbol encoding;
- Code 128 B/C path selection, checksum, and module encoding;
- SVG barcode-sheet construction.

The specification does not require a particular internal algorithm for stable sorting, but simply passing a non-total comparator to `qsort` is insufficient because `qsort` is not required to be stable.

## 12. Out of scope

The following are explicitly out of scope unless an implementer voluntarily adds them without breaking required behavior:

- GUI;
- curses/TUI;
- spreadsheet formulas;
- charts;
- SQL;
- joins;
- group-by aggregation;
- pivot tables;
- macros beyond the defined script language;
- networking or URL fetching;
- HTTP requests;
- QR Code;
- Data Matrix;
- barcode scanning/decoding from images;
- Unicode normalization NFC/NFD;
- locale-aware collation;
- full Unicode case folding;
- IDNA/punycode;
- IPv6 URL hosts;
- authentication/userinfo in URLs;
- cryptography;
- compression;
- concurrent execution;
- plugins.

## 13. Additional features

Additional features are permitted only when they do not alter required syntax or behavior.
Extensions must not make a mandatory input ambiguous.
Tests for required behavior must not depend on optional extensions.
