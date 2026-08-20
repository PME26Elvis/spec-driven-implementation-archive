#ifndef PB_TYPES_H
#define PB_TYPES_H

#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Stable error / diagnostic codes (doc 31). Machine-readable names are */
/* required to be stable across runs for identical invalid input.       */
/* ------------------------------------------------------------------ */
typedef enum {
  PBT_OK = 0,
  PBT_E_EMPTY,
  PBT_E_HEADER,
  PBT_E_VERSION_UNSUPPORTED,
  PBT_E_UTF8,
  PBT_E_NUL,
  PBT_E_LINE_TOO_LONG,
  PBT_E_TOKEN_TOO_LONG,
  PBT_E_SYNTAX,
  PBT_E_DUP_KEY,
  PBT_E_DUP_ID,
  PBT_E_UNKNOWN_SECTION,
  PBT_E_UNKNOWN_OBJECT,
  PBT_E_UNKNOWN_ACTION,
  PBT_E_MISSING_FIELD,
  PBT_E_TYPE,
  PBT_E_NONFINITE,
  PBT_E_NUMERIC_RANGE,
  PBT_E_NUMERIC_OVERFLOW,
  PBT_E_STRING_ESCAPE,
  PBT_E_STRING_UNTERMINATED,
  PBT_E_ACTION_INDEX,
  PBT_E_REFERENCE_MISSING,
  PBT_E_REFERENCE_TYPE,
  PBT_E_GROUP_NESTING,
  PBT_E_GROUP_MEMBERSHIP,
  PBT_E_LAYER,
  PBT_E_LIMIT_FILE,
  PBT_E_LIMIT_OBJECTS,
  PBT_E_LIMIT_EVENTS,
  PBT_E_LIMIT_ACTIONS,
  PBT_E_LIMIT_GROUPS,
  PBT_E_LIMIT_LAYERS,
  PBT_E_ALLOCATION,

  /* scene validation (doc 31.2) */
  VAL_E_NO_SPAWN,
  VAL_E_NO_DRAIN,
  VAL_E_DEGENERATE_GEOMETRY,
  VAL_E_SPAWN_BLOCKED_STATIC,
  VAL_E_LAUNCHER_OWNERSHIP,
  VAL_E_TARGET_RESET,
  VAL_E_SPINNER,
  VAL_E_KICKOUT,
  VAL_E_EVENT_CYCLE_STATIC,
  VAL_E_WORLD_BOUNDS,
  VAL_W_OUTSIDE_WORLD,
  VAL_W_ZERO_SCORE,

  /* save / backing-file (doc 31.3) */
  IO_E_TEMP_CREATE,
  IO_E_WRITE,
  IO_E_FLUSH,
  IO_E_CLOSE,
  IO_E_RENAME,
  IO_E_NO_SPACE,
  IO_E_EXTERNAL_CHANGED,
  IO_E_EXTERNAL_DELETED,
  IO_E_RECOVERY_CORRUPT,
  IO_E_RECOVERY_WRITE,

  /* runtime (doc 31.4) */
  RT_E_EVENT_BUDGET,
  RT_E_NONFINITE_STATE,
  RT_E_BALL_OUT_OF_WORLD,
  RT_E_KICKOUT_EJECT_BLOCKED,
  RT_E_IMPACT_BUDGET,
  RT_E_SPAWN_BLOCKED,
  RT_E_ACTIVE_BALL_LIMIT,

  /* replay / verification (doc 31.5) */
  RPL_E_FORMAT,
  RPL_E_SCENE_MISMATCH,
  RPL_E_VERSION,
  RPL_E_DIVERGENCE,
  DET_E_DIVERGENCE,
  REL_E_SCHEMA,
  REL_E_REQUIREMENT_MISSING,
  REL_E_REQUIREMENT_DUP,
  REL_E_EVIDENCE_MISSING,
  REL_E_GATE_CONTRADICTION,
  REL_E_VERSION_CONTRADICTION,

  PBT_ERR_COUNT
} PbtCode;

const char *pbt_code_name(PbtCode c);
int         pbt_code_is_error(PbtCode c);   /* vs warning */
int         pbt_code_is_warning(PbtCode c);

