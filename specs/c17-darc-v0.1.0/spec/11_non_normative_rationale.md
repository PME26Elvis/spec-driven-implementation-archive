# 11 — Non-Normative Design Rationale

This document explains why several constraints are shaped this way. It is not a substitute for normative requirements.

## Why C17 + POSIX rather than ISO C only

ISO C alone has no directory traversal, symlink, hardlink, fsync, or atomic filesystem publication primitives. Allowing a narrow POSIX OS boundary makes the assignment implementable while still requiring the archive algorithms and data structures to be hand-built.

## Why Buzhash + SHA-256

The rolling hash and cryptographic identity serve different jobs. Buzhash makes content boundaries stable under insertions/deletions; SHA-256 makes object identity and corruption verification cryptographically strong enough for the assignment.

## Why LZ77 + canonical Huffman

This forces implementation of both dictionary matching and entropy coding without requiring a full DEFLATE clone. Independent per-chunk compression preserves random access and dedup identity.

## Why XOR 8+1 parity

The task explicitly requires partial corruption recovery. Pure hashes only detect damage. Full Reed-Solomon would add a large finite-field coding subproject; fixed XOR stripes provide a clear, deterministic, testable recovery capability: exactly one missing/corrupt member per stripe.

## Why the index is derived

Crash safety is simpler and more robust when immutable content-addressed objects and refs are authoritative. A derived index can be rebuilt after interruption instead of requiring database-grade transactional updates for every lookup structure.

## Why diff SVG exists

The assignment is headless, but presentation quality is still testable. A standalone SVG report adds non-GUI report engineering, escaping, layout, determinism, and structured data transformation without introducing a display-server dependency.

## Why YAML is a bounded subset

Full YAML is a very large language with anchors, tags, merges, multiple scalar-resolution schemes, and edge cases unrelated to the archive core. The bounded subset still requires a real indentation-sensitive parser and interoperates with the complete configuration model while keeping engineering volume controlled.

## Why no strict speed score

The intended comparison focuses on completion quality and engineering correctness across differing execution environments. Functional stress sizes catch catastrophically naive designs without turning this assignment into hardware benchmarking.
