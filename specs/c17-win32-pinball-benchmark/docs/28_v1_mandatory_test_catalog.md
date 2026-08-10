# 28 — v1.0 Mandatory Test Catalog and Minimum Coverage

This catalog increases the acceptance floor from the v0.1 baseline. Counts are minimum meaningful tests; duplicate parameterizations that assert the same trivial condition do not satisfy distinct coverage obligations.

## 1. New minimum

v1.0.0 requires at least **420 meaningful automated tests** across the required domains, with all mandatory named cases present.

## 2. Domain minimums

| Domain | Minimum tests |
|---|---:|
| Math/vector/geometry/numeric | 35 |
| Physics integration/collision/CCD/invariants | 85 |
| Pinball mechanisms/nudge/tilt | 45 |
| Event/gameplay/scoring | 45 |
| Scene parser/writer/migration/recovery | 55 |
| Editor/history/clipboard/layers | 60 |
| Replay/headless/determinism/traces | 35 |
| UI state/focus/text/scale/animation | 35 |
| Utilities/release evidence/LOC | 15 |
| Performance/resource/fault-injection integration | 10 |
| **Total minimum** | **420** |

A single test may exercise several domains but counts toward exactly one domain for minimum-count accounting.

## 3. Physics named cases

Mandatory named cases include prior baseline plus:

- `stationary_no_force_10000_steps`;
- `free_flight_velocity_invariant`;
- `equal_mass_elastic_momentum_energy`;
- `wall_tangent_preservation`;
- `simultaneous_corner_contact`;
- `acute_wedge_contact`;
- `wall_bumper_same_toi`;
- `three_ball_simultaneous_contact`;
- `64_ball_headless_stress`;
- `world_escape_detection`;
- `nonfinite_runtime_barrier`;
- `frame_stall_catchup_cap`;
- `speed_multiplier_state_equivalence`.

## 4. Mechanism named cases

As listed in `21_pinball_mechanisms_nudge_tilt_and_targets.md`, plus save/load/editor round-trip for each new object type.

## 5. Sensor/event named cases

- ENTER only;
- STAY repeated one per step;
- EXIT only;
- high-speed ENTER+EXIT same step with no STAY;
- two Sensors crossed same sub-time deterministic order;
- action chain three levels;
- direct cycle;
- indirect cycle;
- action budget exactly at limit;
- one above limit;
- event target disabled/enabled timing;
- spawn action reaching active-ball cap atomically.

## 6. Editor named cases

- overlap cycling;
- Shift selection add/remove;
- marquee replace/union/subtract;
- hidden-layer hit-test exclusion;
- layer visibility not physics enable;
- layer lock transform prevention;
- object lock mixed-selection skip;
- create/ungroup Undo;
- group rotate exact member coordinates;
- Align six variants;
- Distribute four variants;
- mixed Inspector display/model;
- invalid numeric edit no mutation;
- drag 300 pointer events -> one Undo;
- multi-edit -> one Undo;
- Save/edit/Undo clean-state restoration;
- history eviction clean-state correctness;
- clipboard internal-reference remap;
- external reference preservation;
- dangling external reference policy;
- repeated paste unique IDs;
- exact object-limit paste atomic rejection;
- pointer-centered zoom;
- Fit Scene padding;
- Fit Selection;
- pan clamp;
- measurement distance/angle;
- Edit/Play isolation;
- Preview isolation.

## 7. Text/focus/UI named cases

- Tab forward/back ordering;
- modal focus trap/wrap;
- focus restore;
- popup outside click;
- stuck-flipper prevention on focus loss;
- UTF-8 scalar cursor/delete;
- Chinese save/load/copy/paste/search;
- invalid UTF-8 rejection;
- long text horizontal scroll;
- numeric field commit/cancel/invalid;
- each 100/125/150/200 scale hit-test alignment;
- Reduced Motion final states;
- interrupted hover/modal/sidebar/capsule transitions;
- damage repaint after close/resize/expose.

## 8. Persistence/recovery named cases

All cases in `24_reliability_autosave_recovery_and_external_changes.md` are mandatory where deterministic automation is practical.

The parser robustness corpus SHALL be data-driven but each required malformed class must produce an individually reportable test ID.

## 9. Migration named cases

- v1 minimal -> current model defaults;
- v1 full baseline object table -> current model;
- migration save emits header 2;
- migrated semantic round trip;
- unknown version 3 rejected transactionally;
- migration indicator/dirty semantics state test.

## 10. Determinism named cases

- same replay 10x exact checkpoint fingerprint;
- GUI vs headless;
- render frame schedule A vs B;
- window resize during playback;
- UI scale variants;
- debug overlay on/off;
- trace on/off;
- P2 speed multipliers equivalent at same fixed step;
- nudge/tilt replay;
- first-divergence report.

## 11. Save fault injection

At least one test for every required failure stage plus disk-full simulation. Each asserts previous file bytes and dirty-state invariants.

## 12. Resource tests

- 1,000 file open/close descriptor stability;
- 100 repeated-cycle memory stability;
- trace buffer cap;
- Undo memory cap/transaction integrity;
- repeated popup/modal Win32 USER/GDI resource stability where measurable;
- clean worker shutdown if threads are used.

## 13. Performance integration

P1/P2/P3 workloads SHALL have repeatable test commands and machine-readable measurements. Hardware-sensitive timing thresholds are evaluated on the actual evaluation environment and reported, not hard-coded as fake PASS.

## 14. Canonical user journey

J01–J24 SHALL be represented by one or more E2E tests/reproducible scripted acceptance flows. The journey does not add 24 to the 420 count automatically unless steps are distinct executable assertions.

## 15. No skips

Mandatory release run contains zero skipped mandatory test IDs. Platform-irrelevant optional tests may be marked optional but do not count toward 420.


## 16. Windows platform-binding mandatory cases

The Windows release catalog SHALL contain named cases covering at least:

- `win_utf16_surrogate_pair_to_utf8`;
- `win_invalid_isolated_surrogate_rejected_or_replaced_safely`;
- `win_unicode_scene_path_roundtrip`;
- `win_custom_picker_unicode_and_space_path`;
- `win_close_routes_dirty_contract`;
- `win_pointer_capture_loss_cancels_or_finishes_safely`;
- `win_dpi_96_120_144_192_layout_mapping`;
- `win_dpi_change_preserves_world_and_simulation`;
- `win_os_dpi_times_user_scale_mapping`;
- `win_headless_creates_no_hwnd`;
- `win_atomic_replace_sharing_violation_preserves_destination`;
- `win_clipboard_busy_no_data_loss`;
- `win_reparse_cycle_locscan_when_follow_enabled`;
- `win_gdi_user_handle_cycle_stable`.

Equivalent names are allowed when the report mapping makes the coverage unambiguous.
