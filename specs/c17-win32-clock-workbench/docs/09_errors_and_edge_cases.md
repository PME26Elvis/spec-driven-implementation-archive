# 09 — Error Handling and Edge Cases

## 1. Principle

Recoverable errors must preserve the last valid application state whenever possible.

The application must not convert ordinary invalid input into crashes, memory corruption, stale interaction states, or silent data loss.

## 2. Startup configuration missing

If no explicit `--config` path is given and `./clock-config.json` is absent, built-in defaults are used and the status/message area must surface a non-fatal `Using built-in defaults` indication after launch.

If an explicit config path is given and does not exist or cannot be read, startup must exit nonzero before opening the main window and expose a concise diagnostic. It must be written to stderr when a parent/test console is available; an ordinary GUI launch without a console may use the fatal pre-window `MessageBoxW` exception from the scope document.

## 3. Startup configuration invalid

If explicitly requested configuration is syntactically or semantically invalid, startup must exit nonzero before opening the main window. An existing implicit `./clock-config.json` that is unreadable or invalid must fail the same way. The diagnostic must identify the file and syntax/schema location as specified for `cfgcheck`. Safe-default fallback is reserved only for the **absent** implicit `./clock-config.json` case.

## 4. Runtime reload failure

Reload failure—including the active config path being deleted, unreadable, malformed, or schema-invalid—leaves `active_config` and `saved_config` unchanged and surfaces an in-app error. Reload never falls back to built-in defaults.

No partially parsed values may leak into active state.

## 5. Save failure

If config/state save fails:

- in-memory state remains valid;
- prior on-disk file remains intact where atomic-save preconditions permit;
- user receives a clear error;
- application remains usable unless underlying failure prevents continued operation.

## 6. Invalid digital hour

Examples such as `29` in 24-hour mode or `00` in 12-hour display-entry mode must not commit.

The UI indicates invalid input and allows correction or Escape cancellation.

## 7. Invalid minute/second

Values above 59 do not commit.

Typing `6` as a first minute/second digit creates the required one-digit pending buffer. If followed by `0`, the invalid two-digit `60` buffer is visibly rejected and never commits; Enter on the one-digit `6` commits value `06`.

## 8. Empty digital buffer

Backspace may make the pending field buffer empty transiently, but an empty buffer never becomes canonical state and focus loss reverts to the pre-edit value.

Focus loss with empty buffer reverts to prior valid value.

## 9. Non-digit input

Letters and irrelevant punctuation in fixed numeric fields are ignored or rejected with unobtrusive feedback.

They must not corrupt the edit buffer.

## 10. Rapid input

Rapid digit entry, repeated arrow keys, and switching focus quickly must not generate malformed state or dozens of unintended history transactions.

## 11. Midnight forward

Forward simulation through midnight wraps smoothly.

There must be no one-frame display of `24:00:00` in 24-hour mode.

## 12. Midnight reverse

Reverse simulation through midnight wraps smoothly from `00:00:00` to `23:59:59...`.

## 13. High positive rate

At `+100×`, canonical time may cross many minute boundaries rapidly without hand discontinuity beyond expected rendering-frame sampling.

## 14. High negative rate

At `-100×`, the same must hold in reverse.

## 15. Zero rate

At `0×`, simulated time remains unchanged while UI animations continue normally.

Play/Pause remains independently controllable.

## 16. Paused with negative rate

Pausing at a negative rate stores that rate.

Resuming continues backward.

## 17. Rate crossing zero

Dragging rate from positive through zero to negative must smoothly reverse simulation direction.

No sign-stale label or snapping to `+0` is allowed.

## 18. Hand drag through 12

Crossing the 12-o'clock angular boundary in either direction must remain locally continuous.

## 19. Hand drag near center

If pointer enters dead radius near clock center, current candidate time remains stable until a reliable angle is available again.

The hand must not spin randomly.

## 20. Drag far outside clock

Once a valid hand drag has begun, moving pointer far outside the clock still maps by angle around the center and continues the drag.

The distance from center does not amplify time movement.

## 21. Multiple revolutions

Continuous rotations are tracked without discontinuity.

Integer/floating overflow is not plausible within ordinary gestures, but code must avoid unbounded counters of unsafe type.

## 22. Release outside window

Pointer capture/grab must ensure drag completion or safe cancellation when release occurs outside the initial control.

The application must not remain in pressed/drag state indefinitely.

## 23. Window focus/capture loss

Focus loss clears transient hover states and cancels any active hand drag, digital scrub, rate-slider drag, or Settings-slider drag according to the exact cancellation contract in `05_interaction_and_undo.md`. `WM_CAPTURECHANGED`/`WM_CANCELMODE` must resolve active capture gestures under the same policy even if focus itself did not change. Coalesced keyboard adjustments are finalized as one transaction on focus loss, and keyboard modifiers/held-key state must not remain logically stuck. Mouse-leave without capture clears ordinary hover state.

## 24. Resize to minimum

At minimum supported size, every required Clock-view control remains visible and usable.

No text field may have zero/negative size.

## 25. Very large window

The supported client-area physical maximum is `3840×2160`. Runtime sizing logic must enforce that maximum under ordinary user resizing. At that maximum, geometry calculations remain finite and framebuffer allocation uses overflow-safe size calculations.

Before allocating `width * height * bytes_per_pixel`, check multiplication overflow. If the OS transiently reports a larger client extent, the renderer must not use it to perform unchecked allocation/index arithmetic.

## 26. Repeated resize

Rapid resizing must not leak a framebuffer per resize event or use freed memory.

## 27. Minimize/restore

