# 18 — Solver Inspector, Iteration Trace, and Constraint Diagnostics

## 1. Purpose

The Solver Inspector is a required diagnostics subsystem for observing the real contact and joint constraints solved by the production physics engine.

Its purpose is to make unstable, explosive, drifting, over-corrected, under-constrained, or incorrectly warm-started behavior diagnosable without relying only on visual symptoms.

The Inspector must expose the same constraint data that the production sequential-impulse solver actually uses. A decorative panel populated from separately recomputed or example values does not satisfy this requirement.

## 2. Required access points

The application must expose Solver Inspector functionality from the Diagnostics view.

At minimum, the user must be able to inspect a constraint by:

- selecting a rendered contact point/manifold marker in the Sandbox or Diagnostics viewport;
- selecting a rendered joint marker;
- selecting an entry from a deterministic contact list;
- selecting an entry from a deterministic joint list.

Selection by viewport and selection by list must refer to the same stable inspected entity.

## 3. Inspector operating modes

The Inspector must provide:

- **Live** mode for the currently selected contact/joint;
- **Pinned** mode that keeps the selected stable contact feature or joint selected while it remains valid;
- **Capture Next Step** mode that records one complete fixed-step solver trace;
- **Freeze Capture** mode that stops replacement of the captured trace while the simulation continues;
- **Clear Capture**;
- **Export Trace**.

The Inspector must remain usable while the simulation is paused.

When paused, Single Step combined with Capture Next Step must permit deterministic inspection of exactly one physics step.

## 4. Stable identity and selection rules

### 4.1 Joint identity

Joints are selected by stable joint ID.

A joint ID must not change merely because solver arrays are compacted or reordered.

### 4.2 Contact identity

A contact point must be identified by information sufficient to follow a persistent contact across fixed steps when feature matching remains valid.

The identity must include or deterministically derive from at least:

- body A stable ID;
- body B stable ID;
- shape/fixture identity where applicable;
- manifold/contact feature identity;
- contact-point slot or feature pair.

Array index alone is not a valid persistent contact identity.

### 4.3 Contact disappearance

If a pinned contact disappears:

- the UI must visibly report that the contact is no longer active;
- stale memory must not continue to be displayed as current data;
- the last captured trace may remain available and must be labeled historical/frozen;
- automatic reassignment to an unrelated new contact is prohibited.

## 5. Contact summary panel

For the selected contact point, the live summary must show at least:

- contact stable identity;
- body A ID;
- body B ID;
- shape/fixture IDs if the engine has distinct shape instances;
- world-space contact point;
- contact normal;
- contact tangent used by the solver;
- signed separation or penetration depth;
- manifold point count;
- relative velocity at the contact point;
- relative normal velocity;
- relative tangent velocity;
- pair restitution coefficient after combination;
- pair static-friction coefficient after combination;
- pair dynamic-friction coefficient after combination;
- rolling-resistance coefficient/effective pair value if applicable;
- normal effective mass;
- tangent effective mass;
- restitution contribution to bias, if active;
- penetration/Baumgarte bias contribution;
- total normal velocity bias used by the solver;
- accumulated normal impulse;
- accumulated tangent impulse;
- rolling-resistance impulse/torque accumulator if implemented as a solver accumulator;
- warm-start status;
- cached impulse values applied during warm start;
- sleeping/awake state of both bodies.

All vectors must clearly identify world-space versus local-space representation.

## 6. Contact manifold summary

When the selected contact belongs to a manifold with multiple points, the Inspector must show:

- manifold identity;
- manifold normal;
- point count;
- each point identity;
- each point position;
- each point separation/penetration;
- each point accumulated normal impulse;
- each point accumulated tangent impulse.

The UI must make clear which point is currently selected.

Two-point manifolds must not collapse their two impulse accumulators into one displayed value.

## 7. Joint summary panel

For the selected joint, show at least:

