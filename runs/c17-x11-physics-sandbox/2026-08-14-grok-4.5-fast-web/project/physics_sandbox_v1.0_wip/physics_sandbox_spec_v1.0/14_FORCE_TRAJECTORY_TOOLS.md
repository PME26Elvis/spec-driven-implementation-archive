# 14 — Force / Impulse Interaction and Live Motion Analysis

## 1. Purpose

The Sandbox must provide first-class interactive tools for applying externally controlled physical input and observing resulting motion.

These features are not debug-only shortcuts. They are part of the normal product surface and must use the same rigid-body equations, force accumulation, impulse application, angular response, fixed-step simulation, rendering, persistence boundaries, and test infrastructure as the rest of the engine.

The required feature set has two coupled parts:

1. **Force and impulse interaction tools** for applying physically meaningful external input to dynamic bodies.
2. **Live motion analysis** consisting of viewport trajectories, time-series graphs, recording controls, and export of recorded samples.

## 2. Required tools

The Sandbox tool rail must include separate tools named or clearly equivalent to:

- Apply Force.
- Apply Impulse.

They must not be aliases for Mouse Drag.

Mouse Drag is a temporary spring/damped positional constraint. Apply Force and Apply Impulse must directly use the rigid body's force/torque and impulse/angular-impulse equations respectively.

## 3. Eligible bodies

Force and impulse tools may target dynamic bodies only.

When the pointer is over a static or kinematic body:

- the body may still highlight for hit-test feedback;
- committing a force or impulse must be rejected;
- no hidden velocity, force accumulator, or dynamic state may be created;
- the UI must provide a concise visible explanation.

Sleeping dynamic bodies must wake when a non-zero force or impulse is committed to them.

## 4. World-space interaction model

### 4.1 Application point

Pointer-down on a dynamic body selects an application point in world coordinates.

The implementation must retain the application point relative to the body while the gesture is active so that body motion during a live force gesture does not cause the point to jump to an unrelated location.

The UI must offer an **Apply at center of mass** option.

When enabled, the application point is the body's center of mass and no torque may arise solely from an off-center lever arm.

When disabled, the actual hit point is used.

### 4.2 Vector gesture

A drag gesture from the application point defines direction and magnitude.

While previewing, the viewport must render:

- application-point marker;
- directional arrow;
- numeric magnitude;
- vector X/Y components or an equivalent inspectable representation;
- distinct styling for Force versus Impulse mode.

The vector must be derived in world coordinates. Camera zoom and pan must not change the physical result of an otherwise equivalent world-space gesture.

### 4.3 Numeric control

The interaction panel must allow precise numeric entry in addition to pointer gesture control.

At minimum the user can inspect and edit:

- X component;
- Y component.

The UI may additionally expose magnitude and angle, but editing one representation must remain internally consistent with the other.

Invalid, NaN, infinite, or unreasonably overflowing values must be rejected before they reach simulation state.

## 5. Apply Force semantics

Apply Force is a **continuous per-step external force** while the gesture is actively held during a running simulation.

For each fixed physics step in which the gesture remains active:

- add the configured force to the body's force accumulator;
- calculate torque from the current center of mass to the retained application point;
- integrate the resulting linear and angular acceleration through the normal integration path;
- clear transient accumulators according to the engine's normal step lifecycle.

The feature must not directly write a desired velocity.

Releasing the pointer ends the external force immediately for subsequent steps.

If the simulation is paused, the force gesture may be previewed and edited but it must not advance state. The force becomes active only for fixed steps occurring while the gesture is held after simulation resumes.

A visible state must distinguish preview-only paused force from actively applied running force.

## 6. Apply Impulse semantics

Apply Impulse is a **one-shot instantaneous impulse**.

Pointer release commits exactly one impulse for that gesture.

The impulse must:

- update linear velocity by inverse mass;
- update angular velocity when applied off-center using the lever arm and inverse inertia;
- respect rotation lock where applicable;
- wake a sleeping dynamic body;
- not be re-applied on subsequent fixed steps.

Applying an impulse while the simulation is paused is allowed. It changes velocity/angular velocity immediately but must not advance position, angle, simulation time, contact solving, or any other physics step until Play or Single Step occurs.

An impulse of exactly zero must be a no-op.

## 7. Magnitude scaling and safety

The UI must define a documented deterministic mapping between pointer drag distance in world coordinates and physical force/impulse magnitude.

The mapping may use a user-adjustable scale, but:

- the current scale must be visible;
- the default must be defined as a constant;
- the scale must be independent of display DPI and camera zoom;
- changing scale is an editor/UI setting, not a hidden physics change.

Extreme finite values that would predictably overflow internal arithmetic may be refused with a user-visible validation message.

No arbitrary silent clamping is allowed unless the clamp range is explicitly displayed or documented in the product UI.

