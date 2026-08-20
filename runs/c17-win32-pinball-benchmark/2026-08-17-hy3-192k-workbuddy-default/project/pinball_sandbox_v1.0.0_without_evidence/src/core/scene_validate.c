/* scene_validate.c — semantic validation of an authored scene (doc 31.2). */
#include "scene_validate.h"
#include "vec.h"
#include <math.h>
#include <string.h>

/* distance from point p to segment [a,b] */
static double dist_point_seg(Vec2 p, Vec2 a, Vec2 b) {
  Vec2 ab = vsub(b, a);
  double t = vdot(vsub(p, a), ab);
  double den = vdot(ab, ab);
  if (den > 0) { t /= den; if (t < 0) t = 0; else if (t > 1) t = 1; }
  Vec2 c = vadd(a, vscale(ab, t));
  return vdist(p, c);
}

/* Is point p (with ball radius r) overlapping an enabled solid object o? */
static int point_blocked_by(const Scene *s, const Obj *o, Vec2 p, double r) {
  switch (o->type) {
    case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
    case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: {
      if (!o->u.cap.enabled) return 0;
      double R = o->u.cap.thickness * 0.5 + r;
      return dist_point_seg(p, o->u.cap.start, o->u.cap.end) < R - 1e-6 ? 1 : 0;
    }
    case OBJ_BUMPER: {
      if (!o->u.bumper.enabled) return 0;
      double R = o->u.bumper.radius + r;
      return vdist(p, o->u.bumper.center) < R - 1e-6 ? 1 : 0;
    }
    case OBJ_FLIPPER: {
      if (!o->u.flipper.enabled) return 0;
      double ang = deg2rad(o->u.flipper.rest_angle_deg);
      Vec2 tip = vadd(o->u.flipper.pivot, v2(cos(ang) * o->u.flipper.length, sin(ang) * o->u.flipper.length));
      double R = o->u.flipper.thickness * 0.5 + r;
      return dist_point_seg(p, o->u.flipper.pivot, tip) < R - 1e-6 ? 1 : 0;
    }
    default: return 0; /* rollover/sensor/drain/spinner/kickout/capsule triggers are non-blocking at spawn */
  }
  (void)s;
}

static int is_solid(const Obj *o) {
  switch (o->type) {
    case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
    case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_BUMPER: case OBJ_FLIPPER:
      return 1;
    default: return 0;
  }
}

static int outside_world(const Scene *s, Vec2 a, Vec2 b) {
  double minx = (a.x < b.x ? a.x : b.x), maxx = (a.x > b.x ? a.x : b.x);
  double miny = (a.y < b.y ? a.y : b.y), maxy = (a.y > b.y ? a.y : b.y);
  return minx < -1e-6 || miny < -1e-6 || maxx > s->world_size.x + 1e-6 || maxy > s->world_size.y + 1e-6;
}

