#include "trail.h"
#include <string.h>

void ps_trail_sys_init(ps_trail_system *ts) {
    memset(ts, 0, sizeof(*ts));
}

void ps_trail_sys_clear(ps_trail_system *ts) {
    memset(ts, 0, sizeof(*ts));
}

void ps_trail_sys_update(ps_trail_system *ts, const ps_vec2 *positions, const int *ids, int n) {
    for (int i = 0; i < n && i < PS_TRAIL_MAX_BODIES; i++) {
        /* find or create trail slot by id */
        int slot = -1;
        for (int j = 0; j < ts->count; j++) {
            if (ts->body_map[j] == ids[i]) { slot = j; break; }
        }
        if (slot < 0) {
            if (ts->count >= PS_TRAIL_MAX_BODIES) continue;
            slot = ts->count++;
            ts->body_map[slot] = ids[i];
            ts->trails[slot].head = 0;
            ts->trails[slot].count = 0;
        }
        ps_trail *tr = &ts->trails[slot];
        tr->pts[tr->head] = positions[i];
        tr->head = (tr->head + 1) % PS_TRAIL_LEN;
        if (tr->count < PS_TRAIL_LEN) tr->count++;
    }
}
