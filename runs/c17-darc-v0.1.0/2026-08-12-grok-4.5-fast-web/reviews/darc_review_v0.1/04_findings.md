# Detailed Findings

Severity labels: **Critical**, **High**, **Medium**, **Low**.

## F-001 — Critical — legal chunk configuration causes heap-buffer-overflow

`src/snapshot.c:57` allocates `malloc(MAX_CHUNK)` where `MAX_CHUNK` is fixed at 262,144. Runtime boundary logic at roughly lines 69–72 uses `g_darc_config->chunk_max`, which may legally exceed 262,144 under the task pack. Byte writes occur at `src/snapshot.c:67` without checking capacity against the configured maximum.

ASan reproduced an out-of-bounds write with `chunk_min=524288`, `chunk_avg=1048576`, `chunk_max=1048576`.

Evidence: `evidence/asan_heap_overflow.log`.

## F-002 — Critical — `verify --level full` does not verify live graph reachability

`src/verify.c:275-299` scans object files that physically exist and validates those objects individually. It does not recursively traverse each published SNAPSHOT -> TREE -> FILE -> CHUNK graph and prove referenced objects exist and have the right type.

Independent test: create one snapshot, delete every FILE object (type 2), then run full verify. Observed result was exit 0 with zero missing objects. Restore then fails because the deleted FILE object is actually required.

This defeats a central integrity guarantee.

## F-003 — High — GC is deliberately incomplete

`src/verify.c:364-416` contains comments explicitly saying that when any refs remain it keeps everything because “Full tree walk would go here” and “full recursive mark-and-sweep would be needed.”

Therefore deleting one snapshot does not reclaim old-only objects while another snapshot remains. This directly fails required GC reachability/reclamation behavior.

## F-004 — High — hard-link topology is inferred from content identity

Snapshot scanning serializes all regular files as type 1 and reuses FILE CID for detected hardlinks (`src/snapshot.c:282-303`). No distinct hardlink entry/primary-path topology is serialized.

Restore then treats any repeated FILE CID as a hardlink (`src/restore.c:158-171`). Consequently two **independent duplicate files with identical bytes** restore as the same inode.

Independent black-box result confirms equal inode numbers for two originally independent copies.

## F-005 — High — partial restore option is nonfunctional

`src/restore.c:124` explicitly discards `path_filter` with `(void)path_filter`. The CLI accepts `--path`, but the entire tree is restored.

This is a classic “wired CLI, unwired feature” failure.

## F-006 — High — restore does not verify whole-file SHA-256

`src/restore.c:104-109` reads the expected full-file digest and then comments “could re-hash file for verification”; the value is unused.

The mandatory restore contract requires streaming whole-file verification before final publication.

## F-007 — High — restore publishes directly to final path

`src/restore.c:79` opens the final path using `O_TRUNC`. It does not restore to a temporary file, validate full-file hash, fsync, and atomically rename as required. A failed restore may therefore leave a truncated/partial final file.

## F-008 — High — overwrite `never` silently skips instead of conflicting

`src/restore.c:157` does `continue` when an existing path is found and overwrite is disabled. The public command returns success. The task pack requires a restore conflict with exit code 8.

Independent test observed exit 0 with the sentinel destination file unchanged.

## F-009 — High — restore path safety is inadequate

Path validation is based on substring checks such as `strstr(name, "..")` (`src/restore.c:150-151`) and `strstr(dest, "..")` (`src/restore.c:199`). This both rejects benign names containing two dots and fails to implement race-resistant directory-fd/openat-style confinement against symlink substitution.

Fixed `PATH_MAX` concatenation further conflicts with the required long-path behavior.

## F-010 — High — include/exclude configuration is parsed but skipped

JSON arrays are explicitly skipped in `src/config.c:189-205`. Scan traversal has no include/exclude glob evaluation. A configured `**/*.tmp` exclusion was ignored in black-box testing and the file was archived/restored.

## F-011 — High — unknown config keys are silently accepted

`src/config.c:153-154` explicitly ignores unknown keys “for forward compatibility.” The task pack requires unknown keys to fail schema validation so typos are caught.

Independent test: config containing `typo_key` returned `OK` and exit 0.

## F-012 — High — duplicate JSON keys are accepted

The parser does not track keys already seen in an object. Independent test with duplicate `chunk_min` keys validated successfully, contrary to the required error behavior.

## F-013 — High — required config keys do not map correctly

The normative examples use keys such as `chunking.min_bytes`, `chunking.avg_bytes`, `chunking.max_bytes`, `repository.parity_enabled`, and `output.format`. `apply_key()` primarily recognizes alternative names such as `chunking.min`, `parity.enabled`, and bare `format`. Many example values are therefore accepted syntactically but ignored semantically.

## F-014 — Critical/High — `compression.enabled=false` is ignored by snapshot writer

`process_file()` always attempts LZH1 compression. Black-box inspection of a repetitive chunk created with `{ "compression": { "enabled": false } }` still found CHUNK codec 1 (LZH1).

The runtime also uses hard-coded `MIN_SAVINGS=32`, ignoring configured `min_savings_bytes`.

## F-015 — High — default parent does not follow HEAD

CLI snapshot creation only passes a parent when `--parent` is explicitly supplied (`src/main.c:81-99`). The required default is current HEAD when available.

Independent test showed the second snapshot had no parent metadata.

## F-016 — Medium/High — `--timestamp 0` cannot represent the epoch override

