# Mandatory Acceptance Test Catalog

Total catalog cases: **252**.

Unless the Level column says SHOULD, every case is mandatory. The implementation must map each ID to concrete automated test code in its traceability matrix.

## Algorithms

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| ALG-001 | MUST | SHA-256 empty vector | No repo. | Hash empty byte string. | Digest equals standard SHA-256 empty vector. |
| ALG-002 | MUST | SHA-256 abc vector | No repo. | Hash ASCII abc. | Digest equals standard known vector. |
| ALG-003 | MUST | SHA-256 multi-block vector | Known multi-block input. | Hash input. | Digest equals independently recorded expected value. |
| ALG-004 | MUST | SHA-256 million-a | Generate one million ASCII a. | Stream hash. | Digest matches standard vector; no whole-input special path. |
| ALG-005 | MUST | CRC-32C vector | Bytes 123456789. | Compute CRC-32C. | Result is E3069283. |
| ALG-006 | MUST | Buzhash table determinism | Fixed seed per spec. | Generate table twice. | All 256 entries byte-identical. |
| ALG-007 | MUST | Buzhash rolling equivalence | Window fixture >64 bytes. | Compare rolling update against recomputation/reference. | Hashes match at every offset. |
| ALG-008 | MUST | CDC empty | Empty stream. | Chunk. | Zero chunks. |
| ALG-009 | MUST | CDC tiny | 1-byte through 63-byte fixtures. | Chunk. | Exactly one EOF chunk for each nonempty fixture. |
| ALG-010 | MUST | CDC forced max | Fixture intentionally lacks natural cuts. | Chunk. | No chunk exceeds configured max; forced cuts occur at max. |
| ALG-011 | MUST | CDC deterministic boundaries | Fixed pseudorandom fixture and profile. | Chunk twice. | Exact boundary offsets equal golden fixture. |
| ALG-012 | MUST | CDC insertion resilience | Base fixture and copy with 1-byte insertion. | Chunk both. | Unchanged distant regions regain matching chunk CIDs; boundaries are not fixed-size alignment only. |
| ALG-013 | MUST | LZ77 literals | Input with no match >=3. | Tokenize. | Only literal tokens, exact bytes. |
| ALG-014 | MUST | LZ77 repeated data | Repeated pattern fixture. | Tokenize. | Expected deterministic longest matches. |
| ALG-015 | MUST | LZ77 tie break | Fixture with equal-length matches at multiple distances. | Tokenize. | Nearest/smallest distance chosen. |
| ALG-016 | MUST | LZ77 overlap | Long run of same byte. | Tokenize/decode. | Overlapping match decodes to original. |
| ALG-017 | MUST | LZ77 max length | Fixture supports >258-byte match. | Tokenize. | No match token exceeds 258. |
| ALG-018 | MUST | LZ77 max distance | Fixture creates valid match at 32768 and invalid older candidate. | Tokenize. | No distance exceeds 32768. |
| ALG-019 | MUST | Huffman one symbol | Serialized token bytes all one value. | Build canonical Huffman. | Single symbol has length 1 code 0. |
| ALG-020 | MUST | Huffman deterministic tie | Equal frequencies for several symbols. | Build twice/reordered counting input. | Code lengths/codes match golden deterministic ordering. |
| ALG-021 | MUST | Huffman round trip | Representative 0..255 distribution. | Encode/decode. | Exact byte equality. |
| ALG-022 | MUST | Huffman reject oversubscribed | Craft invalid code lengths. | Decode. | Fails corruption/codec validation. |
| ALG-023 | MUST | LZH1 golden simple | Small fixed repeating fixture. | Compress. | Exact encoded bytes match golden fixture. |
| ALG-024 | MUST | LZH1 incompressible fallback | Deterministic pseudorandom chunk. | Store chunk. | Raw codec selected when savings threshold not met. |
| ALG-025 | MUST | LZH1 compressible selected | Large repetitive chunk. | Store chunk. | LZH1 codec selected and round-trips. |
| ALG-026 | MUST | LZH1 truncated bitstream | Valid object then truncate compressed payload. | Decode/verify. | Corruption detected, no overread. |
| ALG-027 | MUST | LZH1 invalid distance | Craft token referring before output. | Decode. | Rejected. |
| ALG-028 | MUST | Robin Hood collision chain | Construct keys with colliding initial buckets. | Insert/find/delete. | All keys correct; probe behavior handles wraparound. |
| ALG-029 | MUST | Robin Hood resize | Insert beyond 70% threshold. | Trigger resize. | All mappings retained; load factor returns <=0.70. |
| ALG-030 | MUST | Parity XOR round trip | 8 known chunks varied lengths. | Build parity, remove each one in turn and recover. | Every single missing member recovers exact CID. |

