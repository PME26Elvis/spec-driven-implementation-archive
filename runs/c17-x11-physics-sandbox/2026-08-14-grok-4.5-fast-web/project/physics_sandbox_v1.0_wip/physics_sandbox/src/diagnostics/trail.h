#ifndef PS_TRAIL_H
#define PS_TRAIL_H
#include "../math/math.h"

#define PS_TRAIL_LEN 32
#define PS_TRAIL_MAX_BODIES 64

typedef struct {
    ps_vec2 pts[PS_TRAIL_LEN];
    int head;
    int count;
} ps_trail;

typedef struct {
    ps_trail trails[PS_TRAIL_MAX_BODIES];
    int body_map[PS_TRAIL_MAX_BODIES]; /* body index */
    int count;
} ps_trail_system;

void ps_trail_sys_init(ps_trail_system *ts);
void ps_trail_sys_update(ps_trail_system *ts, const ps_vec2 *positions, const int *ids, int n);
void ps_trail_sys_clear(ps_trail_system *ts);

#endif
