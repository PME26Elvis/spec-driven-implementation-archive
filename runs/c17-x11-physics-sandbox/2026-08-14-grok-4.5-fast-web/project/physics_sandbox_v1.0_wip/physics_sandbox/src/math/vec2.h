#ifndef PS_VEC2_H
#define PS_VEC2_H

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef float ps_scalar;
#define PS_SCALAR_EPSILON 1e-6f
#define PS_PI 3.14159265358979323846f

typedef struct ps_vec2 {
    ps_scalar x;
    ps_scalar y;
} ps_vec2;

static inline ps_vec2 ps_v2(ps_scalar x, ps_scalar y) {
    ps_vec2 v = {x, y};
    return v;
}

static inline ps_vec2 ps_v2_zero(void) {
    return ps_v2(0.0f, 0.0f);
}

static inline ps_vec2 ps_v2_add(ps_vec2 a, ps_vec2 b) {
    return ps_v2(a.x + b.x, a.y + b.y);
}

static inline ps_vec2 ps_v2_sub(ps_vec2 a, ps_vec2 b) {
    return ps_v2(a.x - b.x, a.y - b.y);
}

static inline ps_vec2 ps_v2_mul(ps_vec2 a, ps_scalar s) {
    return ps_v2(a.x * s, a.y * s);
}

static inline ps_vec2 ps_v2_div(ps_vec2 a, ps_scalar s) {
    return ps_v2(a.x / s, a.y / s);
}

static inline ps_scalar ps_v2_dot(ps_vec2 a, ps_vec2 b) {
    return a.x * b.x + a.y * b.y;
}

/* 2D cross product returns scalar */
static inline ps_scalar ps_v2_cross(ps_vec2 a, ps_vec2 b) {
    return a.x * b.y - a.y * b.x;
}

/* cross(s, v) = (-s*v.y, s*v.x) */
static inline ps_vec2 ps_v2_cross_s_v(ps_scalar s, ps_vec2 v) {
    return ps_v2(-s * v.y, s * v.x);
}

/* cross(v, s) = (s*v.y, -s*v.x) */
static inline ps_vec2 ps_v2_cross_v_s(ps_vec2 v, ps_scalar s) {
    return ps_v2(s * v.y, -s * v.x);
}

static inline ps_scalar ps_v2_len_sq(ps_vec2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline ps_scalar ps_v2_len(ps_vec2 v) {
    return sqrtf(ps_v2_len_sq(v));
}

static inline ps_vec2 ps_v2_normalize(ps_vec2 v) {
    ps_scalar len = ps_v2_len(v);
    if (len < PS_SCALAR_EPSILON) {
        return ps_v2_zero();
    }
    return ps_v2_div(v, len);
}

static inline ps_vec2 ps_v2_perp(ps_vec2 v) {
    return ps_v2(-v.y, v.x);
}

static inline ps_vec2 ps_v2_min(ps_vec2 a, ps_vec2 b) {
    return ps_v2(fminf(a.x, b.x), fminf(a.y, b.y));
}

static inline ps_vec2 ps_v2_max(ps_vec2 a, ps_vec2 b) {
    return ps_v2(fmaxf(a.x, b.x), fmaxf(a.y, b.y));
}

static inline bool ps_v2_finite(ps_vec2 v) {
    return isfinite(v.x) && isfinite(v.y);
}

static inline bool ps_v2_near_zero(ps_vec2 v) {
    return ps_v2_len_sq(v) < PS_SCALAR_EPSILON * PS_SCALAR_EPSILON;
}

#endif /* PS_VEC2_H */
