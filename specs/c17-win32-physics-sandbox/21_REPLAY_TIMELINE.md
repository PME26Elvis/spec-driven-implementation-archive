# Replay Timeline, Scrubber, Checkpoints, and Time-Travel Diagnostics

Version: **1.0**
Status: **Normative**

## 1. Purpose

The deterministic replay system must be usable as an interactive debugging surface, not only as a file-format test. The Timeline lets a user navigate a recorded simulation by fixed physics step, reproduce a failure, inspect contacts/joints around the first bad step, and export a minimal diagnostic artifact.

## 2. Timeline source of truth

The Timeline is driven by:

- deterministic replay commands,
- fixed-step indices,
- deterministic checkpoints,
- derived event markers.

It must not be driven by recorded screenshots or precomputed transforms.

Seeking to a step reconstructs the production physics state by loading a valid earlier checkpoint and replaying the remaining commands through the real engine.

## 3. Required controls

The Timeline UI must provide:

- Record,
- Stop Recording,
- Play,
- Pause,
- Stop Playback,
- Step Forward one fixed step,
- Step Backward one fixed step,
- jump to first step,
- jump to final recorded step,
- draggable/scrubbable fixed-step position,
- numeric step entry,
- playback-speed control independent of physics timestep,
- Add Bookmark,
- Remove Bookmark,
- Previous Bookmark,
- Next Bookmark.

## 4. Step backward semantics

Negative-time integration is not required and must not be faked.

Step Backward reconstructs step `N-1` by:

1. locating the nearest valid checkpoint at or before `N-1`,
2. restoring it,
3. replaying commands forward to `N-1`,
4. verifying the reconstructed digest when a reference digest exists.

## 5. Checkpoint cadence

During recording, automatic checkpoints must be created at a configurable interval.

Default interval: every 300 fixed steps.

The interval must be constrained to 30..3600 steps.

Manual bookmarks may optionally force a checkpoint, but bookmarks themselves must not alter physics.

## 6. Timeline visual model

The Timeline must visibly display:

- current fixed-step cursor,
- total recorded fixed steps,
- command markers,
- contact BEGIN/END markers,
- sensor BEGIN/END markers,
- anomaly markers,
- user bookmarks,
- checkpoint markers,
- replay mismatch marker when present.

Different marker classes must be distinguishable by shape and/or label, not color alone.

## 7. Zoom and pan

The timeline supports horizontal zoom and pan for long recordings.

Requirements:

- fit-all view,
- zoom centered on cursor or pointer,
- pan without changing current physics step,
- labels remain readable at ordinary desktop width,
- marker aggregation may be used when dense but selecting/zooming must reveal individual markers.

## 8. Event marker derivation

Markers must derive from real engine/replay logs.

Hard-coded demonstration markers are prohibited.

At minimum:

- contact begin/end,
- sensor begin/end,
- body wake/sleep,
- CCD TOI,
- invariant/anomaly failure,
- replay divergence,
- user bookmark.

## 9. Timeline and Solver Inspector

Selecting a contact/joint-related marker must provide a route to Solver Inspector context when the corresponding entity still exists at that step.

The Timeline must support a workflow:

1. seek to step before anomaly,
2. Capture Next Step in Solver Inspector,
3. step forward,
4. inspect iteration trace,
5. move backward and repeat without changing the reconstructed physics result.

## 10. Timeline and trajectory graphs

When motion recording is enabled, moving the replay cursor must move the trajectory/time-series cursor to the same fixed-step time where recorded data exists.

The graph must not silently interpolate a different physics state when the Timeline shows an exact fixed step.

## 11. Timeline and scene editing

During playback/scrubbing:

- ordinary physics-affecting scene edits are disabled by default,
- the user may choose `Fork From Here`.

`Fork From Here` creates a new live scene/replay branch at the current reconstructed state.

The branch:

- receives a new replay identity,
- retains provenance metadata pointing to source replay + source step,
- may then be edited normally,
- must not overwrite the original replay silently.

## 12. Bookmarks

A bookmark contains:

- replay ID,
- fixed-step index,
- optional short UTF-8 label,
- optional body/joint/contact reference IDs where stable,
- optional note up to the documented maximum length.

Bookmark creation/removal is replay metadata and does not change the physics state digest.

## 13. Mismatch navigation

When replay verification finds a mismatch, the Timeline must:

- mark the first known mismatch step,
- allow jumping directly to it,
- show expected vs actual digest,
- link to detailed field-level mismatch report when available,
- preserve the reproduction replay artifact.

## 14. Anomaly integration

Anomaly Sentinel events from `17_RELEASE_ACCEPTANCE_SYSTEM.md` must appear as Timeline markers.

Selecting an anomaly shows at least:

- anomaly type,
- step,
- relevant entity IDs,
- measured value,
- configured safety cap/tolerance,
- reproduction artifact path if generated.

## 15. Persistence format