- joint stable ID;
- joint type;
- body A ID;
- body B ID;
- local anchor A;
- local anchor B;
- world anchor A;
- world anchor B;
- current positional/anchor error;
- current relative velocity relevant to the constraint;
- whether warm starting is enabled;
- cached/accumulated impulse state;
- collide-connected setting.

Type-specific fields are required in addition to the common fields.

## 8. Distance-joint diagnostics

For a distance joint, show at least:

- current length;
- target length;
- scalar distance error;
- constraint axis;
- effective mass;
- velocity bias;
- softness/compliance term if used;
- damping-related term if used;
- accumulated scalar impulse;
- spring enabled/disabled state;
- configured stiffness/frequency representation;
- damping ratio or equivalent.

The displayed error must correspond to the same anchors and target used by the production solver.

## 9. Revolute-joint diagnostics

For a revolute/hinge joint, show at least:

- translational anchor-error vector;
- magnitude of anchor error;
- the 2D effective-mass matrix or mathematically equivalent representation used for the two translational rows;
- accumulated translational impulse vector;
- relative joint angle;
- angular limit enabled state;
- lower and upper limit;
- current limit state: inactive, at-lower, at-upper, or equivalent;
- limit bias/error;
- accumulated limit impulse;
- motor enabled state;
- target motor speed;
- current relative angular speed;
- maximum motor torque;
- per-step motor-impulse cap derived from that torque;
- requested motor impulse before clamping;
- applied/clamped motor impulse after clamping.

A motor implemented by direct angular-velocity assignment cannot satisfy this panel because the required solver values would not exist.

## 10. Mouse-joint diagnostics

For an active mouse joint, show at least:

- target world position;
- body anchor position;
- target-to-anchor error vector;
- effective mass representation;
- configured maximum force;
- per-step impulse cap derived from maximum force;
- requested impulse;
- applied/clamped accumulated impulse;
- stiffness/frequency;
- damping;
- whether the grabbed body was awakened by the constraint.

## 11. Solver iteration trace

### 11.1 Required trace phases

A captured fixed step must distinguish at least:

1. pre-warm-start prepared constraint state;
2. immediately after warm starting;
3. after each velocity-solver iteration from 1 through the configured velocity-iteration count;
4. final velocity-constraint state;
5. stabilization/position-solver iterations if the implementation has a distinct stage;
6. final post-step diagnostic state.

If a mathematically equivalent solver structure uses different named phases, the trace must map those phases explicitly and preserve equivalent observability.

### 11.2 Contact iteration samples

For each captured contact point and each velocity iteration, record at least:

- iteration index;
- relative normal velocity before solving that row;
- normal impulse increment for the iteration;
- accumulated normal impulse after clamping;
- relative tangent velocity before friction solving;
- tangent impulse increment;
- accumulated tangent impulse after Coulomb clamping;
- active normal impulse limit state;
- active tangent/static/dynamic friction limit state where representable.

### 11.3 Joint iteration samples

For the inspected joint, capture each relevant solver row or compact equivalent with at least:

- iteration index;
- constraint error or velocity error before solve;
- effective mass value/matrix identifier;
- raw requested impulse increment;
- clamped applied increment;
- accumulated impulse after the iteration;
- limit/motor clamp status where applicable.

## 12. Iteration table and graph

The custom UI must provide both:

- a numeric iteration table;
- at least one small custom-rendered iteration graph.

For contacts, the graph must be able to plot accumulated normal and tangent impulse versus iteration.

For joints, it must be able to plot at least accumulated impulse magnitude and constraint/velocity error versus iteration where those quantities are meaningful.

The graph must use the captured trace data directly.

A fabricated smooth curve that is not backed by recorded solver iterations is prohibited.

## 13. Warm-start visualization

The Inspector must make warm starting directly observable.

For a persistent contact or joint, the capture must show:

- cached impulse from the prior step;
- impulse actually applied during the warm-start phase;
- first iteration state after warm starting.

When warm starting is disabled through the diagnostics option, the Inspector must show a zero/not-applied warm-start state.

The UI must not claim warm starting occurred merely because a cache entry existed.

