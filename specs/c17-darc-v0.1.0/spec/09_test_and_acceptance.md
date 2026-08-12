# 09 — Automated Testing and Acceptance Strategy

## 1. Test-suite ownership

The implementer MUST deliver the test code. This task pack supplies the mandatory behavior catalog, not prewritten product tests.

Tests are part of the implementation scope and count toward Definition of Done.

## 2. Required test layers

The delivered project MUST contain all of:

1. unit tests for algorithms and parsers;
2. repository-format golden tests;
3. CLI integration tests using temporary repositories;
4. end-to-end snapshot/diff/restore tests;
5. corruption-injection tests;
6. crash/fault-injection tests;
7. deterministic/reproducibility tests;
8. randomized/property/reference-model tests;
9. stress tests for many files and large streamed files.

## 3. Public CLI rule

Every user-visible command MUST have at least one test invoking the compiled `darc` executable as a process and checking exit status plus stdout/stderr.

Direct internal-function tests alone are insufficient for CLI acceptance.

## 4. Test isolation

- Each integration/E2E test uses a fresh temporary repository and source tree.
- Tests MUST NOT depend on execution order.
- Tests MUST NOT depend on wall-clock timing unless using generous synchronization and the behavior itself concerns timestamps/locking.
- Tests use fixed snapshot timestamps where exact IDs/bytes are asserted.
- Tests must clean up temporary resources on success and failure where possible.

## 5. Golden vectors

Golden tests MUST use the fixed values in `acceptance/GOLDEN_VECTORS.md` where supplied and include exact expected values for:

- SplitMix64-derived Buzhash table entries at several indices;
- CDC boundary offsets for fixed byte fixtures;
- SHA-256 standard vectors;
- CRC-32C vector;
- LZ77 token sequence for small fixtures;
- canonical Huffman code lengths/codes for fixed token bytes;
- LZH1 encoded payload bytes for at least three small fixtures;
- canonical FILE/TREE/SNAPSHOT payload bytes;
- object CIDs for those canonical payloads;
- normalized config hash for example JSON/YAML;
- deterministic SVG bytes for a fixed report fixture.

Golden values MUST be generated once from the specification/independent reasoning and checked into tests. Tests MUST NOT compute expected outputs by calling the same production function under test.

## 6. Independent reference-model testing

The mandatory C17/POSIX test suite MUST contain simple, test-only reference logic that is structurally independent from the production implementation for:

- filesystem snapshot semantic tree comparison;
- expected path diff classification;
- reachability sets;
- restore tree/content comparison.

Cryptographic/compression algorithm correctness uses the fixed independent vectors in `acceptance/GOLDEN_VECTORS.md` rather than duplicating the production algorithm as the expected-value generator. Optional convenience scripts may exist, but mandatory acceptance must run without them.

Reference logic MUST NOT replace production algorithms; it exists only in tests.

## 7. Randomized tests

At least one deterministic seeded randomized suite MUST generate trees containing:

- nested directories;
- empty files;
- repeated content;
- partially modified large files;
- duplicate files;
- hard links;
- symlinks;
- non-ASCII names;
- invalid-UTF-8 names where test platform permits;
- permission variations.

For each generated scenario:

1. snapshot;
2. mutate tree;
3. create incremental snapshot;
4. diff against reference model;
5. restore both snapshots to separate locations;
6. compare logical trees/content/metadata;
7. verify repository;
8. optionally GC then re-verify/restore.

Use fixed seeds and print failing seed.

## 8. Fault injection

Production OS calls SHOULD be wrapped narrowly enough for test builds to inject failures at deterministic points, including:

- open;
- read;
- write/short-write;
- fsync;
- rename;
- mkdir;
- link;
- symlink;
- allocation.

The test suite MUST simulate crashes by terminating the process at named transaction checkpoints and reopening the repository with a new process.

## 9. Crash checkpoint coverage

At minimum inject termination:

- before first object publish;
- after one new chunk publish;
- after all objects publish but before snapshot ref;
- immediately after snapshot ref;
- before HEAD rename;
- after HEAD rename before index update;
- during parity replacement in GC;
- after new parity durable before old parity deletion;
- during index rewrite;
- after journal completion marker before journal removal.

Each case must prove previous snapshots remain valid.

## 10. Corruption injection

Tests mutate raw repository bytes directly, not through production APIs, and then invoke verify/restore.

Required cases are cataloged in `acceptance/TEST_CATALOG.md`.

## 11. Determinism tests

With fixed source tree metadata, config, and `--timestamp`:

- two independently initialized repositories must produce identical content-derived object CIDs;
- canonical object payload bytes must match;
- snapshot CID must match when all snapshot metadata matches;
- persistent index bytes after rebuild must match;
- text reports with deterministic ordering and no timestamps beyond fixture values must match where specified;
- SVG report bytes must match.

Directory enumeration order MUST be deliberately perturbed in a test harness or fixture creation order to prove sorting independence.

## 12. Streaming tests

A test MUST create a file significantly larger than `max_bytes` and verify many chunks are produced without any single read request equaling the whole file.

The test harness MAY expose wrapped `read` call sizes/counters to assert streaming behavior.

## 13. Performance sanity tests

No environment-specific speed score is required. However, tests MUST demonstrate the implementation can complete without algorithmic failure on:

- 20,000 small files;
- at least 512 MiB total logical data across a mixed fixture;
- a single 512 MiB file generated locally for streaming;
- at least 10 snapshots with overlapping content;
- GC over a repository containing at least 100,000 chunk references.

These are functional stress gates, not timed benchmarks. Fixture generation may be sparse/patterned to keep setup practical, but content must exercise actual reading/chunking where the test claims it.

## 14. Leak/sanitizer tests

When the available compiler supports sanitizers, the project SHOULD provide a documented test target for address/undefined behavior sanitizers. This is recommended but not a mandatory cross-environment Release Gate.

Regardless of sanitizer availability, repeated randomized tests must not crash or exhibit undefined behavior.

## 15. Test command contract

The project MUST provide a single documented command, typically `make test`, that builds C17 test binaries/helpers and runs the mandatory automated suite using only the mandatory project toolchain/OS boundary.

Long stress tests MAY be separated as `make test-stress`, but Release Gate documentation must show they were run before completion is claimed.

## 16. Test result reporting

Test runner output MUST include:

- total passed;
- total failed;
- total skipped;
- failing test IDs;
- randomized seed when relevant.

Mandatory tests may only be skipped for a clearly documented platform capability that is itself outside the mandatory target. Core DARC tests may not be skipped to claim completion.

## 17. Acceptance catalog relationship

Every row in `acceptance/TEST_CATALOG.md` is mandatory unless marked `SHOULD`. The implementation's test names may differ, but a traceability document MUST map every catalog ID to one or more concrete test files/functions.

## 18. Test anti-cheating rules

Prohibited:

- detecting fixture file names to return canned results;
- embedding expected snapshot IDs only for tests while bypassing real algorithms;
- test-only code path replacing production compression/chunking/hash logic;
- accepting corruption in test mode that production rejects or vice versa;
- marking a mandatory test "expected fail" and still claiming completion.