## Configuration

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| CFG-001 | MUST | Example JSON valid | Use examples/config.json. | config validate. | Exit 0 and normalized hash emitted. |
| CFG-002 | MUST | Example YAML valid | Use examples/config.yaml. | config validate. | Exit 0 and same normalized hash as JSON. |
| CFG-003 | MUST | JSON key order irrelevant | Permute keys recursively. | Validate. | Same normalized config/hash. |
| CFG-004 | MUST | YAML comments irrelevant | Add comments/whitespace. | Validate. | Same normalized config/hash. |
| CFG-005 | MUST | CLI precedence | Config sets output json; CLI sets text. | Run command. | Text takes precedence. |
| CFG-006 | MUST | Unknown key rejected | Add typo chunking.avgg_bytes. | Validate. | Exit 2 E_CONFIG_SCHEMA. |
| CFG-007 | MUST | Duplicate JSON key | Duplicate nested key. | Validate. | Rejected. |
| CFG-008 | MUST | JSON trailing comma | Add trailing comma. | Validate. | Rejected with location. |
| CFG-009 | MUST | JSON comment | Insert // comment. | Validate. | Rejected. |
| CFG-010 | MUST | JSON surrogate pair | Use escaped valid supplementary Unicode in snapshot name. | Validate. | Decoded correctly. |
| CFG-011 | MUST | JSON lone surrogate | Use lone high surrogate escape. | Validate. | Rejected. |
| CFG-012 | MUST | Malformed UTF-8 config | Write invalid byte sequence. | Validate. | Rejected safely. |
| CFG-013 | MUST | YAML yes not bool | Set parity_enabled: yes. | Validate. | Rejected type mismatch, not coerced true. |
| CFG-014 | MUST | YAML anchors rejected | Use &a/*a. | Validate. | Rejected. |
| CFG-015 | MUST | YAML tab indentation | Indent with tab. | Validate. | Rejected with line/column. |
| CFG-016 | MUST | YAML duplicate key | Repeat scan key. | Validate. | Rejected. |
| CFG-017 | MUST | Invalid chunk ordering | min >= avg. | Validate. | Rejected. |
| CFG-018 | MUST | avg non-power-of-two | Set avg 70000. | Validate. | Rejected. |
| CFG-019 | MUST | max too large | Set >16MiB. | Validate. | Rejected. |
| CFG-020 | MUST | Invalid output format for command | Set svg then invoke snapshot create. | Run. | Validation failure, no repository mutation. |
| CFG-021 | MUST | Depth limit | Generate >documented nesting limit. | Validate. | Rejected without crash/overflow. |
| CFG-022 | MUST | Huge integer overflow | Use integer beyond accepted range. | Validate. | Rejected. |

## Scan/Snapshot

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| SCN-001 | MUST | Init layout | Empty temp directory. | darc init repo. | Required layout/FORMAT/empty HEAD created. |
| SCN-002 | MUST | Init existing nonempty | Nonempty ordinary directory. | darc init same path. | Fails safely unless explicitly allowed only for empty target. |
| SCN-003 | MUST | Snapshot empty directory | Empty source dir. | Create snapshot fixed timestamp. | Valid snapshot with empty TREE. |
| SCN-004 | MUST | Single tiny file | One 1-byte file. | Snapshot. | FILE with one EOF chunk, restore works. |
| SCN-005 | MUST | Zero-byte file | One empty file. | Snapshot. | FILE zero chunks and empty digest. |
| SCN-006 | MUST | Nested directories | Depth >=8 tree. | Snapshot. | All dirs/files captured with deterministic paths. |
| SCN-007 | MUST | Enumeration order independence | Create same names in different creation orders in two roots. | Snapshot fixed metadata/time. | Equivalent canonical tree CIDs. |
| SCN-008 | MUST | Non-ASCII path | Names include Chinese and emoji UTF-8. | Snapshot/list/restore. | Names preserved exactly. |
| SCN-009 | MUST | Invalid UTF-8 filename | Create raw-byte invalid UTF-8 name via helper. | Snapshot/diff JSON. | Path preserved; text escaped; JSON path_hex present. |
| SCN-010 | MUST | Symlink not followed | Link points to file inside root. | Snapshot default. | Symlink target stored; target not duplicated through link. |
| SCN-011 | MUST | Broken symlink | Symlink target missing. | Snapshot/restore. | Broken symlink preserved. |
| SCN-012 | MUST | Symlink cycle default | Self/cyclic symlink. | Snapshot default. | No recursion; symlink captured. |
| SCN-013 | MUST | Hardlink pair | Two paths same inode. | Snapshot. | One primary + one hardlink topology entry. |
| SCN-014 | MUST | Hardlink deterministic primary | Create same hardlinks in different order. | Snapshot. | Lexicographically first path is primary. |
| SCN-015 | MUST | Duplicate independent files | Copy same bytes into two inodes. | Snapshot. | Separate path entries, shared content/chunks. |
| SCN-016 | MUST | Special FIFO default error | Create FIFO. | Snapshot default. | Fails E_SPECIAL_FILE and no snapshot published. |
| SCN-017 | MUST | Special FIFO skip | Set on_special_file skip. | Snapshot. | Succeeds with skip warning/count; FIFO absent. |
| SCN-018 | MUST | Permission error default | Unreadable file where test permissions make effective. | Snapshot. | Fails E_PERMISSION. |
| SCN-019 | MUST | Permission error skip | Config skip permission. | Snapshot. | Succeeds with explicit skip count. |
| SCN-020 | MUST | Repository self exclusion | Repo located inside source tree. | Snapshot source containing repo. | Repo internals excluded and warning emitted. |
| SCN-021 | MUST | Glob star | Files across directories. | Include *.txt. | Only same-component matches per rules. |
| SCN-022 | MUST | Glob double star | Nested txt files. | Include **/*.txt. | Nested matches included. |
| SCN-023 | MUST | Glob class | a.txt b.txt c.txt. | Pattern [ab].txt. | a,b included; c excluded. |
| SCN-024 | MUST | Glob override | Exclude broad then negate a path. | Snapshot. | Later negation re-includes correctly. |
| SCN-025 | MUST | Hidden default | Hidden and normal files. | Snapshot default. | Both included unless pattern/exclude_hidden says otherwise. |
| SCN-026 | MUST | Hidden excluded | exclude_hidden true. | Snapshot. | Hidden entries excluded consistently. |
| SCN-027 | MUST | File changes once | Fault/helper changes file during first read then stabilizes. | Snapshot. | One retry then correct stable content saved. |
| SCN-028 | MUST | File changes repeatedly | Change on both attempts. | Snapshot. | Fails E_FILE_CHANGED_DURING_SCAN; no ref. |
| SCN-029 | MUST | Large streamed file | Generate 512MiB patterned file. | Snapshot with read instrumentation. | Multiple bounded reads/chunks; no whole-file load. |
| SCN-030 | MUST | Multiple source roots | Two roots with distinct labels. | Snapshot. | Both represented under deterministic root arrangement. |

