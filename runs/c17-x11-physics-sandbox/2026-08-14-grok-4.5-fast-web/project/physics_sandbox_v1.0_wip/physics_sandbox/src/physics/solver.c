#include "solver.h"
#include <string.h>
#include <math.h>

void ps_solver_init(ps_solver *s) {
    memset(s, 0, sizeof(*s));
    s->velocity_iterations = 8;
    s->position_iterations = 3;
    s->baumgarte = 0.2f;
    s->linear_slop = 0.005f;
    s->max_linear_correction = 0.2f;
}

void ps_solver_clear(ps_solver *s) {
    s->manifold_count = 0;
}

void ps_solver_add_manifold(ps_solver *s, const ps_manifold *m) {
    if (s->manifold_count >= PS_MAX_CONTACTS) return;
    s->manifolds[s->manifold_count++] = *m;
}

static void resolve_velocity_contact(ps_manifold *m, int i, ps_scalar dt) {
    (void)dt;
    ps_body *a = m->body_a;
    ps_body *b = m->body_b;
    if (!a || !b) return;
    if (!a->awake && !b->awake) return;

    ps_contact_point *cp = &m->points[i];
    ps_vec2 pa = ps_xform_point(a->xf, a->local_com);
    ps_vec2 pb = ps_xform_point(b->xf, b->local_com);
    ps_vec2 ra = ps_v2_sub(cp->world_point, pa);
    ps_vec2 rb = ps_v2_sub(cp->world_point, pb);

    ps_vec2 va = ps_v2_add(a->linear_vel, ps_v2_cross_s_v(a->angular_vel, ra));
    ps_vec2 vb = ps_v2_add(b->linear_vel, ps_v2_cross_s_v(b->angular_vel, rb));
    ps_vec2 rv = ps_v2_sub(vb, va);

    ps_scalar vn = ps_v2_dot(rv, m->normal);

    ps_scalar rna = ps_v2_cross(ra, m->normal);
    ps_scalar rnb = ps_v2_cross(rb, m->normal);
    ps_scalar k_normal = a->inv_mass + b->inv_mass +
                         rna * rna * a->inv_inertia + rnb * rnb * b->inv_inertia;
    if (k_normal < PS_SCALAR_EPSILON) return;
    ps_scalar inv_k = 1.0f / k_normal;

    ps_scalar bias = 0.0f;
    if (vn < -1.0f) bias = -m->restitution * vn;

    /* warm-start already stored in cp->normal_impulse from previous frame if carried */
    ps_scalar lambda = -inv_k * (vn - bias);
    ps_scalar old = cp->normal_impulse;
    cp->normal_impulse = fmaxf(old + lambda, 0.0f);
    lambda = cp->normal_impulse - old;

    ps_vec2 P = ps_v2_mul(m->normal, lambda);
    if (a->type == PS_BODY_DYNAMIC) {
        a->linear_vel = ps_v2_sub(a->linear_vel, ps_v2_mul(P, a->inv_mass));
        a->angular_vel -= a->inv_inertia * ps_v2_cross(ra, P);
        a->awake = true;
    }
    if (b->type == PS_BODY_DYNAMIC) {
        b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(P, b->inv_mass));
        b->angular_vel += b->inv_inertia * ps_v2_cross(rb, P);
        b->awake = true;
    }

    /* rolling resistance */
    {
        float rr = 0.5f * (a->shape.rolling_resistance + b->shape.rolling_resistance);
        if (rr > 0.0f && cp->normal_impulse > 0.0f) {
            float damp = -rr * cp->normal_impulse * 0.1f;
            if (a->type == PS_BODY_DYNAMIC) a->angular_vel += damp * a->inv_inertia;
            if (b->type == PS_BODY_DYNAMIC) b->angular_vel += damp * b->inv_inertia;
        }
    }

    /* friction */
    ps_vec2 tangent = ps_v2_sub(rv, ps_v2_mul(m->normal, ps_v2_dot(rv, m->normal)));
    ps_scalar tlen = ps_v2_len(tangent);
    if (tlen > PS_SCALAR_EPSILON) {
        tangent = ps_v2_div(tangent, tlen);
        ps_scalar rta = ps_v2_cross(ra, tangent);
        ps_scalar rtb = ps_v2_cross(rb, tangent);
        ps_scalar k_t = a->inv_mass + b->inv_mass +
                        rta * rta * a->inv_inertia + rtb * rtb * b->inv_inertia;
        if (k_t > PS_SCALAR_EPSILON) {
            ps_scalar jt = -ps_v2_dot(rv, tangent) / k_t;
            ps_scalar max_jt = m->friction * cp->normal_impulse;
            ps_scalar old_t = cp->tangent_impulse;
            cp->tangent_impulse = fmaxf(-max_jt, fminf(old_t + jt, max_jt));
            jt = cp->tangent_impulse - old_t;
            ps_vec2 Pt = ps_v2_mul(tangent, jt);
            if (a->type == PS_BODY_DYNAMIC) {
                a->linear_vel = ps_v2_sub(a->linear_vel, ps_v2_mul(Pt, a->inv_mass));
                a->angular_vel -= a->inv_inertia * ps_v2_cross(ra, Pt);
            }
            if (b->type == PS_BODY_DYNAMIC) {
                b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(Pt, b->inv_mass));
                b->angular_vel += b->inv_inertia * ps_v2_cross(rb, Pt);
            }
        }
    }
}