`src/main.c:73-77` treats numeric zero as “no timestamp supplied” and replaces it with current wall-clock time. This breaks the required explicit fixed-zero timestamp use case and weakens deterministic golden/repro tests.

## F-017 — High — special files are silently ignored under default behavior

`src/snapshot.c:321-323` simply frees the entry for unsupported types. The default contract requires an error; skip is allowed only when configured and must be reported.

Independent FIFO test succeeded and published a snapshot with no warning/error.

## F-018 — High — scan races and I/O failures are not handled as specified

`process_file()` uses `while (read(fd, &byte, 1) == 1)` and does not distinguish EOF from read error. It does not stat before/after read or retry a file that changes during scan.

`scan_dir_rec()` also silently continues on `lstat` failure (`src/snapshot.c:273-274`).

## F-019 — Medium/High — file processing is one-byte-at-a-time

The file loop issues one `read()` system call per byte. This is technically streaming but catastrophically inefficient compared with block reads and makes the required large stress workloads impractical.

## F-020 — High — canonical snapshot/profile fields are incomplete

`src/snapshot.c:414-415` writes a 32-byte all-zero profile hash rather than the normalized effective profile hash. Root metadata/source roots and several required snapshot statistics/fields are absent or differ from the contract.

## F-021 — High — diff semantics are far below contract

`src/diff.c` only emits added/removed/modified. It compares entry type/CID and reports changed parent directories as `M`. It does not implement:

- metadata-only `P`;
- type change `T`;
- hardlink topology `H`;
- old/new sizes;
- metadata field deltas;
- chunk reuse percentages/multiset logic;
- correct leaf-level symlink target semantics;
- path filter (`path_filter` is discarded at `src/diff.c:92`);
- NDJSON diff;
- SVG diff.

Its JSON output also prints raw paths without JSON string escaping (`src/diff.c:148`).

## F-022 — High — snapshot selector ambiguity is not detected

Several resolver loops stop at the first prefix match (`src/main.c` snapshot show/parent/restore/diff paths). The contract requires ambiguous prefixes/names to fail rather than select an arbitrary first match.

## F-023 — High — deleting HEAD does not update HEAD

`snapshot delete` removes a ref but never recomputes/writes HEAD (`src/main.c:225-231`). Independent test left HEAD pointing to a snapshot whose ref had been deleted while another ref remained.

Deletion also lacks the required writer lock/journal atomicity.

## F-024 — High — journal/recovery is not a transaction state machine

`darc_journal_begin()` stores only operation and PID. `darc_journal_recover()` deletes temp files and unlinks the journal (`src/repo.c:171-210` region). It does not record intended ref/HEAD changes or transaction stages and cannot implement the specified crash checkpoint semantics.

Notably snapshot creation calls `darc_journal_begin()` and immediately calls `darc_journal_recover()` (`src/snapshot.c:336-339`), which clears the journal it just created. This largely defeats journaling for the active transaction.

## F-025 — High — parity generation is outside snapshot transaction publication

`darc_snapshot_create()` publishes the snapshot/ref/HEAD and commits/unlocks. Only after it returns does `main.c:107-108` call `darc_parity_protect_all()`. A crash between these phases leaves a published snapshot without the required new protection state.

## F-026 — High — parity stripe membership is not canonical sorted-CID grouping

`darc_parity_protect_all()` iterates Robin Hood hash-table slots (`src/verify.c:426-442`) and batches chunks in table iteration order. The task pack requires deterministic CID-sorted grouping of newly introduced/unprotected chunks.

## F-027 — High — persistent index format misses mandatory properties

`src/index.c:116-133` serializes active hash-table slot iteration order directly. It has no integrity checksum and no full-CID sorted canonical records. The Robin Hood table also grows at 90% load (`src/index.c:57`) rather than the required <=70% steady-state load.

## F-028 — High — corrupt existing object can be treated as dedup hit

`darc_repo_has_object()` checks only path existence. `darc_repo_put_object()` returns success if the path exists, without validating the existing object first. The task pack explicitly requires snapshot creation to fail when an existing same-CID object is corrupt rather than silently reusing it.

## F-029 — Medium — object atomic rename does not fsync containing directory

`darc_object_write_file()` fsyncs the temp file then renames it, but does not fsync the object directory after rename (`src/object.c:88-103`). This weakens the required durability contract.

## F-030 — High — verification levels do not match the specified semantics

Quick verify mostly checks whether ref target object paths exist. Full/scrub scan object files but do not validate complete TREE/FILE/SNAPSHOT semantics, totals, type references, tree ordering, full-file digests, Merkle reachability, index consistency, or full parity semantics as required.

## F-031 — High — supplied green catalog is not the mandatory catalog

`tests/catalog_runner.sh` reports only 22 broad checks. `docs/TRACEABILITY.md` admits the full catalog is only partially mapped. The original task pack requires every mandatory test ID to map to concrete tests before completion.

## F-032 — High — stress coverage is orders of magnitude below requirement

`tests/stress.sh` creates 30 small files. The required stress gates include 20,000 files, 512 MiB mixed data, a 512 MiB single file, 10 overlapping snapshots, and >=100k chunk references.

## F-033 — Medium — README status is internally inconsistent with DoD

The README labels many gates PASS while also listing mandatory areas as PARTIAL. Under the task pack, any mandatory partial means completion cannot be claimed. Naming the artifact `gates_closed` is therefore unsupported by its own documentation.
