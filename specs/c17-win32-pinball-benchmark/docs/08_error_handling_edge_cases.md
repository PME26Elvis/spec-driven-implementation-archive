# 08 — Error Handling and Edge Cases

## 1. General rule

Recoverable external errors MUST NOT terminate the application process. Malformed user input is not an internal invariant failure.

Fatal internal invariant failure may terminate after diagnostics if continuation would be unsafe.

## 2. Error categories

At minimum distinguish:

- user property-input validation;
- scene syntax/load errors;
- scene semantic validation errors;
- file I/O errors;
- replay errors;
- runtime physics-safety errors;
- resource-limit errors;
- internal programming errors/assertions in debug builds.

## 3. Dirty-document New/Open/Quit

Dirty document prompts Save / Discard / Cancel.

- Save: continue only after successful save;
- Discard: proceed without saving;
- Cancel: keep current state and abort requested action.

Open remains transactional so a failed target load does not destroy the current valid scene.

## 4. Failed Save/Save As

On path/write/rename failure:

- show error;
- preserve prior document path semantics;
- remain dirty;
- no false success notification;
- clean temporary file where possible;
- preserve previous destination content where possible.

Partial/short writes and close/flush errors count as failure.

## 5. Corrupt/unsupported scene

Malformed or unsupported file produces readable diagnostics, retains current scene, never enters Play with partial state, and does not crash.

## 6. Duplicate ID

Duplicate object ID is rejected on load/validation. Editor rename to existing ID is rejected atomically.

## 7. Numeric range failures

Parsing detects overflow/underflow and integer wrap risk. Huge numbers do not wrap into valid small values.

Literal/derived NaN or infinity is rejected before normal simulation.

## 8. Runtime non-finite state

If a ball becomes NaN/inf at runtime:

- simulation enters controlled fault/pause;
- diagnostic identifies ball and step;
- GUI reports failure;
- headless returns non-zero;
- renderer does not continue using invalid coordinates.

## 9. Degenerate geometry

Zero-length Wall/Ramp/Gate/Slingshot and zero launcher direction are validation Errors. No divide-by-zero normalization.

## 10. Coincident centers

Bumper/ball or ball/ball coincident-center cases use deterministic fallback normal. Result stays finite and reproducible.

## 11. Exact touching contact

Ball exactly touching surface while separating MUST NOT receive repeated closing bounce impulse solely from tolerance overlap.

## 12. Corner trap

Contact iterations remain bounded. Persistent cap hits increment diagnostics; no infinite loop or energy explosion.

## 13. High-speed thin wall

Ball displacement greater than wall thickness during one step must still collide in required acceptance fixture.

## 14. High-speed Sensor

Ball entering and leaving an entire Sensor within one fixed step produces deterministic ENTER then LEAVE in the package high-speed fixture.

## 15. High-speed Drain

Swept Drain crossing drains ball even if final position lies beyond Drain rectangle.

## 16. Multiple balls same position

Adversarial coincident balls remain finite/deterministic and separate according to stable fallback policy.

## 17. Spawn capacity exceeded

Excess spawns are rejected; existing balls unchanged. Diagnostic may aggregate repeated notices to avoid notification spam.

## 18. Runtime blocked spawn

Blocked target spawn rejects new ball safely. Event queue continues. No explosive overlapping ball is created.

## 19. Referenced object deletion

Editor blocks deletion or safely removes dependent references according to its documented consistent policy. Silent dangling reference is prohibited.

## 20. Referenced object rename

All references update atomically. Undo/Redo restores both ID and references together.

## 21. Undo after save

Undo remains functional after save. Document MUST NOT remain falsely clean after content diverges from saved file.

## 22. Save with invalid field edit

If a property field contains uncommitted invalid text, Save must block/focus the error or explicitly revert it. Silent serialization of an unrelated clamped value is prohibited.

## 23. Play with validation errors

Blocked. User sees validation result. Warnings alone do not block.

## 24. Modal input ownership

A blocking modal owns interaction; background controls cannot activate behind it from pointer or keyboard.

## 25. Rapid Play/Pause

Repeated toggles leave one coherent explicit state. No duplicate timers or concurrent simulation loops.

## 26. Single Step

Each accepted activation while paused advances exactly one `1/240 s` step. It does not depend on display frame.

## 27. Speed change while paused

Changing 0.25×…4× does not advance simulation. Next Single Step remains one fixed step.

## 28. Resize during simulation

Window resize does not alter world physics or replay result.

## 29. Window focus loss with held input

Focus loss must synthesize release or otherwise safely clear held logical flipper/launcher state. Replay recording captures a logical release if it changes simulation.

## 30. Launcher held across pause

Charge uses simulation time. Pause stops charge. Single Step advances charge by one fixed step.

## 31. Event cycles

Cyclic event graph hits bounded per-step action cap and diagnoses it rather than recurring forever.

## 32. Disabled event source

Disabled Sensor/Bumper/Slingshot emits no new normal gameplay events after disable phase.

## 33. Disable collider during contact

Subsequent collision queries respect disabled state. No stale-pointer dereference.

## 34. Replay scene mismatch

Strict verified playback blocks/fails on mismatched scene hash. Optional force-play debug mode must be clearly labeled unverified.

## 35. Truncated replay

Partial record is Error. Syntactically complete EOF is allowed.

## 36. Unknown CLI option

Readable error plus non-zero exit status.

## 37. Headless without GUI window/interactive desktop

