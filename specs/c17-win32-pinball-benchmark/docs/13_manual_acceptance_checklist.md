# 13 — Manual Acceptance Checklist

Use PASS / FAIL / NOT CHECKED. This is intentionally practical rather than academic.

## A. Launch and layout

- [ ] A01 Real native Win32 top-level desktop window launches.
- [ ] A02 Delivered build definition shows no prohibited high-level GUI/physics framework.
- [ ] A03 Top bar, left palette, center canvas, right Inspector, bottom status bar exist.
- [ ] A04 Continuous resize does not corrupt content.
- [ ] A05 1024×700 remains usable.
- [ ] A06 Large/maximized window gives extra space mainly to canvas.

## B. Modern UI behavior

- [ ] B01 Hover visibly elevates target with offset/shadow.
- [ ] B02 Ripple starts at actual click point.
- [ ] B03 Border glow fades smoothly.
- [ ] B04 Edit/Play capsule slides.
- [ ] B05 Sidebar collapses/expands with animated geometry/content.
- [ ] B06 Modal opens with scale + opacity.
- [ ] B07 Modal background is actually blurred and dimmed.
- [ ] B08 Closing modal mid-open reverses smoothly.
- [ ] B09 Hover reversal mid-animation does not jump.
- [ ] B10 Frosted top toolbar samples changing app content beneath it.
- [ ] B11 Keyboard focus visible.
- [ ] B12 Disabled control ignores activation.

## C. Object authoring

- [ ] C01 Ball Spawn placed.
- [ ] C02 Wall placed/resized.
- [ ] C03 Ramp placed/rotated.
- [ ] C04 Bumper placed/radius changed.
- [ ] C05 Flipper placed/configured.
- [ ] C06 Sensor placed/resized.
- [ ] C07 Drain placed/resized.
- [ ] C08 Launcher placed/direction/strength configured.
- [ ] C09 One-Way Gate placed/configured.
- [ ] C10 Slingshot placed/configured.

## D. Editor manipulation

- [ ] D01 Single selection.
- [ ] D02 Multiselection.
- [ ] D03 Marquee selection.
- [ ] D04 Move at non-100% zoom tracks pointer correctly.
- [ ] D05 Rotate handle works.
- [ ] D06 Resize handles work.
- [ ] D07 Grid toggles.
- [ ] D08 Snap toggles independently.
- [ ] D09 Duplicate creates new IDs.
- [ ] D10 Copy/Paste creates new IDs.
- [ ] D11 Delete handles dependencies safely.
- [ ] D12 ID rename updates event references.
- [ ] D13 Duplicate-ID rename rejected.

## E. Undo/Redo

- [ ] E01 Create Undo/Redo.
- [ ] E02 Delete Undo/Redo.
- [ ] E03 Drag move is one history command.
- [ ] E04 Property edit Undo/Redo.
- [ ] E05 Rename+references Undo/Redo atomically.
- [ ] E06 New edit after Undo clears Redo branch.
- [ ] E07 At least 100 operations traverse without corruption.

## F. Validation

- [ ] F01 Empty scene reports missing playable objects.
- [ ] F02 Missing Drain is Error.
- [ ] F03 Spawn inside solid is Error.
- [ ] F04 Zero-length geometry is Error.
- [ ] F05 Dangling event reference is Error.
- [ ] F06 Warning and Error visually differ.
- [ ] F07 Validation issue can focus implicated object.
- [ ] F08 Play blocked while Error exists.

## G. Persistence

- [ ] G01 Save writes real `.pbt`.
- [ ] G02 Full table reloads equivalent authored data.
- [ ] G03 Save As works.
- [ ] G04 Dirty marker appears after edit.
- [ ] G05 Successful save clears dirty.
- [ ] G06 Dirty close asks Save/Discard/Cancel.
- [ ] G07 Cancel preserves work.
- [ ] G08 Failed load preserves current scene.
- [ ] G09 Malformed file reports context without crash.
- [ ] G10 Unsupported object/version not silently ignored.