## Incremental/Dedup

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| INC-001 | MUST | Second identical snapshot | Snapshot unchanged tree twice. | Create second with parent. | No new CHUNK/FILE/TREE content objects except snapshot metadata as applicable. |
| INC-002 | MUST | Fast-path unchanged file | Parent snapshot; unchanged stat identity. | Snapshot. | File counted fast-path reused, payload not reread per instrumentation. |
| INC-003 | MUST | One-byte insertion | Large file then insert near beginning. | Second snapshot. | Distant chunks reused; stored delta far below whole file size. |
| INC-004 | MUST | One-byte replacement | Modify byte within one region. | Second snapshot. | Most chunks reused. |
| INC-005 | MUST | Append data | Append to large file. | Second snapshot. | Earlier chunks reused; only tail neighborhood/new data stored. |
| INC-006 | MUST | Metadata-only change | chmod or mtime change without content. | Second snapshot. | FILE content object reused; tree changes; diff P. |
| INC-007 | MUST | Rename as add/delete | Move file path unchanged bytes. | Second snapshot/diff. | Canonical diff D+A; chunks reused. |
| INC-008 | MUST | Duplicate introduced | Add independent copy of existing file. | Snapshot. | No new chunks; duplicate restores independent. |
| INC-009 | MUST | Hardlink introduced | Replace duplicate with hardlink. | Snapshot/diff. | Content reused; hardlink topology change classified H where same path semantics permit. |
| INC-010 | MUST | Parent explicit old snapshot | Three snapshot history. | Create with parent first snapshot. | Parent link exactly requested; incremental comparison uses that parent. |
| INC-011 | MUST | Invalid parent | Unknown CID. | Snapshot. | Exit 4, no mutation except safe transient cleanup. |
| INC-012 | MUST | Corrupt existing duplicate object | Corrupt chunk that new source would hash to. | Snapshot. | Fails corruption; does not overwrite canonical object. |
| INC-013 | MUST | Compression CID independence | Same chunk stored in repositories with compression enabled vs disabled. | Compare CHUNK CID. | CID identical. |
| INC-014 | MUST | Tree subtree reuse | Only one deep leaf changes. | Second snapshot. | Unaffected subtree TREE CIDs reused. |
| INC-015 | MUST | Snapshot fixed-time reproducibility | Equivalent repos/tree/config/timestamp. | Create snapshots. | Snapshot CID and canonical objects identical. |

