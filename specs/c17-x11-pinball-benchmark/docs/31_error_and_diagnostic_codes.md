# 31 — Stable Error and Diagnostic Codes

Human-readable wording may vary, but machine-readable reports and required tests SHALL expose stable codes for the classes below. More specific implementation codes are allowed if they map unambiguously to one required code.

## 1. Scene syntax/load codes

| Code | Meaning |
|---|---|
| `PBT_E_EMPTY` | no non-comment content |
| `PBT_E_HEADER` | malformed/missing magic header |
| `PBT_E_VERSION_UNSUPPORTED` | unsupported table major version |
| `PBT_E_UTF8` | invalid UTF-8 in required text context |
| `PBT_E_NUL` | embedded NUL in textual scene input |
| `PBT_E_LINE_TOO_LONG` | physical line exceeds hard parser limit |
| `PBT_E_TOKEN_TOO_LONG` | token/string/identifier exceeds hard limit |
| `PBT_E_SYNTAX` | malformed section/key/value grammar |
| `PBT_E_DUP_KEY` | duplicate field in one section |
| `PBT_E_DUP_ID` | duplicate layer/object/group/event ID |
| `PBT_E_UNKNOWN_SECTION` | unsupported section class |
| `PBT_E_UNKNOWN_OBJECT` | unsupported object type |
| `PBT_E_UNKNOWN_ACTION` | unsupported event action |
| `PBT_E_MISSING_FIELD` | mandatory field absent |
| `PBT_E_TYPE` | scalar/value has wrong type/shape |
| `PBT_E_NONFINITE` | NaN/Inf spelling or parsed non-finite numeric |
| `PBT_E_NUMERIC_RANGE` | finite value outside required range |
| `PBT_E_NUMERIC_OVERFLOW` | numeric literal cannot be represented safely |
| `PBT_E_STRING_ESCAPE` | unsupported/invalid string escape |
| `PBT_E_STRING_UNTERMINATED` | unterminated quoted string |
| `PBT_E_ACTION_INDEX` | action_count/index set is missing/duplicate/non-contiguous |
| `PBT_E_REFERENCE_MISSING` | referenced ID does not exist |
| `PBT_E_REFERENCE_TYPE` | referenced ID exists but wrong required type |
| `PBT_E_GROUP_NESTING` | group references group / prohibited nesting |
| `PBT_E_GROUP_MEMBERSHIP` | duplicate member or object in multiple groups |
| `PBT_E_LAYER` | invalid/missing layer assignment/order |
| `PBT_E_LIMIT_FILE` | file-size hard limit exceeded |
| `PBT_E_LIMIT_OBJECTS` | authored object limit exceeded |
| `PBT_E_LIMIT_EVENTS` | event limit exceeded |
| `PBT_E_LIMIT_ACTIONS` | per-event authored action limit exceeded |
| `PBT_E_LIMIT_GROUPS` | group limit exceeded |
| `PBT_E_LIMIT_LAYERS` | layer limit exceeded |
| `PBT_E_ALLOCATION` | allocation failure during transactional parse/load |

## 2. Scene validation codes

| Code | Meaning |
|---|---|
| `VAL_E_NO_SPAWN` | no enabled playable Ball Spawn |
| `VAL_E_NO_DRAIN` | no enabled Drain |
| `VAL_E_DEGENERATE_GEOMETRY` | zero/invalid collision geometry |
| `VAL_E_SPAWN_BLOCKED_STATIC` | authored spawn begins invalidly blocked |
| `VAL_E_LAUNCHER_OWNERSHIP` | duplicate/invalid launcher-spawn ownership |
| `VAL_E_TARGET_RESET` | invalid target reset configuration |
| `VAL_E_SPINNER` | invalid spinner inertia/tick/geometry |
| `VAL_E_KICKOUT` | invalid kickout direction/timing/geometry |
| `VAL_E_EVENT_CYCLE_STATIC` | statically detectable invalid immediate cycle when implementation diagnoses it pre-Play |
| `VAL_E_WORLD_BOUNDS` | authored geometry violates required world-bound rule |
| `VAL_W_OUTSIDE_WORLD` | permitted authored element lies partly outside world but remains loadable |
| `VAL_W_ZERO_SCORE` | scoring action/source configured with zero score where allowed |

## 3. Save/backing-file codes

| Code | Meaning |
|---|---|
| `IO_E_TEMP_CREATE` | atomic-save temporary creation failed |
| `IO_E_WRITE` | serialization/write failed |
| `IO_E_FLUSH` | flush/durability operation failed |
| `IO_E_CLOSE` | temporary close failed |
| `IO_E_RENAME` | final atomic replace/rename failed |
| `IO_E_NO_SPACE` | no-space condition recognized |
| `IO_E_EXTERNAL_CHANGED` | backing file content changed externally |
| `IO_E_EXTERNAL_DELETED` | backing file was externally deleted |
| `IO_E_RECOVERY_CORRUPT` | recovery snapshot invalid/corrupt |
| `IO_E_RECOVERY_WRITE` | autosave/recovery write failed |

## 4. Runtime codes

| Code | Meaning |
|---|---|
| `RT_E_EVENT_BUDGET` | per-fixed-step event action budget exceeded |
| `RT_E_NONFINITE_STATE` | required runtime numeric became NaN/Inf |
| `RT_E_BALL_OUT_OF_WORLD` | ball escaped valid world without Drain semantics |
| `RT_E_KICKOUT_EJECT_BLOCKED` | Kickout could not eject after 240 deferred steps |
| `RT_E_IMPACT_BUDGET` | per-ball impact/TOI resolution safety budget exceeded when treated as failure |
| `RT_E_SPAWN_BLOCKED` | required runtime spawn could not be placed under specified policy |
| `RT_E_ACTIVE_BALL_LIMIT` | action attempted to exceed active-ball hard cap |

## 5. Replay/verification codes

| Code | Meaning |
|---|---|
| `RPL_E_FORMAT` | malformed replay |
| `RPL_E_SCENE_MISMATCH` | replay scene semantic fingerprint does not match |
| `RPL_E_VERSION` | unsupported replay/physics behavior version |
| `RPL_E_DIVERGENCE` | expected replay/checkpoint diverged |
| `DET_E_DIVERGENCE` | detcompare found first semantic mismatch |
| `REL_E_SCHEMA` | release result/evidence schema invalid |
| `REL_E_REQUIREMENT_MISSING` | stable requirement absent from evidence manifest |
| `REL_E_REQUIREMENT_DUP` | requirement appears more than once |
| `REL_E_EVIDENCE_MISSING` | referenced proof path/ID missing |
| `REL_E_GATE_CONTRADICTION` | Gate PASS contradicts member requirement status |
| `REL_E_VERSION_CONTRADICTION` | release artifacts disagree on build/task version |

## 6. Severity and determinism

- Parse codes are Errors and transactional.
- Runtime `RT_E_*` codes listed above pause/stop deterministic simulation as specified and are observable in headless output.
- Warnings do not block Play unless another specification explicitly elevates them.
- For identical invalid input, the primary diagnostic code and deterministic diagnostic ordering SHALL be the same across repeated runs.
- If multiple independent errors exist, parser/validator may report several, but the first error order must follow file authored order then stable validation-rule order.

## 7. Error payload

Machine-readable error payload SHOULD contain:

- `code`;
- `severity`;
- human `message`;
- path;
- line/column when known;
- section/object/event ID when known;
- field/action when known;
- bounded token excerpt;
- related ID where reference error applies.

No payload may include unbounded arbitrary file contents.
