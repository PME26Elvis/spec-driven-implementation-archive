#include "ccd.h"
#include "world.h"
#include "collision.h"
#include <math.h>
#include <string.h>
#include <float.h>

/* Conservative distance between two shapes at given transforms (uses existing collide as oracle) */
static float shape_distance(const ps_body *a, ps_xform xfa, const ps_body *b, ps_xform xfb, ps_vec2 *normal_out) {
    ps_body ta = *a, tb = *b;
    ta.xf = xfa; tb.xf = xfb;
    ps_manifold m;
    int hit = 0;
    if (ta.shape.type == PS_SHAPE_CIRCLE && tb.shape.type == PS_SHAPE_CIRCLE)
        hit = ps_collide_circle_circle(&ta, &tb, &m);
    else if (ta.shape.type == PS_SHAPE_CIRCLE || tb.shape.type == PS_SHAPE_CIRCLE)
        hit = ps_collide_circle_polygon(&ta, &tb, &m);
    else
        hit = ps_collide_polygon_polygon(&ta, &tb, &m);
    if (hit) {
        if (normal_out) *normal_out = m.normal;
        return m.points[0].separation; /* negative = penetration */
    }
    /* approximate positive distance via centers if no hit */
    ps_vec2 d = ps_v2_sub(xfb.p, xfa.p);
    float dist = ps_v2_len(d);
    if (normal_out && dist > 1e-8f) *normal_out = ps_v2_div(d, dist);
    float ra = (ta.shape.type == PS_SHAPE_CIRCLE) ? ta.shape.data.circle.radius : 0.5f;
    float rb = (tb.shape.type == PS_SHAPE_CIRCLE) ? tb.shape.data.circle.radius : 0.5f;
    return dist - ra - rb;
}

int ps_shape_cast(const ps_body *a, const ps_xform *xf0, const ps_xform *xf1,
                  const ps_body *b, ps_shape_cast_result *out) {
    if (!a || !b || !out || !xf0 || !xf1) return 0;
    memset(out, 0, sizeof(*out));
    out->fraction = 1.0f;

    /* binary search for first time of contact along the linear+angular sweep */
    const int MAX_IT = 16;
    float t0 = 0.0f, t1 = 1.0f;
    ps_vec2 n = ps_v2(1,0);
    float sep0 = shape_distance(a, *xf0, b, b->xf, &n);
    if (sep0 <= 0.0f) {
        /* already overlapping at start */
        out->hit = 1;
        out->fraction = 0.0f;
        out->normal = n;
        out->point = xf0->p;
        return 1;
    }

    for (int i = 0; i < MAX_IT; i++) {
        float tm = 0.5f * (t0 + t1);
        /* interpolate transform */
        ps_xform xfm;
        xfm.p = ps_v2_add(xf0->p, ps_v2_mul(ps_v2_sub(xf1->p, xf0->p), tm));
        float a0 = ps_rot2_angle(xf0->q);
        float a1 = ps_rot2_angle(xf1->q);
        xfm.q = ps_rot2_from_angle(a0 + (a1 - a0) * tm);

        float sep = shape_distance(a, xfm, b, b->xf, &n);
        if (sep <= 0.0f) {
            t1 = tm;
            out->hit = 1;
            out->fraction = tm;
            out->normal = n;
            out->point = xfm.p;
        } else {
            t0 = tm;
        }
    }
    return out->hit;
}

int ps_compute_toi(const ps_body *a, const ps_body *b, ps_scalar dt, ps_toi_result *out) {
    if (!a || !b || !out || dt <= 0.0f) return 0;
    memset(out, 0, sizeof(*out));
    out->toi = 1.0f;
    out->body_a = (ps_body*)a;
    out->body_b = (ps_body*)b;

    /* predict positions at end of step */
    ps_xform xf1a = a->xf, xf1b = b->xf;
    xf1a.p = ps_v2_add(a->xf.p, ps_v2_mul(a->linear_vel, dt));
    xf1b.p = ps_v2_add(b->xf.p, ps_v2_mul(b->linear_vel, dt));
    float aa = ps_rot2_angle(a->xf.q) + a->angular_vel * dt;
    float ab = ps_rot2_angle(b->xf.q) + b->angular_vel * dt;
    xf1a.q = ps_rot2_from_angle(aa);
    xf1b.q = ps_rot2_from_angle(ab);

    /* relative sweep: fix B, sweep A relative */
    /* simplified: use shape_cast of A against B at its current pose, then refine with both moving */
    ps_shape_cast_result scr;
    /* first try A moving, B static at start */
    if (!ps_shape_cast(a, &a->xf, &xf1a, b, &scr) || !scr.hit) {
        /* try opposite */
        if (!ps_shape_cast(b, &b->xf, &xf1b, a, &scr) || !scr.hit) {
            return 0;
        }
        out->normal = ps_v2_mul(scr.normal, -1.f);
    } else {
        out->normal = scr.normal;
    }
    out->hit = 1;
    out->toi = scr.fraction;
    out->point = scr.point;
    return 1;
}

void ps_world_ccd_step(struct ps_world *w, ps_scalar dt) {
    if (!w || dt <= 0.0f) return;
    const float CCD_SPEED_THRESHOLD = 5.0f; /* world units / s */
    const int MAX_SUB = 4;

    for (int i = 0; i < w->body_count; i++) {
        ps_body *a = &w->bodies[i];
        if (a->type != PS_BODY_DYNAMIC || !a->awake) continue;
        float speed = ps_v2_len(a->linear_vel) + fabsf(a->angular_vel) * 2.0f;
        if (speed < CCD_SPEED_THRESHOLD) continue;

        /* find earliest TOI against any other body */
        float earliest = 1.0f;
        ps_toi_result best = {0};
        for (int j = 0; j < w->body_count; j++) {
            if (i == j) continue;
            ps_body *b = &w->bodies[j];
            if (a->type == PS_BODY_STATIC && b->type == PS_BODY_STATIC) continue;
            ps_toi_result toi;
            if (ps_compute_toi(a, b, dt, &toi) && toi.hit && toi.toi < earliest && toi.toi > 1e-4f) {
                earliest = toi.toi;
                best = toi;
            }
        }
        if (best.hit && earliest < 1.0f) {
            /* advance a to TOI */
            float t = earliest * 0.95f; /* slight bias to avoid tunneling residual */
            a->xf.p = ps_v2_add(a->xf.p, ps_v2_mul(a->linear_vel, dt * t));
            float ang = ps_rot2_angle(a->xf.q) + a->angular_vel * dt * t;
            a->xf.q = ps_rot2_from_angle(ang);
            /* simple response: reflect velocity along normal */
            float vn = ps_v2_dot(a->linear_vel, best.normal);
            if (vn < 0) {
                a->linear_vel = ps_v2_sub(a->linear_vel, ps_v2_mul(best.normal, (1.0f + 0.2f) * vn));
            }
            (void)MAX_SUB;
        }
    }
}