## Diff/Reporting

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| DIF-001 | MUST | Added file | Old empty; new adds file. | Diff. | A exactly once. |
| DIF-002 | MUST | Deleted file | Old file; new removed. | Diff. | D exactly once. |
| DIF-003 | MUST | Modified content | Same path bytes changed. | Diff. | M. |
| DIF-004 | MUST | Permission only | chmod only. | Diff. | P and metadata field mode. |
| DIF-005 | MUST | mtime only | mtime only. | Diff. | P and mtime field. |
| DIF-006 | MUST | Type file to dir | Replace file with directory. | Diff. | T. |
| DIF-007 | MUST | Symlink target change | Same symlink path new target. | Diff. | P with symlink_target field changed. |
| DIF-008 | MUST | Hardlink topology change | Two independent equal files become hardlinked. | Diff. | H classification where content/type/basic metadata otherwise equal. |
| DIF-009 | MUST | Unchanged omitted | Identical snapshots. | Diff text. | No changed rows; zero summary. |
| DIF-010 | MUST | Chunk reuse 100 percent metadata-only | Metadata-only changed file. | Diff. | Content metrics show 100% or not-applicable consistently; no false new bytes. |
| DIF-011 | MUST | Chunk reuse multiset | File repeats same chunk multiple times and changes one occurrence. | Diff. | Reuse count does not overcount multiplicity. |
| DIF-012 | MUST | Path filter file | Changes in several trees. | Diff --path selected file. | Only selected path and selected summary. |
| DIF-013 | MUST | Path filter subtree | Changes in several top dirs. | Diff --path dir. | Only subtree. |
| DIF-014 | MUST | Chinese path display | Changed Chinese path. | Diff text. | Readable UTF-8 path. |
| DIF-015 | MUST | Control-byte path display | Filename contains newline/tab byte. | Diff text. | Escaped, report remains one logical row. |
| DIF-016 | MUST | JSON validity | Representative diff. | --format json parsed externally. | Exactly one valid JSON doc; counters agree. |
| DIF-017 | MUST | NDJSON validity | Large diff. | --format ndjson. | Every line JSON; typed summary present. |
| DIF-018 | MUST | SVG standalone | Representative diff. | --format svg. | Parses as XML/SVG; no external href/script; key metrics present. |
| DIF-019 | MUST | SVG escaping | Path contains &,<,>,quotes. | SVG diff. | Valid XML, text safely escaped. |
| DIF-020 | MUST | SVG deterministic | Fixed diff data/options. | Generate twice. | Byte-identical SVG. |
| DIF-021 | MUST | Color never | Representative diff. | --color never. | No ESC SGR sequences. |
| DIF-022 | MUST | JSON no progress noise | Large diff verbose/progress conditions. | --format json. | stdout remains parseable JSON; diagnostics on stderr. |