## 14. Friction-cone / clamp diagnostics

For an inspected contact, show the current Coulomb bound used to clamp tangent impulse.

At minimum show:

- accumulated normal impulse used to derive the bound;
- coefficient used for the active friction regime/approximation;
- positive and negative tangent-impulse bounds;
- unconstrained requested tangent impulse;
- clamped applied tangent impulse.

This information must permit diagnosis of a body that accelerates laterally because friction is signed or clamped incorrectly.

## 15. Restitution and stabilization diagnostics

The Inspector must distinguish restitution from penetration stabilization.

For an inspected contact, show separately where applicable:

- measured pre-solve relative normal velocity;
- restitution threshold;
- restitution active/inactive decision;
- restitution bias/target contribution;
- penetration/slop amount;
- Baumgarte/stabilization coefficient;
- resulting stabilization bias;
- maximum bias/correction clamp and whether it activated.

This separation is required so excessive bounce caused by stabilization cannot be hidden as restitution.

## 16. Solver island context

For the currently inspected contact/joint, show enough island context to diagnose island construction errors:

- island identifier valid for the current step;
- number of bodies in the island;
- number of contacts in the island;
- number of joints in the island;
- awake/sleep eligibility state;
- solver iteration counts applied to the island.

Island IDs need not persist across steps, but the UI must label them as step-local if they are transient.

## 17. Deterministic ordering

Contact and joint lists displayed by the Inspector must use a deterministic ordering independent of raw pointer values or allocator addresses.

Trace export ordering must be deterministic for identical scene state, fixed-step index, and user inputs.

The Inspector must not reorder production solver constraints merely to obtain a convenient UI ordering.

## 18. Non-interference requirement

Instrumentation must not change physics results.

With identical initial state and deterministic command stream, enabling any combination of:

- Solver Inspector panel visibility;
- live constraint selection;
- pinned selection;
- Capture Next Step;
- trace recording;
- iteration graph rendering;

must not change the canonical post-step physics state digest compared with Inspector disabled.

The release suite must test this explicitly.

Instrumentation is prohibited from:

- changing solver iteration count;
- changing constraint ordering;
- changing warm-start cache contents;
- changing floating-point values used by the solver;
- inserting additional physics substeps;
- waking bodies solely because they are inspected.

## 19. Trace memory bounds

Trace capture must be bounded.

Minimum required behavior:

- ordinary Live mode does not retain an unbounded historical trace;
- a single captured step stores all configured iterations for the selected constraint;
- repeated captures replace the previous capture unless the user explicitly exports/freezes it;
- memory usage for Inspector history cannot grow without bound during a long simulation.

The application must remain stable when repeatedly capturing at least 10,000 fixed steps one at a time.

## 20. Trace export

Export Trace must produce a deterministic machine-readable file.

JSON is required.

The export must include at least:

- schema version;
- scene/fixture stable ID if available;
- fixed-step index;
- simulation time;
- solver configuration relevant to the capture;
- inspected entity type;
- stable entity identity;
- body IDs;
- prepared constraint values;
- warm-start values;
- per-iteration records;
- final accumulated impulses/errors;
- state digest before and after the captured step;
- finite/non-finite status.

Floating-point values must be emitted as numeric values, not preformatted UI strings.

## 21. Trace schema compatibility

The trace schema must contain an explicit integer version.

Unknown newer schema versions must be rejected or reported as unsupported by any provided trace-validation tool rather than silently misparsed.

Adding optional fields may remain backward-compatible, but removing or changing the meaning of required fields requires a schema-version change.

## 22. Solver-trace validation tool

A required C verification tool named `solvertrace` or an equivalently documented dedicated executable must be delivered.

It must be able to run a deterministic fixture without the GUI and emit the same solver-trace schema used by the Inspector.

At minimum it must support:

- selecting fixture/scene;
- selecting fixed-step index or capture condition;
- selecting a contact by stable body/feature identity where deterministic;
- selecting a joint by stable joint ID;
- output path;
- machine-readable nonzero exit status on failure.