## H. Simulation controls

- [ ] H01 Play creates runtime from authored scene.
- [ ] H02 Pause freezes physics.
- [ ] H03 Single Step advances exactly one fixed step.
- [ ] H04 Resume continues.
- [ ] H05 Restart resets runtime only.
- [ ] H06 0.25×/0.5×/1×/2×/4× work.
- [ ] H07 Return to Edit discards runtime enable/ball state.

## I. Launcher and flippers

- [ ] I01 Launcher charge meter grows while held.
- [ ] I02 Charge clamps at full.
- [ ] I03 Short/long holds produce different speed.
- [ ] I04 Pause stops charge progression.
- [ ] I05 Left flipper engages/returns.
- [ ] I06 Right flipper engages/returns.
- [ ] I07 Moving flipper imparts energy to ball.
- [ ] I08 Focus loss does not leave controls stuck.

## J. Physics sanity

- [ ] J01 Default gravity accelerates +Y.
- [ ] J02 Wall bounce reflects appropriately.
- [ ] J03 Endpoint collision works.
- [ ] J04 High-speed thin wall does not tunnel.
- [ ] J05 Bumper impulse and qualified score work.
- [ ] J06 Slingshot impulse works.
- [ ] J07 Allowed gate direction passes.
- [ ] J08 Blocked gate direction collides.
- [ ] J09 Two balls physically collide.
- [ ] J10 Simple corner does not explode/jitter unboundedly.

## K. Multiball and turns

- [ ] K01 Event can spawn additional ball.
- [ ] K02 At least 8 visible balls simulate independently.
- [ ] K03 16-ball stress succeeds.
- [ ] K04 Ball-ball collision remains enabled.
- [ ] K05 Sensors trigger independently per ball.
- [ ] K06 One drain does not end turn if another ball remains.
- [ ] K07 Last active ball ends one turn.
- [ ] K08 Simultaneous drains end exactly one turn.
- [ ] K09 Game over after configured turns.

## L. Score/events

- [ ] L01 Bumper score configurable.
- [ ] L02 Sensor ADD_SCORE works.
- [ ] L03 First combo event is 1×.
- [ ] L04 Timely subsequent events increase combo.
- [ ] L05 Combo caps at 5×.
- [ ] L06 Combo exact timeout boundary works.
- [ ] L07 Multiball 2× bonus applies.
- [ ] L08 Temporary multiplier override works/expires.
- [ ] L09 ENABLE/DISABLE runtime action works without dirtying scene.
- [ ] L10 OPEN_GATE duration works.
- [ ] L11 LIGHT_INDICATOR visible.
- [ ] L12 Cyclic event case is bounded/diagnosed.

## M. Physics Inspector

- [ ] M01 Debug overlay toggles.
- [ ] M02 Collider geometry shown.
- [ ] M03 Sensor/Drain bounds shown.
- [ ] M04 Velocity vector shown.
- [ ] M05 Contact point/normal shown.
- [ ] M06 Selected ball position/velocity/speed numeric.
- [ ] M07 Last collision object ID shown.
- [ ] M08 Step index/simulation time shown.
- [ ] M09 Debug overlay does not alter replay result.

## N. Simulation Preview

- [ ] N01 Preview runs in Edit Mode.
- [ ] N02 Uses authored geometry.
- [ ] N03 Ending Preview restores exact authored state.
- [ ] N04 Preview score/events do not dirty document.
- [ ] N05 Undo history unchanged by Preview runtime.

## O. Replay

- [ ] O01 Recording can start from fresh session.
- [ ] O02 Launcher/flipper logical transitions recorded.
- [ ] O03 `.pbr` saves.
- [ ] O04 Replay reloads.
- [ ] O05 Scene mismatch detected.
- [ ] O06 Conflicting live gameplay input disabled during playback.
- [ ] O07 Pause/Single Step inspect playback.
- [ ] O08 Final score/state match original run.

