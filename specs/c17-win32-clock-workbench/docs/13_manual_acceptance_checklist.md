# 13 — Manual Acceptance Checklist

Use this checklist after automated tests pass. Every `[ ]` below is required unless explicitly marked optional. Begin visual/interaction checks from the shipped default configuration unless the item explicitly instructs changing a setting; this keeps animation, glow, blur, and transition checks comparable.

## Build and launch

- [ ] Clean source package builds Project A and Project B.
- [ ] Main application launches without requiring network access.
- [ ] Main window opens at a sensible default size.
- [ ] No missing-resource error appears in a normal launch.
- [ ] Initial displayed time comes from config/state, not the machine wall clock.


## Application scene transition

- [ ] Ordinary launch visibly combines scene scale and opacity into the final UI.
- [ ] Controls do not accept accidental mutating input while launch transition is incomplete.
- [ ] Normal close after any dirty-settings decision visibly performs the reverse scene transition.
- [ ] Animation-disabled mode opens/closes without getting stuck in an intermediate scene state.

## Main Clock view

- [ ] Clock face is visibly custom-rendered and modern.
- [ ] All three hands are visible and distinguishable.
- [ ] Major/minor ticks are correctly arranged.
- [ ] Digital `HH:MM:SS` matches analog time.
- [ ] Playback state and rate are visible.

## Hour hand drag

- [ ] Hovering hour hand gives feedback.
- [ ] Pressing/grabbing hour hand feels practical.
- [ ] Dragging updates hand continuously.
- [ ] Digital display updates during drag.
- [ ] Crossing 12 o'clock does not jump backward unexpectedly.
- [ ] Release commits one action.
- [ ] Ctrl+Z restores pre-drag time.
- [ ] Ctrl+Y restores dragged time.

## Minute hand drag

- [ ] Minute hand is directly draggable.
- [ ] Full revolution advances/subtracts one hour as direction implies.
- [ ] Seconds/fraction behavior is coherent.
- [ ] Analog and digital remain synchronized.

## Second hand drag

- [ ] Second hand is directly draggable.
- [ ] Full revolution advances/subtracts one minute.
- [ ] Crossing 59/00 is continuous.
- [ ] Escape during drag restores starting time.
- [ ] With second-hand style `tick` and `drag_snap=off`/`on_release`, active second-hand dragging remains continuous and resumes tick-style rendering after release/cancel; `drag_snap=live` steps only by its configured snap increment.

## Digital editor

- [ ] Hour field can be focused and typed.
- [ ] Minute field can be focused and typed.
- [ ] Second field can be focused and typed.
- [ ] Valid value commits to analog clock immediately.
- [ ] Invalid `60` minute/second does not commit.
- [ ] Invalid hour does not commit.
- [ ] Escape cancels an edit.
- [ ] Up/Down increments/decrements field.
- [ ] Tab and Shift+Tab move focus sensibly.


## Digital field scrub-drag

- [ ] Hour field can be scrub-dragged vertically.
- [ ] Minute field can be scrub-dragged vertically.
- [ ] Second field can be scrub-dragged vertically.
- [ ] Tiny pointer jitter below threshold behaves as ordinary click/focus.
- [ ] Upward/downward scrub changes the correct unit with natural wrapping.
- [ ] Analog hands update during scrub in the same frame.
- [ ] Playback is temporarily suspended during scrub and resumes afterward when appropriate.
- [ ] One long scrub requires exactly one Ctrl+Z to undo.
- [ ] Ctrl+Y redoes the scrub.
- [ ] Escape/focus loss cancels scrub with no history entry.

## 12-hour mode

- [ ] Switching to 12-hour mode preserves actual time.
- [ ] Midnight displays as 12 AM.
- [ ] Noon displays as 12 PM.
- [ ] AM/PM manipulation updates analog time correctly.

## Playback

- [ ] Play advances time at positive rate.
- [ ] Pause freezes simulated time.
- [ ] Resume retains previous rate.
- [ ] Slider anchor positions map to `-100, -10, -1, 0, +1, +10, +100` as specified.
- [ ] Rate can be set to `0×`.
- [ ] Rate can be negative.
- [ ] Negative rate runs time backward visibly.
- [ ] Backward midnight wrap is correct.
- [ ] `-100×` remains usable and does not corrupt state.
- [ ] Rate label never shows excessive floating garbage.
- [ ] Reset-to-`1×` works and is undoable.