The GUI Inspector and `solvertrace` must share production instrumentation/data structures rather than implementing unrelated duplicate tracing logic.

## 23. Mandatory solver-inspector validation cases

The release-blocking suite must implement the following cases.

### SINSP-01 Contact summary consistency

Create a deterministic circle-on-floor resting contact.

Compare Inspector/trace values with the production contact constraint immediately before solving.

Required exact/equivalent matches include IDs, normal, point, effective masses, bias terms, and accumulated impulses.

### SINSP-02 Two-point manifold independence

Use a flat rectangle resting on a floor with a two-point manifold.

Verify the Inspector exposes two distinct point identities and two independently evolving normal/tangent accumulators.

### SINSP-03 Warm-start continuity

Capture two consecutive persistent-contact steps.

The second step's reported cached impulse must equal the valid final cached impulse from the first step within serialization precision and feature-matching rules.

### SINSP-04 Warm-start disabled

Disable warm starting through the diagnostics configuration.

Verify the trace reports no applied warm-start impulse while ordinary iterative solving still functions.

### SINSP-05 Friction clamp

Use a deterministic sliding-block fixture.

For every captured iteration, verify the accumulated tangent impulse remains within the reported Coulomb bound and opposes relative tangential motion.

### SINSP-06 Restitution threshold decision

Run two otherwise identical contacts immediately below and above the configured restitution threshold.

Verify reported restitution-active state and bias contribution match the threshold rule.

### SINSP-07 Baumgarte separation

Use a controlled shallow-penetration fixture.

Verify stabilization bias is reported separately from restitution and does not exceed the configured correction/bias cap.

### SINSP-08 Distance-joint trace

Capture a disturbed distance joint.

Verify current length, target length, error, effective mass, and accumulated impulse agree with the production joint row.

### SINSP-09 Revolute motor clamp

Request motor impulse beyond the configured maximum-motor-torque limit.

Verify requested impulse exceeds the cap and reported applied impulse is clamped to the exact per-step limit within tolerance.

### SINSP-10 Revolute limit state

Exercise inactive, lower-limit, and upper-limit states.

Verify the Inspector reports the correct active state and corresponding accumulated limit impulse sign/range.

### SINSP-11 Mouse-joint maximum force

Drag a body with a target displacement large enough to saturate maximum force.

Verify reported requested impulse is clamped to the configured per-step maximum.

### SINSP-12 Per-iteration accumulation

For a selected contact, verify that the final trace-row accumulated impulse equals the production final accumulated impulse and that each intermediate row follows the solver's accumulation/clamp rule.

### SINSP-13 Deterministic trace

Run the same fixture, command stream, and capture step five times.

The canonical trace content excluding explicitly non-deterministic metadata must be byte-identical or canonical-hash identical.

### SINSP-14 Instrumentation non-interference

Run a deterministic 2,000-step fixture with Inspector/tracing fully disabled and again with live Inspector plus repeated trace captures.

The canonical physics state digest at every compared fixed step must match exactly under the project's determinism model.

### SINSP-15 Selection invalidation

Pin a contact, separate the bodies until End contact, then create an unrelated contact.

Verify the UI/trace marks the original selection inactive and never silently retargets to the unrelated contact.

### SINSP-16 Pause/single-step capture

Pause the application, arm Capture Next Step, invoke one Single Step, and verify exactly one fixed step and exactly one trace are produced.

### SINSP-17 Trace export round trip

Export a captured contact and joint trace.

The trace validator must parse it, preserve required fields, and reproduce the canonical trace digest.

### SINSP-18 Capture stress

Perform at least 10,000 one-step captures while cycling among contacts and joints.

Require finite physics state, bounded Inspector memory, no stale selection dereference, and no trace-schema corruption.

### SINSP-19 Solver configuration reflection

Change velocity-iteration count among at least 1, 5, 10, and 20.

Captured trace row count and final iteration index must reflect the real configured solver iteration count exactly.

### SINSP-20 Stable-list ordering

