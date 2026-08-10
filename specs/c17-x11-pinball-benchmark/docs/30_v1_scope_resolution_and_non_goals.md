# 30 — v1.0 Scope Resolution, Explicit Exclusions, and Conflict Rules

## 1. Purpose

This document records final mandatory v1.0.0 scope decisions and prevents optional ideas from being mistaken for requirements.

## 2. Included expansion set

The v1.0.0 package incorporates proposal items:

- 1 through 40;
- 41;
- 43;
- 44;
- 46 through 51.

These have been normalized into subsystem requirements rather than retained as informal feature numbers.

## 3. Explicitly excluded proposals

Not mandatory:

- proposal 42: general persistent Status/Diagnostics Center;
- proposal 45: Ghost Trajectory Debugger.

Implementations may add extras only if they do not weaken, replace, obscure, or break mandatory behavior.

## 4. Dependency/platform boundary

Product engineering target remains C17 desktop software on Linux/X11.

Allowed baseline platform APIs are limited to the dependency policy in Assignment/Product documents. The task package defines product/engineering requirements, not agent/tool execution workflow.

No requirement mandates how screenshots are captured, how packages are installed, or how a specific model/framework should operate.

## 5. UI engine maturity

The custom UI engine SHALL now satisfy not only visual effects but desktop-quality focus, text editing, UTF-8 preservation, HiDPI scaling, animation interruption, clipping/damage repaint, modal capture, command palette, and responsive resizing.

## 6. Physics maturity

Physics scope includes deterministic fixed-step solver, CCD, multiball, moving flippers, simultaneous contact ordering, numeric safety, energy/invariant checks, nudge/tilt, spinner dynamics, targets, Kickout capture/ejection, and stress behavior.

Arbitrary general convex polygon rigid bodies remain out of scope.

## 7. Editor maturity

Editor scope includes groups, layers, locking, exact Inspector transforms, deterministic selection, align/distribute, robust structured clipboard, semantic dirty state, autosave/recovery, external conflict handling, version migration, measurement, validation, and performance floors.

## 8. Reliability maturity

A visually functioning app that can lose/corrupt user scenes, overwrite external changes silently, or enter startup crash loops does not meet v1.0.0.

## 9. Verification maturity

Passing the visual checklist alone is insufficient. The final release requires:

- >=420 meaningful automated tests;
- canonical fixtures/checkpoints;
- invariants/soak/performance/resource tests;
- canonical E2E journey;
- machine-readable evidence mapping;
- all Release Gates PASS.

## 10. Conflict precedence

If task-package documents conflict, use this precedence for v1.0.0:

1. `30_v1_scope_resolution_and_non_goals.md` for inclusion/exclusion/platform-scope questions;
2. subsystem-specific normative document for subsystem behavior;
3. `17_reference_defaults_and_limits.md` for consolidated numeric defaults only when not contradicted by subsystem-specific value;
4. `14_traceability_matrix.md` for verification association, not behavior definition;
5. README/Assignment summary last.

Any discovered conflict SHALL be treated as a task-package defect and resolved before implementation release claim rather than guessed silently.