## P. Headless/utilities

- [ ] P01 Headless runs with no GUI window/interactive desktop dependency.
- [ ] P02 Headless JSON state emitted.
- [ ] P03 Headless replay verifies result.
- [ ] P04 scenecheck reports issues and useful exit status.
- [ ] P05 locscan categorizes source/docs/tests.
- [ ] P06 locscan JSON config works.
- [ ] P07 locscan YAML config works.
- [ ] P08 locscan excludes binaries/generated/log/evidence.
- [ ] P09 test runner emits category totals.
- [ ] P10 test runner emits JSON summary.

## Q. Automated evidence

- [ ] Q01 ≥420 meaningful automated cases.
- [ ] Q02 All mandatory tests pass.
- [ ] Q03 gravity_drop passes.
- [ ] Q04 perfect_bounce passes.
- [ ] Q05 friction_ramp passes.
- [ ] Q06 high_speed_thin_wall passes.
- [ ] Q07 flipper_strike passes.
- [ ] Q08 bumper_ring passes.
- [ ] Q09 sensor_crossing passes.
- [ ] Q10 eight_ball_collision passes.
- [ ] Q11 drain_test passes.
- [ ] Q12 multiball_stress passes.
- [ ] Q13 1,000,000-step run passes.
- [ ] Q14 nontrivial replay 10× deterministic.
- [ ] Q15 GUI/headless equivalence passes.

## R. Visual evidence

- [ ] R01 V01–V38 present/indexed.
- [ ] R02 A01–A25 present/indexed.
- [ ] R03 Evidence version matches delivered version.
- [ ] R04 Evidence is real runtime, not mockup.

## S. Release honesty

- [ ] S01 RELEASE_CHECKLIST lists every Gate.
- [ ] S02 KNOWN_ISSUES exists.
- [ ] S03 Mandatory known failures are not marked PASS.
- [ ] S04 Version matches test/evidence version.
- [ ] S05 No placeholder primary control remains.

## T. Advanced editor

- [ ] T01 Group/ungroup works and one Undo reverses each action.
- [ ] T02 Layer create/rename/reorder/show/hide/lock works.
- [ ] T03 Hidden layer does not disable physics.
- [ ] T04 Locked object remains selectable but cannot be authored-modified.
- [ ] T05 Overlapping click cycles deterministically.
- [ ] T06 Marquee replace/union/subtract semantics match spec.
- [ ] T07 Align/distribute commands produce exact deterministic result.
- [ ] T08 Multi-selection Inspector mixed state behaves correctly.
- [ ] T09 Copy/paste remaps internal references and fresh IDs.
- [ ] T10 Undo to saved semantic state clears dirty marker.
- [ ] T11 Pointer-centered zoom, Fit Scene, Fit Selection work.
- [ ] T12 Measurement distance/angle matches coordinates.

## U. Additional pinball mechanisms

- [ ] U01 Drop Target drops, becomes non-solid, scores once, resets correctly.
- [ ] U02 Stand-up Target remains solid after qualified hit.
- [ ] U03 Rollover high-speed crossing activates once.
- [ ] U04 Spinner visibly/physically rotates and scores ticks.
- [ ] U05 Kickout captures, holds, and ejects same runtime ball.
- [ ] U06 Nudge directions alter free-ball velocity as specified.
- [ ] U07 Repeated nudge triggers Tilt.
- [ ] U08 Tilt suppresses score/flipper/launcher but physics continues.
- [ ] U09 Next turn clears default Tilt state.

## V. Desktop-quality UI engine