Create the same bodies/joints in different insertion orders when the physical fixture is otherwise canonicalized.

The Inspector's deterministic presentation order must follow its documented stable-key rule rather than allocator/pointer order.

## 24. Solver Inspector E2E cases

### E2E-SI-01 Select contact from viewport

- open a contact-producing built-in scene;
- enable contact overlay;
- click a rendered contact point;
- verify Solver Inspector opens/selects the same body pair and contact feature;
- verify values update while the contact persists.

### E2E-SI-02 Select joint from viewport

- open the bridge or pendulum fixture;
- click a rendered joint marker;
- verify the joint Inspector shows the correct joint ID/type and anchors.

### E2E-SI-03 Capture one paused step

- pause;
- select a contact;
- arm Capture Next Step;
- Single Step;
- verify a frozen iteration trace appears with the configured number of iterations.

### E2E-SI-04 Pin and lose contact

- pin a contact;
- apply an impulse causing separation;
- verify the UI changes to inactive/historical state without selecting another contact.

### E2E-SI-05 Trace graph integrity

- capture a contact step;
- select normal-impulse series;
- verify graph samples correspond one-for-one with the trace table records;
- switch to tangent impulse and verify the same data-path rule.

### E2E-SI-06 Export trace

- capture a joint or contact;
- export trace;
- run trace validation;
- verify the exported entity identity and fixed-step index match the UI capture.

## 25. Visual acceptance evidence

Required visual Solver Inspector evidence includes:

- contact summary with contact point selected in viewport;
- a two-point manifold showing distinct per-point impulses;
- a captured iteration table;
- an iteration graph;
- warm-start values before iteration 1;
- friction clamp values in a sliding fixture;
- revolute motor requested-versus-clamped impulse;
- pinned contact after it becomes inactive;
- joint Inspector on the suspension bridge;
- trace export success state.

At least one captured frame must show the physical contact/joint in the viewport and its corresponding Inspector identity simultaneously.

## 26. Acceptance report integration

The aggregate release report must include a `solver_inspector` result group containing at least:

- all `SINSP-*` case results;
- all `E2E-SI-*` case results;
- Inspector non-interference digest result;
- deterministic trace result;
- capture-stress result;
- trace schema-validation result;
- required visual-evidence presence checks.

Any mandatory Solver Inspector failure blocks release.

## 27. Prohibited substitutes

The following do not satisfy Solver Inspector requirements:

- displaying only body velocity/position rather than solver constraints;
- recomputing approximate impulses after the solver finishes;
- showing a single final impulse without iteration history;
- hard-coded example traces;
- using test-only duplicate constraints not connected to the production solver;
- selecting contacts only by unstable array index;
- omitting clamp/bias/warm-start information;
- logging only to stdout while leaving the required GUI Inspector absent;
- a GUI panel that cannot identify which viewport contact/joint it represents;
- tracing that changes physics results;
- unbounded trace-history memory growth.

## 28. Completion condition

Solver Inspector is complete only when:

- all required live fields are backed by production constraint state;
- contact and joint selection are stable and safe;
- one-step iteration capture works;
- warm start, friction clamps, restitution, stabilization, motors, limits, and effective masses are observable where applicable;
- trace export uses the required schema;
- `solvertrace` or the dedicated equivalent is delivered and tested;
- all `SINSP-*` tests pass;
- all required Solver Inspector E2E tests pass;
- instrumentation non-interference passes;
- required Solver Inspector evidence is present;
- the aggregate release report marks the `solver_inspector` group PASS.

## 29. v1.0 CCD and Timeline integration

TOI-created contacts must be inspectable and identify TOI fraction/sub-step/origin as defined in `19_CCD_TOI_SHAPE_CAST.md`.

Replay Timeline must be able to seek to a selected anomaly/contact step and invoke Capture Next Step without changing the reconstructed digest.

Golden scenarios requiring Solver Inspector evidence must capture the actual Golden run or a deterministic replay of that exact fixture/build/configuration; synthetic example traces do not satisfy evidence.