/* ------------------------------------------------------------------ */
/* Object types (doc 06.11) — 15 required types.                      */
/* ------------------------------------------------------------------ */
typedef enum {
  OBJ_BALL_SPAWN = 0,
  OBJ_WALL,
  OBJ_RAMP,
  OBJ_BUMPER,
  OBJ_FLIPPER,
  OBJ_SENSOR,
  OBJ_DRAIN,
  OBJ_LAUNCHER,
  OBJ_ONE_WAY_GATE,
  OBJ_SLINGSHOT,
  OBJ_DROP_TARGET,
  OBJ_STANDUP_TARGET,
  OBJ_ROLLOVER,
  OBJ_SPINNER,
  OBJ_KICKOUT,
  OBJ_COUNT
} ObjType;

const char *obj_type_name(ObjType t);
ObjType     obj_type_from_name(const char *s); /* OBJ_COUNT if unknown */
const char *obj_type_prefix(ObjType t);        /* lowercase id prefix for editor */

/* ------------------------------------------------------------------ */
/* Event trigger / action tokens (doc 06.42 / 06.43).                  */
/* ------------------------------------------------------------------ */
typedef enum {
  TRIG_SENSOR_ENTER, TRIG_SENSOR_STAY, TRIG_SENSOR_EXIT,
  TRIG_BUMPER_HIT, TRIG_SLINGSHOT_HIT, TRIG_BALL_DRAINED,
  TRIG_TARGET_HIT, TRIG_TARGET_DROPPED,
  TRIG_ROLLOVER_ENTER, TRIG_SPINNER_TICK,
  TRIG_KICKOUT_CAPTURE, TRIG_KICKOUT_EJECT,
  TRIG_TILT_STARTED, TRIG_TILT_CLEARED,
  TRIG_COUNT
} TriggerType;

const char *trigger_name(TriggerType t);
TriggerType trigger_from_name(const char *s);

typedef enum {
  ACT_ADD_SCORE, ACT_SPAWN_BALL, ACT_ENABLE_OBJECT, ACT_DISABLE_OBJECT,
  ACT_START_MULTIBALL, ACT_SET_MULTIPLIER_OVERRIDE, ACT_OPEN_GATE,
  ACT_LIGHT_INDICATOR, ACT_RESET_TARGET, ACT_SET_TARGET_DROPPED,
  ACT_EJECT_KICKOUT, ACT_CLEAR_TILT,
  ACT_COUNT
} ActionType;

const char *action_name(ActionType a);
ActionType  action_from_name(const char *s);

/* ------------------------------------------------------------------ */
/* Generic diagnostic payload (doc 31.7). One struct, bounded fields.  */
/* ------------------------------------------------------------------ */
#define PB_DIAG_MSG_MAX   256
#define PB_DIAG_TOKEN_MAX  64

typedef enum { SEV_ERROR = 0, SEV_WARNING = 1, SEV_INFO = 2 } Severity;

typedef struct {
  PbtCode   code;
  Severity  severity;
  int       line;        /* 1-based; 0 if unknown */
  int       column;      /* 1-based; 0 if unknown */
  char      message[PB_DIAG_MSG_MAX];
  char      section_id[PB_DIAG_TOKEN_MAX];
  char      object_id[PB_DIAG_TOKEN_MAX];
  char      event_id[PB_DIAG_TOKEN_MAX];
  char      field[PB_DIAG_TOKEN_MAX];
  char      token[PB_DIAG_TOKEN_MAX];   /* bounded excerpt */
  char      related_id[PB_DIAG_TOKEN_MAX];
  char      path[PB_DIAG_MSG_MAX];
} Diag;

typedef struct {
  Diag   *items;
  size_t  count;
  size_t  cap;
} DiagList;

void diag_list_init(DiagList *d);
void diag_list_free(DiagList *d);
void diag_push(DiagList *d, Diag di);
void diag_push_code(DiagList *d, PbtCode code, int line, const char *msg);

/* Math scalar type is double (doc 15.1, doc 17.3). */
typedef double real;

#endif /* PB_TYPES_H */
