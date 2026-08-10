# 24 — Reliability, Autosave, Recovery, External Modification, Migration, and Safe Startup

## 1. Reliability goal

User-authored scene data SHALL be protected against common application crashes, partial writes, save failures, external edits, and startup crash loops.

## 2. Canonical save identity

When a scene is loaded from or successfully saved to a path, application records a file identity snapshot including at least:

- canonical path as resolved by the application's file layer;
- file size;
- modification time with available resolution;
- deterministic content fingerprint of bytes read/written.

Content fingerprint is authoritative when metadata alone is ambiguous.

## 3. External modification detection

Before overwriting an existing scene path that was previously loaded/saved, application SHALL compare current disk identity with remembered identity.

If file content changed externally:

- do not overwrite automatically;
- present conflict UI identifying the path and change;
- provide at least Cancel, Save As, and Overwrite Anyway;
- Overwrite Anyway still uses atomic save;
- successful external-change resolution updates remembered identity.

## 4. External deletion

If the backing file was externally deleted while document remains open:

- Save may recreate it only after explicit informational confirmation;
- Save As remains available;
- dirty authored state remains intact.

## 5. Autosave purpose

Autosave writes recovery snapshots. It SHALL NOT silently replace the user's formal `.pbt` file and SHALL NOT clear document dirty state.

## 6. Autosave trigger

When authored document is dirty, write recovery snapshot:

- after 30 seconds of user inactivity following an authored edit; or
- no later than 120 seconds after the first un-autosaved authored edit even if user remains active.

View-only actions do not trigger autosave.

## 7. Autosave atomicity

Recovery snapshots use temp-write + flush/close + atomic rename semantics comparable to normal save.

A failed autosave:

- leaves authored document untouched;
- does not clear dirty state;
- surfaces a non-modal warning;
- retries only after a later edit/interval, not in a tight loop.

## 8. Recovery content

Recovery snapshot SHALL preserve the full authored semantic scene plus metadata needed to identify:

- original backing path if any;
- original file fingerprint if known;
- application/package scene-format version;
- recovery creation time for human display only;
- semantic fingerprint of recovered authored state.

Runtime Play state is excluded.

## 9. Clean shutdown

After a clean application shutdown with no dirty scene, stale recovery for that scene SHALL be removed or marked ignorable.

After a clean shutdown in which user explicitly discards dirty changes, recovery corresponding to discarded changes SHALL be removed so discarded work is not offered again unexpectedly.

## 10. Crash recovery discovery

On startup, before opening a normal last-session scene automatically, scan the application's recovery location for valid snapshots.

If a newer valid recovery differs semantically from its last known formal save, offer recovery.

Options:

- Recover;
- Discard Recovery;
- Inspect/Save As without overwriting original.

## 11. Recovery safety

Recovering opens the recovered state as dirty authored document. It SHALL NOT overwrite the formal path until user explicitly saves.

Corrupt recovery files are reported and skipped; they SHALL NOT prevent normal startup.

## 12. Safe-start crash loop prevention

The application SHALL maintain minimal startup-session marker outside scene data.

If the previous two consecutive launches failed/terminated before reaching a stable main-window-ready marker while attempting automatic last-session restore, next launch enters Safe Startup:

- do not automatically reopen last-session scene;
- open an empty/new document shell;
- explain that automatic restore was skipped;
- recovery files remain available manually;
- user may explicitly reopen the problematic scene.

No network or external service is required.

## 13. Last-session behavior

Remembering last-opened path is allowed. Automatic reopen is optional, but if implemented it SHALL obey Safe Startup and transactional load rules.

## 14. Scene format versions

Current v1.0.0 canonical writer emits:

`PINBALL_TABLE 2`

The parser SHALL support:

- `PINBALL_TABLE 1` legacy input as defined by the v0.1 baseline subset;
- `PINBALL_TABLE 2` current format.

Unknown major version greater than 2 is rejected transactionally.

## 15. Migration from format 1 to 2

Loading v1 scene creates current in-memory semantic model using deterministic defaults for newly introduced properties:

- all old objects assigned to `Gameplay` layer;
- object `locked=false`;
- table nudge/tilt settings use current defaults;
- no groups;
- current mechanism types absent unless authored in v2;
- current authored scene seed defaults to 0;
- current UI-independent metadata initialized deterministically.

