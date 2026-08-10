# 05 — Gameplay, Scoring, Multiball, and Event System

## 1. Game session

Play Mode creates a game session distinct from editor state. Default starting turns/balls: 3.

A turn begins when the first active ball for that turn is created/available for launch. A turn ends only when active ball count reaches zero after the turn has begun and after same-step event consequences have been processed.

Event-spawned multiball balls do not consume additional reserved turns individually.

## 2. Required runtime game states

At minimum:

- `READY`;
- `PLAYING`;
- `PAUSED`;
- `TURN_END_TRANSITION`;
- `GAME_OVER`;
- `REPLAY_PLAYBACK` where applicable.

State transitions MUST be explicit and testable.

## 3. Ready state

READY displays score and remaining turns and permits launcher interaction when a launchable ball exists. Default state begins with a stationary ball at the primary enabled Ball Spawn.

## 4. Score representation

Score capacity must be at least unsigned/signed 64-bit. Wraparound is prohibited. On overflow risk, saturate or enter a checked error state; chosen policy must be documented and tested.

## 5. Scoring sources

Required scoring-capable sources/actions:

- Bumper;
- Slingshot;
- Sensor/Event `ADD_SCORE`.

Defaults:

- Bumper base score: 100;
- Slingshot base score: 50.

## 6. Qualified-hit cooldown

Bumper and Slingshot use per-ball/per-object cooldown to prevent contact spam.

Defaults:

- Bumper: 0.10 simulation seconds;
- Slingshot: 0.10 simulation seconds.

A new separated impact after cooldown may score again. Continuous overlap must not score every fixed step.

Normative boundary: retrigger qualifies when elapsed time is `>= cooldown`.

## 7. Combo system

Default combo rules:

- each qualified positive scoring event begins or extends combo;
- combo window: 2.0 simulation seconds;
- scoring event before timeout increments combo count;
- `combo_multiplier = clamp(combo_count, 1, 5)`;
- if elapsed time since prior qualifying score is `>= 2.0`, the new event starts a new combo at count 1 and multiplier 1×.

Award ordering is normative:

1. determine whether prior combo expired;
2. set/increment combo count;
3. derive combo multiplier;
4. derive multiball multiplier;
5. award score.

## 8. Multiball

Multiball means two or more simultaneously active independent physics balls.

Required:

- each ball has unique runtime ID;
- all balls collide with table and each other;
- each ball independently triggers sensors/scoring;
- each ball can drain independently;
- turn remains active while at least one ball remains;
- active count visible in UI;
- replay reproduces spawn/drain order and all ball states.

## 9. Multiball scoring bonus

While active ball count is at least 2, scoring receives 2× multiball multiplier.

Normative formula:

`awarded = base_score × combo_multiplier × multiball_multiplier`

where multiball multiplier is 2 when active balls ≥2, otherwise 1.

## 10. Event system model

An event binding links one source trigger to one or more ordered actions.

Required source triggers:

- `SENSOR_ENTER`;
- `SENSOR_EXIT`;
- `SENSOR_STAY`;
- `BUMPER_HIT`;
- `SLINGSHOT_HIT`;
- `BALL_DRAINED`.

Optional `GAME_START` and `TURN_START` may be added.

## 11. Required action types

At minimum:

1. `ADD_SCORE`
2. `SPAWN_BALL`
3. `ENABLE_OBJECT`
4. `DISABLE_OBJECT`
5. `SET_MULTIPLIER_OVERRIDE`
6. `START_MULTIBALL`
7. `OPEN_GATE`
8. `LIGHT_INDICATOR`

## 12. Action list ordering

Actions attached to one event are executed in explicit authored list order. Order survives save/load. If editor exposes action reordering, it participates in Undo/Redo.

## 13. ADD_SCORE

Parameter:

- `amount`: integer 0–1,000,000,000.

Score is affected by normal combo and multiball multipliers. Zero is legal but validation Warning.

## 14. SPAWN_BALL

Parameters:

- target Ball Spawn ID;
- `count`: 1–16;
- optional initial-velocity override.

Runtime creates as many as capacity and collision-safety rules permit. Each rejected spawn is diagnosed. Each created ball gets deterministic new runtime ID.

## 15. START_MULTIBALL

Convenience action with parameters:

- target spawn ID;
- `add_count`: 1–15.

It attempts to add that many balls, subject to capacity and blocked-spawn policy.

## 16. ENABLE_OBJECT / DISABLE_OBJECT

These mutate runtime enabled state only. They do not dirty authored scene. Restart and return to Edit restore authored enabled state.

Disabling a collider takes effect at the deterministic event-action phase; stale contact data must not dereference invalid state.

## 17. SET_MULTIPLIER_OVERRIDE

Provides a temporary scoring multiplier independent of combo.

Required parameters:

- multiplier integer 1–10;
- duration 0.05–30.0 simulation seconds.

Normative score formula while active:

`awarded = base × combo × multiball × override`

If multiple override actions overlap, the most recently executed override replaces the previous one. Duration expiration restores override=1. This runtime state is reset on Restart/turn reset as documented by implementation; normative v1 resets on Restart and Game Over, but persists across a turn while its duration remains.

## 18. OPEN_GATE

`OPEN_GATE target=<gate_id> duration=<seconds>` makes a targeted One-Way Gate non-solid for the given runtime duration.

