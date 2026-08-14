#include "replay.h"
#include <string.h>

void ps_replay_init(ps_replay_buffer *r) {
    memset(r, 0, sizeof(*r));
    r->capacity = PS_REPLAY_MAX_FRAMES;
    r->cursor = -1;
}

void ps_replay_capture(ps_replay_buffer *r, const ps_body *bodies, int body_count) {
    if (r->count >= r->capacity) {
        /* shift left */
        memmove(&r->frames[0], &r->frames[1], sizeof(ps_replay_frame) * (r->capacity - 1));
        r->count = r->capacity - 1;
    }
    ps_replay_frame *f = &r->frames[r->count++];
    f->body_count = body_count < PS_REPLAY_MAX_BODIES ? body_count : PS_REPLAY_MAX_BODIES;
    for (int i = 0; i < f->body_count; i++) {
        f->bodies[i].id = bodies[i].id;
        f->bodies[i].pos = bodies[i].xf.p;
        f->bodies[i].angle = ps_rot2_angle(bodies[i].xf.q);
        f->bodies[i].lin_vel = bodies[i].linear_vel;
        f->bodies[i].ang_vel = bodies[i].angular_vel;
    }
}

void ps_replay_restore(const ps_replay_buffer *r, int frame_index, ps_body *bodies, int body_count) {
    if (frame_index < 0 || frame_index >= r->count) return;
    const ps_replay_frame *f = &r->frames[frame_index];
    for (int i = 0; i < f->body_count && i < body_count; i++) {
        /* match by index for simplicity */
        bodies[i].xf.p = f->bodies[i].pos;
        bodies[i].xf.q = ps_rot2_from_angle(f->bodies[i].angle);
        bodies[i].linear_vel = f->bodies[i].lin_vel;
        bodies[i].angular_vel = f->bodies[i].ang_vel;
    }
}

int ps_replay_frame_count(const ps_replay_buffer *r) {
    return r->count;
}
