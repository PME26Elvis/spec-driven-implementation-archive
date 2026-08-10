# 17 — Reference Defaults, Limits, and Timing Constants

## 1. Purpose

This document consolidates normative v1 defaults, supported ranges, timing constants, and hard limits that are otherwise defined in subsystem documents.

It is a lookup aid, not an independent source of alternative values. If a value here conflicts with the subsystem-specific normative specification, the subsystem specification wins and this table must be corrected before release.

## 2. Version identity

| Item | Normative value |
|---|---:|
| Task package version | 1.0.0 |
| Scene format magic | `PINBALL_TABLE 2` (reader also supports legacy 1) |
| Replay format magic | `PINBALL_REPLAY 1` |
| Physics behavior version | 1 |
| C language baseline | C17 |
| Desktop target | Linux + X11 |

## 3. World and viewport

| Item | Default | Supported range / rule |
|---|---:|---|
| World width | 1600 | 640–8192 logical units |
| World height | 1000 | 480–8192 logical units |
| Coordinate origin | top-left | fixed |
| +X | right | fixed |
| +Y | down | fixed |
| Canvas zoom | fit table | 25%–400% |
| Application window | 1440×900 recommended | minimum 1024×700 |

World coordinates and physics state use `double` precision.

## 4. Simulation timing

| Item | Value |
|---|---:|
| Fixed timestep | `1/240 s` |
| Integrator | semi-implicit Euler |
| Required speed multipliers | 0.25×, 0.5×, 1×, 2×, 4× |
| Single Step | exactly one fixed step |
| Wall-clock source for animation | monotonic |

Simulation-speed multiplier changes how quickly fixed steps are consumed relative to real time; it does not change `dt`.

## 5. Global physics defaults

| Item | Default / normative value |
|---|---:|
| Gravity X | 0 |
| Gravity Y | 980 logical units/s² |
| Penetration slop | 0.02 logical units |
| Position correction fraction | 0.60 |
| Solver iterations | 8 |
| Max TOI/impact resolutions per ball per fixed step | 16 |
| Numeric comparison epsilon where explicitly applicable | `1e-9` |
| Same-build replay/checkpoint tolerance | `1e-7` unless stricter exact field applies |

The equations and precise collision semantics are defined in `15_normative_physics_math.md`.

## 6. Default ball

| Property | Default | Validation range |
|---|---:|---:|
| Radius | 12 | 4–64 |
| Mass | 1.0 | 0.05–100 |
| Restitution | 0.78 | 0–1.25 |
| Friction | 0.08 | 0–2 |
| Linear damping | 0.03 | 0–10 |
| Maximum speed | 3000 | 100–10000 |

Non-finite values are invalid regardless of numeric range.

## 7. Active-ball limits

| Item | Default | Supported |
|---|---:|---:|
| Max active balls | 16 | 1–64 |
| Release multiball stress | 16 simultaneous balls | mandatory |
| Required stress duration | 30 simulated seconds | minimum |

The implementation must genuinely simulate every active ball and ball-ball contacts.

## 8. Launcher defaults

| Property | Default |
|---|---:|
| Minimum launch speed | 400 |
| Maximum launch speed | 1800 |
| Full-charge time | 1.2 simulated seconds |
| Charge curve | linear, clamped |

An enabled Launcher owns exactly one referenced Ball Spawn. More than one enabled Launcher targeting the same Ball Spawn is a validation Error.

## 9. Active collision impulses

| Object | Default additional impulse |
|---|---:|
| Bumper | 500 |
| Slingshot | 350 |

These impulses are additional to passive collision response and follow the ordering defined in the physics math specification.

## 10. Scoring defaults

| Item | Default |
|---|---:|
| Starting turns | 3 |
| Bumper base score | 100 |
| Slingshot base score | 50 |
| Per-source score cooldown | 0.10 simulated seconds |
| Combo window | 2.0 simulated seconds |
| Combo multiplier | 1×–5× |
| Multiball multiplier | 2× while active balls ≥2 |
| `ADD_SCORE` amount | 0–1,000,000,000 |
| Score storage | at least 64-bit, no wraparound |

An event at elapsed time exactly `>= 2.0` seconds from the prior qualifying scoring event begins a new combo at 1×.

## 11. Event limits

| Item | Range / limit |
|---|---:|
| Actions executed per fixed step | max 4096 |
| `SPAWN_BALL count` | 1–16 |
| `START_MULTIBALL add_count` | 1–15 |
| Multiplier override | integer 1–10 |
| Multiplier override duration | 0.05–30.0 s |
| Gate-open duration | 0.05–30.0 s |
| Light-indicator duration | 0.05–10.0 s |

Event ordering includes fixed-step index and event sub-time before authored-object and runtime-ID tie breakers.

## 12. Editor defaults

| Item | Default / requirement |
|---|---|
| Minor grid | 10 logical units |
| Major grid | 50 logical units |
| Translation snap options | 5, 10, 25, 50 |
| Default angle snap | 15° |
| Undo/Redo capacity | at least 100 persistent commands |
| Object ID length | max 63 ASCII bytes |
| Object IDs | unique within document |

Persistent authored edits participate in dirty state and Undo/Redo. View-only changes do not.

## 13. UI animation reference values