Minimization must not replace the last valid client geometry with `0×0`, must not persist a minimized/zero geometry, and must not require a zero-sized framebuffer allocation. After restore, the next visible frame must fully redraw correctly and hit testing must use the restored client geometry.

Large-gap simulation clamp applies when playing. Paused/`0×`/edit-suspended time never accumulates as a resume backlog.

## 28. Settings scroll bounds

Scrolling above top clamps to top.

Scrolling below final content clamps to bottom.

Dynamic nav effect returns exactly to its base state at top.

## 29. Blur radius zero

With configured blur zero, modals/nav still dim/tint as designed and remain functional.

No divide-by-zero or invalid kernel occurs.

## 30. Animation disabled

Disabling animation immediately or on next state transition must not leave components halfway between states permanently.

## 31. UI scale changes

Changing UI scale through Settings triggers safe relayout/framebuffer use.

Pointer hit testing must use the new scale immediately after the committed change.

## 32. Unsupported glyph

If an optional/user-provided string contains a glyph unavailable in the embedded font, render a deterministic replacement glyph without corrupting adjacent text or UTF-8 storage.

## 33. Malformed UTF-8 in config

JSON strings that directly contain malformed UTF-8 bytes must be rejected.

YAML file text must be valid UTF-8 for this assignment.

Diagnostics must report the 1-based line and column of the first offending byte/sequence, consistent with the parser diagnostic contract.

## 34. JSON nesting depth

Both parsers use the fixed mandatory nesting limit of 64 from `08_config_json_yaml.md`. Exceeding it produces a parse error before any configuration state is applied.

## 35. YAML indentation depth

YAML uses the same fixed maximum structural nesting depth of 64.

## 36. Oversized config

The maximum configuration input size is exactly 1 MiB as specified in `08_config_json_yaml.md`. Larger files fail clearly rather than causing uncontrolled allocation.

## 37. Unknown config property

Unknown properties fail schema validation.

The error identifies the property path.

## 38. Duplicate config property

Duplicate keys fail parsing/validation consistently in both JSON and YAML.

## 39. Numeric locale

Configuration numeric parsing and serialization must use `.` decimal semantics regardless of user locale.

## 40. State file corruption

If optional persisted runtime state is corrupt:

- configuration can still load;
- runtime state falls back to configured defaults;
- user receives a warning;
- corrupt state is not silently overwritten until a deliberate save/exit policy action occurs.

## 41. Partial state file

Truncated JSON state is treated as corruption, not partially applied.

## 42. Undo at stack beginning

Undo when no entry exists does nothing safely and button is disabled.

## 43. Redo at stack end

Redo when no entry exists does nothing safely and button is disabled.

## 44. History capacity

When history exceeds capacity, oldest entries are discarded without corrupting current/redo state.

## 45. Undo after midnight edit

History stores normalized canonical time and restoring it must not produce out-of-range values.

## 46. Redo invalidation

New committed edit after undo clears redo immediately and updates button disabled state.

## 47. Reload config with history

Reloading configuration preserves existing time/playback history. A successful Reload replaces both `active_config` and `saved_config` with the validated file contents and leaves dirty state false immediately after the Reload. If it materially changes `active_config`, it appends one `config` or `config_batch` history entry capable of representing any changed schema leaf, including leaves that are not GUI-editable. Undo/redo of that entry changes only `active_config`, so Undo after Reload may make Settings dirty relative to the newly loaded baseline and never performs hidden config-file writes. `Apply` performs persistence only and does not add a duplicate logical-state entry.

## 48. Settings Revert with animation

Revert must restore logical settings immediately and visuals must animate/retarget from current state without waiting for old animations to finish.

## 49. Modal stacking

At most one primary modal may be open at a time.

If a secondary confirmation is needed, modal stack depth must be bounded and focus/input isolation remain correct.

## 50. Close application with unsaved settings

If Settings is dirty, closing the application must show the same in-app `Save`, `Discard`, `Cancel` confirmation required when leaving Settings. No auto-apply policy is permitted for this assignment.

Before dirty-settings and persistence shutdown logic runs, transient input is resolved deterministically: pointer drags are cancelled under their gesture-start restore rules; a pending typed digital buffer resolves exactly as ordinary focus loss (valid non-empty buffer commits, invalid/empty buffer reverts); and an open coalesced keyboard-adjustment sequence commits its already-applied changes as one history transaction. Temporary edit suspension then ends.

## 51. Close during active drag

Window-close request during an active pointer drag cancels that gesture before shutdown logic.

No partially written state file may be produced.

## 52. File paths with spaces

Configuration and state paths containing spaces must work when supplied as proper command-line arguments.

## 53. Read-only config directory

Save fails gracefully while existing loaded configuration remains active.

## 54. Permission denied state file

If required runtime-state persistence fails during a user-requested normal exit, the application must remain alive and show an in-app error/confirmation with at least `Retry`, `Exit without saving state`, and `Cancel`. Only explicit `Exit without saving state` may continue shutdown after the failed state write. The failure must never become a crash.

## 55. Config/state target collision

A configuration whose resolved `persistence.state_file` aliases the active configuration target is invalid and must be rejected before normal runtime use. The application must never accept such a configuration and later overwrite its own config with runtime-state JSON. `cfgcheck FILE` performs the same contextual collision validation.

## 56. Allocation failure testability

Memory-allocation wrappers may provide a deterministic failure-injection mode in tests.

Parser and framebuffer critical paths must have deterministic tests that inject or simulate allocation failure and verify a controlled error path with no state corruption or out-of-bounds access.
