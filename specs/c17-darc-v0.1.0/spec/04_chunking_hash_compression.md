# 04 — Chunking, Rolling Hash, Cryptographic IDs, and Compression

## 1. Content-defined chunking goal

DARC divides regular-file byte streams into content-defined chunks so small insertions/deletions only perturb nearby chunk boundaries and unchanged regions remain deduplicable.

Fixed-size chunking alone does not satisfy the requirement.

## 2. Buzhash64 table generation

The rolling hash uses a deterministic 256-entry table generated at startup or compile time with SplitMix64.

Seed:

```text
0xD6E8FEB86659FD93
```

For table index `i` from 0 through 255:

```text
state = state + 0x9E3779B97F4A7C15  (mod 2^64)
z = state
z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9 (mod 2^64)
z = (z ^ (z >> 27)) * 0x94D049BB133111EB (mod 2^64)
z = z ^ (z >> 31)
T[i] = z
```

The initial `state` is the seed above.

## 3. Rolling hash window

Format v1 window size is exactly 64 bytes.

For the initial bytes of a chunk, before the window is full:

```text
h = rol64(h, 1) XOR T[in_byte]
```

After the window is full, for outgoing byte `out_byte` and incoming byte `in_byte`:

```text
h = rol64(h, 1) XOR T[in_byte] XOR rol64(T[out_byte], 64 mod 64)
```

Because the window is 64, `rol64(x, 0) == x`. The explicit expression is retained to define the general Buzhash rule.

At the start of every new chunk, rolling state and window are reset.

All arithmetic is unsigned modulo 2^64.

## 4. Boundary rule

Defaults:

```text
min = 16 KiB
avg = 64 KiB
max = 256 KiB
```

After `min` bytes have accumulated, cut a chunk when:

```text
(h & (avg - 1)) == 0
```

where `avg` MUST be a power of two.

If no natural boundary occurs, force a cut at `max` bytes.

EOF ends the final chunk even if smaller than `min`.

An empty file has zero chunks and a valid FILE object.

## 5. CDC determinism

Given the same byte stream and chunking profile, boundary offsets MUST be identical across runs and implementations conforming to format v1.

Golden boundary fixtures are mandatory tests.

## 6. Rolling hash role

Buzhash is only a boundary detector. It MUST NOT be used as the object content ID or integrity proof.

## 7. SHA-256 requirement

SHA-256 MUST be implemented in project-owned C source according to the standard 512-bit-block compression function.

Required uses:

- CHUNK CIDs;
- FILE/TREE/SNAPSHOT/PARITY CIDs;
- full-file digest;
- normalized configuration/profile hash;
- Merkle verification.

The implementation MUST pass standard known-answer vectors including empty string, `abc`, a multi-block vector, and one million `a` bytes.

## 8. CRC-32C requirement

CRC-32C (Castagnoli polynomial) MUST be project-owned and used for framed-object early damage detection.

Golden vectors MUST include `123456789` => `0xE3069283` under the conventional reflected CRC-32C definition.

## 9. LZH1 compression overview

Every CHUNK object is independently eligible for compression. Compression is deterministic and consists of:

1. LZ77 tokenization of the raw chunk;
2. deterministic token serialization;
3. canonical Huffman coding of the serialized token byte stream.

No external compression library may be used.

## 10. LZ77 tokenization

Parameters:

```text
window size = 32768 bytes
minimum match = 3 bytes
maximum match = 258 bytes
```

At each input position:

- a candidate distance may refer only to a start byte within the previous 32768 already-emitted bytes;
- match comparison for offset `k` uses the LZ77 overlap rule: compare input byte `i+k` with logical byte `i+k-distance`, allowing bytes earlier in the same match to become the source when `k >= distance`;
- choose the longest available match up to 258 bytes;
- on equal length, choose the smallest distance (nearest previous occurrence);
- emit a match only if length >= 3;
- otherwise emit one literal byte;
- after emitting a match, advance by its full length; overlapping match semantics are allowed during decompression.

The search algorithm used to find the required match is implementation-defined, but emitted tokens MUST obey the deterministic longest-match/tie-break rules.

## 11. Token serialization before Huffman

The LZ77 token stream serializes to bytes:

Literal token:

```text
0x00 literal_byte
```

Match token:

```text
0x01 distance_u16_le length_u16_le
```

Constraints:

- distance 1..32768;
- length 3..258.

Any decoder encountering invalid token values MUST reject the payload.

## 12. Canonical Huffman stage

The byte stream from token serialization is Huffman-coded over symbols 0..255.

Requirements:

- frequency count uses the entire serialized token byte stream;
- build a binary Huffman tree;
- when node weights tie, deterministic ordering is by the minimum symbol contained in the subtree, then leaf before internal node if still needed;
- derive code lengths;
- assign canonical Huffman codes sorted by `(code_length, symbol)`;
- bitstream is emitted most-significant bit first within each code and packed from high bit to low bit in each output byte;
- unused low bits of the final byte are zero;
- the uncompressed serialized-token-byte length is stored to define exact decode termination.

Special cases:

- zero-length token byte stream: no Huffman payload;
- one distinct symbol: assign code length 1 and code `0`.

## 13. LZH1 stored payload

Canonical LZH1 payload:

```text
magic[4] = "LZH1"
token_bytes_len u64_le
code_lengths[256] u8
bitstream_bytes_len u64_le
bitstream[bitstream_bytes_len]
```

The decoder reconstructs canonical codes solely from `code_lengths`.

## 14. Compression selection

For each CHUNK, construct both candidate framed-payload sizes conceptually:

- raw codec 0 payload = chunk bytes;
- codec 1 payload = canonical LZH1 bytes.

Use codec 1 only when it saves at least `compression.min_savings_bytes` stored payload bytes. Otherwise use raw.

CID is always computed from original CHUNK semantic bytes and therefore is independent of compression choice.

## 15. Compression safety

Decoder MUST validate:

- header lengths do not overflow;
- Huffman code lengths describe a decodable prefix code;
- bitstream does not read past stored length;
- token stream does not exceed expected decoded chunk length;
- LZ77 distance never references before output start;
- final decoded length exactly equals framed `uncompressed_len`;
- recomputed CHUNK CID matches expected CID.

## 16. Chunk deduplication behavior

Before storing a newly produced chunk:

1. compute its CID;
2. consult the object/chunk index and canonical object path;
3. if a healthy object with that CID already exists, reuse it and do not create another canonical copy;
4. if an object exists but is corrupt, snapshot creation MUST fail with repository corruption rather than silently overwrite history;
5. if absent, write the new immutable object through the crash-safe temporary-object path.

Two equal chunk byte sequences MUST produce one canonical CHUNK object regardless of source file or snapshot.

## 17. Adversarial expectations

Tests MUST cover:

- incompressible pseudorandom bytes;
- all-zero chunks;
- one-symbol data;
- repeating short patterns;
- long matches requiring overlap;
- maximum distance and maximum match length;
- malformed Huffman tables;
- truncated bitstreams;
- crafted invalid LZ77 distances;
- content changes around CDC boundaries.
