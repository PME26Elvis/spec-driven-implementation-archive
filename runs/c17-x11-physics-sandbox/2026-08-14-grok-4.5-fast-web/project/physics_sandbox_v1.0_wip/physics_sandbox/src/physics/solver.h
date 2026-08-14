#ifndef PS_SOLVER_H
#define PS_SOLVER_H

#include "collision.h"

#define PS_MAX_CONTACTS 512

typedef struct ps_solver {
    ps_manifold manifolds[PS_MAX_CONTACTS];
    int manifold_count;
    int velocity_iterations;
    int position_iterations;
    ps_scalar baumgarte;
    ps_scalar linear_slop;
    ps_scalar max_linear_correction;
} ps_solver;

void ps_solver_init(ps_solver *s);
void ps_solver_clear(ps_solver *s);
void ps_solver_add_manifold(ps_solver *s, const ps_manifold *m);
void ps_solver_solve_velocity(ps_solver *s, ps_scalar dt);
void ps_solver_solve_position(ps_solver *s);

#endif /* PS_SOLVER_H */
