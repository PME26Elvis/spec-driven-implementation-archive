#ifndef PS_UNDO_H
#define PS_UNDO_H
#include "../physics/world.h"

#define PS_UNDO_MAX 32

typedef struct {
    int body_count;
    /* store minimal body snapshots */
    struct {
        uint32_t id;
        int type;
        float px, py, angle;
        int shape_type;
        float r_or_hx, hy, density, friction, restitution;
    } bodies[128];
} ps_undo_snapshot;

typedef struct {
    ps_undo_snapshot stack[PS_UNDO_MAX];
    int top;
} ps_undo_stack;

void ps_undo_init(ps_undo_stack *u);
void ps_undo_push(ps_undo_stack *u, const ps_world *w);
int  ps_undo_pop(ps_undo_stack *u, ps_world *w); /* returns 1 if restored */

#endif
