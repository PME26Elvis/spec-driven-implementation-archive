# 01 — Product Scope and User Workflows

## 1. Product goal

The product is an interactive desktop sandbox for constructing, simulating, inspecting, saving, loading, and validating 2D rigid-body scenes.

A user must be able to begin with an empty or built-in scene, create bodies and joints, tune physical properties, run or single-step the simulation, inspect collision behavior, save the scene, reload it later, and export diagnostic data.

## 2. Top-level layout

The application window must contain:

- A top navigation bar.
- A central content region.
- In Sandbox view, a left creation/tool rail.
- In Sandbox view, the world viewport.
- In Sandbox view, a right inspector panel.
- In Sandbox view, a bottom status/diagnostic strip.
- Modal and popover layers rendered by the application.

The layout must remain usable at window sizes from **960×640** through **1920×1080**.

At widths below 1180 pixels, the right inspector may collapse behind an explicit inspector button, but all inspector functions must remain accessible.

## 3. Top navigation

Required top-level destinations:

- Sandbox
- Scenes
- Diagnostics
- About

The active destination must be shown by an animated capsule indicator that slides between destinations rather than teleporting instantly.

The navigation bar must support the dynamic frosted behavior specified in `06_UI_UX_RENDERING.md` when scrollable content moves beneath it.

## 4. Sandbox view

### 4.1 Default state

On first launch without a previously selected scene, Sandbox must display a small built-in starter scene containing:

- One static floor.
- One dynamic rectangle.
- One dynamic circle.
- Gravity enabled.
- Simulation initially paused.

The user must be able to press Play immediately.

### 4.2 Simulation transport controls

The Sandbox must expose:

- Play
- Pause
- Single Step
- Reset to scene-start state
- Restart current scene from saved definition
- Simulation speed selector
- Fixed time-step display

Required simulation speed options:

- 0.25×
- 0.5×
- 1×
- 2×
- 4×

Changing simulation speed must change simulated-time advancement, not the physical coefficients stored on bodies.

Single Step must advance exactly one fixed physics step while remaining paused.

### 4.3 World controls

Global controls must include:

- Gravity X
- Gravity Y
- Solver velocity iterations
- Solver position/stabilization iterations if separately implemented
- Sleep enable/disable
- Debug visualization master switch

Gravity defaults to `(0, +9.81)` in the application's world convention, with positive Y downward.

### 4.4 Body creation tools

The left tool rail must provide tools for:

- Select
- Pan
- Create Circle
- Create Rectangle
- Create Convex Polygon
- Create Distance Joint
- Create Revolute Joint
- Mouse Drag mode
- Apply Force
- Apply Impulse
- Delete selected object

Body creation must work by direct interaction in the viewport.

#### Circle creation

The user defines center and radius by pointer interaction.

Minimum radius: **0.05 world units**.

#### Rectangle creation

The user defines an axis-aligned initial rectangle by drag gesture. Rotation may be changed after creation.

Minimum side length: **0.05 world units**.

#### Convex polygon creation

The user places vertices in sequence and explicitly closes/commits the polygon.

Requirements:

- Minimum 3 vertices.
- Maximum 64 vertices.
- Polygon must be simple and convex.
- Winding may be clockwise or counter-clockwise on input.
- Internal storage must normalize winding consistently.
- Duplicate adjacent vertices must be rejected or removed with a visible explanation.
- Degenerate area must be rejected.
- Concave input must be rejected with an explicit user-visible error.
- The application must not silently triangulate, decompose, or approximate concave input.

### 4.5 Selection model

The user can select a body or joint by clicking it.

Requirements:

- Exactly one primary selection at a time is sufficient for v1.0.
- Selected bodies show a clear selection outline.
- Selected joints show their anchors and constraint line/axis.
- Clicking empty world space clears selection.
- Selection must survive pausing and camera movement.
- Deleting selection updates the inspector immediately.

### 4.6 Body inspector

For dynamic bodies, inspector fields must include:

- Stable body ID
- Optional human-readable name
- Body type
- Shape type
- Position X/Y
- Rotation angle
- Linear velocity X/Y
- Angular velocity
- Mass
- Density
- Moment of inertia, read-only if derived
- Restitution
- Static friction coefficient
- Dynamic friction coefficient
- Rolling resistance coefficient
- Linear damping
- Angular damping
- Collision category
- Collision mask
- Awake/sleeping state
- Rotation lock toggle

Static bodies must expose relevant geometry/material/filter properties but must not expose editable finite mass or dynamic velocity as if they were simulated dynamic state.