| Effect | Normative range / value |
|---|---|
| Hover transition | 140–200 ms |
| Hover elevation | 1–3 px plus shadow |
| Click ripple | 280–450 ms |
| Glow transition | 120–220 ms |
| Capsule indicator slide | 180–280 ms |
| Modal open/close | 180–300 ms |
| Modal initial scale | 0.94–0.97 |
| Modal final scale | 1.0 |
| Modal opacity | 0→1 on open |
| Modal backdrop blur | 6–12 px equivalent |
| Modal backdrop dark alpha | 0.25–0.45 |
| Primary easing | `cubic-bezier(0.22, 1.00, 0.36, 1.00)` |

Animations must be interruptible and reverse from their current interpolated state rather than snapping to an endpoint.

## 14. Scene I/O limits

| Item | Requirement |
|---|---|
| Default extension | `.pbt` |
| Encoding | UTF-8 |
| Normal file size support | at least 16 MiB |
| User-facing parsed string | at least 4096 bytes safely handled |
| Save strategy | atomic replacement |
| Load strategy | transactional |

Object and event counts must be bounded defensively. The parser must reject malformed or unsupported data without corrupting the currently open scene.

## 15. Replay defaults

| Item | Requirement |
|---|---|
| Default extension | `.pbr` |
| Input time base | fixed-step index |
| Scene fingerprint | FNV-1a 64-bit or stronger deterministic fingerprint |
| Randomness | explicit deterministic seed if any randomness exists |
| GUI/headless core | same production simulation/event logic |

Wall-clock timestamps are not a substitute for fixed-step replay input timing.

## 16. Required automated verification scale

| Item | Minimum |
|---|---:|
| Mandatory meaningful automated test cases | 420 |
| Long-run headless simulation | 1,000,000 fixed steps |
| Determinism repetitions for required scenario | 10 identical runs |
| Visual static evidence IDs | V01–V38 |
| Transition/interaction evidence IDs | A01–A25 |

The required test count is a floor, not a substitute for the specifically named cases and Release Gates.

## 17. locscan defaults and policies

`locscan` must support both JSON and the required YAML subset, physical line counting, categories, exclusions, symlink policy, UTF-8 paths, and machine-readable output.

Default policy examples in `acceptance/locscan_example.json` and `acceptance/locscan_example.yaml` exclude build outputs, logs, caches, binary media, generated result directories, and visual evidence from authored-code/document totals.

## 18. Values that are intentionally implementation-defined

The following may vary only where the subsystem documents permit it:

- exact internal data structures;
- broad-phase acceleration structure;
- exact CCD algorithm, provided normative outcomes and anti-tunneling requirements are met;
- software-rasterization implementation details;
- font family and glyph-rasterization strategy within allowed platform boundaries;
- exact panel width at normal window size;
- file-picker visual composition;
- build-system choice compatible with the dependency policy;
- optional extra shortcuts;
- optional extra diagnostics.

Implementation-defined does not mean untested. Externally observable required behavior remains subject to the acceptance package.

## 19. v1.0 editor/layer/history limits

| Item | Normative value |
|---|---:|
| Required object types | 15 |
| Maximum authored objects | 10,000 |
| Maximum groups | 2,000 |
| Maximum layers | 64; editor must support at least 16 |
| Group nesting | prohibited in v1.0.0 |
| Object group membership | at most one group |
| Default paste offset | (20,20) logical units |
| Overlap-click cycle window | 900 ms |
| Marquee activation displacement | 4 UI units |
| Undo minimum retained commands | 100 |
| Default Undo memory cap | 128 MiB |

## 20. Nudge/Tilt defaults and ranges

| Item | Default | Validation range |
|---|---:|---:|
| Nudge impulse | 85 | 0–500 logical units/s |
| Nudge tilt cost | 1.0 | 0–10 |
| Tilt threshold | 3.0 | 0.1–100 |
| Tilt decay | 0.75 | 0–20 /s |
| Nudge cooldown | 0.08 s | 0–2 s |

## 21. Additional mechanism defaults

| Item | Default / range |
|---|---|
| Drop/Stand-up target min hit speed | default 80; range 0–5000 |
| Drop reset delay | default 1.0 s; range 0.05–30 s when AFTER_DELAY |
| Rollover width | default 24; range 4–256 |
| Spinner inertia | default 1.0; range 0.001–1000 |
| Spinner angular damping | default 1.0 s^-1; range 0–50 |
| Spinner tick angle | default 30°; range 1–180° |
| Kickout capture radius | default 24; range 4–128 |
| Kickout eject speed | default 900; range 0–5000 |
| Kickout hold time | default 0.75 s; range 0–30 s |

## 22. Timing/reliability defaults

| Item | Normative value |
|---|---:|
| Real-time stall threshold | >250 ms wall delta |
| Catch-up max per rendered frame | 60 fixed steps |
| Autosave inactivity | 30 s |
| Autosave maximum dirty interval | 120 s |
| Safe Startup trigger | 2 consecutive failed auto-restore launches |
| Live Event Trace retention | >=10,000 records |
| Selected-ball Collision Trace | >=256 records |

## 23. Required UI scales

100%, 125%, 150%, and 200%. Reduced Motion transition maximum for non-essential transitions: 80 ms.

## 24. Performance workload limits

P1/P2/P3 composition and response/memory thresholds are normative in document 25. Performance measurements never permit reducing fixed-step physics quality.

## 25. Current format versions

Canonical scene writer: `PINBALL_TABLE 2`. Legacy scene reader: versions 1 and 2. Current replay magic remains `PINBALL_REPLAY 1` unless implementation-specific optional extensions are versioned compatibly.
