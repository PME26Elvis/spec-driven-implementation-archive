# Algorithms and Data Structures

All required algorithms below are implemented in project C source; production does not call external compression, hashing, archive, traversal, JSON, or YAML tools.

## Content-defined chunking

`src/buzhash.c` implements 64-bit Buzhash rolling CDC. The 256-entry table is deterministically derived with SplitMix64. The default 64-byte rolling window uses minimum/average/maximum boundaries of 16 KiB / 64 KiB / 256 KiB. Since average size is a power of two, natural boundaries use the low-bit mask; max size forces a boundary. Files are read in bounded 64 KiB I/O buffers.

## Hashing and checksums

`src/sha256.c` is a project-owned SHA-256 implementation used for semantic CIDs, file full digests, and normalized config/profile hashes. `src/crc32c.c` implements CRC-32C for framed-object and derived-format corruption detection.

## Compression

`src/lzh1.c` implements deterministic LZ77 tokenization followed by canonical Huffman coding. CHUNK storage chooses LZH1 only when its encoded payload saves at least `compression.min_savings_bytes`; otherwise raw storage is retained. Decoding is bounded by the declared semantic length and rejects malformed/over-expanding streams.

## Merkle model

CHUNK CIDs are referenced by FILE; FILE/TREE children are referenced by TREE; SNAPSHOT references a root TREE. Recomputing and validating these content-derived identifiers provides Merkle integrity. Full restore also checks FILE's complete logical SHA-256 before atomic publication.

## Deduplication and incremental path

A semantic CHUNK is stored once per CID. Independent equal files can share chunks while remaining separate files/inodes after restore. Hardlink identity is represented separately in TREE topology.

The incremental scan cache may trust unchanged stat identity to reuse a prior FILE CID; canonical objects never contain device/inode cache identity. Within one snapshot transaction, a CID that has already been fully verified is cached as verified, preventing repeated decompression of the same immutable duplicate while retaining first-use integrity validation.

## Robin Hood index

`src/rhmap.c` implements the in-memory Robin Hood hash table. `index/chunks.idx` is a deterministic persistent CID-sorted derived index with checksum; `index rebuild` reconstructs it from immutable object files.

## XOR parity

New CHUNK CIDs are sorted and grouped into stripes of up to eight data members. The PARITY semantic payload stores member CIDs/lengths and an XOR byte vector padded to the largest member length. One unavailable member can be reconstructed from the remaining members plus parity and is accepted only after recomputing the expected semantic CID. Two unavailable members in one stripe are unrecoverable.