Kinematic bodies must allow user-controlled velocity while ignoring force-based acceleration.

### 4.7 Shape geometry editing

When paused, a selected shape may be edited:

- Circle radius.
- Rectangle width and height.
- Convex polygon local vertices.

Geometry edits must recompute:

- AABB.
- Mass if density-driven.
- Moment of inertia.
- Broad-phase proxy.
- Cached contacts as necessary.

Invalid edits must be rejected without corrupting the previous valid body.

### 4.8 Direct manipulation

A dynamic body can be dragged during simulation using a mouse constraint.

Dragging behavior must:

- Preserve physical interaction with other bodies.
- Use a spring/damped constraint, not teleport the body every frame.
- Wake sleeping bodies.
- Release cleanly when pointer button is released.
- Remain stable if the pointer moves quickly.

Paused editing may additionally allow direct transform editing without physical dragging.

### 4.9 External force and impulse interaction

The Sandbox must provide distinct **Apply Force** and **Apply Impulse** tools.

They must support both pointer-defined vectors and precise numeric X/Y components. Off-center application must generate torque through the normal rigid-body equations. Center-of-mass application must be available explicitly.

Apply Force is continuous only across fixed simulation steps for which the gesture remains active. Apply Impulse commits exactly once. Neither feature may be implemented by teleporting transforms or by a decorative velocity override.

Full interaction, visualization, numerical verification, recorder integration, and prohibited-substitute requirements are normative in `14_FORCE_TRAJECTORY_TOOLS.md`.

### 4.10 Camera

The viewport must support:

- Pan.
- Zoom centered on pointer position or viewport center.
- Reset camera.
- Fit entire scene.

Zoom range: at least **0.1× to 20×** relative to default scale.

Camera movement must not alter physics world coordinates.

### 4.11 Undo/redo

Undo and redo are required for editor-state changes performed while paused, including:

- Create body.
- Delete body.
- Transform body.
- Change shape dimensions.
- Change material/property fields.
- Create/delete joints.
- Global gravity changes.

Minimum history depth: **50 commands**.

Running physics simulation does not need to create undo entries for each time step.

Resetting to scene-start state must not destroy the saved editor definition.

## 5. Scene library view

The Scenes view must provide a scrollable list/grid of:

- Built-in scenes.
- User-saved scenes discovered from the application's scene location.

Each scene entry displays at least:

- Name.
- Source type: built-in or user.
- Body count.
- Joint count.
- Short description.

Built-in scenes required:

1. Free Fall & Boundary Bounce
2. Collision Manifold Lab
3. Stable Five-Block Tower
4. Friction Ramp
5. Restitution Comparison
6. Pendulum
7. Suspension Bridge
8. Ragdoll / Linked Body Chain
9. Broad-Phase Stress Grid
10. Sleeping Island

Opening a scene with unsaved editor changes must trigger a confirmation modal.

## 6. Diagnostics view

The Diagnostics view must expose live or latest-frame values for:

- Physics FPS / render FPS.
- Fixed simulation step.
- Simulated time.
- Body counts by static/dynamic/kinematic.
- Awake body count.
- Sleeping body count.
- Broad-phase proxy count.
- Broad-phase candidate pair count.
- Narrow-phase tested pair count.
- Active manifold count.
- Active contact-point count.
- Joint count.
- Solver iteration counts.
- Physics step duration.
- Broad-phase duration.
- Narrow-phase duration.
- Solver duration.
- Render duration.

Diagnostics must include a resettable rolling history graph for at least:

- Physics step duration.
- Candidate pair count.
- Active contact count.

The graph may be custom-rendered with lines/bars but may not depend on an external plotting library.

### 6.1 Motion Analysis

Diagnostics/Sandbox must also expose a Motion Analysis recorder for selected bodies. It must provide world-space trajectory trails and readable time-series plots of position, velocity, speed, angle, angular velocity, and kinetic-energy channels.

Recording is keyed to fixed physics steps rather than render frames and must support simultaneous recording of at least 8 bodies.

The live graph requires labeled axes, legend, numeric readout, auto-range, history navigation, pause-correct behavior, and integration with trajectory export.

Detailed recorder capacity, sample channels, graph behavior, E2E requirements, and numerical consistency requirements are normative in `14_FORCE_TRAJECTORY_TOOLS.md`.

## 7. Debug overlays

Sandbox debug controls must independently toggle:

- Shape outlines.
- AABBs.
- Dynamic AABB-tree fat bounds.
- Centers of mass.
- Linear velocity vectors.
- Angular velocity indicator.
- Contact points.
- Contact normals.
- Penetration depth indicator.
- Joint anchors.
- Joint axes/constraint lines.
- Sleeping-state tint.
- Broad-phase candidate connections.
- Recent externally applied force/impulse vectors.
- Recorded world-space body trajectory trails.

