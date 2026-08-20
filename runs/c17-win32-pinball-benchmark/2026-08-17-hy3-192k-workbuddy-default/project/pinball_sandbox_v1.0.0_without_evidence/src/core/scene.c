#include "scene.h"
#include "hash.h"
#include <stdlib.h>
#include <string.h>

void scene_init(Scene *s) {
  memset(s, 0, sizeof(*s));
  s->objects = NULL; s->obj_count = 0; s->obj_cap = 0;
  s->layers  = NULL; s->layer_count = 0; s->layer_cap = 0;
  s->groups  = NULL; s->group_count = 0; s->group_cap = 0;
  s->events  = NULL; s->event_count = 0; s->event_cap = 0;
  /* sensible defaults */
  s->world_size = v2(1600, 1000);
  s->gravity = v2(0, 980);
  s->max_active_balls = 16;
  s->starting_turns = 3;
  s->default_ball_radius = 12;
  s->default_ball_mass = 1;
  s->default_ball_restitution = 0.78;
  s->default_ball_friction = 0.08;
  s->default_ball_damping = 0.03;
  s->default_ball_max_speed = 3000;
  s->nudge_impulse = 85;
  s->nudge_tilt_cost = 1;
  s->tilt_threshold = 3;
  s->tilt_decay_per_second = 0.75;
  s->nudge_cooldown = 0.08;
}

void scene_free(Scene *s) {
  free(s->objects); free(s->layers); free(s->groups); free(s->events);
  scene_init(s);
}

void scene_clear(Scene *s) { scene_free(s); }

static void *grow(void *base, int *cap, int count, size_t sz, int mincap) {
  if (count < *cap) return base;
  int ncap = *cap ? (*cap * 2) : (mincap ? mincap : 16);
  void *nb = realloc(base, (size_t)ncap * sz);
  if (!nb) return base; /* allocation failure */
  *cap = ncap;
  return nb;
}

int scene_add_object(Scene *s, const Obj *o) {
  s->objects = grow(s->objects, &s->obj_cap, s->obj_count, sizeof(Obj), 64);
  if (s->obj_count >= s->obj_cap) return 0;
  s->objects[s->obj_count] = *o;
  s->objects[s->obj_count].order = s->obj_count;
  s->obj_count++;
  return 1;
}
int scene_add_layer(Scene *s, const Layer *l) {
  s->layers = grow(s->layers, &s->layer_cap, s->layer_count, sizeof(Layer), 8);
  if (s->layer_count >= s->layer_cap) return 0;
  s->layers[s->layer_count++] = *l;
  return 1;
}
int scene_add_group(Scene *s, const Group *g) {
  s->groups = grow(s->groups, &s->group_cap, s->group_count, sizeof(Group), 8);
  if (s->group_count >= s->group_cap) return 0;
  s->groups[s->group_count++] = *g;
  return 1;
}
int scene_add_event(Scene *s, const Event *e) {
  s->events = grow(s->events, &s->event_cap, s->event_count, sizeof(Event), 16);
  if (s->event_count >= s->event_cap) return 0;
  s->events[s->event_count++] = *e;
  return 1;
}

Obj   *scene_find_object(Scene *s, const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < s->obj_count; i++)
    if (strcmp(s->objects[i].id, id) == 0) return &s->objects[i];
  return NULL;
}
Layer *scene_find_layer(Scene *s, const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < s->layer_count; i++)
    if (strcmp(s->layers[i].id, id) == 0) return &s->layers[i];
  return NULL;
}
Group *scene_find_group(Scene *s, const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < s->group_count; i++)
    if (strcmp(s->groups[i].id, id) == 0) return &s->groups[i];
  return NULL;
}
Event *scene_find_event(Scene *s, const char *id) {
  if (!id) return NULL;
  for (int i = 0; i < s->event_count; i++)
    if (strcmp(s->events[i].id, id) == 0) return &s->events[i];
  return NULL;
}

