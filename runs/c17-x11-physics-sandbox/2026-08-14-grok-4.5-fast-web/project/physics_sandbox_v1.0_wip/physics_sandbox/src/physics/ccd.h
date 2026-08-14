#ifndef PS_CCD_H
#define PS_CCD_H

#include "body.h"
#include "collision.h"

struct ps_world;
typedef struct ps_world ps_world;

typedef struct {
    ps_vec2 point;
    ps_vec2 normal;
    ps_scalar fraction; /* [0,1] along the sweep */
    int hit;
} ps_shape_cast_result;

typedef struct {
    ps_scalar toi; /* time of impact in [0,1] relative to the step */
    ps_vec2 normal;
    ps_vec2 point;
    int hit;
    ps_body *body_a;
    ps_body *body_b;
} ps_toi_result;

/* Shape Cast: sweep shape of body A from xf0 to xf1 against body B (static or dynamic at current pose) */
int ps_shape_cast(const ps_body *a, const ps_xform *xf0, const ps_xform *xf1,
                  const ps_body *b, ps_shape_cast_result *out);

/* Conservative TOI between two moving bodies over the current time step fraction [0,1] */
int ps_compute_toi(const ps_body *a, const ps_body *b, ps_scalar dt, ps_toi_result *out);

/* World-level CCD: for fast-moving bodies, sub-step to TOI and resolve */
void ps_world_ccd_step(struct ps_world *w, ps_scalar dt);

#endif
