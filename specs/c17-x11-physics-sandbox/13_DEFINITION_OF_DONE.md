# 13 — Definition of Done and Final Stopping Condition

## 1. Stopping condition

The task is complete only when **every mandatory item in this document is true at the same final revision**.

“Mostly complete”, “prototype complete”, “core logic complete”, or “remaining polish only” are not stopping conditions.

## 2. Build/output completeness

- [ ] Main Linux X11 desktop application source is present.
- [ ] Production code is C17.
- [ ] Required test/verification/dev-tool code is C17.
- [ ] Build metadata exists and identifies all required executable targets.
- [ ] No mandatory feature depends on a forbidden GUI/physics/parser library.
- [ ] Main executable starts as an actual X11 desktop application.

## 3. Custom rendering/UI engine

- [ ] Application owns and presents a software-rendered framebuffer.
- [ ] Custom renderer supports required fills, strokes, alpha, clipping, off-screen surfaces, blur, shadow/glow.
- [ ] Custom UI engine owns widget state, hit testing, focus, scrolling, modal stack, animation.
- [ ] No native/high-level ready-made UI toolkit supplies required widgets.
- [ ] Button hover elevation works.
- [ ] Button/card click ripple works and originates at click coordinate.
- [ ] Focus/selection border glow works.
- [ ] Capsule indicator slides between destinations.
- [ ] Inspector/required sections animate collapse/expand.
- [ ] Modal uses scale+opacity transition.
- [ ] Modal backdrop progressively dims and blurs application content.
- [ ] Frosted navigation blur/shadow increases smoothly with scroll.
- [ ] Layout remains usable at 960×640 and 1920×1080.

## 4. Physics math and body model

- [ ] Vec2 operations implemented and tested.
- [ ] 2×2 rotation transform implemented and tested.
- [ ] Local/world transform conversions implemented.
- [ ] Static body implemented.
- [ ] Dynamic body implemented.
- [ ] Kinematic body implemented.
- [ ] Mass/inertia for circle implemented.
- [ ] Mass/inertia for rectangle implemented.
- [ ] Mass/centroid/inertia for convex polygon implemented.
- [ ] Forces and torques work.
- [ ] Point impulses affect linear and angular velocity.
- [ ] Fixed time step is independent of render rate.
- [ ] Semi-implicit Euler is used.
- [ ] Fixed-step catch-up is bounded.

## 5. Shape scope

- [ ] Circle supported.
- [ ] Rectangle supported.
- [ ] Convex polygon with 3–64 vertices supported.
- [ ] Convex polygon winding normalized.
- [ ] Degenerate polygon rejected.
- [ ] Self-intersecting polygon rejected.
- [ ] Concave polygon explicitly rejected.
- [ ] Concave polygon is not silently triangulated/decomposed/approximated.

## 6. Broad phase

- [ ] Production broad phase is dynamic AABB tree.
- [ ] Fat AABBs implemented.
- [ ] Proxy creation/removal/movement implemented.
- [ ] Cost-based insertion or equivalent implemented.
- [ ] Tree balancing/rotation implemented.
- [ ] Candidate pairs deduplicated.
- [ ] Randomized brute-force oracle shows no missed true AABB-overlap pairs.
- [ ] Production narrow phase does not unconditionally scan all N² body pairs.

## 7. Narrow phase and manifolds

- [ ] Circle-circle implemented.
- [ ] Circle-polygon implemented including vertex case.
- [ ] Polygon-polygon SAT evaluates axes from both polygons.
- [ ] SAT produces consistent normal and penetration.
- [ ] Polygon manifold uses reference/incident clipping or equivalent true contact construction.
- [ ] Face-face contact can produce two manifold points.
- [ ] Contact points are finite and geometrically meaningful.
- [ ] Contact feature identity supports warm starting.
- [ ] Debug contact crosshair uses actual manifold coordinate.

## 8. Contact solver

- [ ] Sequential impulse solver implemented.
- [ ] Normal effective mass implemented.
- [ ] Accumulated normal impulse clamped non-negative.
- [ ] Restitution implemented.
- [ ] Restitution low-speed threshold implemented.
- [ ] Static/dynamic Coulomb friction implemented.
- [ ] Tangent impulses depend on normal impulse limit.
- [ ] Rolling resistance implemented separately from global angular damping.
- [ ] Warm starting enabled in normal release mode.
- [ ] Baumgarte-style stabilization implemented.
- [ ] Deep overlap correction bounded.