## Restore

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| RST-001 | MUST | Full restore basic | Snapshot mixed tree. | Restore to empty destination. | Logical tree/content match. |
| RST-002 | MUST | Modes preserved | Executable/non-executable files. | Restore. | Mode bits match archived values where OS permits. |
| RST-003 | MUST | mtime preserved | Fixed mtimes. | Restore. | mtime_ns matches archive within filesystem precision supported by target. |
| RST-004 | MUST | Symlink preserved | Relative and absolute symlink targets as bytes. | Restore. | Symlink object recreated, not followed. |
| RST-005 | MUST | Broken symlink preserved | Broken link. | Restore. | Still broken with identical target bytes. |
| RST-006 | MUST | Hardlink preserved | Hardlink group >=3. | Restore. | Members share inode and content. |
| RST-007 | MUST | Duplicate independent | Two independent identical files. | Restore. | Content equal but inodes distinct. |
| RST-008 | MUST | Empty file/dir | Snapshot contains both. | Restore. | Present correctly. |
| RST-009 | MUST | Partial file restore | Snapshot many paths. | restore --path file. | Only selected file plus needed parent dirs. |
| RST-010 | MUST | Partial dir restore | Snapshot multiple dirs. | restore --path subtree. | Only subtree restored. |
| RST-011 | MUST | Partial hardlink primary outside | Select hardlink member without primary. | Restore partial. | Regular file content restored and topology-loss warning emitted per spec exception. |
| RST-012 | MUST | Overwrite never conflict | Existing target file. | Restore default. | Exit 8; existing file unchanged. |
| RST-013 | MUST | Overwrite files | Existing regular target. | Restore overwrite files. | File atomically replaced. |
| RST-014 | MUST | Overwrite files dir conflict | Existing directory where file needed. | Restore overwrite files. | Rejected. |
| RST-015 | MUST | Overwrite all type change | Conflicting file/dir within target. | Restore overwrite all. | Correct final snapshot tree. |
| RST-016 | MUST | Traversal malicious path | Construct/modify manifest path with .. via corruption fixture. | Restore. | E_RESTORE_ESCAPE; no outside write. |
| RST-017 | MUST | Absolute malicious path | Corrupt path to absolute. | Restore. | Rejected. |
| RST-018 | MUST | Destination symlink escape | Precreate symlink parent to outside target. | Restore. | Rejected; outside sentinel unchanged. |
| RST-019 | MUST | Chunk corruption no repair | Corrupt referenced chunk. | Restore. | Fails; no corrupt final file published. |
| RST-020 | MUST | Single missing parity recover use | Remove one protected chunk. | Restore. | Content recovered and exact; repository mutation behavior matches explicit repair policy. |
| RST-021 | MUST | Two missing unrecoverable | Remove two same-stripe chunks. | Restore. | Exit 7; no placeholder bytes. |
| RST-022 | MUST | Full file digest mismatch | Craft FILE/chunks mismatch fixture. | Restore. | Fails before final file rename. |
| RST-023 | MUST | Large restore streaming | 512MiB archived file. | Restore with instrumentation. | Streaming, correct digest. |
| RST-024 | MUST | Invalid UTF-8 path restore | Archive raw-byte name. | Restore. | Raw name reproduced where filesystem permits. |

## Verify/Recovery

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| VER-001 | MUST | Healthy quick | Healthy repo. | verify quick. | HEALTHY exit 0. |
| VER-002 | MUST | Healthy full | Healthy repo. | verify full. | HEALTHY exit 0. |
| VER-003 | MUST | Healthy scrub | Healthy repo with parity. | verify scrub. | HEALTHY exit 0. |
| VER-004 | MUST | Raw payload bit flip | Flip chunk stored payload byte. | verify full. | Detected by CRC/CID, identifies CID. |
| VER-005 | MUST | Compressed payload flip | Flip compressed bitstream byte. | verify full. | Detected, no unsafe decode. |
| VER-006 | MUST | Object truncation | Truncate chunk object. | verify full. | Detected as corrupt/truncated. |
| VER-007 | MUST | Header length corruption | Change stored_len without CRC fix. | verify. | Header CRC/format corruption detected. |
| VER-008 | MUST | Payload CRC corruption | Change payload CRC field. | verify. | Detected. |
| VER-009 | MUST | Missing one protected chunk | Delete one chunk object. | verify scrub. | DEGRADED-REPAIRABLE nonzero. |
| VER-010 | MUST | Repair one missing chunk | Prior fixture. | verify scrub --repair. | Chunk reconstructed, CID matches, final REPAIRED exit 0. |
| VER-011 | MUST | Corrupt one protected chunk | Bit flip one member. | verify --repair. | Reconstruct/replace safely, final healthy. |
| VER-012 | MUST | Two missing same stripe | Delete two members. | verify --repair. | UNRECOVERABLE exit 7. |
| VER-013 | MUST | Missing parity healthy data | Delete parity object only. | verify scrub. | DEGRADED-REPAIRABLE. |
| VER-014 | MUST | Regenerate parity | Prior fixture. | verify scrub --repair. | Parity regenerated; chunks unchanged. |
| VER-015 | MUST | Corrupt parity healthy data | Flip parity bytes. | verify scrub --repair. | Parity replaced, data untouched. |
| VER-016 | MUST | Missing index | Delete chunks.idx. | verify quick. | Reports index problem/degraded, canonical snapshots still readable. |
| VER-017 | MUST | Repair index | Delete/corrupt index. | verify --repair or index rebuild. | Deterministic index rebuilt. |
| VER-018 | MUST | Index extra record | Inject record for missing object. | verify. | Detected. |
| VER-019 | MUST | Index missing record | Remove index record only. | verify. | Detected; object remains usable canonically. |
| VER-020 | MUST | TREE child missing | Delete child FILE/TREE object. | verify full. | Merkle/reference failure with affected snapshot/path when derivable. |
| VER-021 | MUST | TREE byte corruption | Flip tree payload. | verify full. | CID/Merkle corruption. |
| VER-022 | MUST | SNAPSHOT corruption | Flip snapshot payload. | snapshot list/verify. | Corrupt ref target reported; no crash. |
| VER-023 | MUST | Malformed ref hex | Edit ref text invalid. | verify/gc dry-run. | Corruption reported; GC refuses unsafe deletion. |
| VER-024 | MUST | Ref missing target | Point ref to nonexistent CID. | verify. | Detected. |
| VER-025 | MUST | Deep soft-parent history | Create >=100 snapshots with retained refs and parent links. | list/show/verify selected snapshots. | Traversal remains bounded/correct; deleting an old parent ref does not invalidate descendants. |
| VER-026 | MUST | Verify JSON | Mixed degraded repo. | verify --format json. | Valid structured object counts/status. |
| VER-027 | MUST | Verify SVG | Mixed degraded repo. | verify --format svg. | Standalone valid SVG with health counts. |
| VER-028 | MUST | Repair never hides history | Unrecoverable chunk. | verify --repair. | Snapshot refs remain; status stays UNRECOVERABLE. |

