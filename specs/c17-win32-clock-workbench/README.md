# Analog Clock Workbench — C17/Win32 Task Pack v1.0.2-win32

## Purpose

This task pack defines a complete software implementation assignment consisting of two mandatory projects:

- **Project A — Development Utilities**: small C17 command-line tools used to inspect, validate, and measure the main project.
- **Project B — Analog Clock Workbench**: a modern interactive desktop clock simulator with a fully hand-built UI, software rendering, time-state synchronization, undo/redo, animation, and JSON/YAML configuration support.

The task pack defines product behavior, engineering constraints, data formats, required tests, deliverables, forbidden substitutions, Definition of Done, and release gates.

This is a **standalone Windows variant** of the assignment. It does not require a second desktop backend or cross-platform abstraction layer; satisfying this package means satisfying the Windows/Win32 contract defined here.

## Core challenge

The implementation must use **C17** and the ordinary **Win32 desktop API** on 64-bit Windows 10 22H2 / Windows 11 with a deliberately narrow platform interface. Win32 may be used for top-level window creation, message/event delivery, keyboard/mouse input, DPI/resize/focus notifications, pointer capture, high-resolution monotonic timing, filesystem substrate operations, and presenting an application-owned pixel buffer. Higher-level GUI frameworks and rendering libraries are not allowed.

All application UI controls, layout, hit-testing, focus handling, animation, state synchronization, software graphics, text rendering, blur, shadows, modal behavior, configuration parsing, and undo/redo semantics are application responsibilities.

## Mandatory outcomes

A conforming submission must provide all of the following:

1. A buildable C17 source tree.
2. Project A utilities and their tests.
3. Project B desktop application and its tests.
4. JSON and YAML configuration loading using the same logical schema.
5. A single canonical simulated-time model shared by every UI representation.
6. Direct mouse manipulation of analog clock hands.
7. Editable digital time synchronized bidirectionally with the analog clock.
8. Positive, zero, and negative playback rates controlled by a continuous slider.
9. Pause/resume behavior.
10. Transactional undo and redo for user-editing actions.
11. Hand-built modern UI visual effects and transitions.
12. Deterministic test modes and validation evidence described by this task pack.
13. All release gates in `docs/12_release_gates.md` satisfied.

## Documentation map

- `docs/01_scope_and_constraints.md` — platform, language, allowed interfaces, scope boundaries.
- `docs/02_project_a_dev_tools.md` — mandatory development utilities.
- `docs/03_project_b_product.md` — application product requirements.
- `docs/04_time_model_and_sync.md` — canonical time state and synchronization rules.
- `docs/05_interaction_and_undo.md` — direct manipulation, keyboard behavior, undo/redo.
- `docs/06_ui_visual_spec.md` — pages, components, modern visual behavior.
- `docs/07_rendering_and_animation.md` — software renderer and animation requirements.
- `docs/08_config_json_yaml.md` — shared configuration schema, JSON/YAML behavior.
- `docs/09_errors_and_edge_cases.md` — invalid input, resize, corruption, boundary behavior.
- `docs/10_testing_and_validation.md` — mandatory tests and deterministic validation hooks.
- `docs/11_delivery_and_forbidden_substitutions.md` — deliverables and prohibited shortcuts.
- `docs/12_release_gates.md` — Definition of Done and stopping conditions.
- `docs/13_manual_acceptance_checklist.md` — concise human acceptance checklist.
- `docs/14_stateprobe_fixture_schema.md` — exact deterministic state/history fixture contract.

## Interpretation rule

When two requirements appear to conflict, apply them in this order:

1. Release gates and explicit MUST/MUST NOT statements.
2. Behavioral contracts in the topic-specific document.
3. General scope and engineering constraints.
4. Visual examples and explanatory notes.

A visually convincing prototype that violates required state, parsing, undo, persistence, or rendering behavior is not complete.
