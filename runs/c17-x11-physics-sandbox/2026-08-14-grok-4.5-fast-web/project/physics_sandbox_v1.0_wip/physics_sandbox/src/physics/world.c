#include "world.h"
#include "collision.h"
#include "ccd.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

typedef struct {
    ps_world *w;
    int body_a;
    int *candidates;
    int *count;
    int max_c;
} query_ctx;

static void query_cb(int proxy_id, void *ctx) {
    query_ctx *c = (query_ctx *)ctx;
    if (proxy_id == c->body_a) return;
    if (*c->count < c->max_c) {
        c->candidates[(*c->count)++] = proxy_id;
    }
}

void ps_world_init(ps_world *w) {
    memset(w, 0, sizeof(*w));
    w->gravity = ps_v2(0.0f, 9.81f);
    w->time_step = 1.0f / 60.0f;
    w->velocity_iterations = 10;
    w->position_iterations = 4;
    w->allow_sleep = true;
    w->bounds_min = ps_v2(-50.0f, -50.0f);
    w->bounds_max = ps_v2(50.0f, 50.0f);
    w->next_id = 1;
    ps_bvh_init(&w->broadphase);
    ps_solver_init(&w->solver);
    ps_cache_init(&w->contact_cache);
    ps_matrix_init(&w->collision_matrix);
    ps_replay_init(&w->replay);
}

ps_body *ps_world_create_body(ps_world *w, ps_body_type type) {
    if (w->body_count >= PS_MAX_BODIES) return NULL;
    int idx = w->body_count++;
    ps_body *b = &w->bodies[idx];
    memset(b, 0, sizeof(*b));
    b->id = w->next_id++;
    b->type = type;
    b->xf = ps_xform_identity();
    b->xf_prev = b->xf;
    b->awake = true;
    b->category_bits = 0x0001;
    b->mask_bits = 0xFFFF;
    b->proxy_id = -1;
    if (type != PS_BODY_DYNAMIC) {
        b->inv_mass = 0.0f;
        b->inv_inertia = 0.0f;
    }
    return b;
}

void ps_world_destroy_body(ps_world *w, int index) {
    if (index < 0 || index >= w->body_count) return;
    ps_body *b = &w->bodies[index];
    if (b->proxy_id >= 0) {
        ps_bvh_destroy_proxy(&w->broadphase, b->proxy_id);
        b->proxy_id = -1;
    }
    if (index < w->body_count - 1) {
        w->bodies[index] = w->bodies[w->body_count - 1];
    }
    w->body_count--;
}

ps_joint *ps_world_create_joint(ps_world *w) {
    if (w->joint_count >= PS_MAX_WORLD_JOINTS) return NULL;
    ps_joint *j = &w->joints[w->joint_count++];
    memset(j, 0, sizeof(*j));
    return j;
}

void ps_world_clear_forces(ps_world *w) {
    for (int i = 0; i < w->body_count; i++) {
        w->bodies[i].force = ps_v2_zero();
        w->bodies[i].torque = 0.0f;
    }
}

void ps_world_sync_proxies(ps_world *w) {
    for (int i = 0; i < w->body_count; i++) {
        ps_body *b = &w->bodies[i];
        if (b->type == PS_BODY_DYNAMIC && !b->awake) {
            /* leave proxy; queries will still see it but solver skips non-awake */
            continue;
        }
        ps_vec2 amin, amax;
        ps_shape_compute_aabb(&b->shape, b->xf, &amin, &amax);
        ps_aabb aabb = {amin, amax};
        if (b->proxy_id < 0) {
            b->proxy_id = ps_bvh_create_proxy(&w->broadphase, &aabb, i);
        } else {
            ps_vec2 disp = ps_v2_sub(b->xf.p, b->xf_prev.p);
            ps_bvh_move_proxy(&w->broadphase, b->proxy_id, &aabb, disp);
        }
    }
}