## Index/GC

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| GCI-001 | MUST | Index rebuild deterministic | Healthy fixed repo. | Rebuild index twice. | Byte-identical chunks.idx. |
| GCI-002 | MUST | Index sorted records | Index with many CIDs. | Inspect bytes/parser. | Records sorted full CID. |
| GCI-003 | MUST | Index checksum detection | Flip index byte. | repo inspect/verify. | Detected. |
| GCI-004 | MUST | GC no unreachable | All objects reachable. | gc dry-run then gc. | Zero semantic objects removed. |
| GCI-005 | MUST | Orphan chunk collected | Create safe orphan object fixture. | gc. | Unreachable object removed; healthy refs unaffected. |
| GCI-006 | MUST | Historical snapshot retains old chunks | Two snapshots, old content removed in HEAD but old ref kept. | gc. | Old-only chunks retained. |
| GCI-007 | MUST | Deleted snapshot enables reclaim | Remove snapshot ref through documented fixture/admin setup. | gc. | Now-unreachable objects reclaimed. |
| GCI-008 | MUST | Dry run byte immutability | Repo with garbage. | Hash every repo file before/after gc --dry-run. | No bytes/files changed except no mutation at all. |
| GCI-009 | MUST | Parity affected by dead member | Stripe includes live/dead chunks. | gc repack. | Live chunks protected by new valid parity before old stripe removal. |
| GCI-010 | MUST | Parity deterministic repack | Same live set in equivalent repos. | gc repack. | Same stripe memberships/parity CIDs. |
| GCI-011 | MUST | GC after many snapshots | 10 overlapping snapshots plus deleted refs. | gc then scrub verify. | Healthy; retained snapshots restore. |
| GCI-012 | MUST | GC refuses corrupt ref | Malformed/missing ref target. | gc. | Fails corruption without deletion. |
| GCI-013 | MUST | GC stats accurate | Known fixture sizes. | gc dry-run. | Reclaimable object/byte counts equal independently computed set. |
| GCI-014 | MUST | Index updated after GC | GC removes objects. | lookup/index verify. | No stale removed records; all live records present. |
| GCI-015 | MUST | Empty repo GC | No snapshots. | gc. | Safe, no crash; only allowable internal derived cleanup. |

