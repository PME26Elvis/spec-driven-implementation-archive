#ifndef PS_WORLD_H
#define PS_WORLD_H

#include "body.h"
#include "bvh.h"
#include "solver.h"
#include "joint.h"
#include "contact_cache.h"
#include "matrix.h"
#include "replay.h"

#define PS_MAX_BODIES 1024
#define PS_MAX_WORLD_JOINTS 128

typedef struct ps_world {
    ps_body bodies[PS_MAX_BODIES];
    int body_count;
    uint32_t next_id;
    ps_vec2 gravity;
    ps_scalar time_step;
    int velocity_iterations;
    int position_iterations;
    bool allow_sleep;
    ps_vec2 bounds_min;
    ps_vec2 bounds_max;
    ps_bvh broadphase;
    ps_solver solver;
    ps_contact_cache contact_cache;
    ps_collision_matrix collision_matrix;
    ps_replay_buffer replay;
    ps_joint joints[PS_MAX_WORLD_JOINTS];
    int joint_count;
} ps_world;

void ps_world_init(ps_world *w);
ps_body *ps_world_create_body(ps_world *w, ps_body_type type);
void ps_world_destroy_body(ps_world *w, int index);
ps_joint *ps_world_create_joint(ps_world *w);
void ps_world_step(ps_world *w, ps_scalar dt);
void ps_world_clear_forces(ps_world *w);
void ps_world_sync_proxies(ps_world *w);

#endif /* PS_WORLD_H */
