# 00 — Assignment Brief

## 1. Assignment

Implement a production-quality Linux desktop application named **Pinball Sandbox**. The application is a 2D pinball table editor, deterministic physics simulator, and playable pinball environment.

The program MUST be implemented in C17 and MUST NOT rely on a high-level GUI framework, rendering engine, physics engine, serialization framework, or third-party test framework. The implementation is expected to build its own small UI and 2D simulation subsystems on top of low-level X11/Linux primitives.

## 2. Product goals

The product SHALL allow a user to:

- construct a pinball table visually;
- place and configure supported table objects;
- save and reopen the table;
- validate malformed or inconsistent table configurations;
- run deterministic physics simulation;
- control flippers and launcher interactively;
- play with one or multiple balls;
- observe score, combo, multiplier, ball count, and game state;
- pause, resume, restart, and single-step simulation;
- inspect physics state and collision diagnostics;
- record and replay deterministic input traces;
- run the same simulation core without the GUI for regression verification.

## 3. Engineering goals

The assignment intentionally exercises:

- C architecture and ownership discipline;
- low-level Linux/X11 desktop programming;
- software 2D rendering;
- custom UI layout and interaction;
- animation state management;
- deterministic fixed-step simulation;
- collision detection and response;
- continuous collision detection for fast balls;
- moving-collider handling for flippers;
- editor command history and reversible operations;
- robust custom text serialization;
- save/load safety;
- replay design;
- headless execution;
- diagnostics and observability;
- automated testing;
- engineering-tool implementation.

## 4. Non-goals

Version 1.0 of the assignment does NOT require:

- 3D rendering;
- arbitrary polygon rigid bodies;
- network multiplayer;
- online accounts;
- online leaderboards;
- cloud synchronization;
- scripting languages such as Lua or JavaScript;
- plugin systems;
- audio synthesis;
- complex particle editors;
- theme marketplaces;
- mobile support;
- Windows or macOS support.

Optional additions MUST NOT replace or destabilize required functionality.

## 5. Expected maturity level

The assignment is not satisfied by a prototype. At minimum, the result SHALL exhibit these qualities:

- all primary workflows are complete;
- malformed input does not crash the application;
- unsaved work is protected;
- editor operations can be undone and redone;
- simulation results are repeatable;
- multiball is real, not cosmetic;
- resize behavior remains usable;
- visual states are polished and consistent;
- automated tests cover normal and adversarial behavior;
- diagnostics make physics failures inspectable;
- release evidence proves major requirements.

## 6. Top-level user workflow

A normal user can perform this sequence:

1. Launch the application.
2. Create a new table.
3. Place boundaries, one spawn, a launcher, two flippers, bumpers, sensors, a gate, and a drain.
4. Configure dimensions and material properties in the Inspector.
5. Run `Validate Scene` and resolve blocking errors.
6. Save the table.
7. Switch to Play Mode.
8. Launch a ball with press-and-release strength control.
9. Operate the flippers.
10. Trigger bumpers, sensors, scoring, and multiball.
11. Pause the simulation.
12. Inspect velocity, contacts, IDs, and collision normals.
13. Advance with Single Step.
14. Resume and finish the current ball.
15. Restart the table.
16. Record and save a replay.
17. Reopen the table and replay the recorded run.
18. Verify the same simulation through headless mode.

The finished product MUST support this sequence without hidden manual editing of files.

## 7. Required modes

- **Edit Mode** — table geometry and configuration are editable; gameplay simulation is not permanently advancing.
- **Play Mode** — table geometry is immutable; runtime state advances through the deterministic physics engine.
- **Simulation Preview** — temporary non-destructive runtime from Edit Mode.

## 8. Stopping condition

The implementation effort stops only when every mandatory Definition of Done item and Release Gate is satisfied, or when the implementer explicitly reports remaining failures.

An unfinished item MUST be described as unfinished. A failing test MUST be described as failing. A known physics instability MUST NOT be hidden behind visual polish.

## 9. v1.0 maturity expansion

The v1.0.0 assignment additionally requires desktop-quality authoring and reliability rather than demo-grade happy paths:

- groups, layers, locking, exact transforms, alignment/distribution, structured clipboard;
- mature focus/text/UTF-8/HiDPI/reduced-motion behavior in the hand-built UI engine;
- Drop Target, Stand-up Target, Rollover, Spinner, Kickout, Nudge, and Tilt;
- simultaneous-contact determinism, numeric-safety barriers, invariants, golden checkpoints, soak/performance/resource gates;
- autosave/recovery, external-modification conflict protection, scene migration, safe startup;
- Event Trace, Collision Trace, Scene Statistics, determinism comparison, Command Palette, Measurement tool;
- official full reference table and canonical cross-subsystem E2E journey;
- machine-readable requirement-to-evidence manifest.

The application remains a fixed Linux/X11 C17 product requirement. No part of this package prescribes a model, agent framework, MCP/tool workflow, screenshot utility, package-install procedure, or development process.