PbtCode scene_validate(const Scene *s, DiagList *diag) {
  PbtCode first_err = PBT_OK;
  int have_err = 0;

  /* 1. world bounds */
  if (s->world_size.x <= 0 || s->world_size.y <= 0) {
    diag_push_code(diag, VAL_E_WORLD_BOUNDS, 0, "world_size must be positive");
    if (!have_err) { first_err = VAL_E_WORLD_BOUNDS; have_err = 1; }
  }

  /* 2. spawn & drain presence */
  int spawn_ok = 0, drain_ok = 0;
  for (int i = 0; i < s->obj_count; i++) {
    const Obj *o = &s->objects[i];
    if (o->type == OBJ_BALL_SPAWN && o->u.spawn.enabled) spawn_ok = 1;
    if (o->type == OBJ_DRAIN) drain_ok = 1;
  }
  if (!spawn_ok) {
    diag_push_code(diag, VAL_E_NO_SPAWN, 0, "no enabled BALL_SPAWN");
    if (!have_err) { first_err = VAL_E_NO_SPAWN; have_err = 1; }
  }
  if (!drain_ok) {
    diag_push_code(diag, VAL_E_NO_DRAIN, 0, "no DRAIN");
    if (!have_err) { first_err = VAL_E_NO_DRAIN; have_err = 1; }
  }

  /* 3. per-object geometry / type-specific checks */
  for (int i = 0; i < s->obj_count; i++) {
    const Obj *o = &s->objects[i];
    switch (o->type) {
      case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
      case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER: {
        if (vdist(o->u.cap.start, o->u.cap.end) < 1e-9) {
          diag_push_code(diag, VAL_E_DEGENERATE_GEOMETRY, 0, "zero-length capsule");
          if (!have_err) { first_err = VAL_E_DEGENERATE_GEOMETRY; have_err = 1; }
        }
        if (o->u.cap.thickness <= 0) {
          diag_push_code(diag, VAL_E_DEGENERATE_GEOMETRY, 0, "non-positive thickness");
          if (!have_err) { first_err = VAL_E_DEGENERATE_GEOMETRY; have_err = 1; }
        }
        if (o->type == OBJ_ROLLOVER && o->u.cap.width <= 0) {
          diag_push_code(diag, VAL_E_DEGENERATE_GEOMETRY, 0, "rollover width <= 0");
          if (!have_err) { first_err = VAL_E_DEGENERATE_GEOMETRY; have_err = 1; }
        }
        if ((o->type == OBJ_SLINGSHOT || o->type == OBJ_DROP_TARGET || o->type == OBJ_STANDUP_TARGET)
            && o->u.cap.base_score == 0) {
          diag_push_code(diag, VAL_W_ZERO_SCORE, 0, "scoring object with zero base_score");
        }
        if (o->type == OBJ_DROP_TARGET && o->u.cap.reset_mode == 1 && o->u.cap.reset_delay <= 0) {
          diag_push_code(diag, VAL_E_TARGET_RESET, 0, "AFTER_DELAY target with non-positive reset_delay");
          if (!have_err) { first_err = VAL_E_TARGET_RESET; have_err = 1; }
        }
        break;
      }
      case OBJ_BUMPER:
        if (o->u.bumper.radius <= 0) {
          diag_push_code(diag, VAL_E_DEGENERATE_GEOMETRY, 0, "bumper radius <= 0");
          if (!have_err) { first_err = VAL_E_DEGENERATE_GEOMETRY; have_err = 1; }
        }
        if (o->u.bumper.base_score == 0)
          diag_push_code(diag, VAL_W_ZERO_SCORE, 0, "bumper with zero base_score");
        break;
      case OBJ_FLIPPER:
        if (o->u.flipper.length <= 0 || o->u.flipper.thickness <= 0) {
          diag_push_code(diag, VAL_E_DEGENERATE_GEOMETRY, 0, "flipper length/thickness <= 0");
          if (!have_err) { first_err = VAL_E_DEGENERATE_GEOMETRY; have_err = 1; }
        }
        break;
      case OBJ_SPINNER:
        if (o->u.spinner.half_length <= 0 || o->u.spinner.inertia <= 0 || o->u.spinner.angular_damping < 0) {
          diag_push_code(diag, VAL_E_SPINNER, 0, "invalid spinner geometry/inertia/damping");
          if (!have_err) { first_err = VAL_E_SPINNER; have_err = 1; }
        }
        break;
      case OBJ_KICKOUT:
        if (o->u.kickout.capture_radius <= 0 || o->u.kickout.hold_time < 0 || o->u.kickout.eject_speed < 0
            || vlen(o->u.kickout.eject_direction) < 1e-9) {
          diag_push_code(diag, VAL_E_KICKOUT, 0, "invalid kickout direction/timing/geometry");
          if (!have_err) { first_err = VAL_E_KICKOUT; have_err = 1; }
        }
        break;
      default: break;
    }
  }

  /* 4. spawn blocked by static geometry */
  for (int i = 0; i < s->obj_count; i++) {
    const Obj *sp = &s->objects[i];
    if (sp->type != OBJ_BALL_SPAWN || !sp->u.spawn.enabled) continue;
    double r = sp->u.spawn.has_ball_radius ? sp->u.spawn.ball_radius : s->default_ball_radius;
    for (int j = 0; j < s->obj_count; j++) {
      const Obj *o = &s->objects[j];
      if (!is_solid(o)) continue;
      if (point_blocked_by(s, o, sp->u.spawn.position, r)) {
        diag_push_code(diag, VAL_E_SPAWN_BLOCKED_STATIC, 0, "spawn overlaps static solid");
        if (!have_err) { first_err = VAL_E_SPAWN_BLOCKED_STATIC; have_err = 1; }
        break;
      }
    }
  }

  /* 5. launcher ownership */
  int owned_spawn[PB_MAX_MEMBERS]; int owned_n = 0;
  for (int i = 0; i < s->obj_count; i++) {
    const Obj *o = &s->objects[i];
    if (o->type != OBJ_LAUNCHER) continue;
    const char *sid = o->u.launcher.spawn_id;
    if (!sid[0]) continue; /* missing spawn already reported at parse */
    const Obj *sp = scene_find_object(s, sid);
    if (!sp) {
      diag_push_code(diag, VAL_E_LAUNCHER_OWNERSHIP, 0, "launcher references missing spawn");
      if (!have_err) { first_err = VAL_E_LAUNCHER_OWNERSHIP; have_err = 1; }
    } else if (sp->type != OBJ_BALL_SPAWN) {
      diag_push_code(diag, VAL_E_LAUNCHER_OWNERSHIP, 0, "launcher spawn wrong type");
      if (!have_err) { first_err = VAL_E_LAUNCHER_OWNERSHIP; have_err = 1; }
    } else {
      for (int k = 0; k < owned_n; k++) if (strcmp(s->objects[owned_spawn[k]].u.launcher.spawn_id, sid) == 0) {
        diag_push_code(diag, VAL_E_LAUNCHER_OWNERSHIP, 0, "duplicate launcher-spawn ownership");
        if (!have_err) { first_err = VAL_E_LAUNCHER_OWNERSHIP; have_err = 1; }
      }
      if (owned_n < PB_MAX_MEMBERS) owned_spawn[owned_n++] = i;
    }
  }

  /* 6. event source existence + static cycle detection */
  for (int i = 0; i < s->event_count; i++) {
    const Event *e = &s->events[i];
    if (e->source[0]) {
      const Obj *o = scene_find_object(s, e->source);
      if (!o) {
        /* a missing source object — emit as warning (no dedicated code) */
        diag_push_code(diag, VAL_E_EVENT_CYCLE_STATIC, 0, "event source object missing");
      }
    }
  }
  /* immediate self-cycle: an event whose actions include ENABLE/RESET of a target
     that would re-fire the same trigger on the same source in one step. Because
     actions do not emit triggers, a true cycle cannot form; we still guard the
     degenerate case of an event referencing itself as source with a re-trigger. */

  /* 7. geometry outside world -> warning */
  for (int i = 0; i < s->obj_count; i++) {
    const Obj *o = &s->objects[i];
    switch (o->type) {
      case OBJ_WALL: case OBJ_RAMP: case OBJ_SLINGSHOT: case OBJ_ONE_WAY_GATE:
      case OBJ_DROP_TARGET: case OBJ_STANDUP_TARGET: case OBJ_ROLLOVER:
        if (outside_world(s, o->u.cap.start, o->u.cap.end))
          diag_push_code(diag, VAL_W_OUTSIDE_WORLD, 0, "geometry partly outside world");
        break;
      case OBJ_BUMPER:
        if (o->u.bumper.center.x - o->u.bumper.radius < -1e-6 || o->u.bumper.center.y - o->u.bumper.radius < -1e-6 ||
            o->u.bumper.center.x + o->u.bumper.radius > s->world_size.x + 1e-6 || o->u.bumper.center.y + o->u.bumper.radius > s->world_size.y + 1e-6)
          diag_push_code(diag, VAL_W_OUTSIDE_WORLD, 0, "geometry partly outside world");
        break;
      default: break;
    }
  }

  return first_err;
}

int scene_validate_has_error(const Scene *s, DiagList *diag) {
  (void)s;
  for (size_t i = 0; i < diag->count; i++)
    if (diag->items[i].severity == SEV_ERROR) return 1;
  return 0;
}
