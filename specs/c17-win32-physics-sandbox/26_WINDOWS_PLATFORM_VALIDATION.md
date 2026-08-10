# 26 — Windows Platform Validation and Acceptance

Version: **1.0 Windows sibling**
Status: **Normative final / release blocking**

## 1. Purpose

This document defines Windows-specific validation that is added to, not substituted for, the platform-independent v1.0 validation suites.

All Linux/X11-independent mandatory IDs retain their original meaning. Windows adds `WIN-*` and `E2E-WIN-*` cases to prove the native platform layer does not introduce stale input, DPI mapping errors, framebuffer corruption, Unicode/IME loss, timing drift, or prohibited ready-made UI substitutions.

## 2. General Windows oracle rules

Windows platform cases must validate production code paths.

Where a test can be headless at the platform-abstraction layer, it may test deterministic conversion/state code directly. Where native message/window behavior is essential, the real application/native window path must be exercised.

A test that merely asserts a mocked message handler was called is insufficient for release-critical native-window behavior.

## 3. `WIN-01` Native top-level window and client framebuffer

Launch the production application and establish a native Win32 top-level window.

Verify:

- valid client area exists;
- application-owned framebuffer dimensions match current client device pixels;
- at least one completed production frame is presented;
- no mandatory UI exists only as separate native child controls.

## 4. `WIN-02` Message-loop responsiveness under simulation

Run a nontrivial physics scene while generating ordinary paint/input/window messages.

Verify the application continues to dispatch messages and the simulation does not run as a blocking window-procedure loop.

## 5. `WIN-03` High-resolution timer monotonicity and conversion

Validate the production Windows monotonic timer wrapper across a long sequence of samples.

Require:

- no backward time;
- finite non-negative deltas;
- stable tick-to-seconds conversion;
- no overflow in the tested duration range.

## 6. `WIN-04` Fixed-step independence from message cadence

Run identical deterministic input under at least three native message/presentation cadences, including bursty repaint activity.

Final canonical physics state digests must match.

## 7. `WIN-05` DIB/framebuffer pixel and stride correctness

Render a deterministic pixel-pattern fixture through the production framebuffer/presentation path.

Verify corners, row orientation, stride, channel mapping, and client clipping. This catches vertically inverted DIBs, stride overruns, and channel/order mistakes.

## 8. `WIN-06` Paint recovery after occlusion/invalidation

Force the client content to require repaint after being obscured/invalidated.

The restored presented frame must correspond to current application state, not uninitialized/stale memory.

Physics digest before/after a paint-only recovery sequence must remain unchanged.

## 9. `WIN-07` Repeated resize safety

Resize across at least 500 deterministic client-size changes covering small, wide, tall, and ordinary sizes.

Require:

- no crash/out-of-bounds write;
- framebuffer matches current client pixels;
- layout remains valid at supported sizes;
- no physics digest change from resize-only input;
- no stale stretched-frame acceptance.

## 10. `WIN-08` Minimize/restore and zero-client handling

Repeatedly minimize and restore while a deterministic scene is active.

Require:

- zero/suppressed render size is handled safely;
- no unbounded catch-up on restore;
- no divide-by-zero;
- documented run/pause behavior is followed;
- canonical outcome matches the corresponding timing policy reference.

## 11. `WIN-09` Per-monitor DPI scale transition

Exercise at least three effective DPI scales representative of 100%, 150%, and 200%, through real per-monitor DPI notifications when available or the production DPI-transition logic under a native test harness.

Verify:

- layout/device conversion updates once;
- framebuffer pixel dimensions are correct;
- controls remain hittable;
- glyph cache/layout remains valid;
- physics state digest is unchanged by DPI-only transition.

## 12. `WIN-10` DPI pointer-hit-test mapping

At multiple DPI scales, target a known custom button/numeric field at normalized visual coordinates.

The same visual target must receive the input. No double-scaling or missing-scaling coordinate bug is allowed.

## 13. `WIN-11` DPI-invariant world-space force gesture

Apply an equivalent world-space force/impulse gesture at two DPI scales with the same camera/world geometry.

Committed application point and vector must match within the force-tool numeric tolerance and produce matching physics digests.

## 14. `WIN-12` Mouse capture outside client area

Begin a slider/body/splitter drag in the client area, move the cursor outside while held, then release outside.

