#include "types.h"
#include <string.h>
#include <stdlib.h>

static const char *g_code_names[PBT_ERR_COUNT + 1];

static void init_names(void) {
  static int done = 0;
  if (done) return;
  done = 1;
  g_code_names[PBT_OK]                       = "PBT_OK";
  g_code_names[PBT_E_EMPTY]                  = "PBT_E_EMPTY";
  g_code_names[PBT_E_HEADER]                 = "PBT_E_HEADER";
  g_code_names[PBT_E_VERSION_UNSUPPORTED]    = "PBT_E_VERSION_UNSUPPORTED";
  g_code_names[PBT_E_UTF8]                   = "PBT_E_UTF8";
  g_code_names[PBT_E_NUL]                    = "PBT_E_NUL";
  g_code_names[PBT_E_LINE_TOO_LONG]          = "PBT_E_LINE_TOO_LONG";
  g_code_names[PBT_E_TOKEN_TOO_LONG]         = "PBT_E_TOKEN_TOO_LONG";
  g_code_names[PBT_E_SYNTAX]                 = "PBT_E_SYNTAX";
  g_code_names[PBT_E_DUP_KEY]                = "PBT_E_DUP_KEY";
  g_code_names[PBT_E_DUP_ID]                 = "PBT_E_DUP_ID";
  g_code_names[PBT_E_UNKNOWN_SECTION]         = "PBT_E_UNKNOWN_SECTION";
  g_code_names[PBT_E_UNKNOWN_OBJECT]         = "PBT_E_UNKNOWN_OBJECT";
  g_code_names[PBT_E_UNKNOWN_ACTION]         = "PBT_E_UNKNOWN_ACTION";
  g_code_names[PBT_E_MISSING_FIELD]          = "PBT_E_MISSING_FIELD";
  g_code_names[PBT_E_TYPE]                   = "PBT_E_TYPE";
  g_code_names[PBT_E_NONFINITE]              = "PBT_E_NONFINITE";
  g_code_names[PBT_E_NUMERIC_RANGE]           = "PBT_E_NUMERIC_RANGE";
  g_code_names[PBT_E_NUMERIC_OVERFLOW]        = "PBT_E_NUMERIC_OVERFLOW";
  g_code_names[PBT_E_STRING_ESCAPE]          = "PBT_E_STRING_ESCAPE";
  g_code_names[PBT_E_STRING_UNTERMINATED]     = "PBT_E_STRING_UNTERMINATED";
  g_code_names[PBT_E_ACTION_INDEX]           = "PBT_E_ACTION_INDEX";
  g_code_names[PBT_E_REFERENCE_MISSING]       = "PBT_E_REFERENCE_MISSING";
  g_code_names[PBT_E_REFERENCE_TYPE]          = "PBT_E_REFERENCE_TYPE";
  g_code_names[PBT_E_GROUP_NESTING]          = "PBT_E_GROUP_NESTING";
  g_code_names[PBT_E_GROUP_MEMBERSHIP]        = "PBT_E_GROUP_MEMBERSHIP";
  g_code_names[PBT_E_LAYER]                  = "PBT_E_LAYER";
  g_code_names[PBT_E_LIMIT_FILE]             = "PBT_E_LIMIT_FILE";
  g_code_names[PBT_E_LIMIT_OBJECTS]           = "PBT_E_LIMIT_OBJECTS";
  g_code_names[PBT_E_LIMIT_EVENTS]           = "PBT_E_LIMIT_EVENTS";
  g_code_names[PBT_E_LIMIT_ACTIONS]          = "PBT_E_LIMIT_ACTIONS";
  g_code_names[PBT_E_LIMIT_GROUPS]           = "PBT_E_LIMIT_GROUPS";
  g_code_names[PBT_E_LIMIT_LAYERS]           = "PBT_E_LIMIT_LAYERS";
  g_code_names[PBT_E_ALLOCATION]             = "PBT_E_ALLOCATION";

  g_code_names[VAL_E_NO_SPAWN]               = "VAL_E_NO_SPAWN";
  g_code_names[VAL_E_NO_DRAIN]               = "VAL_E_NO_DRAIN";
  g_code_names[VAL_E_DEGENERATE_GEOMETRY]    = "VAL_E_DEGENERATE_GEOMETRY";
  g_code_names[VAL_E_SPAWN_BLOCKED_STATIC]   = "VAL_E_SPAWN_BLOCKED_STATIC";
  g_code_names[VAL_E_LAUNCHER_OWNERSHIP]     = "VAL_E_LAUNCHER_OWNERSHIP";
  g_code_names[VAL_E_TARGET_RESET]           = "VAL_E_TARGET_RESET";
  g_code_names[VAL_E_SPINNER]                = "VAL_E_SPINNER";
  g_code_names[VAL_E_KICKOUT]                = "VAL_E_KICKOUT";
  g_code_names[VAL_E_EVENT_CYCLE_STATIC]     = "VAL_E_EVENT_CYCLE_STATIC";
  g_code_names[VAL_E_WORLD_BOUNDS]           = "VAL_E_WORLD_BOUNDS";
  g_code_names[VAL_W_OUTSIDE_WORLD]          = "VAL_W_OUTSIDE_WORLD";
  g_code_names[VAL_W_ZERO_SCORE]             = "VAL_W_ZERO_SCORE";

  g_code_names[IO_E_TEMP_CREATE]             = "IO_E_TEMP_CREATE";
  g_code_names[IO_E_WRITE]                   = "IO_E_WRITE";
  g_code_names[IO_E_FLUSH]                   = "IO_E_FLUSH";
  g_code_names[IO_E_CLOSE]                   = "IO_E_CLOSE";
  g_code_names[IO_E_RENAME]                  = "IO_E_RENAME";
  g_code_names[IO_E_NO_SPACE]                = "IO_E_NO_SPACE";
  g_code_names[IO_E_EXTERNAL_CHANGED]        = "IO_E_EXTERNAL_CHANGED";
  g_code_names[IO_E_EXTERNAL_DELETED]        = "IO_E_EXTERNAL_DELETED";
  g_code_names[IO_E_RECOVERY_CORRUPT]        = "IO_E_RECOVERY_CORRUPT";
  g_code_names[IO_E_RECOVERY_WRITE]          = "IO_E_RECOVERY_WRITE";

  g_code_names[RT_E_EVENT_BUDGET]            = "RT_E_EVENT_BUDGET";
  g_code_names[RT_E_NONFINITE_STATE]         = "RT_E_NONFINITE_STATE";
  g_code_names[RT_E_BALL_OUT_OF_WORLD]       = "RT_E_BALL_OUT_OF_WORLD";
  g_code_names[RT_E_KICKOUT_EJECT_BLOCKED]   = "RT_E_KICKOUT_EJECT_BLOCKED";
  g_code_names[RT_E_IMPACT_BUDGET]           = "RT_E_IMPACT_BUDGET";
  g_code_names[RT_E_SPAWN_BLOCKED]           = "RT_E_SPAWN_BLOCKED";
  g_code_names[RT_E_ACTIVE_BALL_LIMIT]       = "RT_E_ACTIVE_BALL_LIMIT";

  g_code_names[RPL_E_FORMAT]                 = "RPL_E_FORMAT";
  g_code_names[RPL_E_SCENE_MISMATCH]         = "RPL_E_SCENE_MISMATCH";
  g_code_names[RPL_E_VERSION]                = "RPL_E_VERSION";
  g_code_names[RPL_E_DIVERGENCE]             = "RPL_E_DIVERGENCE";
  g_code_names[DET_E_DIVERGENCE]             = "DET_E_DIVERGENCE";
  g_code_names[REL_E_SCHEMA]                 = "REL_E_SCHEMA";
  g_code_names[REL_E_REQUIREMENT_MISSING]    = "REL_E_REQUIREMENT_MISSING";
  g_code_names[REL_E_REQUIREMENT_DUP]        = "REL_E_REQUIREMENT_DUP";
  g_code_names[REL_E_EVIDENCE_MISSING]       = "REL_E_EVIDENCE_MISSING";
  g_code_names[REL_E_GATE_CONTRADICTION]     = "REL_E_GATE_CONTRADICTION";
  g_code_names[REL_E_VERSION_CONTRADICTION]  = "REL_E_VERSION_CONTRADICTION";
}

