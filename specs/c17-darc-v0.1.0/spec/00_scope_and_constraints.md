# 00 — Scope and Engineering Constraints

## 1. Product objective

DARC is a local, versioned, deduplicating archival repository. A user initializes a repository, snapshots one or more source paths, inspects historical snapshots, compares them, restores them, verifies repository integrity, repairs a bounded class of damage, and garbage-collects unreachable objects.

The product is headless. It MUST NOT require a graphical display server to build, test, or operate.

## 2. Language and platform contract

- All production implementation code MUST be C17.
- All required algorithms named in this specification MUST be implemented in project-owned C source code.
- The target operating-system model is Linux/POSIX.
- The implementation MAY use ISO C17 and low-level POSIX operating-system interfaces for filesystem traversal, file metadata, hard links, symbolic links, file locking, memory mapping, clocks, fsync, rename, and similar OS-boundary functions.
- POSIX interfaces are an operating-system boundary, not permission to delegate required algorithms to prebuilt utilities.
- The implementation MUST build without C++, Rust, Go, Java, Python, Node.js, or other production runtime dependencies.
- The mandatory build and mandatory automated test suite MUST be self-contained in C17/POSIX plus the project build files. Optional convenience analysis scripts in other languages MAY be included, but completion MUST NOT depend on them.

## 3. Forbidden substitutes

The following are prohibited for required production behavior:

- Third-party libraries for archiving, compression, hashing, cryptography, databases, Merkle trees, chunking, JSON parsing, YAML parsing, diffing, repository storage, or recovery.
- Linking against zlib, libarchive, OpenSSL, libsodium, SQLite, LMDB, LevelDB, RocksDB, xxHash, or equivalent substitutes for required algorithms.
- Invoking external commands such as `tar`, `zip`, `gzip`, `bzip2`, `xz`, `zstd`, `sha256sum`, `openssl`, `find`, `rsync`, `diff`, `cp`, `sqlite3`, or another archive/backup program to perform required product logic.
- `system()`, `popen()`, shell pipelines, or temporary shell scripts used to outsource required product behavior.
- Placeholder functions that return fixed success, fixed hashes, canned statistics, or hard-coded test answers.
- Fake deduplication that only compares file names, timestamps, or whole-file sizes.
- Fake incremental backup that always rewrites every object into a new snapshot namespace.
- Fake recovery that silently deletes corrupted data, substitutes zero bytes, or reports success without reproducing the original bytes.
- A repository represented only as a loose collection of source files plus JSON manifests.
- Tests that bypass the public CLI for functionality that is specified as CLI-visible acceptance behavior.

## 4. Required self-implemented algorithms and subsystems

At minimum the project MUST contain project-owned implementations of:

- 64-bit rolling Buzhash used for content-defined chunk boundaries.
- SHA-256 used for cryptographic content IDs and Merkle hashes.
- CRC-32C used for framed-object/header damage detection.
- LZ77 tokenization.
- Canonical Huffman coding and decoding.
- JSON parser for the required configuration/data subset defined by this task pack.
- YAML parser for the required YAML subset defined by this task pack.
- In-memory Robin Hood hash table used by the chunk/object index implementation.
- Canonical deterministic serialization/deserialization of repository records.
- Merkle tree construction and verification.
- XOR parity stripe generation and single-member recovery.
- Transaction/journal state handling for crash-safe snapshot publication.

## 5. Major functional scope

DARC MUST support:

- repository initialization;
- scanning directory trees;
- regular files;
- directories;
- symbolic links;
- hard links;
- zero-length files;
- sparse files, at least functionally as byte streams even if hole layout is not preserved;
- binary file contents;
- path names that are valid Linux path bytes, including non-ASCII names;
- snapshots with parent linkage;
- safe snapshot-ref deletion prior to GC;
- incremental snapshot creation;
- diffing snapshots;
- restoring snapshots;
- chunk and object deduplication;
- index rebuilding;
- integrity verification;
- bounded repair using parity data;
- unreachable-object garbage collection;
- interrupted-operation recovery;
- deterministic encoding;
- JSON/YAML configuration;
- human-readable and machine-readable reporting.

## 6. Deliberate non-goals for v0.1.0

Unless another document explicitly states otherwise, the following are out of scope:

- network transport or cloud APIs;
- remote repositories;
- GUI/TUI interfaces;
- encryption-at-rest;
- multi-writer distributed coordination;
- Windows/macOS portability;
- preservation of ACLs, xattrs, SELinux labels, capabilities, or filesystem-specific compression flags;
- perfect preservation of sparse-hole topology;
- device nodes, sockets, FIFOs, or mounted-filesystem recursion;
- full SQL-like querying;
- full RFC-grade JSON Schema engine;
- full YAML 1.2 language support beyond the required subset.

Out-of-scope items MUST NOT be presented as completed mandatory features.

## 7. Repository safety principles

- Objects are immutable after publication.
- Snapshot publication is atomic: a snapshot is either absent or fully referenceable.
- Derived indexes may be rebuilt from canonical objects.
- A failed or interrupted command MUST NOT make a previously valid snapshot unreadable.
- Destructive operations MUST have a dry-run mode where specified.
- Restore MUST refuse dangerous path traversal.
- Verification MUST never silently mutate repository state unless an explicit repair option is supplied.
- Repair MUST never claim success when recovered bytes differ from the cryptographic content ID.

## 8. Streaming and bounded-memory requirement

- Regular file payloads MUST be processed as streams/chunks.
- The implementation MUST NOT require loading an entire arbitrary-size input file into RAM.
- Compression and hashing MUST operate chunk-by-chunk.
- Snapshot manifests MAY be accumulated in memory only if a tested fallback or streaming design prevents memory use from growing with total file payload size.
- A single 2 GiB file is a valid logical input even if acceptance fixtures use smaller files.

## 9. Determinism definition

"Deterministic archive format" means that, given identical semantic input records and identical explicit configuration parameters, canonical object payload bytes and object IDs MUST be identical across runs on the same format version.

Wall-clock creation time is snapshot metadata and therefore normally changes a snapshot record. Tests that require bit-for-bit reproducibility MUST use the specified fixed timestamp override. Chunk, file, tree, parity, and other content-derived objects MUST remain deterministic regardless of wall-clock time.

## 10. Quality bar

This task is not satisfied by a teaching demo. Required behaviors must be integrated, failure-aware, test-covered, and inspectable through the public CLI. Every mandatory feature must have success tests, negative tests, and at least one edge/corruption test where relevant.
