#ifndef PB_VEC_H
#define PB_VEC_H

#include <math.h>

typedef struct { double x, y; } Vec2;

static inline Vec2 v2(double x, double y) { Vec2 v; v.x = x; v.y = y; return v; }
static inline Vec2 vadd(Vec2 a, Vec2 b) { return v2(a.x + b.x, a.y + b.y); }
static inline Vec2 vsub(Vec2 a, Vec2 b) { return v2(a.x - b.x, a.y - b.y); }
static inline Vec2 vscale(Vec2 a, double s) { return v2(a.x * s, a.y * s); }
static inline Vec2 vneg(Vec2 a) { return v2(-a.x, -a.y); }
static inline double vdot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
static inline double vcross(Vec2 a, Vec2 b) { return a.x * b.y - a.y * b.x; }
static inline double vlen2(Vec2 a) { return a.x * a.x + a.y * a.y; }
static inline double vlen(Vec2 a) { return sqrt(vlen2(a)); }
static inline double vdist2(Vec2 a, Vec2 b) { return vlen2(vsub(a, b)); }
static inline double vdist(Vec2 a, Vec2 b) { return sqrt(vdist2(a, b)); }

static inline Vec2 vnorm(Vec2 a) {
  double l = vlen(a);
  if (l <= 0.0) return v2(0.0, 0.0);
  return vscale(a, 1.0 / l);
}

/* Closest point on segment [a,b] to point p. */
static inline Vec2 closest_on_segment(Vec2 p, Vec2 a, Vec2 b) {
  Vec2 ab = vsub(b, a);
  double t = vdot(vsub(p, a), ab);
  double denom = vdot(ab, ab);
  if (denom <= 0.0) return a;
  t /= denom;
  if (t < 0.0) t = 0.0;
  else if (t > 1.0) t = 1.0;
  return vadd(a, vscale(ab, t));
}

/* Deterministic sign: returns -1,0,1 */
static inline int sign_d(double x) { return x < 0.0 ? -1 : (x > 0.0 ? 1 : 0); }

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double deg2rad(double d) { return d * M_PI / 180.0; }

#endif /* PB_VEC_H */