void ps_solver_solve_velocity(ps_solver *s, ps_scalar dt) {
    for (int iter = 0; iter < s->velocity_iterations; iter++) {
        for (int i = 0; i < s->manifold_count; i++) {
            ps_manifold *m = &s->manifolds[i];
            for (int p = 0; p < m->point_count; p++) {
                resolve_velocity_contact(m, p, dt);
            }
        }
    }
}

void ps_solver_solve_position(ps_solver *s) {
    for (int iter = 0; iter < s->position_iterations; iter++) {
        for (int i = 0; i < s->manifold_count; i++) {
            ps_manifold *m = &s->manifolds[i];
            ps_body *a = m->body_a;
            ps_body *b = m->body_b;
            if (!a || !b) continue;
            for (int p = 0; p < m->point_count; p++) {
                ps_contact_point *cp = &m->points[p];
                ps_vec2 wa = ps_xform_point(a->xf, cp->local_point_a);
                ps_vec2 wb = ps_xform_point(b->xf, cp->local_point_b);
                ps_vec2 d = ps_v2_sub(wb, wa);
                ps_scalar sep = ps_v2_dot(d, m->normal);
                ps_scalar C = fminf(0.0f, sep + s->linear_slop);
                if (C >= 0.0f) continue;
                ps_vec2 ra = ps_v2_sub(wa, a->xf.p);
                ps_vec2 rb = ps_v2_sub(wb, b->xf.p);
                ps_scalar rna = ps_v2_cross(ra, m->normal);
                ps_scalar rnb = ps_v2_cross(rb, m->normal);
                ps_scalar k = a->inv_mass + b->inv_mass +
                              rna * rna * a->inv_inertia + rnb * rnb * b->inv_inertia;
                if (k < PS_SCALAR_EPSILON) continue;
                ps_scalar impulse = -s->baumgarte * C / k;
                impulse = fmaxf(-s->max_linear_correction, fminf(impulse, s->max_linear_correction));
                ps_vec2 P = ps_v2_mul(m->normal, impulse);
                if (a->type == PS_BODY_DYNAMIC && a->awake) {
                    a->xf.p = ps_v2_sub(a->xf.p, ps_v2_mul(P, a->inv_mass));
                    float dang = -a->inv_inertia * ps_v2_cross(ra, P);
                    a->xf.q = ps_rot2_from_angle(ps_rot2_angle(a->xf.q) + dang);
                }
                if (b->type == PS_BODY_DYNAMIC && b->awake) {
                    b->xf.p = ps_v2_add(b->xf.p, ps_v2_mul(P, b->inv_mass));
                    float dang = b->inv_inertia * ps_v2_cross(rb, P);
                    b->xf.q = ps_rot2_from_angle(ps_rot2_angle(b->xf.q) + dang);
                }
            }
        }
    }
}