Require continued deterministic drag updates until release and no stuck pressed/dragging state afterward.

## 15. `WIN-13` Unexpected capture loss

Start an active captured gesture, then force capture loss/change before button-up.

The application must cancel or safely terminate according to tool policy, release internal capture ownership, and remain usable.

## 16. `WIN-14` Focus-loss held-key reconciliation

Activate a continuous/shortcut-relevant key state, then deactivate/focus another window before receiving a normal key-up.

On reactivation the application must not behave as though the key remains permanently held.

## 17. `WIN-15` Modal/focus ownership

Open an application-rendered modal while another custom editor/control has focus.

Verify:

- modal receives intended keyboard focus;
- blocked background controls cannot activate;
- closing restores/correctly resolves focus;
- no native `MessageBox`/dialog is used for the required workflow.

## 18. `WIN-16` UTF-8 / UTF-16 BMP round trip

Round-trip representative ASCII, Latin, CJK, and mixed BMP text through the production Win32 string boundary.

Decoded UTF-8 must exactly match input.

## 19. `WIN-17` UTF-16 surrogate-pair round trip

Round-trip characters requiring surrogate pairs, including at least one supplementary-plane scalar.

Require exact Unicode scalar preservation and no split-surrogate corruption.

## 20. `WIN-18` Invalid UTF-16 boundary rejection

Feed unpaired/high/low surrogate-invalid sequences to the boundary conversion layer.

Require deterministic rejection or replacement according to a documented policy, with no buffer overrun, truncation into a different valid filename, or silent ANSI fallback.

## 21. `WIN-19` Unicode Windows path save/load round trip

Save and reload a scene using a path containing non-ASCII Unicode, including Chinese characters.

Require:

- saved file is accessible at the intended path;
- scene contents round-trip;
- dirty-state behavior is correct;
- reported path round-trips through the custom UI without ANSI loss.

## 22. `WIN-20` Windows safe-save replacement failure

Cause the final replacement/rename phase of safe save to fail, such as through a conflicting lock/permission fixture.

Require the prior valid target to remain available when the OS permits, dirty state to remain true, and the UI to report failure rather than success.

## 23. `WIN-21` IME composition lifecycle

In a focused custom text editor, exercise a real or production-equivalent Imm32 composition sequence.

Verify Start → one or more Update → Commit → End behavior, visible composition state, and exactly one committed insertion.

## 24. `WIN-22` IME composition cancellation

Begin/update composition then cancel/end without a result string.

No stale text may be committed and the custom editor must leave composition state cleanly.

## 25. `WIN-23` Chinese IME scene/body name commit

Commit Chinese text through the production IME path into a name field, commit the field property, save scene JSON, reload, and verify exact UTF-8 value.

This proves the IME path is connected to real product state and persistence.

## 26. `WIN-24` IME focus transfer and deletion safety

While composition is active, transfer focus, close the editor, or delete the selected body through a valid workflow.

The application must not dereference stale editor/body state or commit composition into a different object.

## 27. `WIN-25` Close-window unsaved state machine

With dirty scene state, issue the native top-level close request.

Verify it opens the custom Save/Discard/Cancel modal and that all three decisions obey the same semantics as New/Open unsaved workflows.

A native MessageBox cannot satisfy this test.

## 28. `WIN-26` Window move / monitor identity non-interference

Move the window without changing scene/camera/input commands, including across monitors when available.

After accounting for DPI visual transition, canonical physics state must not change solely because window/monitor coordinates changed.

## 29. `WIN-27` Wheel and high-resolution scroll accumulation

Feed standard wheel deltas and smaller/high-resolution accumulated deltas into scroll/zoom paths.

Require deterministic accumulation, no lost direction changes, bounded zoom/scroll, and no physics mutation when the action only scrolls UI.

## 30. `WIN-28` Client coordinate and signed-position robustness

Exercise client/screen conversions with the top-level window located at negative virtual-desktop coordinates where supported.

Custom hit testing and camera pointer mapping must remain correct; unsigned-coordinate wrap is prohibited.

## 31. `WIN-29` Platform instrumentation non-interference

Run the same deterministic 2,000-step fixture with ordinary native presentation/DPI/focus diagnostic instrumentation enabled and disabled, without product commands differing.

Canonical physics digests at comparison steps must be identical.