static void detect_collisions(ps_world *w) {
    ps_solver_clear(&w->solver);
    int candidates[64];
    for (int i = 0; i < w->body_count; i++) {
        ps_body *a = &w->bodies[i];
        if (a->proxy_id < 0) continue;
        ps_vec2 amin, amax;
        ps_shape_compute_aabb(&a->shape, a->xf, &amin, &amax);
        /* expand a bit for safety */
        amin.x -= 0.05f; amin.y -= 0.05f;
        amax.x += 0.05f; amax.y += 0.05f;
        ps_aabb q = {amin, amax};
        int count = 0;
        query_ctx ctx = {w, i, candidates, &count, 64};
        ps_bvh_query(&w->broadphase, &q, query_cb, &ctx);

        for (int k = 0; k < count; k++) {
            int j = candidates[k];
            if (j <= i) continue; /* avoid double */
            ps_body *b = &w->bodies[j];
            if (a->type == PS_BODY_STATIC && b->type == PS_BODY_STATIC) continue;
            if (!a->awake && !b->awake) continue;
            if ((a->category_bits & b->mask_bits) == 0 || (b->category_bits & a->mask_bits) == 0) continue;
            if (!ps_matrix_should_collide(&w->collision_matrix, a->category_bits, b->category_bits)) continue;

            ps_manifold m;
            int hit = 0;
            if (a->shape.type == PS_SHAPE_CIRCLE && b->shape.type == PS_SHAPE_CIRCLE) {
                hit = ps_collide_circle_circle(a, b, &m);
            } else if (a->shape.type == PS_SHAPE_CIRCLE || b->shape.type == PS_SHAPE_CIRCLE) {
                hit = ps_collide_circle_polygon(a, b, &m);
            } else {
                hit = ps_collide_polygon_polygon(a, b, &m);
            }
            if (hit) {
                ps_cache_lookup(&w->contact_cache, &m);
                ps_solver_add_manifold(&w->solver, &m);
            }
        }
    }
}

void ps_world_step(ps_world *w, ps_scalar dt) {
    if (dt <= 0.0f) return;

    /* forces -> velocity */
    for (int i = 0; i < w->body_count; i++) {
        ps_body *b = &w->bodies[i];
        if (b->type != PS_BODY_DYNAMIC || !b->awake) continue;
        b->force = ps_v2_add(b->force, ps_v2_mul(w->gravity, b->mass));
        b->linear_vel = ps_v2_add(b->linear_vel, ps_v2_mul(b->force, b->inv_mass * dt));
        b->angular_vel += b->torque * b->inv_inertia * dt;
        b->linear_vel = ps_v2_mul(b->linear_vel, 1.0f / (1.0f + dt * b->linear_damping));
        b->angular_vel *= 1.0f / (1.0f + dt * b->angular_damping);
    }

    ps_world_sync_proxies(w);
    detect_collisions(w);
    w->solver.velocity_iterations = w->velocity_iterations;
    w->solver.position_iterations = w->position_iterations;
    ps_solver_solve_velocity(&w->solver, dt);

    /* store warm-start */
    for (int mi = 0; mi < w->solver.manifold_count; mi++) {
        ps_cache_store(&w->contact_cache, &w->solver.manifolds[mi]);
    }
    /* joint velocity constraints */
    for (int ji = 0; ji < w->joint_count; ji++) {
        ps_joint_solve_velocity(&w->joints[ji], dt);
    }

    /* CCD for fast bodies */
    ps_world_ccd_step(w, dt);

    /* velocity -> position */
    for (int i = 0; i < w->body_count; i++) {
        ps_body *b = &w->bodies[i];
        if (b->type == PS_BODY_STATIC) continue;
        b->xf_prev = b->xf;
        b->xf.p = ps_v2_add(b->xf.p, ps_v2_mul(b->linear_vel, dt));
        ps_scalar ang = ps_rot2_angle(b->xf.q) + b->angular_vel * dt;
        b->xf.q = ps_rot2_from_angle(ang);
    }

    ps_solver_solve_position(&w->solver);

    /* soft safety bounds (walls are real static bodies; this is last-resort only) */
    for (int i = 0; i < w->body_count; i++) {
        ps_body *b = &w->bodies[i];
        if (b->type != PS_BODY_DYNAMIC) continue;
        if (b->xf.p.x < w->bounds_min.x - 5.0f) { b->xf.p.x = w->bounds_min.x; b->linear_vel.x = 0; }
        if (b->xf.p.x > w->bounds_max.x + 5.0f) { b->xf.p.x = w->bounds_max.x; b->linear_vel.x = 0; }
        if (b->xf.p.y < w->bounds_min.y - 5.0f) { b->xf.p.y = w->bounds_min.y; b->linear_vel.y = 0; }
        if (b->xf.p.y > w->bounds_max.y + 5.0f) { b->xf.p.y = w->bounds_max.y; b->linear_vel.y = 0; }
    }

    /* simple sleeping */
    if (w->allow_sleep) {
        for (int i = 0; i < w->body_count; i++) {
            ps_body *b = &w->bodies[i];
            if (b->type != PS_BODY_DYNAMIC) continue;
            float ke = 0.5f * b->mass * ps_v2_len_sq(b->linear_vel) + 0.5f * b->inertia * b->angular_vel * b->angular_vel;
            if (ke < 0.01f) {
                b->sleep_timer += dt;
                if (b->sleep_timer > 0.5f) {
                    b->awake = false;
                    b->linear_vel = ps_v2_zero();
                    b->angular_vel = 0.0f;
                }
            } else {
                b->sleep_timer = 0.0f;
                b->awake = true;
            }
        }
    }

    ps_world_clear_forces(w);
    ps_replay_capture(&w->replay, w->bodies, w->body_count);
}