## 9. Sleeping

- [ ] Linear/angular sleep thresholds exist.
- [ ] Continuous sleep timer exists.
- [ ] Connected islands sleep coherently enough for stable stacks.
- [ ] Sleeping bodies stop ordinary integration/solver work.
- [ ] Impact wakes bodies.
- [ ] Mouse grab wakes body.
- [ ] Applied force/impulse wakes body.
- [ ] Sleeping diagnostics/debug view works.

## 10. Joints and constraints

- [ ] Constraint framework uses Jacobian/effective-mass sequential impulses or equivalent.
- [ ] Distance joint implemented.
- [ ] Distance spring/damping option implemented.
- [ ] Revolute/hinge joint implemented.
- [ ] Hinge angular limits implemented.
- [ ] Hinge motor with torque limit implemented.
- [ ] Mouse joint implemented as physical constraint.
- [ ] Joint warm starting implemented.
- [ ] Joint deletion/body deletion lifecycle safe.
- [ ] Joint debug overlays implemented.

## 11. Sandbox user functionality

- [ ] Play.
- [ ] Pause.
- [ ] Single Step.
- [ ] Reset.
- [ ] Simulation speed control.
- [ ] Gravity editing.
- [ ] Solver iteration editing.
- [ ] Circle creation.
- [ ] Rectangle creation.
- [ ] Convex polygon creation.
- [ ] Distance joint creation.
- [ ] Hinge creation.
- [ ] Select body/joint.
- [ ] Delete.
- [ ] Body inspector.
- [ ] Geometry editing while paused.
- [ ] Material editing.
- [ ] Collision filter editing.
- [ ] Mouse physical dragging.
- [ ] Camera pan.
- [ ] Camera zoom.
- [ ] Fit scene.
- [ ] Undo/redo with at least 50-command capacity.

## 12. Scene library and built-in scenes

- [ ] Scenes view implemented.
- [ ] Scroll behavior implemented.
- [ ] Required built-in Free Fall scene.
- [ ] Required Collision Manifold scene.
- [ ] Required Five-Block Tower scene.
- [ ] Required Friction Ramp scene.
- [ ] Required Restitution Comparison scene.
- [ ] Required Pendulum scene.
- [ ] Required Suspension Bridge scene.
- [ ] Required Ragdoll/Linked scene.
- [ ] Required Broad-Phase Stress scene.
- [ ] Required Sleeping Island scene.
- [ ] Every built-in scene passes production scene validation.

## 13. Diagnostics and overlays

- [ ] Physics/render FPS shown.
- [ ] Body awake/sleep counts shown.
- [ ] Candidate pair count shown.
- [ ] Narrow-phase count shown.
- [ ] Manifold/contact count shown.
- [ ] Joint count shown.
- [ ] Subsystem timings shown.
- [ ] Rolling history graph works.
- [ ] AABB overlay works.
- [ ] Fat AABB overlay works.
- [ ] COM overlay works.
- [ ] Velocity overlay works.
- [ ] Contact-point overlay works.
- [ ] Contact-normal overlay works.
- [ ] Penetration overlay works.
- [ ] Joint overlay works.

## 14. Persistence and export

- [ ] Custom JSON parser implemented.
- [ ] Custom JSON serializer implemented.
- [ ] Scene format version implemented.
- [ ] Full scene validation implemented.
- [ ] Unique ID/reference validation implemented.
- [ ] Transactional load implemented.
- [ ] Failed load keeps previous scene.
- [ ] Safe-save/replace behavior implemented.
- [ ] Dirty state implemented.
- [ ] New/Save/Save As/Open/Revert implemented.
- [ ] Unsaved change modal has Save/Discard/Cancel.
- [ ] Deterministic serialization ordering implemented.
- [ ] Trajectory CSV export implemented.
- [ ] Physics statistics export implemented.

## 15. Error and boundary handling

- [ ] Invalid numeric edit preserves previous valid value.
- [ ] NaN/infinity rejected from editor/file data.
- [ ] Allocation failures checked in required paths.
- [ ] Malformed JSON rejected.
- [ ] Unknown format version rejected.
- [ ] Duplicate IDs rejected.
- [ ] Invalid joint references rejected.
- [ ] Invalid shape dimensions rejected.
- [ ] Concave/self-intersecting polygon errors are explicit.
- [ ] Window resize safe during simulation.
- [ ] Close protocol works with unsaved changes.
- [ ] Physics non-finite state auto-pauses and reports body/subsystem.
- [ ] Catch-up loop cannot run unbounded.