## Interaction synchronization

- [ ] Starting hand drag while playing temporarily suspends auto advancement.
- [ ] Releasing drag resumes playback if previously playing.
- [ ] Digital edit while playing does not fight automatic progression.
- [ ] Rate slider can change rate while time continues.
- [ ] No visible one-frame analog/digital mismatch after an edit.

## Undo/Redo

- [ ] Undo/Redo buttons enable/disable appropriately.
- [ ] Ctrl+Z works.
- [ ] Ctrl+Y works.
- [ ] Ctrl+Shift+Z works.
- [ ] One long drag takes one undo.
- [ ] Cancelled drag does not create undo entry.
- [ ] Digital edit undo works.
- [ ] Play/Pause undo works.
- [ ] Slider-rate undo works.
- [ ] New edit after Undo clears Redo.

## Button visual behavior

- [ ] Mandatory buttons lift/elevate on hover.
- [ ] Border glow appears smoothly.
- [ ] Pointer down creates clipped ripple from press location.
- [ ] Press/release feedback is visible.
- [ ] Keyboard focus state is distinct from hover.
- [ ] Disabled buttons look disabled.

## Navigation

- [ ] Clock/Settings selector uses pill/capsule style.
- [ ] Active capsule slides rather than teleports with animations enabled.
- [ ] Page content transitions smoothly.
- [ ] Inactive page cannot accidentally receive clicks during transition.
- [ ] Tabbing to a Settings control below the viewport auto-scrolls it visibly below the anchored nav rather than leaving hidden focus.

## Settings

- [ ] Settings page contains real controls, not labels/placeholders.
- [ ] Leaving dirty Settings shows Save/Discard/Cancel and each choice follows the specified behavior.
- [ ] 12/24-hour setting works.
- [ ] Smooth/tick second-hand setting works.
- [ ] Face style setting works.
- [ ] Drag snap setting works.
- [ ] UI scale setting works at 1.0, 1.25, 1.5.
- [ ] Animation enable/disable works.
- [ ] Animation speed multiplier visibly affects UI transition durations.
- [ ] Modal blur setting visibly changes real spatial blur.
- [ ] Navigation blur setting visibly changes frosted nav.
- [ ] Persistence toggles are wired.
- [ ] Default reset time can be edited.
- [ ] Default playback rate can be edited and affects the documented startup/default behavior rather than silently overwriting the current runtime rate.
- [ ] Starting with absent implicit `./clock-config.json` is clean/defaulted; explicit Apply can create it, and a later missing-file Reload fails without discarding the active settings.

## Settings scrolling and frosted navigation

- [ ] Settings content scrolls with wheel.
- [ ] Top nav remains anchored.
- [ ] At top, nav blur/shadow is absent or minimal.
- [ ] Scrolling down increases blur continuously.
- [ ] Scrolling down increases bottom shadow continuously.
- [ ] Nav compacts/collapses smoothly.
- [ ] Scrolling back to top reverses all effects.
- [ ] Content behind nav is actually blurred rather than only darkened.

## Modal

- [ ] A confirmation/error modal is custom-rendered in-app.
- [ ] Opening combines scale and opacity.
- [ ] Background darkens progressively.
- [ ] Background spatially blurs progressively.
- [ ] Modal itself remains sharp.
- [ ] Background controls cannot be clicked.
- [ ] Keyboard focus is trapped appropriately.
- [ ] Closing transition is smooth.
- [ ] Rapid open/close does not leave visual state stuck.

## Resize

- [ ] Window can resize to minimum supported size.
- [ ] Mandatory controls remain usable at minimum.
- [ ] Runtime sizing respects the `3840×2160` physical client maximum.
- [ ] Minimize/restore does not reopen at zero/tiny geometry or corrupt the framebuffer.
- [ ] Clock remains circular.
- [ ] Repeated resize does not leave trails/garbage.
- [ ] Hand hit testing still matches visual hand after resize.
- [ ] Large window remains stable.

## JSON configuration

- [ ] Example JSON config loads.
- [ ] GUI reflects loaded settings.
- [ ] Saving produces valid reloadable JSON.
- [ ] Unknown key generates error.
- [ ] Duplicate key generates error.
- [ ] Invalid number/type generates error.
- [ ] Unicode escape handling passes automated tests.

## YAML configuration

