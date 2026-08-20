#ifndef PB_SCENE_H
#define PB_SCENE_H

#include "types.h"
#include "vec.h"

#define PB_ID_MAX      64
#define PB_NAME_MAX    256
#define PB_MAX_ACTIONS 256
#define PB_MAX_MEMBERS 256

typedef struct {
  Vec2 position;
  Vec2 initial_velocity;
  int  enabled;
  int  has_ball_radius;
  double ball_radius;
} SpawnObj;

typedef struct {
  Vec2 start, end;
  double thickness, restitution, friction;
  int enabled;
  /* per-type extras (harmless for plain WALL/RAMP) */
  Vec2 allowed_direction;   /* ONE_WAY_GATE */
  double impulse;           /* SLINGSHOT */
  long base_score;          /* SLINGSHOT / DROP / STANDUP / KICKOUT-ish */
  double cooldown;          /* SLINGSHOT / DROP / STANDUP */
  double width;             /* ROLLOVER width (capsule radius = width/2) */
  double min_hit_speed;     /* DROP / STANDUP */
  int reset_mode;           /* DROP_TARGET: 0 MANUAL_EVENT,1 AFTER_DELAY,2 ON_NEW_BALL */
  double reset_delay;       /* DROP_TARGET AFTER_DELAY */
  int initially_raised;     /* DROP_TARGET */
} CapsuleObj; /* WALL, RAMP, SLINGSHOT, ONE_WAY_GATE, DROP_TARGET, STANDUP_TARGET, ROLLOVER(width) */

typedef struct {
  Vec2 center;
  double radius, restitution, friction, impulse;
  long base_score;
  double cooldown;
  int enabled;
} BumperObj;

typedef struct {
  Vec2 pivot;
  double length, thickness;
  double rest_angle_deg, active_angle_deg, engage_speed_deg_s, return_speed_deg_s;
  double restitution, friction;
  int input; /* 0 = LEFT_FLIPPER, 1 = RIGHT_FLIPPER */
  int enabled;
} FlipperObj;

typedef struct {
  double x, y, w, h;
  int enabled;
  int debug_visible;
} SensorObj; /* SENSOR, DRAIN */

typedef struct {
  Vec2 position;
  char spawn_id[PB_ID_MAX];
  Vec2 direction;
  double min_speed, max_speed, full_charge_time;
  int charge_curve; /* 0 = LINEAR */
  int enabled;
} LauncherObj;

typedef struct {
  Vec2 pivot;
  double half_length, thickness, rest_angle_deg, angular_damping, inertia;
  double restitution, friction, score_per_tick, tick_angle_deg;
  int enabled;
} SpinnerObj;

typedef struct {
  Vec2 center;
  double capture_radius;
  Vec2 eject_direction;
  double eject_speed, hold_time;
  long base_score;
  int enabled;
} KickoutObj;

typedef struct {
  ObjType type;
  char id[PB_ID_MAX];
  char layer[PB_ID_MAX];
  int  locked;
  int  order;            /* authored stable order index */
  union {
    SpawnObj   spawn;
    CapsuleObj cap;
    BumperObj  bumper;
    FlipperObj flipper;
    SensorObj  sensor;
    LauncherObj launcher;
    SpinnerObj spinner;
    KickoutObj kickout;
  } u;
} Obj;

typedef struct {
  char id[PB_ID_MAX];
  char name[PB_NAME_MAX];
  int  visible;
  int  locked;
  int  order;
} Layer;

typedef struct {
  char id[PB_ID_MAX];
  char name[PB_NAME_MAX];
  Vec2 pivot;
  int  member_count;
  char members[PB_MAX_MEMBERS][PB_ID_MAX];
} Group;

typedef struct {
  ActionType type;
  long   amount;
  char   spawn[PB_ID_MAX];
  int    count;
  char   target[PB_ID_MAX];
  int    multiplier;
  double duration;
  int    dropped;
} Action;

typedef struct {
  char id[PB_ID_MAX];
  char source[PB_ID_MAX];
  TriggerType trigger;
  int  action_count;
  Action actions[PB_MAX_ACTIONS];
} Event;

typedef struct {
  int   format; /* 1 or 2 */
  char  name[PB_NAME_MAX];
  Vec2  world_size;
  Vec2  gravity;
  int   max_active_balls;
  int   starting_turns;
  double default_ball_radius, default_ball_mass, default_ball_restitution;
  double default_ball_friction, default_ball_damping, default_ball_max_speed;
  uint64_t scene_seed;
  double nudge_impulse, nudge_tilt_cost, tilt_threshold, tilt_decay_per_second, nudge_cooldown;
  int   has_seed;

  Obj   *objects; int obj_count, obj_cap;
  Layer *layers;  int layer_count, layer_cap;
  Group *groups;  int group_count, group_cap;
  Event *events;  int event_count, event_cap;
} Scene;

void  scene_init(Scene *s);
void  scene_free(Scene *s);
void  scene_clear(Scene *s);          /* reset to empty, keeps nothing */
int   scene_add_object(Scene *s, const Obj *o);
int   scene_add_layer(Scene *s, const Layer *l);
int   scene_add_group(Scene *s, const Group *g);
int   scene_add_event(Scene *s, const Event *e);

Obj  *scene_find_object(Scene *s, const char *id);
Layer*scene_find_layer(Scene *s, const char *id);
Group*scene_find_group(Scene *s, const char *id);
Event*scene_find_event(Scene *s, const char *id);

/* Object centroid helpers (world space). */
double obj_center_x(const Obj *o);
double obj_center_y(const Obj *o);
/* Remove object by index (shifts remaining down). */
void   scene_remove_object_at(Scene *s, int idx);

/* Deterministic fingerprint over canonical authored content (doc 07.25 / 17.15). */
uint64_t scene_fingerprint(const Scene *s);

/* Deep clone used for transactional load (doc 06.28). */
void scene_clone_into(const Scene *src, Scene *dst);
void scene_move(Scene *dst, Scene *src); /* dst takes src's contents, src emptied */

#endif /* PB_SCENE_H */