## 16. Automated tests

- [ ] Unit suite exists.
- [ ] Integration suite exists.
- [ ] E2E suite exists.
- [ ] Physics validation suite exists.
- [ ] Parser/scene regression suite exists.
- [ ] Performance benchmark exists.
- [ ] Dev-tool self-tests exist.
- [ ] All mandatory tests pass at final revision.
- [ ] No mandatory test is skipped.

## 17. Physics validation

- [ ] VAL-01 Free fall passes.
- [ ] VAL-02 Elastic head-on momentum/energy passes.
- [ ] VAL-03 Off-center angular momentum passes.
- [ ] VAL-04 Resting contact passes.
- [ ] VAL-05 Five-block tower passes.
- [ ] VAL-06 Friction ramp passes.
- [ ] VAL-07 Restitution comparison passes.
- [ ] VAL-08 Distance joint passes.
- [ ] VAL-09 Hinge passes.
- [ ] VAL-10 Suspension bridge passes.
- [ ] VAL-11 Sleeping island passes.
- [ ] VAL-12 Broad-phase oracle passes.
- [ ] 120-second simulated soak test passes.

## 18. Developer tools

- [ ] `locscan` equivalent exists in C17.
- [ ] `locscan` supports ignore/config behavior.
- [ ] `locscan` reports human-readable Markdown documentation line total.
- [ ] `locscan` excludes build/log/result/binary evidence from source/doc totals.
- [ ] `fixturegen` equivalent exists in C17.
- [ ] `fixturegen` is deterministic by seed.
- [ ] `scenecheck` equivalent exists in C17 and uses production validation logic.
- [ ] `physverify` equivalent exists in C17 and uses production physics engine.
- [ ] `perfbench` equivalent exists in C17.
- [ ] Every required tool has self-tests.

## 19. Performance

- [ ] PERF-100 report exists.
- [ ] PERF-1000 report exists.
- [ ] PERF-BROAD-5000 report exists.
- [ ] Median and p95 reported.
- [ ] Candidate/contact counts reported.
- [ ] Production broad phase demonstrates sparse candidate reduction on sparse benchmark.
- [ ] Required timing gates pass, subject only to documented acceptance-machine exception permitted by the test spec.

## 20. Acceptance evidence

- [ ] Evidence index exists.
- [ ] Default Sandbox screenshot exists.
- [ ] Hover evidence exists.
- [ ] Ripple animation evidence exists.
- [ ] Border glow evidence exists.
- [ ] Capsule slide evidence exists.
- [ ] Panel collapse evidence exists.
- [ ] Modal scale/opacity evidence exists.
- [ ] Modal blur/dim evidence exists.
- [ ] Frosted nav scroll evidence exists.
- [ ] Collision manifold contact evidence exists.
- [ ] Two-contact manifold evidence exists.
- [ ] Stable tower evidence exists.
- [ ] Tower disturbance evidence exists.
- [ ] Friction/restitution evidence exists.
- [ ] Bridge impact evidence exists.
- [ ] Linked/ragdoll evidence exists.
- [ ] Sleeping evidence exists.
- [ ] Test reports exist.
- [ ] Physics validation report exists.
- [ ] Performance report exists.
- [ ] Dev-tool reports exist.

## 21. External force and impulse interaction

- [ ] Apply Force and Apply Impulse are distinct tools.
- [ ] Dynamic bodies can receive center-of-mass interactions.
- [ ] Dynamic bodies can receive off-center interactions.
- [ ] Off-center force generates torque from lever arm.
- [ ] Off-center impulse changes angular velocity through inverse inertia.
- [ ] Static/kinematic targets reject force/impulse without hidden dynamic state.
- [ ] Sleeping body wakes on non-zero force/impulse.
- [ ] Force is applied only for active fixed steps.
- [ ] Force release stops subsequent application.
- [ ] Impulse is committed exactly once.
- [ ] Paused impulse changes velocity but not time/position.
- [ ] Pointer gesture maps deterministically to world-space vector.
- [ ] Camera zoom/pan does not alter equivalent physical input.
- [ ] Numeric X/Y vector entry works and validates non-finite values.
- [ ] Application-point/vector/magnitude preview exists.
- [ ] Center-of-mass mode exists.
- [ ] Committed impulse has transient visible feedback.
- [ ] Force/impulse actions do not incorrectly dirty editor scene or enter undo history.
- [ ] Reset clears runtime effects by restoring editor scene state.
- [ ] Required numerical verification cases pass.

