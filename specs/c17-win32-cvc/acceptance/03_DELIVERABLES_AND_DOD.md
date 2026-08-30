# Deliverables, Definition of Done, and Release Gates

## 1. Required Submission Contents

The implementation submission MUST contain:

1. all production C source and headers;
2. build files necessary for a clean build;
3. automated tests and their fixtures;
4. a test runner or documented single test command;
5. user-facing README with build and usage instructions;
6. implementation notes describing repository serialization format;
7. implementation notes describing any permitted choice points in this spec;
8. test results/evidence from a clean run;
9. license information for any non-production test-only material if applicable.

Generated build outputs are not required as source deliverables.

## 2. Required Build Property

A clean checkout/extraction MUST be buildable without editing source files or hard-coded absolute paths.

Warnings are not automatically failures, but severe warnings indicating undefined behavior, type truncation, missing prototypes, or ignored I/O errors MUST be addressed.

## 3. Definition of Done

The task is DONE only when all conditions are true:

- all required CLI commands exist and are wired to real behavior;
- repository uses content-addressed loose objects in the fixed canonical v1 format;
- handwritten SHA-256 passes known vectors;
- handwritten JSON parser plus deterministic UTF-8 validation and UTF-8/UTF-16 Windows-boundary conversion meet required cases;
- handwritten glob matcher and repository traversal meet grammar/scope rules without delegated walkers;
- handwritten Myers shortest-edit diff works and byte-safe diff rendering preserves the Windows UTF-8 output contract for arbitrary eligible blob bytes;
- branch/switch safety semantics work;
- fast-forward and divergent merge work;
- conflict state, exact-root resolve, re-resolution checks, continue, and abort work;
- rollback creates a new history-preserving commit;
- Windows file/directory symbolic-link storage/restoration works without dereference and unsupported reparse points are never traversed;
- binary and >8 MiB regular files are excluded as specified;
- atomic ref/object write rules and detected-failure working-tree rollback rules are implemented;
- required Win32 `LockFileEx` shared/exclusive byte-range repository locking exists and `.cvc/lock` remains zero bytes;
- `verify` detects mandatory corruption classes;
- mandatory tests pass;
- manual checklist has no known mandatory failures;
- no prohibited substitute implementation is used.

## 4. Release Gates

### Gate R1 — Build

PASS when production executable and tests build from clean submitted sources.

FAIL if build requires missing undocumented generated source or manual patching.

### Gate R2 — Core Snapshot Integrity

PASS when init/save/status/log/diff/restore operate on real loose content-addressed snapshots, canonical v1 object bytes independently validate, and object hashes verify.

FAIL if snapshots are directory copies masquerading as object storage.

### Gate R3 — Parser and Filter Correctness

PASS when JSON parser and glob matcher satisfy mandatory tests.

FAIL if parsing/matching is delegated to external libraries or commands.

### Gate R4 — History and Branches

PASS when branches form real commit history and switch protects local data.

FAIL if branches are independent copied directories without shared object/history semantics.

### Gate R5 — Merge

PASS when merge-base, recursive tree merge, fast-forward, three-way clean text merge, conflict, resolve/re-resolve, continue, and abort behaviors pass.

FAIL if merge always chooses one side, always conflicts, or shells out to Git/diff/patch.

### Gate R6 — Rollback

PASS when rollback creates a new commit with target tree and old HEAD parent.

FAIL if it merely moves a branch pointer backward.

### Gate R7 — Safety and Verification

PASS when Win32 reader/writer locking, flushed same-volume object/ref update strategy, corruption detection, Unicode/long-path behavior, and reparse-point traversal defenses are demonstrated.

FAIL if repository can knowingly point to incomplete/missing objects after ordinary injected failure tests.

### Gate R8 — Test Completeness

PASS when all mandatory categories in `01_TEST_MATRIX.md` have real tests or a clearly equivalent mapping.

FAIL if mandatory functionality is marked skipped, TODO, expected-fail, placeholder, or untested without equivalent evidence.

### Gate R9 — Prohibited Implementations

PASS only when none of `04_PROHIBITED_IMPLEMENTATIONS.md` apply.

## 5. Stop Conditions

An implementer MUST NOT claim completion while any Release Gate is failing.

If work stops with failures remaining, the submission MUST explicitly state:

- which gates fail;
- which tests fail;
- what functionality remains incomplete.

A polished README, demo transcript, or partial UI-like terminal output cannot substitute for passing gates.

## 6. Non-Gates

The following are not completion criteria by themselves:

- source-line count;
- token count;
- implementation time;
- benchmark runtime speed;
- number of files/modules;
- visual polish of terminal output.

These may be observed externally but do not replace functional acceptance.

## 7. Optional Enhancements

Optional features are allowed only after all mandatory behavior is complete and provided they do not alter required semantics.

Examples:

- colored terminal output with automatic non-TTY disable;
- extra log formatting;
- extra verification detail;
- performance caches that preserve correctness.

Optional features MUST NOT become hidden dependencies for required tests.