Application SHALL visibly indicate that the scene was migrated and will be written as format 2 on next Save.

## 16. Migration dirty state

Loading a valid legacy v1 scene does not immediately count as a user edit. The document may display a migration-needed indicator while remaining semantically clean relative to loaded legacy content.

Saving writes v2 and establishes new clean checkpoint.

## 17. Downgrade

Saving back to v1 format is not required.

## 18. Atomic save fault injection

The implementation SHALL provide a test seam capable of deterministically injecting failure at minimum stages:

1. temporary file creation;
2. mid-serialization write after some bytes;
3. flush/sync;
4. temporary close;
5. final rename/replace.

This seam may be compiled into tests or configured in test harness; it need not be exposed in production UI.

## 19. Save-failure invariants

For every injected save failure:

- prior valid destination bytes remain intact if destination previously existed;
- document remains dirty;
- remembered file identity remains prior successful identity;
- temporary artifacts are cleaned when safely possible;
- error identifies failing stage without claiming success.

## 20. Disk-full behavior

A deterministic test double or bounded filesystem may simulate no-space write. Same save-failure invariants apply.

## 21. Transactional load failure

Malformed/corrupt/unsupported scenes are parsed into temporary model. On failure:

- current document semantic fingerprint unchanged;
- Undo history unchanged;
- dirty state unchanged;
- current backing path unchanged;
- selection/view state may remain unchanged;
- clear diagnostic returned.

## 22. Parser diagnostic minimum

A parse/validation diagnostic SHALL contain when known:

- severity;
- stable diagnostic code;
- file path;
- 1-based line number;
- section/type;
- object/event ID;
- field/action name;
- offending token excerpt bounded to safe length;
- human-readable reason.

## 23. Required parser robustness corpus

Acceptance corpus SHALL include at least:

- empty file;
- comments only;
- truncated header;
- unsupported major version;
- invalid UTF-8;
- NUL byte;
- line exceeding 1 MiB;
- token exceeding string/ID hard limit;
- duplicate table/object/event ID;
- duplicate property;
- unknown object/action;
- missing required field;
- wrong scalar type;
- NaN/Inf spellings;
- exponent overflow;
- extremely large integer;
- negative unsigned-like count;
- malformed vector/tuple;
- unknown escape;
- unterminated string;
- dangling target reference;
- wrong target type;
- event action index gap/duplicate;
- event cycle near budget;
- object count exactly at and one above limit;
- event/action count exactly at and one above limit;
- valid Chinese UTF-8 names;
- CRLF valid file;
- valid legacy format migration.

## 24. Hard-limit failure

When parser/editor reaches a normative hard limit, it SHALL fail gracefully before integer overflow/allocation runaway.

Error diagnostics must identify the exceeded limit and actual count/size where practical.

## 25. Resource allocation failure

Core operations SHALL handle allocation failure paths without dereferencing null/invalid pointers. Automated fault injection SHOULD cover representative parser, clipboard, Undo, trace-buffer, and scene-copy allocations.

At minimum, allocation failure during transactional load/save MUST be tested.

## 26. Recovery tests

Mandatory automated/integration cases:

- dirty inactivity creates recovery;
- continuous activity still autosaves within 120s logical test clock;
- autosave failure preserves dirty document;
- clean save removes/supersedes recovery;
- discard removes stale recovery;
- simulated crash leaves recovery offered next launch;
- recovered document opens dirty and does not overwrite original;
- corrupt recovery skipped safely;
- two failed restore launches trigger Safe Startup.

## 27. External-change tests

Mandatory cases:

- unchanged disk Save succeeds;
- external byte change conflicts;
- metadata-only change with same content does not false-conflict when fingerprint proves equality;
- external delete requires confirmation;
- Overwrite Anyway succeeds atomically;
- Cancel preserves dirty state;
- Save As resolves to new path without touching externally changed original.

## 28. Large boundary fixture recipes

To keep the task package compact while still fixing exact boundary inputs, `acceptance/boundary_generation_manifest.json` normatively defines deterministic recipes for exact/over-limit object, event, action, group, and layer cases. A conforming test suite SHALL materialize equivalent inputs and verify the specified outcomes. The generation mechanism is implementation-defined; the counts and semantic content are not.