Must run. Creating an HWND, initializing GUI-only DPI/clipboard/IME/rendering state, or otherwise requiring an interactive desktop during headless verification is a failure.

## 38. Framebuffer size arithmetic

Zero/tiny/huge resize requests must not produce negative dimensions, integer overflow, buffer overflow, or unchecked allocation multiplication.

## 39. Allocation failure

Significant allocations are checked. Null is not dereferenced. Graceful fatal error is acceptable when safe continuation is impossible.

## 40. Object-count stress

Normal required stress table has 500 authored objects. Implementation's hard cap must be at least 10,000 objects before a documented resource-limit rejection is permitted.

## 41. Ball-count stress

Configured range includes up to 64 active balls. Release stress requires at least 16 simultaneous balls with ball-ball collision.

## 42. Long names and UTF-8

Table display name up to 1024 UTF-8 bytes loads safely if valid and clips/wraps in UI. Object IDs remain ASCII 63-byte max.

## 43. Paths

Spaces and Unicode path components work. Normal save/load must not build unsafe shell commands from user paths and must not corrupt paths through an ANSI-code-page round trip.

## 44. Read-only destination

Save failure reports and preserves current scene/dirty state.

## 45. Missing target file

Open missing path reports I/O error and keeps current document.

## 46. Empty/header-only files

Empty file: missing-header Error. Header-only: missing table/required fields Error.

## 47. Empty authored new table

New scene may temporarily have no objects and can be saved as syntactically valid authored data, but Play remains blocked by semantic validation.

## 48. Combo boundary

At elapsed combo time `<2.0`, event extends combo. At `>=2.0`, it starts a new combo at 1×.

## 49. Cooldown boundary

At elapsed time exactly equal to cooldown, retrigger qualifies.

## 50. Simultaneous drains

Multiple balls draining same step and leaving zero active balls end exactly one turn.

## 51. Drain-triggered respawn

If `BALL_DRAINED` action spawns a new ball in same step, turn-end decision occurs after event consequences. Nonzero active count means turn continues.

## 52. UI animation under low FPS

Elapsed-time animations reach correct endpoint even if only a few frames render. Modal/panel cannot become permanently stuck halfway.

## 53. Rapid animation reversal

Repeated enter/leave, modal open/close, and panel collapse/expand events remain continuous and finite with no state jump or negative opacity/scale.

## 54. Editor drag cancellation

Escape during move/rotate/resize restores exact pre-drag authored values and produces no Undo command.

## 55. Duplicate/Paste references

Duplicating a set with internal references must not accidentally bind duplicated event target back to original when both source and target were duplicated, if implementation follows recommended remap behavior. Whatever policy is chosen must be deterministic and documented.

## 56. Validation ordering

Running validation repeatedly on unchanged scene produces same issue ordering.

## 57. Save/load unknown field

Known-section unknown key yields warning and safe load if all required data valid; no false assumption that unknown field affected behavior.

## 58. File-size/resource limit

Input over documented limit fails before pathological allocation. Current open scene remains intact.

## 59. Replay event order

Same-step logical actions execute in deterministic order. Duplicate DOWN events without UP must follow documented idempotent/validation policy and cannot cause unbounded repeated actuation.

## 60. Game over input

Flipper/launcher gameplay inputs after GAME_OVER do not mutate physics. UI navigation/restart remain usable.

## 31. v1.0 reliability and editor edge cases

Mandatory additional handling is defined by documents 19, 20, 21, 23, and 24. In particular, the product must safely handle locked mixed selections, clipboard destination limits, external file conflicts, autosave/recovery corruption, legacy migration, parser fuzz corpus, popup/modal focus ownership, unsupported UI scale transitions, blocked target/Kickout restoration, event cycles, non-finite runtime state, and frame stalls.

## 32. No silent repair of authored invalid data

Except for explicitly defined legacy migration defaults, the loader/editor SHALL NOT silently clamp or invent valid values for malformed authored fields. Reject with contextual diagnostic or require explicit user correction.

## 33. Runtime failure preservation

For deterministic runtime failures such as `EVENT_BUDGET_EXCEEDED`, `NON_FINITE_STATE`, `BALL_OUT_OF_WORLD`, or `KICKOUT_EJECT_BLOCKED`, pause and preserve inspectable runtime state. Do not auto-restart the session and erase evidence.

## 34. Safe resource-limit behavior

All object/event/string/file/history/trace limits in document 17 are hard safety boundaries. Crossing a limit through external input or editor operation must fail atomically and clearly.


## 35. Windows-specific edge cases

The Windows variant additionally SHALL handle:

- `WM_CLOSE`/Alt+F4 through the dirty-document contract;
- loss of mouse capture during an active editor drag without leaving a stuck transaction;
- DPI change during Edit, Play, paused replay, modal, and text editing without mutating simulation/authored data;
- minimize/restore and full client repaint without stale framebuffer content;
- Windows Unicode filename/path input containing Chinese characters;
- destination replacement failure caused by sharing/locking violation while preserving the previous destination;
- read-only file attributes and access-denied paths as transactional I/O failures;
- a path longer than legacy `MAX_PATH` without stack/buffer overflow or truncation;
- clipboard temporarily unavailable/locked without crash or data loss;
- malformed/unpaired UTF-16 input at the OS boundary without producing invalid internal UTF-8;
- abrupt `WM_CAPTURECHANGED` or focus loss during pressed gameplay/editor input;
- no dependence of fixed-step outcomes on `GetMessage`/`PeekMessage` frequency, paint frequency, timer resolution, or monitor refresh rate.