const char *pbt_code_name(PbtCode c) {
  init_names();
  if (c < 0 || c >= PBT_ERR_COUNT) return "PBT_E_UNKNOWN_CODE";
  return g_code_names[c] ? g_code_names[c] : "PBT_E_UNKNOWN_CODE";
}

int pbt_code_is_error(PbtCode c) {
  return c != PBT_OK;
}
int pbt_code_is_warning(PbtCode c) {
  return c == VAL_W_OUTSIDE_WORLD || c == VAL_W_ZERO_SCORE;
}

/* ---- object types ---- */
const char *obj_type_name(ObjType t) {
  switch (t) {
    case OBJ_BALL_SPAWN:    return "BALL_SPAWN";
    case OBJ_WALL:          return "WALL";
    case OBJ_RAMP:          return "RAMP";
    case OBJ_BUMPER:        return "BUMPER";
    case OBJ_FLIPPER:       return "FLIPPER";
    case OBJ_SENSOR:        return "SENSOR";
    case OBJ_DRAIN:         return "DRAIN";
    case OBJ_LAUNCHER:      return "LAUNCHER";
    case OBJ_ONE_WAY_GATE:  return "ONE_WAY_GATE";
    case OBJ_SLINGSHOT:     return "SLINGSHOT";
    case OBJ_DROP_TARGET:   return "DROP_TARGET";
    case OBJ_STANDUP_TARGET:return "STANDUP_TARGET";
    case OBJ_ROLLOVER:      return "ROLLOVER";
    case OBJ_SPINNER:       return "SPINNER";
    case OBJ_KICKOUT:       return "KICKOUT";
    default:                return "UNKNOWN";
  }
}
ObjType obj_type_from_name(const char *s) {
  for (int i = 0; i < OBJ_COUNT; i++)
    if (strcmp(s, obj_type_name((ObjType)i)) == 0) return (ObjType)i;
  return OBJ_COUNT;
}

