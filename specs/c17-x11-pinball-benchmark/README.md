# Pinball Sandbox Benchmark Task Package

Version: 1.0.0  
Target product: Linux desktop 2D pinball physics sandbox and table editor  
Implementation language: C17

## 1. Purpose

This package defines a complete, fixed software implementation assignment and its acceptance contract. It specifies product behavior, engineering constraints, required deliverables, and verification outcomes; it does not prescribe a development workflow.

The assignment is deliberately broader than a student demo. The expected result is a coherent desktop application with a hand-built UI layer, deterministic physics, a table editor, reusable scene files, replay support, headless verification, debugging instrumentation, automated tests, and engineering utilities.

## 2. Normative language

The keywords MUST, MUST NOT, REQUIRED, SHALL, SHALL NOT, SHOULD, SHOULD NOT, and MAY are normative. MUST/SHALL requirements are mandatory for release acceptance. A SHOULD requirement may only be omitted when the implementation documents a concrete technical reason and the omission does not violate a Release Gate.

## 3. Scope summary

The finished deliverable contains two classes of software.

### Main application

- Linux/X11 desktop application.
- Edit Mode for building a pinball table.
- Play Mode for deterministic simulation and gameplay.
- Hand-built software-rendered UI.
- Single-ball and multiball simulation.
- Save/load, undo/redo, validation, replay, diagnostics, and visual polish.

### Required engineering utilities

- categorized source/document line counter with JSON/YAML configuration support;
- standalone scene validator;
- headless simulation and regression runner;
- replay validator;
- deterministic trace comparison capability;
- release/evidence consistency validator;
- unified automated test runner.

## 4. Platform and dependency boundary

The target platform is Linux with X11. The implementation MUST be written in C17.

Permitted platform facilities are intentionally low-level:

- ISO C17 standard library;
- X11 through Xlib and/or XCB for window creation, input/event delivery, selections/clipboard, low-level locale/font-set/text-input primitives, basic platform interaction, and presentation;
- minimal Linux/POSIX primitives needed for monotonic timing, filesystem durability, process exit status, and equivalent operating-system services.

The following are prohibited:

- GTK;
- Qt;
- SDL;
- GLFW;
- Cairo;
- Skia;
- Electron;
- browser/WebView-based UI;
- ncurses as the primary UI;
- OpenGL, Vulkan, or another graphics engine used to avoid implementing the required software renderer;
- third-party physics engines;
- third-party scene, serialization, animation, test, parsing, or UI frameworks;
- copied/generated prebuilt implementations that replace a required subsystem.

The use of X11 does not exempt the implementation from building the required UI system. The program MUST NOT use stock X11 widgets as substitutes for required controls.

## 5. Required document reading order

1. `docs/00_assignment_brief.md`
2. `docs/01_product_requirements.md`
3. `docs/02_ui_ux_spec.md`
4. `docs/03_editor_spec.md`
5. `docs/04_physics_spec.md`
6. `docs/05_gameplay_scoring_events.md`
7. `docs/06_scene_file_format.md`
8. `docs/07_replay_headless_cli.md`
9. `docs/08_error_handling_edge_cases.md`
10. `docs/09_testing_validation.md`
11. `docs/10_visual_acceptance.md`
12. `docs/11_engineering_utilities.md`
13. `docs/12_delivery_dod_release_gates.md`
14. `docs/13_manual_acceptance_checklist.md`
15. `docs/14_traceability_matrix.md`
16. `docs/15_normative_physics_math.md`
17. `docs/16_desktop_interaction_spec.md`
18. `docs/17_reference_defaults_and_limits.md`
19. `docs/18_prohibited_substitutions_and_integrity.md`
20. `docs/19_advanced_editor_contracts.md`
21. `docs/20_runtime_timing_determinism_and_numeric_safety.md`
22. `docs/21_pinball_mechanisms_nudge_tilt_and_targets.md`
23. `docs/22_diagnostics_tracing_and_comparison.md`
24. `docs/23_text_input_focus_hidpi_and_reduced_motion.md`
25. `docs/24_reliability_autosave_recovery_and_external_changes.md`
26. `docs/25_performance_memory_and_resource_stability.md`
27. `docs/26_reference_table_and_canonical_user_journey.md`
28. `docs/27_release_evidence_manifest.md`
29. `docs/28_v1_mandatory_test_catalog.md`
30. `docs/29_command_palette.md`
31. `docs/30_v1_scope_resolution_and_non_goals.md`
32. `docs/31_error_and_diagnostic_codes.md`

## 6. Conflict resolution

If two requirements appear to conflict, apply this order of authority:

1. `docs/30_v1_scope_resolution_and_non_goals.md` for final v1.0 scope/exclusion/conflict questions.
2. `docs/12_delivery_dod_release_gates.md` for release status and mandatory completion conditions.
3. subsystem-specific normative documents.
4. `docs/17_reference_defaults_and_limits.md` for consolidated defaults where subsystem text does not override it.
5. `docs/01_product_requirements.md`.
6. `docs/00_assignment_brief.md`.
7. examples and non-normative explanatory text.

An implementation MUST NOT silently choose whichever interpretation is easier. If a genuine ambiguity remains, the implementation's delivery notes MUST identify it and state the chosen interpretation.

## 7. Anti-substitution rule

A visually convincing demonstration is not sufficient. Every visible control MUST be wired to its specified behavior. Every required persistence path MUST use real files. Every required verification mode MUST execute the same production logic that the GUI uses unless a document explicitly permits otherwise.

Disallowed substitutions include:

- hard-coded screenshots instead of live rendering;
- predetermined final ball coordinates instead of physics simulation;
- fake multiball where only one ball owns a physics state;
- animation implemented as an instant state change plus a delay;
- Save buttons that do not persist the complete scene;
- replay output that records timestamps but cannot reproduce inputs;
- headless mode using a separate simplified physics path;
- placeholder inspector values;
- tests that assert constants without invoking production code;
- a scene validator that accepts all input;
- a line counter that counts generated outputs despite configured exclusions.

## 8. Required final artifacts

The implementation delivered in response to this task package MUST include:

- complete source code;
- build definition;
- all required automated tests;
- canonical and adversarial test data created by the implementer;
- required engineering utilities;
- visual evidence set;
- machine-readable `RELEASE_RESULT.json` conforming to `schemas/release_result_schema.json`;
- machine-readable `RELEASE_EVIDENCE.json` conforming to `schemas/release_evidence_schema.json`;
- human-readable test report;
- release checklist with every gate marked PASS/FAIL/BLOCKED/NOT RUN;
- completed visual-evidence index and test report using the required information fields (templates are provided under `templates/`);
- known-issues list, even if empty;
- exact software version string.

No implementation may claim completion while a mandatory Release Gate is failing.

## 9. Package fixtures

This package includes acceptance fixtures and expected-value data. They define minimum externally visible behavior and MUST NOT be deleted, weakened, or replaced by easier cases. Implementations MAY add additional tests and fixtures.

## 10. Version status

This is the normative **v1.0.0** task package. Mandatory scope is frozen for this benchmark revision. Later package versions may correct contradictions or issue explicitly versioned changes; an implementation claiming v1.0.0 is evaluated against this package as delivered.
