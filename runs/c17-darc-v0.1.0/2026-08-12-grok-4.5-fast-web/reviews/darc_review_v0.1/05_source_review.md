# Source Review

## Positive observations

The implementation is not a pure mock. Several components are real and useful:

- project-owned SHA-256 and CRC-32C implementations;
- deterministic Buzhash table and a content-defined boundary engine;
- actual LZ77/Huffman compressor/decompressor rather than shelling out;
- immutable content-addressed object framing with CRC and CID checks on reads;
- a project-owned Robin Hood-style in-memory index;
- recursive directory traversal, symlinks, content objects, trees, snapshots;
- parity creation/recovery code exists;
- test scripts execute the public CLI for basic workflows.

These make the project a plausible intermediate implementation rather than a placeholder-only submission.

## Architectural issue: representation shortcuts collapse distinct semantics

The most consequential shortcut is using FILE CID equality as both content identity and hardlink topology. Content dedup and filesystem identity are different concepts. Because the format does not carry a distinct hardlink record/group, restore cannot distinguish two independent equal-content files from two links to one inode.

This is a good example of why the task pack intentionally made metadata/topology first-class.

## Architectural issue: verification is physical-object-centric rather than graph-centric

The verifier checks objects that happen to be present. A backup repository's integrity, however, is defined by the transitive closure of published roots. Verification must start from refs and prove every expected edge and object exists and is semantically valid. Physical scanning alone cannot detect an object that is missing precisely because it is no longer there to scan.

## Architectural issue: transaction model is not encoded

The journal lacks operation stages and intended mutations, so it cannot decide whether to roll forward or roll back specific publication states. Deleting a stale journal is cleanup, not crash recovery.

## Configuration architecture

The config parser and runtime are loosely coupled. The parser accepts many keys but silently ignores them, while production logic still uses compile-time constants or alternate key spellings. A stronger implementation should normalize JSON/YAML into one complete typed configuration object, validate it once, compute the required hashes, and pass that object explicitly to every subsystem.

## Fixed-size path handling

There are numerous `PATH_MAX` buffers and `snprintf` warnings. This is inconsistent with the specification's dynamic path requirement and creates truncation ambiguity. A repository tool manipulating arbitrary user paths should centralize checked dynamic path joining and, for restore safety, rely on directory file descriptors rather than concatenated absolute strings.

## Error propagation

Many lower-level failures collapse to `-1`, and CLI code frequently converts them to generic `E_IO`. The specified stable error taxonomy is not implemented deeply enough to distinguish corruption, conflict, lock, unsupported format, unsafe restore path, etc.

## Testability/fault injection

OS operations are called directly throughout core code. There is no narrow fault-injection layer for deterministic short writes, fsync/rename failures, allocation failures, or named crash checkpoints. This makes the required crash-safety suite difficult to implement faithfully.

## Performance design

Reading one byte per syscall in `process_file()` is a major scalability problem. The algorithms can remain byte-stream incremental while reading blocks (e.g. tens or hundreds of KiB) and feeding bytes from the block to the rolling hash. The current approach would distort any large-file stress test.

## Documentation-to-code traceability

The delivered `docs/repository_format.md` is only about ten lines and cannot serve as an implementation-accurate description of a format with multiple canonical object types, framing, parity, index, journal, and deterministic rules. Likewise `TRACEABILITY.md` is area-level rather than test-ID-level.
