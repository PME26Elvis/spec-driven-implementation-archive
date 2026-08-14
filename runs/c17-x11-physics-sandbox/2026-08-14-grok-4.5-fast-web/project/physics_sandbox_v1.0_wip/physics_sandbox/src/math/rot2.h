#ifndef PS_ROT2_H
#define PS_ROT2_H

#include "vec2.h"

typedef struct ps_rot2 {
    ps_scalar s; /* sin */
    ps_scalar c; /* cos */
} ps_rot2;

static inline ps_rot2 ps_rot2_from_angle(ps_scalar angle) {
    ps_rot2 r;
    r.s = sinf(angle);
    r.c = cosf(angle);
    return r;
}

static inline ps_rot2 ps_rot2_identity(void) {
    ps_rot2 r = {0.0f, 1.0f};
    return r;
}

static inline ps_vec2 ps_rot2_mul_v(ps_rot2 r, ps_vec2 v) {
    return ps_v2(r.c * v.x - r.s * v.y, r.s * v.x + r.c * v.y);
}

static inline ps_vec2 ps_rot2_mul_t_v(ps_rot2 r, ps_vec2 v) {
    /* transpose: inverse for rotation */
    return ps_v2(r.c * v.x + r.s * v.y, -r.s * v.x + r.c * v.y);
}

static inline ps_rot2 ps_rot2_mul(ps_rot2 a, ps_rot2 b) {
    ps_rot2 r;
    r.s = a.s * b.c + a.c * b.s;
    r.c = a.c * b.c - a.s * b.s;
    return r;
}

static inline ps_scalar ps_rot2_angle(ps_rot2 r) {
    return atan2f(r.s, r.c);
}

#endif /* PS_ROT2_H */
