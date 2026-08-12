# Test Gap Analysis

## Why the supplied suite is green

The suite predominantly tests happy-path existence:

- can a repository initialize;
- can a snapshot be created;
- can a basic restore reproduce a small file;
- can a hardlink example restore as a hardlink;
- can simple diff JSON contain the word `modified`;
- can simple stats outputs be produced;
- can one parity member be removed and recreated;
- can a byte flip cause a nonzero verify exit.

Those are useful smoke tests, but they do not distinguish a complete implementation from several important shortcuts.

## Examples of false confidence

### Hardlink test

The supplied test creates a real hardlink pair and confirms the restored inode is shared. It never tests the inverse property: **two independent files with the same contents must not become hardlinks**. The implementation therefore passes the hardlink test while its model is still wrong.

### Diff test

The test only greps for `modified`. A changed child alters its ancestor TREE CID, so a simplistic implementation can report the top directory as modified and satisfy the grep without correctly classifying the leaf or metadata change.

### Verify corruption test

The supplied corruption test flips a byte in an object that physically remains present, which the physical-object scanner can detect. It does not delete a live non-parity FILE/TREE object, which exposes the missing reachability traversal.

### GC test

The supplied GC check uses dry-run but does not assert a known unreachable object is reclaimed while another snapshot remains. Thus the source's explicit “keep everything if any ref exists” shortcut passes.

### Config validation

The supplied config tests only check that the example files return success and that an unsupported extension fails. They do not assert that example settings actually affect behavior or that schema errors are rejected.

## Missing mandatory categories

Major absent or inadequately covered categories include:

- exact CDC boundary golden vectors;
- multi-block/million-byte SHA vectors;
- LZ77 tie/max/invalid-distance vectors;
- malformed Huffman tables;
- Robin Hood collision/wraparound/load-factor tests;
- JSON Unicode surrogate handling and duplicate keys;
- YAML anchors/tabs/legacy booleans/duplicates;
- glob semantics;
- invalid UTF-8 filenames;
- file mutation during scan;
- incremental fast-path instrumentation;
- metadata-only and type/hardlink diff statuses;
- chunk reuse metric correctness;
- diff/verify SVG and NDJSON requirements;
- restore traversal/symlink-escape attacks;
- overwrite policy matrix;
- full-file digest verification;
- missing FILE/TREE reachability verification;
- parity two-member unrecoverable case;
- index corruption/checksum/order behavior;
- all required crash checkpoints and injected I/O failures;
- nontrivial GC reachability/parity repack;
- deterministic cross-repository bytes/CIDs/index/SVG;
- 20k/512MiB/100k-ref stress gates;
- >=50-seed randomized lifecycle/reference-model testing.

## Traceability failure

The task pack requires a mapping from every mandatory catalog ID to concrete test code. The delivered traceability document instead says the full catalog is partially mapped and summarizes broad areas. This alone prevents the Test Gate and DoD from passing.