## 22. Motion Analysis and trajectory visualization

- [ ] Motion recording starts/stops/clears through product UI.
- [ ] Recorder samples fixed physics steps, not render frames.
- [ ] At least 8 bodies can be recorded simultaneously.
- [ ] At least 30 seconds at 60 fixed steps/s per recorded body is supported.
- [ ] Required sample channels are retained.
- [ ] World-space trajectory trail exists.
- [ ] Trail remains correct under camera pan/zoom.
- [ ] At least 4096 trail samples/body × 8 visible bodies is handled correctly.
- [ ] Custom-rendered time-series graph exists without plotting library.
- [ ] Graph has readable time/value axes.
- [ ] Graph has quantity/unit label.
- [ ] Graph has stable body legend.
- [ ] Graph supports position, velocity, speed, angle, angular velocity, and kinetic-energy channels.
- [ ] At least one channel can show multiple bodies simultaneously.
- [ ] Graph auto-range handles constant, signed, and one-sided data.
- [ ] Cursor/scrub readout resolves nearest sample.
- [ ] History can be fit/reset, horizontally navigated, and inspected after recording.
- [ ] Pausing produces no extra fixed-step samples.
- [ ] Recorder behavior across Reset is explicit and prevents false connecting segments.
- [ ] Trajectory CSV uses actual retained recorder samples.
- [ ] Export includes fixed-step index and required derived channels.
- [ ] Export does not clear recorder.
- [ ] No-sample export is reported clearly rather than silently succeeding misleadingly.
- [ ] Recorder consistency and graph data-path automated tests pass.
- [ ] Required visual/E2E acceptance evidence exists.

## 23. Advanced physics validation

- [ ] VAL-13 force-free translation/rotation passes.
- [ ] VAL-14 constant torque passes.
- [ ] VAL-15 projectile trajectory passes.
- [ ] VAL-16 analytic/representation-consistent mass properties pass.
- [ ] VAL-17 unequal-mass restitution reference passes.
- [ ] VAL-18 contact action/reaction consistency passes.
- [ ] VAL-19 static/dynamic friction numeric validation passes.
- [ ] VAL-20 dissipative energy non-growth passes.
- [ ] VAL-21 high mass-ratio collision stability passes.
- [ ] VAL-22 deep-overlap recovery passes.
- [ ] VAL-23 tangency/grazing robustness passes.
- [ ] VAL-24 translation invariance passes.
- [ ] VAL-25 rotation invariance passes.
- [ ] VAL-26 insertion-order/ID permutation passes.
- [ ] VAL-27 fixed-step render-cadence invariance passes.
- [ ] VAL-28 same-input determinism passes.
- [ ] VAL-29 time-step convergence passes.
- [ ] VAL-30 hinge motor/torque-limit validation passes.
- [ ] VAL-31 damped distance-spring validation passes.
- [ ] VAL-32 long-chain constraint stress passes.
- [ ] VAL-33 sleep/wake boundary validation passes.
- [ ] VAL-34 dynamic-tree lifecycle oracle passes.
- [ ] VAL-35 collision-filter matrix/live-update validation passes.
- [ ] VAL-36 polygon representation invariance passes.
- [ ] VAL-37 thin/high-aspect-ratio robustness passes.
- [ ] VAL-38 geometry-scale robustness passes.
- [ ] VAL-39 deterministic randomized finite-state fuzz passes for all mandatory seeds.
- [ ] VAL-40 manifold geometric-validity validation passes.
- [ ] Regression corpus contains at least 60 stable non-Golden fixtures.
- [ ] PERF-DENSE-500 report exists.
- [ ] PERF-JOINT-200 report exists.
- [ ] PERF-CHURN-5000 report exists.
- [ ] Repeated lifecycle stress across at least 250 cycles passes.
- [ ] Advanced validation summary reports actual metrics and thresholds, not only booleans.
- [ ] Missing/NaN mandatory metrics fail rather than silently pass.

## 24. Prohibited-substitution final check