- [ ] V01 Tab/Shift+Tab traversal is usable.
- [ ] V02 Modal traps focus and restores focus after close.
- [ ] V03 UTF-8 Chinese name survives edit/save/reload/copy/paste.
- [ ] V04 Text cursor/delete never corrupts UTF-8 sequence.
- [ ] V05 100/125/150/200% scales remain sharp/usable/aligned.
- [ ] V06 Reduced Motion changes transition motion but not final state/function.
- [ ] V07 Rapid animation reversal has no snap/stale input capture.
- [ ] V08 Resize/expose/popup close leaves no stale framebuffer.
- [ ] V09 Command Palette executes same actions as normal controls.

## W. Reliability

- [ ] W01 Autosave creates recovery but does not clear dirty/formal-save state.
- [ ] W02 Abnormal termination offers recoverable dirty scene.
- [ ] W03 Recovery never overwrites original until explicit Save.
- [ ] W04 Externally modified backing file triggers conflict UI.
- [ ] W05 Save fault stages preserve previous valid destination bytes.
- [ ] W06 Legacy `PINBALL_TABLE 1` opens/migrates and next Save emits version 2.
- [ ] W07 Unsupported newer version rejected transactionally.
- [ ] W08 Two failed auto-restore launches trigger Safe Startup.
- [ ] W09 Parser robustness corpus produces contextual errors without crash.

## X. Determinism, diagnostics, and resource quality

- [ ] X01 Simultaneous collision cases repeat deterministically.
- [ ] X02 Fixed-step invariants meet normative tolerances.
- [ ] X03 Frame stall does not create giant physics timestep.
- [ ] X04 Runtime NaN/Inf pauses with diagnostic, not silent clamp.
- [ ] X05 Event Trace shows real ordered actions.
- [ ] X06 Collision Trace shows real contact/impulse data.
- [ ] X07 Scene Statistics changes with production state.
- [ ] X08 detcompare identifies deliberately introduced first divergence.
- [ ] X09 P1/P2/P3 measurements are present and P2 has zero backlog drops.
- [ ] X10 repeated-cycle memory and descriptor stability gates pass.

## Y. Canonical E2E/evidence

- [ ] Y01 Official reference table validates with zero Errors.
- [ ] Y02 J01–J24 journey is fully evidenced.
- [ ] Y03 RELEASE_EVIDENCE.json contains all stable requirements.
- [ ] Y04 releasecheck succeeds.
- [ ] Y05 No mandatory Gate is inferred PASS solely from a screenshot or prose claim.


## Z. Windows platform binding

- [ ] Z01 Client UI uses no native child controls/Common Controls as required feature substitutes.
- [ ] Z02 Custom Open/Save As picker works with a path containing spaces and Chinese characters.
- [ ] Z03 Alt+F4/system close routes through dirty Save/Discard/Cancel.
- [ ] Z04 Drag remains coherent outside the window and capture loss cannot leave a stuck drag/pressed control.
- [ ] Z05 Clipboard text interoperates with Unicode text and a temporarily unavailable clipboard fails safely.
- [ ] Z06 Chinese IME committed text reaches a custom text field without a hidden native EDIT control.
- [ ] Z07 Window moved between differing DPI monitors (or equivalent DPI-change test) rerasterizes sharply and preserves canvas/simulation state.
- [ ] Z08 96/120/144/192 DPI acceptance or available-equivalent evidence is recorded.
- [ ] Z09 Minimize/restore/uncover/resize leaves no stale framebuffer artifacts.
- [ ] Z10 Required blur is software app-content blur, not DWM Acrylic/Mica/backdrop.
- [ ] Z11 Headless verification succeeds from a noninteractive-capable path without creating HWNDs.
- [ ] Z12 Repeated modal/picker/resize/DPI cycles do not leak USER/GDI/HANDLE resources.
- [ ] Z13 Save replacement denied by sharing/read-only/access condition preserves previous valid destination.
- [ ] Z14 Long path handling has no `MAX_PATH` truncation/overflow; permitted long paths work or produce transactional OS error.
- [ ] Z15 Normal GUI launch has appropriate desktop-app behavior and no unwanted console window.