Contact-point visualization must mark each actual manifold point, not an arbitrary midpoint between bodies.

The acceptance appearance for contact points includes a red crosshair centered at the calculated contact coordinate.

## 8. Save/load workflows

Sandbox must provide:

- New Scene.
- Save.
- Save As.
- Open Scene.
- Revert to saved.

Unsaved changes must be tracked.

Closing the window, opening another scene, or creating a new scene while unsaved changes exist must require a confirmation modal with:

- Save
- Discard
- Cancel

A failed Save operation must not clear the dirty state.

## 9. Export workflows

Required exports:

### 9.1 Scene snapshot export

Export a deterministic JSON snapshot of the editor scene definition.

### 9.2 Body trajectory export

For a user-selected recording interval, export CSV containing at least:

- simulation_time
- body_id
- position_x
- position_y
- angle
- velocity_x
- velocity_y
- angular_velocity
- fixed_step_index
- speed
- translational_kinetic_energy
- rotational_kinetic_energy

### 9.3 Physics statistics export

Export CSV or JSON containing frame/step diagnostics over the recording interval.

Export failure must show an actionable error and keep the application usable.

## 10. Keyboard shortcuts

At minimum:

- Space: Play/Pause
- Period or Right Arrow while paused: Single Step
- Delete: Delete selection
- Ctrl+Z: Undo
- Ctrl+Shift+Z or Ctrl+Y: Redo
- Ctrl+S: Save
- Ctrl+Shift+S: Save As
- Ctrl+O: Open
- F: Fit scene
- Escape: Cancel current creation gesture / close topmost dismissible modal

Shortcuts must not trigger while a text/numeric field is actively consuming the same key combination where that would corrupt editing.

## 11. About view

About must include:

- Application name/version.
- Supported shape types.
- Supported joint types.
- Concave polygon exclusion notice.
- Keyboard/mouse controls reference.

It must not contain placeholder links or non-functional buttons.

## 12. Solver Inspector workflow

Diagnostics must contain a first-class Solver Inspector defined normatively in `18_SOLVER_INSPECTOR.md`.

The user must be able to move from an observed physical anomaly to the underlying solver state without leaving the application:

- select a rendered contact or joint;
- inspect current constraint values;
- pin the entity where stable identity permits;
- capture exactly one fixed-step solver trace;
- inspect warm-start and per-iteration impulse evolution;
- export the trace for reproducible validation.

The workflow must support paused + Single Step operation so unstable behavior can be advanced one deterministic fixed step at a time.

A generic body-properties panel does not satisfy this workflow.

## 13. v1.0 continuous collision, filtering, and time-travel tools

The final product must additionally provide the integrated workflows defined in documents 19–22:

- per-body DISCRETE/BULLET collision mode;
- rotationally aware CCD/TOI for required shapes;
- interactive Shape Cast / Sweep Query tool;
- 16-category Collision Matrix plus per-body category/mask/group editing;
- Replay Timeline with scrubber, bookmarks, automatic/manual checkpoints, step backward reconstruction, mismatch/anomaly markers, and Fork From Here;
- Golden Scenario browser/report access sufficient to identify all twelve final acceptance scenarios and their latest PASS/BLOCKED state.

These controls must manipulate the same production physics state used by automated acceptance. Decorative duplicate demo modes do not satisfy the requirement.

### 13.1 Diagnostics linkage

A user observing abnormal motion must be able to navigate among:

- viewport body/contact/joint selection,
- Timeline fixed-step location,
- anomaly marker,
- Solver Inspector capture,
- trajectory/time-series graph,
- replay reproduction artifact.

The application is therefore expected to function as both a physics sandbox and a deterministic physics-debugging environment.

### 13.2 Final built-in scene set

The Scenes view must expose built-in scenes sufficient to exercise all Golden Scenario categories. Golden fixtures may be protected/read-only copies so user editing cannot change the normative acceptance input.

## 14. Windows sibling platform behavior

The Windows version retains every product workflow in this document and adds the native-platform behavior defined in `25_WINDOWS_PLATFORM_ADAPTATION.md`.

Open/Save As and unsaved-close decisions remain application-rendered workflows; Windows native Common Dialog/IFileDialog/MessageBox cannot replace them.

Window DPI, monitor position, paint activity, focus state, and uncommitted IME composition are presentation/input-platform state and must not independently change simulation state.