## Crash/Fault

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| CRS-001 | MUST | Crash before object publish | Existing valid repo. | Kill snapshot at checkpoint before first final object rename. | Old repo valid; no new snapshot ref. |
| CRS-002 | MUST | Crash after one chunk publish | New multi-chunk snapshot. | Kill after first immutable chunk publish. | Old snapshots valid; new ref absent; orphan collectible. |
| CRS-003 | MUST | Crash after all objects before ref | New snapshot objects durable. | Kill before ref publication. | New snapshot invisible; objects may be garbage. |
| CRS-004 | MUST | Crash after ref before HEAD | Publish named/id ref then kill before HEAD update. | Reopen/recover. | Published snapshot remains valid; HEAD old or recoverably updated per journal, never malformed. |
| CRS-005 | MUST | Crash before HEAD rename | Transaction ready. | Kill. | HEAD remains previous valid value. |
| CRS-006 | MUST | Crash after HEAD before index | Kill after HEAD durable. | Run recovery/verify. | New snapshot valid; index rebuilt/updated. |
| CRS-007 | MUST | Crash during index rewrite | Index temp partially written. | Kill/reopen. | Authoritative objects/refs valid; index recoverable. |
| CRS-008 | MUST | Crash after completion marker | Transaction complete marker durable, journal remains. | Reopen. | Recognized complete; cleanup safe. |
| CRS-009 | MUST | Short write object | Inject short write/failure. | Snapshot. | No truncated canonical object published. |
| CRS-010 | MUST | fsync object failure | Inject fsync failure. | Snapshot. | No new snapshot ref; old valid. |
| CRS-011 | MUST | rename object failure | Inject rename failure. | Snapshot. | No published ref; cleanup/recovery safe. |
| CRS-012 | MUST | disk full before ref | Inject ENOSPC. | Snapshot. | Exit I/O; old snapshots valid. |
| CRS-013 | MUST | allocation failure parser | Fail nth allocation. | config validate. | Clean failure/no crash. |
| CRS-014 | MUST | allocation failure snapshot | Fail selected allocations. | Snapshot. | No partial visible snapshot. |
| CRS-015 | MUST | GC crash before new parity durable | Affected parity stripe. | Kill during new parity creation. | Old parity/live data retained. |
| CRS-016 | MUST | GC crash after new parity before old delete | Kill at checkpoint. | Reopen. | At least one valid protection mapping survives; live chunks intact. |
| CRS-017 | MUST | GC crash during object deletion | Garbage and live objects. | Kill after some unreachable deletes. | All live objects remain; recovery/verify healthy. |
| CRS-018 | MUST | Writer lock conflict | Hold writer lock in process A. | Start snapshot/gc in B. | B exits 9 E_REPO_LOCKED; no corruption. |
| CRS-019 | MUST | Stale lock behavior | Create stale lock metadata representing dead process per implementation scheme. | Start mutating command. | Safely distinguishes stale vs live; no blind destructive unlock. |

## Format/Security/Stress

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| FMT-001 | MUST | Object golden CHUNK | Fixed small chunk. | Serialize object. | Framing fields and CID match golden bytes. |
| FMT-002 | MUST | FILE golden | Fixed chunk list/digest. | Serialize payload/object. | Exact golden bytes/CID. |
| FMT-003 | MUST | TREE golden sort | Children supplied unsorted. | Build TREE. | Serialized entries bytewise sorted, golden CID. |
| FMT-004 | MUST | SNAPSHOT golden | Fixed metadata/root/time. | Serialize. | Exact golden bytes/CID. |
| FMT-005 | MUST | No native struct dump | Build with altered packing/inspection test as feasible. | Compare canonical outputs. | No padding-dependent bytes; format stable. |
| FMT-006 | MUST | Unknown object version | Craft version unsupported. | Read/verify. | Exit 10 unsupported, not guessed. |
| FMT-007 | MUST | Unknown codec | Craft codec id unknown. | Read/verify. | Rejected unsupported/corrupt as specified. |
| FMT-008 | MUST | Lowercase object path | Create objects. | Inspect repository. | CID path uses lower-case 2+62 hex. |
| FMT-009 | MUST | Fixed timestamp cross-repo | Equivalent semantic source trees. | Snapshot both. | Canonical snapshot/object bytes equal. |
| FMT-010 | MUST | Directory creation order | Equivalent tree created in reverse order. | Snapshot. | Same root tree CID. |
| SEC-001 | MUST | Restore path dot-dot | Malicious manifest. | Restore. | Outside sentinel unchanged. |
| SEC-002 | MUST | Restore symlink parent race baseline | Destination has symlink parent before command. | Restore. | Rejected. |
| SEC-003 | MUST | Huge framed length | Craft near-u64 stored length. | Verify. | Rejected before allocation overflow. |
| SEC-004 | MUST | Decompression overrun claim | Craft compressed stream expanding past declared len. | Verify/decode. | Rejected. |
| SEC-005 | MUST | Malformed path NUL semantic fixture | Craft object if parser allows raw bytes. | Restore/verify. | Rejected as invalid repository/path. |
| SEC-006 | MUST | No shell delegation smoke | Run with PATH containing trap executables named tar/find/sha256sum/etc. | Exercise core workflows. | Trap executables never invoked. |
| SEC-007 | MUST | Repository source self recursion | Repo under source. | Snapshot repeatedly. | Repo size does not recursively archive itself. |
| SEC-008 | MUST | Control characters error output | Corrupt object associated with weird path. | Verify text. | Output remains structurally readable/escaped. |
| STR-001 | MUST | 20k small files | Generate 20,000 deterministic files. | Snapshot/diff/restore/verify. | Completes correctly. |
| STR-002 | MUST | 512MiB mixed logical data | Generate mixed repetitive/random fixture. | Snapshot. | Completes, dedup/compression stats plausible and verifiable. |
| STR-003 | MUST | Single 512MiB file | Generate streaming fixture. | Snapshot/restore. | No whole-file buffering; content exact. |
| STR-004 | MUST | 10 overlapping snapshots | Mutate small portions over 10 snapshots. | Create all, verify, diff adjacent. | All history valid, substantial reuse. |
| STR-005 | MUST | 100k chunk references | Construct files/snapshots producing >=100k refs. | verify/gc. | Completes correctly without fixed small limits. |
| STR-006 | MUST | Random tree seeds | At least documented fixed set of >=50 seeds. | Full randomized lifecycle. | All pass or failing seed reported. |
| STR-007 | MUST | Repeated verify/repair idempotence | Healthy repaired repo. | Run repair-capable verify repeatedly. | Second and later runs make no mutation and report healthy. |
| STR-008 | MUST | Repeated GC idempotence | Repo after successful GC. | Run GC again. | No further unreachable removals; same live semantics. |