/* Lowercase id prefix for newly created objects (editor). */
const char *obj_type_prefix(ObjType t) {
  switch (t) {
    case OBJ_BALL_SPAWN:    return "spawn";
    case OBJ_WALL:          return "wall";
    case OBJ_RAMP:          return "ramp";
    case OBJ_BUMPER:        return "bumper";
    case OBJ_FLIPPER:       return "flipper";
    case OBJ_SENSOR:        return "sensor";
    case OBJ_DRAIN:         return "drain";
    case OBJ_LAUNCHER:      return "launcher";
    case OBJ_ONE_WAY_GATE:  return "gate";
    case OBJ_SLINGSHOT:     return "sling";
    case OBJ_DROP_TARGET:   return "target";
    case OBJ_STANDUP_TARGET:return "standup";
    case OBJ_ROLLOVER:      return "rollover";
    case OBJ_SPINNER:       return "spinner";
    case OBJ_KICKOUT:       return "kickout";
    default:                return "obj";
  }
}

/* ---- triggers ---- */
const char *trigger_name(TriggerType t) {
  switch (t) {
    case TRIG_SENSOR_ENTER:   return "SENSOR_ENTER";
    case TRIG_SENSOR_STAY:    return "SENSOR_STAY";
    case TRIG_SENSOR_EXIT:    return "SENSOR_EXIT";
    case TRIG_BUMPER_HIT:     return "BUMPER_HIT";
    case TRIG_SLINGSHOT_HIT:  return "SLINGSHOT_HIT";
    case TRIG_BALL_DRAINED:   return "BALL_DRAINED";
    case TRIG_TARGET_HIT:     return "TARGET_HIT";
    case TRIG_TARGET_DROPPED: return "TARGET_DROPPED";
    case TRIG_ROLLOVER_ENTER: return "ROLLOVER_ENTER";
    case TRIG_SPINNER_TICK:   return "SPINNER_TICK";
    case TRIG_KICKOUT_CAPTURE:return "KICKOUT_CAPTURE";
    case TRIG_KICKOUT_EJECT:  return "KICKOUT_EJECT";
    case TRIG_TILT_STARTED:   return "TILT_STARTED";
    case TRIG_TILT_CLEARED:   return "TILT_CLEARED";
    default:                  return "UNKNOWN";
  }
}
TriggerType trigger_from_name(const char *s) {
  /* legacy v1 alias (doc 06.42) */
  if (strcmp(s, "SENSOR_LEAVE") == 0) return TRIG_SENSOR_EXIT;
  for (int i = 0; i < TRIG_COUNT; i++)
    if (strcmp(s, trigger_name((TriggerType)i)) == 0) return (TriggerType)i;
  return TRIG_COUNT;
}

