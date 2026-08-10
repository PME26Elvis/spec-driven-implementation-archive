# Task Package Changelog

## 1.0.0

This is the first frozen full-scope benchmark release.

### Product/editor expansion

- expanded mandatory gameplay/editor object set from 10 to 15 types;
- added Drop Target, Stand-up Target, Rollover, Spinner, and Kickout;
- added deterministic Nudge/Tilt system;
- added Groups, Layers, Locking, exact transforms, alignment/distribution, overlap-selection cycling, structured clipboard reference remapping, Measurement, and semantic dirty-state behavior;
- made Undo/Redo transaction and memory behavior explicit;
- formalized Edit/runtime/Preview state isolation.

### UI engine maturity

- added complete focus and keyboard-navigation contract;
- added UTF-8 scalar-safe text editing and Chinese preservation/search expectations;
- added 100/125/150/200% UI scale requirements;
- added Reduced Motion;
- added popup/modal capture, focus restoration, animation interruption, clipping/damage repaint requirements;
- added Command Palette.

### Physics/determinism

- formalized simultaneous-contact ordering and same-TOI contact sets;
- added real-time stall/catch-up policy without variable physics timestep;
- added numeric finite-value barriers and inspectable runtime failure states;
- added deterministic PRNG policy when optional randomness exists;
- added golden intermediate checkpoints and invariant tolerances;
- added Spinner fixed-pivot angular impulse/friction/tick mathematics;
- added Kickout swept capture/ejection and blocked-ejection semantics;
- added Nudge/Tilt ordering within fixed steps;
- added 64-ball headless stress alongside existing long-run/multiball stress.

### Persistence/reliability

- current canonical writer is now `PINBALL_TABLE 2`;
- format-1 reader/migration is mandatory;
- added layers/groups/editor metadata to format 2;
- added autosave recovery that never silently replaces formal saves;
- added external modification/deletion conflict handling;
- added Safe Startup crash-loop prevention;
- added deterministic atomic-save fault injection and disk-full requirements;
- added stable parse/runtime/release diagnostic code classes;
- added malformed parser corpus and large-boundary generation recipes.

### Diagnostics and engineering utilities

- added Event Trace, Collision Trace, Scene Statistics;
- added `detcompare` first-divergence capability;
- added `releasecheck` machine-readable release/evidence consistency validation;
- explicitly excluded a general Diagnostics Center and Ghost Trajectory from mandatory v1.0 scope.

### Acceptance/release quality

- automated-test floor increased from 245 to 420 meaningful tests with domain minimums;
- visual evidence expanded to V01–V38 and A01–A25;
- added P1/P2/P3 performance workloads and resource-stability gates;
- added official current-format full playable reference table;
- added canonical J01–J24 end-to-end user journey;
- added 163 stable requirement IDs;
- added `RELEASE_EVIDENCE.json` schema and requirement-to-proof contract;
- expanded Release Gates for Advanced Editor, Mechanisms/Tilt, Desktop Interaction/HiDPI, Reliability/Recovery, Diagnostics/Trace, Performance/Resource, Canonical E2E, and Release Evidence.

## 0.1.0

Initial structured specification package establishing C17/X11 constraints, hand-built UI/software rendering, 10 baseline pinball objects, deterministic fixed-step physics/CCD, editor, gameplay/events, replay/headless verification, engineering utilities, visual evidence, and Release Gates.