## CLI

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| CLI-001 | MUST | Top help | No repo. | darc --help. | Exit 0, required commands listed. |
| CLI-002 | MUST | Subcommand help | No repo. | Every required subcommand --help. | Exit 0, args/destructive notes/examples present. |
| CLI-003 | MUST | Version | No repo. | darc --version. | Exit 0, stable version string. |
| CLI-004 | MUST | Missing repo | Run repo-required command with nonexistent repo. | snapshot list. | Exit 3 and E_REPO_NOT_FOUND. |
| CLI-005 | MUST | Unknown option | Any command. | Pass unknown flag. | Exit 2 with usage error. |
| CLI-006 | MUST | Ambiguous snapshot prefix | Create/find two matching prefixes in fixture. | snapshot show prefix. | Exit 4 E_SNAPSHOT_AMBIGUOUS. |
| CLI-007 | MUST | Unique snapshot prefix | Known unique >=8 prefix. | snapshot show. | Correct snapshot selected. |
| CLI-008 | MUST | Duplicate snapshot name ambiguity | Two snapshots same name. | snapshot show name. | Ambiguity error. |
| CLI-009 | MUST | HEAD selector | Repo with snapshots. | snapshot show HEAD. | Current HEAD shown. |
| CLI-010 | MUST | Stdout/stderr separation | Trigger warning/progress in JSON mode. | Run command. | stdout parseable data; warning/progress stderr. |
| CLI-011 | MUST | Quiet preserves data | Run list --quiet. | Inspect stdout. | Required list data still emitted; non-error chatter suppressed. |
| CLI-012 | MUST | Exit corruption precedence | Operation finds corruption plus presentation warning. | Run. | Integrity exit code, not success. |
| CLI-013 | MUST | Snapshot list sort | Several fixed timestamps. | snapshot list. | Newest-first deterministic. |
| CLI-014 | MUST | Snapshot show fields | Known snapshot. | snapshot show. | All required metadata/stat fields present. |
| CLI-015 | MUST | GC dry-run wording | Repo with garbage. | gc --dry-run. | Clearly states no changes and proposed reclaim. |

## Snapshot Deletion

| ID | Level | Test | Setup | Action | Expected result |
|---|---|---|---|---|---|
| DEL-001 | MUST | Delete non-HEAD snapshot | At least three published refs. | `snapshot delete TARGET --yes`. | Target ref removed; canonical objects remain before GC; other refs unchanged. |
| DEL-002 | MUST | Delete dry-run immutable | Snapshot exists. | Hash repo tree before/after `snapshot delete TARGET --dry-run`. | Zero mutation; proposed changes reported. |
| DEL-003 | MUST | Delete requires yes | Snapshot exists. | Delete without `--yes` and without `--dry-run`. | Exit 2; ref remains. |
| DEL-004 | MUST | Delete HEAD deterministic move | HEAD plus remaining snapshots with fixed timestamps/tie. | Delete HEAD --yes. | HEAD selects greatest created_ns, then greatest CID tie-break. |
| DEL-005 | MUST | Delete last snapshot | Exactly one published snapshot. | Delete it --yes. | refs empty, HEAD empty, objects still present until GC. |
| DEL-006 | MUST | Child survives deleted soft parent | Snapshot B names A as parent; delete A ref; keep B. | verify/restore B, then GC. | B remains valid/restorable; missing soft parent is informational; A-only data may reclaim if not used by B. |