/* ---- deterministic fingerprint ---- */
static void hash_double(uint64_t *h, double v) {
  fnv1a64_update(h, &v, sizeof(v));
}
static void hash_vec(uint64_t *h, Vec2 v) { hash_double(h, v.x); hash_double(h, v.y); }

uint64_t scene_fingerprint(const Scene *s) {
  uint64_t h; fnv1a64_init(&h);
  int f = s->format;
  fnv1a64_update(&h, &f, sizeof(f));
  fnv1a64_update(&h, s->name, strlen(s->name));
  hash_vec(&h, s->world_size);
  hash_vec(&h, s->gravity);
  fnv1a64_update(&h, &s->max_active_balls, sizeof(s->max_active_balls));
  fnv1a64_update(&h, &s->starting_turns, sizeof(s->starting_turns));
  hash_double(&h, s->default_ball_radius);
  hash_double(&h, s->default_ball_mass);
  hash_double(&h, s->default_ball_restitution);
  hash_double(&h, s->default_ball_friction);
  hash_double(&h, s->default_ball_damping);
  hash_double(&h, s->default_ball_max_speed);
  fnv1a64_update(&h, &s->scene_seed, sizeof(s->scene_seed));
  hash_double(&h, s->nudge_impulse);
  hash_double(&h, s->nudge_tilt_cost);
  hash_double(&h, s->tilt_threshold);
  hash_double(&h, s->tilt_decay_per_second);
  hash_double(&h, s->nudge_cooldown);

  /* objects in authored order */
  for (int i = 0; i < s->obj_count; i++) {
    Obj *o = &s->objects[i];
    int t = (int)o->type;
    fnv1a64_update(&h, &t, sizeof(t));
    fnv1a64_update(&h, o->id, strlen(o->id));
    fnv1a64_update(&h, o->layer, strlen(o->layer));
    fnv1a64_update(&h, &o->locked, sizeof(o->locked));
    switch (o->type) {
      case OBJ_BALL_SPAWN:
        hash_vec(&h, o->u.spawn.position);
        hash_vec(&h, o->u.spawn.initial_velocity);
        fnv1a64_update(&h, &o->u.spawn.enabled, 1);
        fnv1a64_update(&h, &o->u.spawn.has_ball_radius, 1);
        hash_double(&h, o->u.spawn.ball_radius);
        break;
      case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
      case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER:
        hash_vec(&h, o->u.cap.start);
        hash_vec(&h, o->u.cap.end);
        hash_double(&h, o->u.cap.thickness);
        hash_double(&h, o->u.cap.restitution);
        hash_double(&h, o->u.cap.friction);
        fnv1a64_update(&h, &o->u.cap.enabled, 1);
        break;
      case OBJ_BUMPER:
        hash_vec(&h, o->u.bumper.center);
        hash_double(&h, o->u.bumper.radius);
        hash_double(&h, o->u.bumper.restitution);
        hash_double(&h, o->u.bumper.friction);
        hash_double(&h, o->u.bumper.impulse);
        fnv1a64_update(&h, &o->u.bumper.base_score, sizeof(o->u.bumper.base_score));
        hash_double(&h, o->u.bumper.cooldown);
        fnv1a64_update(&h, &o->u.bumper.enabled, 1);
        break;
      case OBJ_FLIPPER:
        hash_vec(&h, o->u.flipper.pivot);
        hash_double(&h, o->u.flipper.length);
        hash_double(&h, o->u.flipper.thickness);
        hash_double(&h, o->u.flipper.rest_angle_deg);
        hash_double(&h, o->u.flipper.active_angle_deg);
        hash_double(&h, o->u.flipper.engage_speed_deg_s);
        hash_double(&h, o->u.flipper.return_speed_deg_s);
        hash_double(&h, o->u.flipper.restitution);
        hash_double(&h, o->u.flipper.friction);
        fnv1a64_update(&h, &o->u.flipper.input, 1);
        fnv1a64_update(&h, &o->u.flipper.enabled, 1);
        break;
      case OBJ_SENSOR: case OBJ_DRAIN:
        hash_double(&h, o->u.sensor.x);
        hash_double(&h, o->u.sensor.y);
        hash_double(&h, o->u.sensor.w);
        hash_double(&h, o->u.sensor.h);
        fnv1a64_update(&h, &o->u.sensor.enabled, 1);
        fnv1a64_update(&h, &o->u.sensor.debug_visible, 1);
        break;
      case OBJ_LAUNCHER:
        hash_vec(&h, o->u.launcher.position);
        fnv1a64_update(&h, o->u.launcher.spawn_id, strlen(o->u.launcher.spawn_id));
        hash_vec(&h, o->u.launcher.direction);
        hash_double(&h, o->u.launcher.min_speed);
        hash_double(&h, o->u.launcher.max_speed);
        hash_double(&h, o->u.launcher.full_charge_time);
        fnv1a64_update(&h, &o->u.launcher.charge_curve, 1);
        fnv1a64_update(&h, &o->u.launcher.enabled, 1);
        break;
      case OBJ_SPINNER:
        hash_vec(&h, o->u.spinner.pivot);
        hash_double(&h, o->u.spinner.half_length);
        hash_double(&h, o->u.spinner.thickness);
        hash_double(&h, o->u.spinner.rest_angle_deg);
        hash_double(&h, o->u.spinner.angular_damping);
        hash_double(&h, o->u.spinner.inertia);
        hash_double(&h, o->u.spinner.restitution);
        hash_double(&h, o->u.spinner.friction);
        hash_double(&h, o->u.spinner.score_per_tick);
        hash_double(&h, o->u.spinner.tick_angle_deg);
        fnv1a64_update(&h, &o->u.spinner.enabled, 1);
        break;
      case OBJ_KICKOUT:
        hash_vec(&h, o->u.kickout.center);
        hash_double(&h, o->u.kickout.capture_radius);
        hash_vec(&h, o->u.kickout.eject_direction);
        hash_double(&h, o->u.kickout.eject_speed);
        hash_double(&h, o->u.kickout.hold_time);
        fnv1a64_update(&h, &o->u.kickout.base_score, sizeof(o->u.kickout.base_score));
        fnv1a64_update(&h, &o->u.kickout.enabled, 1);
        break;
      default: break;
    }
  }

  /* layers sorted by order for canonical output */
  for (int k = 0; k < s->layer_count; k++) {
    int best = -1;
    for (int i = 0; i < s->layer_count; i++)
      if (s->layers[i].order == k) { best = i; break; }
    if (best < 0) continue;
    Layer *l = &s->layers[best];
    fnv1a64_update(&h, l->id, strlen(l->id));
    fnv1a64_update(&h, l->name, strlen(l->name));
    fnv1a64_update(&h, &l->visible, 1);
    fnv1a64_update(&h, &l->locked, 1);
    fnv1a64_update(&h, &l->order, sizeof(l->order));
  }

  for (int i = 0; i < s->group_count; i++) {
    Group *g = &s->groups[i];
    fnv1a64_update(&h, g->id, strlen(g->id));
    fnv1a64_update(&h, g->name, strlen(g->name));
    hash_vec(&h, g->pivot);
    for (int m = 0; m < g->member_count; m++)
      fnv1a64_update(&h, g->members[m], strlen(g->members[m]));
  }

  for (int i = 0; i < s->event_count; i++) {
    Event *e = &s->events[i];
    fnv1a64_update(&h, e->id, strlen(e->id));
    fnv1a64_update(&h, e->source, strlen(e->source));
    int tr = (int)e->trigger;
    fnv1a64_update(&h, &tr, sizeof(tr));
    fnv1a64_update(&h, &e->action_count, sizeof(e->action_count));
    for (int a = 0; a < e->action_count; a++) {
      Action *ac = &e->actions[a];
      int at = (int)ac->type;
      fnv1a64_update(&h, &at, sizeof(at));
      fnv1a64_update(&h, &ac->amount, sizeof(ac->amount));
      fnv1a64_update(&h, ac->spawn, strlen(ac->spawn));
      fnv1a64_update(&h, &ac->count, sizeof(ac->count));
      fnv1a64_update(&h, ac->target, strlen(ac->target));
      fnv1a64_update(&h, &ac->multiplier, sizeof(ac->multiplier));
      hash_double(&h, ac->duration);
      fnv1a64_update(&h, &ac->dropped, 1);
    }
  }
  return fnv1a64_final(h);
}