## 8. Visual feedback and history

Committed impulses must leave a short-lived visual indicator at the application point so an observer can see what was applied.

Active force gestures must remain visibly represented for their full duration.

The visual indicator must not itself affect simulation state.

Debug visualization must optionally show recent externally applied force/impulse vectors with:

- body ID association;
- application point;
- vector direction;
- magnitude;
- force/impulse type.

The history is transient runtime diagnostics and is not required in saved scene JSON.

## 9. Undo/redo and scene dirty state

Force and impulse application change **runtime simulation state**, not the editor scene definition.

Therefore:

- applying force/impulse does not create an editor undo entry;
- applying force/impulse alone does not mark the scene dirty;
- Reset returns bodies to the editor scene definition and clears the effect of prior runtime force/impulse interactions;
- committing current simulated transforms through any separately defined editor operation follows that operation's normal dirty/undo behavior.

## 10. Motion recording

The application must provide a Motion Analysis panel available from the Sandbox and/or Diagnostics view.

The user must be able to start, stop, clear, and reset a recording session.

Recording must operate on fixed simulation steps, not render frames.

The user can choose one or more bodies to record.

Required capacity:

- at least 8 bodies recorded simultaneously;
- at least 30 seconds of history at 60 fixed steps per second for each recorded body without dropping samples solely because the renderer is slower than physics;
- a configurable sampling interval equal to one fixed step or an integer multiple of it.

Stopping simulation does not implicitly erase recorded data.

Resetting the scene must clearly either begin a new recording segment or clear history according to the user's selected action; it must not silently join discontinuous positions into one continuous trajectory.

## 11. Required recorded channels

For every sampled body, the recorder must retain at least:

- simulation time;
- fixed-step index;
- stable body ID;
- position X;
- position Y;
- rotation angle;
- linear velocity X;
- linear velocity Y;
- scalar speed;
- angular velocity;
- kinetic translational energy;
- kinetic rotational energy.

Optional channels may include acceleration, momentum, contact count, or accumulated impulses.

Derived values must be computed from the same sampled state rather than from unrelated render interpolation.

## 12. Viewport trajectory trail

A recorded body can display a trajectory trail directly in world space.

Requirements:

- trail follows historical center-of-mass positions;
- trail remains spatially attached to world coordinates while camera pans/zooms;
- trail may fade by age but old-to-new ordering must remain understandable;
- current body position must be distinguishable from historical trail;
- trail visibility can be toggled per recorded body or globally;
- clearing recording history clears the corresponding trail;
- trail rendering must not mutate physics state.

The renderer must handle at least **4096 trail samples per body** for 8 simultaneously visible bodies without correctness failures.

If visual decimation is used for performance, export and underlying recorded data must retain the full requested sampling resolution.

## 13. Time-series graph

The Motion Analysis panel must include a custom-rendered time-series graph.

No external plotting/chart library may be used.

The graph must support at least the following selectable channels:

- position X/Y;
- velocity X/Y;
- speed;
- angle;
- angular velocity;
- translational kinetic energy;
- rotational kinetic energy.

At minimum, the user can display one selected channel for multiple recorded bodies simultaneously.

The graph must provide:

- labeled horizontal time axis;
- value axis with numeric labels;
- current units or quantity label;
- legend mapping rendered series to stable body name/ID;
- auto-ranging of Y values;
- sensible handling of constant-valued series;
- clipping to graph bounds;
- cursor hover or scrub readout showing nearest sample time/value;
- clear empty-state presentation when no samples exist.

A graph that is only a decorative sparkline without readable axes/values does not satisfy this requirement.

## 14. Live update behavior

While recording and simulation are running:

- the graph updates incrementally;
- trajectory trails extend incrementally;
- recording must not depend on render FPS;
- a slow render frame must not produce duplicate physics samples for the same step;
- pausing simulation freezes the time axis and creates no additional fixed-step samples.

The UI may use a sliding time window for live display, but the full retained recording remains available for export until explicitly cleared or capacity policy removes it.

If a bounded ring buffer is used, its capacity and dropped-oldest behavior must be explicit in the UI or specification constants.

## 15. Graph navigation

The graph must support:

- reset/fit to current recorded range;
- horizontal time-window zoom or selectable visible time range;
- horizontal pan/scrub through retained history after recording stops.

Graph navigation is UI state and must not alter simulation time.

## 16. Recording and export integration

Trajectory CSV export must be able to export the Motion Analysis recorder's actual retained samples.

The export must preserve:

- deterministic row ordering;
- stable body IDs;
- simulation time;
- fixed-step index;
- all required recorded channels from Section 11, unless the user explicitly selects a documented subset.

Exporting must not clear the recorder.

If no samples exist, the export action must not silently create a misleading empty-success file; the UI must clearly report that there is no recorded trajectory to export.

