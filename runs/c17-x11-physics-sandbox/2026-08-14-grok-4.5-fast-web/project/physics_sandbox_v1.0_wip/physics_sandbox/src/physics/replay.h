#ifndef PS_REPLAY_H
#define PS_REPLAY_H
#include "body.h"

#define PS_REPLAY_MAX_FRAMES 120
#define PS_REPLAY_MAX_BODIES 64

typedef struct {
    ps_vec2 pos;
    float angle;
    ps_vec2 lin_vel;
    float ang_vel;
    uint32_t id;
} ps_replay_body_state;

typedef struct {
    int body_count;
    ps_replay_body_state bodies[PS_REPLAY_MAX_BODIES];
} ps_replay_frame;

typedef struct {
    ps_replay_frame frames[PS_REPLAY_MAX_FRAMES];
    int count;
    int cursor; /* current playback index, -1 = live */
    int capacity;
} ps_replay_buffer;

void ps_replay_init(ps_replay_buffer *r);
void ps_replay_capture(ps_replay_buffer *r, const ps_body *bodies, int body_count);
void ps_replay_restore(const ps_replay_buffer *r, int frame_index, ps_body *bodies, int body_count);
int  ps_replay_frame_count(const ps_replay_buffer *r);

#endif