- [ ] Example YAML config loads.
- [ ] YAML nested mappings work.
- [ ] YAML comments work.
- [ ] YAML quoted strings work.
- [ ] YAML sequence support works in parser tests.
- [ ] Unsupported anchors/tags/block scalars are rejected clearly.
- [ ] Saving YAML produces reloadable supported subset.
- [ ] Equivalent YAML/JSON normalize identically in `cfgcheck`.

## Config reload/save error handling

- [ ] Reloading malformed config shows error without replacing active valid settings.
- [ ] Error identifies useful location/reason.
- [ ] Correcting file and reloading applies it.
- [ ] Save failure is surfaced without app crash.
- [ ] Existing valid config is not knowingly truncated by failed save.
- [ ] A `state_file` target that resolves to the active config file is rejected rather than allowed to overwrite config on exit.

## Project A: locscan

- [ ] `locscan` recursively counts configured authored files.
- [ ] Build/log/results/cache paths can be excluded.
- [ ] Binary file detection works.
- [ ] Documentation total and non-documentation authored total are reported separately.
- [ ] Glob rules `*`, `?`, `**` follow the specified root-relative grammar.
- [ ] Stable JSON output schema and deterministic ordering match the spec.
- [ ] JSON output works.
- [ ] JSON config works.
- [ ] YAML config works.
- [ ] Equivalent scans are deterministic.

## Project A: cfgcheck

- [ ] Valid JSON returns success.
- [ ] Valid YAML returns success.
- [ ] Invalid configs return nonzero.
- [ ] `--dump-normalized` works.
- [ ] Equivalent JSON/YAML dumps match.

## Project A: stateprobe

- [ ] Valid fixture passes.
- [ ] Out-of-range time fails.
- [ ] Invalid history fixture fails.
- [ ] Derived-angle mismatch fails.
- [ ] Shipped `stateprobe_good.json` passes.
- [ ] Shipped bad-angle and bad-history fixtures fail.
- [ ] `stateprobe normalize` is byte-for-byte idempotent.

## Tests

- [ ] Main documented test command succeeds.
- [ ] Failure causes nonzero exit status.
- [ ] Time-model tests exist.
- [ ] Undo/Redo tests exist.
- [ ] JSON tests exist.
- [ ] YAML tests exist.
- [ ] Renderer tests exist.
- [ ] Project A tests exist.
- [ ] Deterministic GUI validation exists.
- [ ] `validation/test-summary.txt` is generated from actual results.

## Forbidden shortcuts check

- [ ] No GTK/Qt/SDL/GLFW GUI implementation.
- [ ] No Cairo/Skia/GDI+/Direct2D/DirectWrite/FreeType rendering substitution; GDI is used only for permitted raw-DIB presentation.
- [ ] No external JSON/YAML parser.
- [ ] No pre-rendered screenshot UI.
- [ ] Blur is not merely a dark translucent overlay.
- [ ] Analog/digital time do not use independent authoritative states.
- [ ] Negative playback is real.
- [ ] Hand dragging is pointer-angle based.
- [ ] Mandatory settings are connected.
- [ ] No mandatory TODO/placeholder behavior remains.

## Windows platform check

- [ ] Normal launch creates exactly one application-owned top-level Win32 window and no mandatory native child controls.
- [ ] Resize/minimize/restore do not corrupt framebuffer memory or desynchronize hit testing.
- [ ] Moving the window between different-DPI monitors (where available) does not double-scale the UI or change configured `ui_scale`.
- [ ] Pointer capture keeps drag interaction coherent outside the client area; capture/focus loss cancels safely for time/rate/Settings slider gestures.
- [ ] Ordinary mouse-leave without capture clears hover state and does not synthesize activation or cancel unrelated keyboard focus.
- [ ] Holding a discrete action shortcut such as Space does not auto-repeat Play/Pause toggles; only explicitly repeatable fields/sliders repeat.
- [ ] GDI is used only for permitted DIB/lifecycle substrate work, not to draw mandatory controls/text/effects, and the final DIB transfer is 1:1 rather than a hidden GDI scaling path.
- [ ] Config/runtime-state/Project-A paths with non-ASCII Windows path components work when passed through the real command line, without ANSI-code-page corruption.
- [ ] Explicit config/cfgcheck rejects ambiguous drive-relative/current-drive-rooted/device-namespace path forms required to be unsupported.

## Final status

- [ ] All G01–G25 release gates pass.
- [ ] No known release-blocking defect remains.
- [ ] Delivery package contains source, docs, examples, fixtures, and tests.