Range: 0.05–30 s. On expiration, authored enabled/closed behavior resumes unless another runtime action has since disabled the object explicitly. Implementation must define deterministic precedence; normative priority is latest explicit runtime action.

## 19. LIGHT_INDICATOR

Sets target object's visual activation indicator for configurable 0.05–10.0 simulation seconds. It is feedback, not a lighting engine. The visual action does not alter physics unless combined with another action.

## 20. Event recursion and queueing

Actions MUST NOT recursively call unbounded event handlers in the same C stack. Event consequences use deterministic queue/phase processing.

Per-step action execution cap: 4096.

If cap is hit:

- stop processing additional actions for that step;
- increment diagnostic;
- present runtime warning in GUI;
- headless/test path returns a detectable failure/diagnostic according to scenario expectations;
- do not crash.

## 21. Event references

Authored actions reference stable object IDs. Dangling references are validation Errors.

## 22. Event timing phase

Normative v1 phase order for each fixed step:

1. sample/apply logical inputs scheduled for this step;
2. update kinematic/motor state such as flippers;
3. integrate/resolve ball physics and detect trigger/contact events;
4. queue detected gameplay events in deterministic order;
5. execute queued actions in deterministic order;
6. apply queued ball removals/spawns and runtime enable-state changes at safe synchronization points;
7. evaluate turn-end/game-over after event consequences;
8. publish diagnostics/checkpoint state.

Implementations may internally split phases further but external results must match this ordering.

## 23. Deterministic event ordering

When multiple events occur in one step, canonical order is:

1. event detection fixed-step index;
2. event sub-time / time-of-impact within that fixed step, ascending;
3. source authored object order;
4. triggering ball runtime ID;
5. trigger enum deterministic order;
6. action list index.

For a swept Sensor crossing that both enters and leaves during one fixed step, ENTER therefore precedes LEAVE because its crossing sub-time is earlier. Sub-time ties use the remaining keys above. Implementations that internally discover events in a different traversal order MUST sort or otherwise produce this canonical external order.

Equivalent stable ordering is acceptable only if documented and all package fixtures are satisfied.

## 24. Flipper controls

Multiple enabled flippers may share one logical input. Each follows its own authored angles/speeds. Left/right logical actions are recorded independently in replay.

## 25. Launcher power UI

While LAUNCH is held:

- visible charge meter follows simulation-time charge fraction;
- reaches 100% at full-charge time;
- stays clamped if held longer;
- release launches once;
- meter returns to zero after launch/cancel.

Replay records press/release at fixed-step indices.

## 26. Drain handling

On Drain crossing:

- mark ball drained exactly once;
- queue/remove it safely at deterministic phase;
- emit `BALL_DRAINED`;
- update active-ball count after action processing;
- do not subtract multiple reserved turns for simultaneous drains.

One turn ends when the last active ball of that turn is gone after same-step events.

## 27. Event spawn on last drain

If `BALL_DRAINED` action spawns a replacement ball in the same step, turn-end evaluation happens after the spawn action. If active count becomes nonzero, turn continues.

## 28. Game over

When no turns remain and active-ball count is zero:

- state becomes GAME_OVER;
- final score remains visible;
- gameplay inputs no longer change physics;
- Restart creates a fresh session;
- authored table remains unchanged.

## 29. Session high score

At minimum maintain high score for current application run. Persistent high score is optional. If implemented, corrupt high-score storage MUST NOT prevent table editing/play.

## 30. Runtime HUD

Play HUD SHALL show:

- score;
- combo multiplier;
- temporary override multiplier when not 1×;
- multiball indicator;
- remaining turns;
- active ball count;
- paused state;
- simulation speed when not 1× or debug mode active.

## 31. Pause semantics

While paused:

- no fixed step advances except Single Step;
- combo/override timers do not advance except Single Step;
- launcher charge does not advance;
- UI animations may continue;
- editor mutation remains disabled;
- debug inspection remains usable.

## 32. Replay gameplay equivalence

Replay MUST reproduce scoring events, combo timing, override timing, multiball state, drain order, turn transitions, active balls, and final score exactly for same build/scene.

## 30. Legacy trigger alias

When migrating `PINBALL_TABLE 1`, legacy `SENSOR_LEAVE` SHALL be interpreted as current `SENSOR_EXIT`. Canonical format-2 writer emits `SENSOR_EXIT`.

## 31. Additional scoring/event sources

Drop Target, Stand-up Target, Rollover, Spinner, and Kickout scoring/triggers are mandatory under document 21. Their scores enter the same combo/multiball/override formula unless explicitly suppressed by Tilt.

## 32. Nudge/Tilt gameplay state

The game-state model SHALL include Tilt meter and `TILTED` runtime state. Tilt suppresses player flipper/launcher actuation and score awards while continuing physics/drain semantics, then clears under the turn lifecycle specified in document 21.

## 33. Sensor occupancy

Current Sensors use ENTER/STAY/EXIT semantics from document 20. Staying inside may emit at most one STAY per fixed step; high-speed enter-and-exit inside one step emits ENTER then EXIT and no STAY.

## 34. Event cycle failure

Exceeding the 4096 action budget is a deterministic runtime error that pauses simulation and fails replay/headless verification. It is not merely a warning in v1.0.0.

## 35. Event traces

Every executed trigger/action must be traceable in production Event Trace when tracing is enabled, without changing execution order or deterministic results.