- [ ] No GTK/Qt/SDL/Cairo/OpenGL or equivalent prohibited GUI/rendering framework is used.
- [ ] No external physics/collision engine is used.
- [ ] No external JSON/parser library is used.
- [ ] No fake/precomputed contact points are used.
- [ ] No hard-coded passing benchmark/test result replaces real execution.
- [ ] No required control is a placeholder or disconnected from real state.
- [ ] No final world-boundary collision relies on a simplistic coordinate clamp/velocity flip instead of the collision solver.
- [ ] No running-simulation mouse drag teleports body transforms.
- [ ] No visual-only line is presented as a physical joint.
- [ ] No advanced analytic oracle simply calls the production routine whose result it is meant to verify.
- [ ] No dynamic-tree oracle reuses the dynamic tree as its own reference.
- [ ] No randomized/fuzz failure is hidden by discarding its seed.

## 25. Pre-final declaration prerequisite

Sections 1–24 are mandatory prerequisites, but they are not sufficient for v1.0 because Sections 26–31 add the Solver Inspector and final v1.0 feature/audit scope.

Any known mandatory failure, missing evidence item, failing or skipped test, prohibited substitution, missing mandatory metric, or unimplemented required control means the task remains incomplete. Final declaration is governed only by Section 32.

## 26. Solver Inspector

- [ ] Diagnostics contains the required Solver Inspector.
- [ ] Contacts can be selected from viewport and deterministic contact list.
- [ ] Joints can be selected from viewport and deterministic joint list.
- [ ] Contact selection uses stable body/shape/feature identity rather than array index alone.
- [ ] Lost contacts become explicitly inactive and never silently retarget to unrelated contacts.
- [ ] Contact summary exposes production point, normal, tangent, relative velocity, effective masses, biases, pair material values, and accumulated impulses.
- [ ] Two-point manifolds expose two distinct point identities and impulse accumulators.
- [ ] Distance-joint diagnostics expose target/current length, error, effective mass, bias/softness, and accumulated impulse.
- [ ] Revolute diagnostics expose anchor error, effective-mass representation, motor and limit states, requested/clamped impulses, and accumulated impulses.
- [ ] Mouse-joint diagnostics expose target error, maximum-force-derived impulse cap, requested/applied impulse, stiffness, and damping.
- [ ] Capture Next Step records pre-warm-start, post-warm-start, every configured velocity iteration, and final solver state.
- [ ] Separate stabilization/position iterations are traced when the implementation has that stage.
- [ ] Per-iteration contact trace records normal/tangent requested increments and accumulated clamped impulses.
- [ ] Per-iteration joint trace records error, requested impulse, clamped impulse, and accumulator state.
- [ ] Custom iteration table exists.
- [ ] Custom iteration graph exists and consumes real trace data.
- [ ] Warm-start cache and actual applied warm-start impulse are observable.
- [ ] Friction clamp bounds and requested/applied tangent impulses are observable.
- [ ] Restitution and Baumgarte/stabilization contributions are shown separately.
- [ ] Solver-island context is available for the inspected entity.
- [ ] Trace capture memory is bounded.
- [ ] JSON trace export contains schema version, solver configuration, entity identity, iteration records, and before/after state digests.
- [ ] Required `solvertrace` verification executable is delivered.
- [ ] `SINSP-01` through `SINSP-20` pass.
- [ ] `E2E-SI-01` through `E2E-SI-06` pass.
- [ ] Inspector/tracing non-interference digest validation passes.
- [ ] Deterministic repeated trace validation passes.
- [ ] 10,000-capture stress validation passes.
- [ ] Solver Inspector acceptance evidence is complete.
- [ ] Aggregate `solver_inspector` release group is PASS.

## 27. v1.0 Continuous Collision Detection and Shape Cast

- [ ] Every body exposes DISCRETE/BULLET collision mode.
- [ ] Circle, rectangle, and convex polygon BULLET CCD are implemented.
- [ ] CCD accounts for angular rotation, not only center translation.
- [ ] Swept broad phase uses production Dynamic AABB Tree.
- [ ] TOI is a continuous separation/root search, not fixed micro-stepping substitute.
- [ ] Earliest deterministic TOI is processed first.
- [ ] TOI iteration/sub-step caps are bounded and observable.
- [ ] CCD uses real manifold + sequential impulse response.
- [ ] CCD works against static, kinematic, dynamic, and sensor targets.
- [ ] Thin sensor crossing cannot silently disappear.
- [ ] CCD mode persists and participates in replay/checkpoint/digest.
- [ ] Shape Cast supports circle, rectangle, convex polygon translation sweeps.
- [ ] Shape Cast returns target, fraction, point, normal, distance, sensor flag.
- [ ] Shape Cast is non-mutating and deterministic.
- [ ] `CCD-01` through `CCD-30` PASS.
- [ ] `CAST-01` through `CAST-18` PASS.
- [ ] Required CCD performance and evidence artifacts exist.