Timeline metadata may be stored inside the replay container or in a sidecar file, but versioning and association must be explicit.

Persist at least:

- replay format version,
- checkpoint format version,
- timeline metadata version,
- bookmarks,
- derived marker cache if present,
- source-replay provenance for forks.

Derived marker caches may be regenerated and may not be the sole source of truth.

## 16. Long replay behavior

The Timeline must support mandatory 100,000-step replay fixtures without requiring one full world snapshot per step.

Memory use must remain bounded by documented checkpoint/metadata storage rather than unbounded frame capture.

## 17. Seeking correctness

For any target step T in a deterministic replay, state reconstructed by Timeline seek must match state reached by uninterrupted forward replay to T.

The canonical state digest is the primary exact acceptance comparison.

## 18. Seek performance reporting

`perfbench` must report:

- cold seek to random step,
- warm seek near current step,
- step backward latency,
- checkpoint load time,
- replay-forward reconstruction steps/second,
- memory footprint for 100,000-step timeline metadata/checkpoints.

No fixed absolute UI latency gate is imposed across all hardware, but release evidence must include measurements and must not use an O(total steps) full restart for every one-step backward operation when a nearer checkpoint exists.

## 19. Mandatory timeline tests

- **TLN-01** seek step 0 digest equals initial state.
- **TLN-02** seek final step digest equals uninterrupted final digest.
- **TLN-03** seek 10 deterministic random steps equals uninterrupted reference digests.
- **TLN-04** Step Forward changes exactly one fixed step.
- **TLN-05** Step Backward reconstructs exactly previous fixed step.
- **TLN-06** 100 forward/back oscillations remain digest exact.
- **TLN-07** automatic checkpoint nearest-predecessor selection is correct.
- **TLN-08** changing checkpoint cadence does not change reconstructed physics state.
- **TLN-09** Play at 0.25x/1x/4x presentation speed reaches identical physics digests.
- **TLN-10** dragging scrubber while paused does not run unrequested extra physics steps.
- **TLN-11** contact markers match production contact lifecycle log.
- **TLN-12** sensor markers match sensor lifecycle log.
- **TLN-13** sleep/wake markers match body state transitions.
- **TLN-14** CCD marker fraction/step matches TOI report.
- **TLN-15** anomaly marker matches Sentinel report.
- **TLN-16** induced replay mismatch marks first known mismatch and links reproduction.
- **TLN-17** bookmarks round-trip save/load.
- **TLN-18** bookmark add/remove does not change physics digest.
- **TLN-19** Fork From Here starts from exact current digest and leaves original replay unchanged.
- **TLN-20** fork provenance persists.
- **TLN-21** Timeline + Solver Inspector recapture at same step produces identical solver trace.
- **TLN-22** Timeline cursor and motion graph cursor agree on fixed-step time.
- **TLN-23** 100,000-step replay supports seek without unbounded per-frame snapshot growth.
- **TLN-24** malformed checkpoint referenced by timeline fails transactionally with diagnostic.
- **TLN-25** deleted/corrupt replay cannot leave application in partially reconstructed world state.
- **TLN-26** repeated random seek sequence is deterministic across five runs.
- **TLN-27** render cadence does not affect seek result.
- **TLN-28** window resize/debug-overlay changes do not affect seek result.

## 20. E2E workflows

Mandatory E2E:

- **E2E-TLN-01** record a bridge impact, add bookmark at visible oscillation, stop, scrub backward/forward, verify reconstruction;
- **E2E-TLN-02** record a known anomaly fixture, jump to anomaly marker, capture Solver Inspector next step;
- **E2E-TLN-03** load long replay, jump numeric step, step backward, step forward, verify displayed digest/step;
- **E2E-TLN-04** fork from bookmarked step, apply impulse, verify original replay remains unchanged;
- **E2E-TLN-05** save/load replay with bookmarks and checkpoints;
- **E2E-TLN-06** induce controlled replay mismatch in verification fixture and navigate to mismatch marker.

## 21. Acceptance evidence

Evidence includes:

- Timeline overview screenshot,
- dense-marker zoom screenshot,
- anomaly-to-Solver-Inspector workflow frame sequence or short recording,
- seek digest comparison report,
- 100,000-step memory/performance report,
- fork provenance example,
- mismatch navigation report.

## 22. Release blocking conditions

Release is BLOCKED if:

- seek reconstruction differs from uninterrupted replay,
- Step Backward uses fake inverse physics and produces divergent state,
- timeline markers are decorative/hard-coded,
- bookmarks alter physics state,
- checkpoints are not validated before mutation,
- original replay can be silently overwritten by fork,
- mismatch/anomaly cannot be navigated to a reproducible step,
- any mandatory `TLN-*` test fails/skips,
- evidence is missing.

## 23. Complete condition

Replay Timeline is complete when deterministic reconstruction, navigation, checkpoints, markers, Solver Inspector integration, forking, persistence, long-recording behavior, tests, and acceptance evidence all pass the global release system.
