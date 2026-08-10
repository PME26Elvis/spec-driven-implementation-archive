# Acceptance Fixtures and Machine-Readable Contracts

This directory contains task-package-owned acceptance inputs. They are specification data, not implementation code, and MUST NOT be weakened, deleted, or replaced by easier cases in a submitted implementation.

## 1. Baseline legacy physics fixtures

The original ten deterministic scenario fixtures intentionally remain `PINBALL_TABLE 1` inputs:

- `fixtures/gravity_drop.pbt`;
- `fixtures/perfect_bounce.pbt`;
- `fixtures/friction_ramp.pbt`;
- `fixtures/high_speed_thin_wall.pbt`;
- `fixtures/flipper_strike.pbt`;
- `fixtures/bumper_ring.pbt`;
- `fixtures/sensor_crossing.pbt`;
- `fixtures/eight_ball_collision.pbt`;
- `fixtures/drain_test.pbt`;
- `fixtures/multiball_stress.pbt`.

This is deliberate: v1.0.0 requires legacy format-1 loading/migration while preserving the baseline physical outcomes.

`scenario_manifest.json` defines their minimum setup/predicates.

## 2. Current format-2 fixtures

- `fixtures/reference_full_game_v2.pbt` — official playable full-table acceptance scene exercising all 15 mandatory object types and current v2 editor metadata;
- `fixtures/stationary_no_force_v2.pbt` — invariant/golden fixture;
- `fixtures/free_flight_v2.pbt` — invariant/golden fixture;
- `fixtures/elastic_head_on_v2.pbt` — equal-mass elastic collision fixture;
- `fixtures/valid_chinese_v2.pbt` — valid UTF-8/Chinese current-format parser fixture;
- `fixtures/valid_crlf_legacy.pbt` — valid CRLF legacy input.

`fixtures/editor_full_table.pbt` is the current-format all-object editor fixture (same comprehensive semantic content as the official reference table). `fixtures/legacy_editor_full_table_v1.pbt` preserves the original 10-object-type format-1 table specifically for migration compatibility tests.

## 3. Golden checkpoints

`golden_checkpoints_v1.json` contains intermediate expected checkpoints for at least five canonical scenarios. These exist to prevent implementations from matching only a final state after diverging internally.

Numeric tolerances are defined by the manifest and the normative physics documents. Exact discrete fields remain exact.

## 4. Parser robustness corpus

`parser_corpus_manifest.json` maps adversarial files under `malformed/` to required primary diagnostic classes. The corpus includes invalid UTF-8/NUL data and a deliberately overlong line; some corpus files are therefore not normal human-readable text.

The implementation MAY add more malformed/fuzz fixtures but may not omit the task-package corpus.

## 5. Stable requirement list

`requirement_ids.json` is generated from the normative traceability matrix and enumerates every stable v1.0.0 requirement ID expected in final `RELEASE_EVIDENCE.json`.

The source of truth remains `docs/14_traceability_matrix.md`; the JSON exists for mechanical validation.

## 6. Canonical E2E manifest

`canonical_user_journey.json` gives stable J01–J24 step IDs and expected outcome summaries corresponding to document 26. It does not prescribe the automation/capture mechanism.

## 7. Test catalog metadata

`test_catalog_v1.json` records the minimum 420-test domain floors and key named mandatory cases. The detailed normative list remains document 28.

## 8. Headless scenario initialization

For scenario execution without replay, the acceptance harness may initialize enabled Ball Spawn objects in the exact manner declared by the relevant manifest. Tests requiring multiple initial balls or moving flippers MAY construct runtime states through the production physics/game API rather than inventing a separate solver.

## 9. Anti-substitution

Production code MUST NOT branch on acceptance fixture names, expected fingerprints, or known expected coordinates to synthesize passing output. Acceptance data must exercise general production behavior.


## Windows platform parity

`platform_parity_manifest.json` records the frozen Linux v1.0.0 source package hash and SHA-256 values for platform-independent fixtures/schemas that remain byte-identical in this Windows fork. This is intended to keep physics/editor/replay workload comparable while the native platform binding changes.