## 28. v1.0 Collision Matrix and filtering

- [ ] At least 16 category bits exist with editable scene names.
- [ ] Per-body category bits, mask bits, signed group index exist.
- [ ] Two-direction mask rule is correct.
- [ ] Same positive group always allows; same negative group always denies.
- [ ] Filtering semantics are shared across discrete, CCD, sensor, and query paths.
- [ ] Runtime filter change removes stale contacts/impulses correctly.
- [ ] Sensor runtime filtering emits correct lifecycle transition.
- [ ] Functional 16-category Collision Matrix UI exists.
- [ ] Matrix edit is symmetric and undoable/redoable.
- [ ] Body filter Inspector is functional.
- [ ] Filter state persists/replays/checkpoints.
- [ ] `COLF-01` through `COLF-24` PASS.
- [ ] Required filtering evidence exists.

## 29. v1.0 Replay Timeline

- [ ] Timeline is based on fixed-step command replay/checkpoints, not recorded transforms.
- [ ] Record/Play/Pause/Stop/Step Forward/Step Backward work.
- [ ] Scrub/numeric seek reconstruct exact target step.
- [ ] Step Backward uses checkpoint + forward replay rather than inverse physics.
- [ ] Automatic checkpoint cadence works and is bounded.
- [ ] Contact/sensor/sleep/wake/CCD/anomaly/mismatch/bookmark markers are real.
- [ ] Timeline zoom/pan is usable.
- [ ] Bookmarks persist and do not alter physics digest.
- [ ] Fork From Here preserves provenance and original replay.
- [ ] Timeline integrates with Solver Inspector and motion graph cursor.
- [ ] 100,000-step replay/timeline behavior is bounded and tested.
- [ ] `TLN-01` through `TLN-28` PASS.
- [ ] Required Timeline evidence exists.

## 30. v1.0 Golden Scenario Acceptance

- [ ] All twelve immutable Golden fixtures exist and pass integrity checks.
- [ ] GOLD-01 analytic free fall PASS.
- [ ] GOLD-02 elastic momentum collision PASS.
- [ ] GOLD-03 friction ramp PASS.
- [ ] GOLD-04 five-block tower PASS.
- [ ] GOLD-05 pyramid perturbation PASS.
- [ ] GOLD-06 pendulum constraint PASS.
- [ ] GOLD-07 motorized revolute joint PASS.
- [ ] GOLD-08 suspension bridge impact PASS.
- [ ] GOLD-09 ragdoll/linked-body drop PASS.
- [ ] GOLD-10 sensor/filter course PASS.
- [ ] GOLD-11 high-speed CCD thin wall PASS.
- [ ] GOLD-12 mixed stress playground PASS.
- [ ] Golden aggregate is exactly 12/12 PASS.
- [ ] Every Golden failure path can produce a reproduction bundle.
- [ ] Golden evidence index is complete.

## 31. v1.0 Specification/traceability/release audits

- [ ] v1.0 frozen scope is respected.
- [ ] At least 60 non-Golden deterministic regression fixtures exist.
- [ ] Mandatory test-ID registry in `24_MANDATORY_TEST_REGISTRY.md` is complete and the implementation-side registry matches it.
- [ ] All 284 mandatory named functional/validation/E2E IDs PASS.
- [ ] All 11 mandatory named performance workloads execute with acceptable results.
- [ ] All 5 stable-envelope fixtures and both mandatory CCD fixtures pass.
- [ ] Every mandatory test ID is discovered, executed, and PASS.
- [ ] No mandatory skip exists.
- [ ] No mandatory timeout exists.
- [ ] No mandatory flaky result exists.
- [ ] Every mandatory user-visible feature maps to automated verification and evidence where required.
- [ ] Every physics validation identifies a valid oracle class.
- [ ] Traceability matrix has no mandatory gap.
- [ ] Anti-placeholder audit PASS.
- [ ] Prohibited-dependency/engineering-constraint audit PASS.
- [ ] Evidence build identity matches tested build identity.
- [ ] Final `releasecheck` status is PASS.

## 32. Final v1.0 stopping rule

The implementation may declare the assignment complete only when Sections 1–31 of this document are simultaneously satisfied, every release gate including Gate M is PASS, and the Golden Suite is 12/12 PASS.

A known mandatory defect followed by a promise to fix later is BLOCKED, not complete.