## 17. Numerical correctness requirements

Force/impulse verification must include these release-blocking deterministic cases:

- **FRC-01** Center-of-mass impulse changes linear velocity without introducing angular velocity for a body with zero initial angular velocity.
- **FRC-02** Equal impulse applied at different off-center points produces identical linear velocity change but different angular velocity according to lever arm.
- **FRC-03** Equal and opposite center impulses on an isolated body restore the original linear velocity within floating-point tolerance.
- **FRC-04** Constant center-of-mass force on an isolated body produces acceleration consistent with `a = F / m` under the engine's semi-implicit Euler integration.
- **FRC-05** Applying the same world-space interaction at different camera zoom levels yields equivalent physical state within tolerance.
- **FRC-06** A force held for `N` fixed steps affects exactly `N` steps.
- **FRC-07** A one-shot impulse is applied exactly once.
- **FRC-08** Applying an impulse while paused does not advance simulation time or position.

Tolerance values must be defined by the verification suite and be strict enough to catch implementation mistakes rather than visual approximation.

## 18. Recorder verification requirements

Automated verification must demonstrate these release-blocking cases:

- **REC-01** fixed-step sample count is correct for a known recording duration;
- **REC-02** no sample is created while simulation time is paused;
- **REC-03** recorded position/velocity equals engine state at the same fixed-step index within tolerance;
- **REC-04** CSV row count and ordering match retained recorder data;
- **REC-05** reset/segment behavior does not create a fake continuous line across discontinuous scene states;
- **REC-06** ring-buffer eviction, if implemented, is deterministic; if no ring buffer is used, bounded-capacity behavior is tested instead;
- **REC-07** graph auto-range handles positive-only, negative-only, mixed-sign, and constant data;
- **REC-08** nearest-sample cursor lookup chooses the expected fixed-step sample;
- **REC-09** trail transforms correctly under camera pan/zoom because samples remain in world coordinates.

## 19. Required E2E scenarios

The GUI/E2E suite must include at least:

### 19.1 E2E-FT-01 — Center impulse

- load or create a known dynamic body;
- pause simulation;
- choose Apply Impulse;
- apply a known numeric center impulse;
- verify velocity changes and position/time do not advance;
- single-step and verify motion begins from the new velocity.

### 19.2 E2E-FT-02 — Off-center impulse

- apply an impulse away from center of mass;
- verify visible angular motion after stepping;
- verify interaction arrow/application marker appeared.

### 19.3 E2E-FT-03 — Sustained force

- run simulation in a scene without interfering contacts;
- hold a known force for a deterministic number of fixed steps;
- release;
- verify acceleration/velocity against expected tolerance.

### 19.4 E2E-FT-04 — Live trajectory

- record a moving body;
- verify world-space trail becomes visible;
- open Motion Analysis;
- verify a chosen channel renders with axes and legend;
- pause and confirm sample count stops growing;
- export retained samples;
- verify exported data corresponds to recorded history.

## 20. Acceptance evidence

Required evidence for these features includes:

- screenshot/frame showing Apply Force preview with application point, vector, and magnitude;
- screenshot/frame showing committed off-center impulse and resulting rotation;
- short frame sequence or recording showing sustained force followed by release;
- screenshot of at least two simultaneous body trajectory trails;
- screenshot of Motion Analysis graph with axes, legend, and at least two body series;
- test report covering Section 17 numerical cases;
- E2E report covering Section 19 workflows;
- representative exported trajectory CSV used by an automated round-trip/consistency check.

The specification requires the evidence artifacts but does not prescribe the mechanism used to capture screenshots or recordings.

## 21. Forbidden substitutes

The following do not satisfy this feature:

- teleporting a body to simulate force;
- directly setting arbitrary velocity and labeling it Apply Force;
- repeatedly re-applying an impulse every frame and labeling it continuous force;
- applying all off-center interactions at center of mass;
- producing torque with a hard-coded visual spin unrelated to inertia/lever arm;
- trajectory trails based on render-frame interpolation instead of recorded physics state;
- generating graph data independently from the actual simulation recorder;
- using an external charting/plotting library;
- exporting fabricated or pre-generated motion samples;
- showing a static example graph unrelated to the selected bodies;
- treating a decorative line with no axes/readable values as the required Motion Analysis graph.

## 26. v1.0 Replay Timeline integration

When a deterministic replay is loaded and Motion Analysis samples are available or reproducibly regenerated, the Timeline cursor and graph cursor must refer to the same fixed-step time.

Seeking backward/forward must not fabricate trajectory samples from screen-space paths. Samples used after seek must come from the reconstructed production state/recorder path.

Golden evidence may use trajectory plots, but the plotted data must be the exact measured sample stream from the Golden run or its deterministic replay.
</file>