void scene_clone_into(const Scene *src, Scene *dst) {
  scene_free(dst);
  *dst = *src;
  dst->objects = NULL; dst->obj_cap = 0;
  dst->layers = NULL;  dst->layer_cap = 0;
  dst->groups = NULL;  dst->group_cap = 0;
  dst->events = NULL;  dst->event_cap = 0;
  for (int i = 0; i < src->obj_count; i++) scene_add_object(dst, &src->objects[i]);
  for (int i = 0; i < src->layer_count; i++) scene_add_layer(dst, &src->layers[i]);
  for (int i = 0; i < src->group_count; i++) scene_add_group(dst, &src->groups[i]);
  for (int i = 0; i < src->event_count; i++) scene_add_event(dst, &src->events[i]);
}

void scene_move(Scene *dst, Scene *src) {
  scene_free(dst);
  *dst = *src;
  /* steal buffers */
  src->objects = NULL; src->obj_count = 0; src->obj_cap = 0;
  src->layers = NULL;  src->layer_count = 0; src->layer_cap = 0;
  src->groups = NULL;  src->group_count = 0; src->group_cap = 0;
  src->events = NULL;  src->event_count = 0; src->event_cap = 0;
}

/* ------------------------------------------------------------------ */
/* Geometry helpers (object centroid in world space).                  */
double obj_center_x(const Obj *o) {
  switch (o->type) {
    case OBJ_BUMPER:      return o->u.bumper.center.x;
    case OBJ_SPINNER:     return o->u.spinner.pivot.x;
    case OBJ_FLIPPER:     return o->u.flipper.pivot.x;
    case OBJ_KICKOUT:     return o->u.kickout.center.x;
    case OBJ_SENSOR: case OBJ_DRAIN: return o->u.sensor.x + o->u.sensor.w * 0.5;
    case OBJ_BALL_SPAWN:  return o->u.spawn.position.x;
    case OBJ_LAUNCHER:    return o->u.launcher.position.x;
    default:              return (o->u.cap.start.x + o->u.cap.end.x) * 0.5;
  }
}
double obj_center_y(const Obj *o) {
  switch (o->type) {
    case OBJ_BUMPER:      return o->u.bumper.center.y;
    case OBJ_SPINNER:     return o->u.spinner.pivot.y;
    case OBJ_FLIPPER:     return o->u.flipper.pivot.y;
    case OBJ_KICKOUT:     return o->u.kickout.center.y;
    case OBJ_SENSOR: case OBJ_DRAIN: return o->u.sensor.y + o->u.sensor.h * 0.5;
    case OBJ_BALL_SPAWN:  return o->u.spawn.position.y;
    case OBJ_LAUNCHER:    return o->u.launcher.position.y;
    default:              return (o->u.cap.start.y + o->u.cap.end.y) * 0.5;
  }
}

/* Remove the object at the given index, shifting the rest down. */
void scene_remove_object_at(Scene *s, int idx) {
  if (idx < 0 || idx >= s->obj_count) return;
  for (int i = idx; i < s->obj_count - 1; i++) s->objects[i] = s->objects[i + 1];
  s->obj_count--;
}