/* ---- actions ---- */
const char *action_name(ActionType a) {
  switch (a) {
    case ACT_ADD_SCORE:             return "ADD_SCORE";
    case ACT_SPAWN_BALL:            return "SPAWN_BALL";
    case ACT_ENABLE_OBJECT:         return "ENABLE_OBJECT";
    case ACT_DISABLE_OBJECT:        return "DISABLE_OBJECT";
    case ACT_START_MULTIBALL:       return "START_MULTIBALL";
    case ACT_SET_MULTIPLIER_OVERRIDE: return "SET_MULTIPLIER_OVERRIDE";
    case ACT_OPEN_GATE:             return "OPEN_GATE";
    case ACT_LIGHT_INDICATOR:       return "LIGHT_INDICATOR";
    case ACT_RESET_TARGET:          return "RESET_TARGET";
    case ACT_SET_TARGET_DROPPED:    return "SET_TARGET_DROPPED";
    case ACT_EJECT_KICKOUT:         return "EJECT_KICKOUT";
    case ACT_CLEAR_TILT:            return "CLEAR_TILT";
    default:                       return "UNKNOWN";
  }
}
ActionType action_from_name(const char *s) {
  for (int i = 0; i < ACT_COUNT; i++)
    if (strcmp(s, action_name((ActionType)i)) == 0) return (ActionType)i;
  return ACT_COUNT;
}

/* ---- diagnostics ---- */
void diag_list_init(DiagList *d) {
  d->items = NULL; d->count = 0; d->cap = 0;
}
void diag_list_free(DiagList *d) {
  free(d->items); d->items = NULL; d->count = 0; d->cap = 0;
}
static void safe_copy(char *dst, size_t n, const char *src) {
  if (!src) { dst[0] = 0; return; }
  strncpy(dst, src, n - 1); dst[n - 1] = 0;
}
void diag_push(DiagList *d, Diag di) {
  if (d->count == d->cap) {
    size_t ncap = d->cap ? d->cap * 2 : 16;
    Diag *ni = (Diag*)realloc(d->items, ncap * sizeof(Diag));
    if (!ni) return; /* allocation failure: drop (transient) */
    d->items = ni; d->cap = ncap;
  }
  d->items[d->count++] = di;
}
void diag_push_code(DiagList *d, PbtCode code, int line, const char *msg) {
  Diag di; memset(&di, 0, sizeof(di));
  di.code = code;
  di.severity = pbt_code_is_warning(code) ? SEV_WARNING : SEV_ERROR;
  di.line = line;
  safe_copy(di.message, sizeof(di.message), msg ? msg : pbt_code_name(code));
  diag_push(d, di);
}