## 32. `WIN-30` Prohibited native-control/API audit

The static/build/dependency audit must fail if required UI/rendering behavior is implemented with prohibited native/high-level substitutes, including required child controls, Common Dialog/IFileDialog, MessageBox, Direct2D/DirectWrite layout, DWM backdrop blur, Direct3D, or WebView.

The audit must distinguish an allowed OS non-client frame/IME candidate UI from prohibited client-area substitutions.

## 33. Windows full-application E2E cases

The following eight Windows-specific E2E result IDs are mandatory.

### `E2E-WIN-01` Launch, resize, minimize, restore

Launch production app, exercise resize/minimize/restore, and verify valid custom UI/framebuffer after recovery.

### `E2E-WIN-02` Cross-DPI interaction

Exercise a DPI transition, then click/drag known custom controls and a world body; verify correct targets and unchanged physics for DPI-only portions.

### `E2E-WIN-03` Capture outside window

Drag a body outside the client area and release; verify clean capture release and no stuck state.

### `E2E-WIN-04` Focus/Alt-Tab safety

Interrupt keyboard/mouse interaction with focus loss and return; verify no stuck key/capture and deterministic continuation.

### `E2E-WIN-05` Chinese IME persistence

Enter Chinese scene/body text via the production IME path, save, reopen, and verify exact persisted text.

### `E2E-WIN-06` Unicode Save As/Open

Use the required custom path UI to save under a Unicode Windows path, open the result, and verify scene identity/state.

### `E2E-WIN-07` Native close with unsaved changes

Issue top-level close while dirty and exercise Save/Discard/Cancel branches through the custom application modal.

### `E2E-WIN-08` Platform-state versus replay determinism

Record a deterministic replay while injecting paint, move, resize, DPI, and focus transitions that do not create product commands; replay/reference canonical state must remain identical.

## 34. Windows evidence set

The acceptance package must contain evidence sufficient to inspect:

- `WIN-EV-01` normal Windows desktop application view with custom client UI;
- `WIN-EV-02` resized narrow/wide supported layouts;
- `WIN-EV-03` 100% and high-DPI views showing scale/layout adaptation;
- `WIN-EV-04` pointer capture body drag outside the client boundary, as a sequence/recording;
- `WIN-EV-05` custom unsaved-change modal triggered from native close;
- `WIN-EV-06` active IME composition in a custom text field and final committed Chinese text;
- `WIN-EV-07` Unicode Save As/Open path workflow using application-rendered UI;
- `WIN-EV-08` platform validation summary listing WIN-01..WIN-30;
- `WIN-EV-09` E2E Windows summary listing E2E-WIN-01..08;
- `WIN-EV-10` prohibited-native-substitution audit;
- `WIN-EV-11` DPI/input non-interference digest comparison;
- `WIN-EV-12` framebuffer/present/resize stress summary.

Capture method is not prescribed.

## 35. Windows platform release group

`releasecheck` must expose a release-blocking `windows_platform` group containing:

- all `WIN-01` through `WIN-30` results;
- all `E2E-WIN-01` through `E2E-WIN-08` results;
- Windows evidence-presence/integrity checks;
- prohibited-native-substitution audit status;
- source/build identity match for Windows evidence and test reports.

Any missing, skipped, flaky, timed-out, malformed, or non-PASS mandatory Windows result makes this group BLOCKED.

## 36. Cross-platform equivalence audit

The Windows package must preserve the non-platform functional meaning of every mandatory Linux sibling subsystem/test family.

The equivalence audit must explicitly verify:

- no physics tolerance was weakened for Windows;
- no mandatory non-platform test family was removed;
- no built-in/Golden scenario was removed;
- scene schema remains semantically compatible;
- custom UI requirements were not replaced by native controls;
- Windows-only platform state is excluded from canonical physics state;
- Window/DPI/paint/focus/IME events that are not committed product commands do not enter replay as physics-affecting commands.

## 37. Final Windows platform condition

The Windows sibling may reach top-level PASS only when:

- all 30 `WIN-*` cases PASS;
- all 8 `E2E-WIN-*` cases PASS;
- `windows_platform` release group PASS;
- all non-platform v1.0 mandatory cases and workloads PASS;
- Golden aggregate remains 12/12 PASS;
- final Windows dependency/prohibition/equivalence audits PASS.
