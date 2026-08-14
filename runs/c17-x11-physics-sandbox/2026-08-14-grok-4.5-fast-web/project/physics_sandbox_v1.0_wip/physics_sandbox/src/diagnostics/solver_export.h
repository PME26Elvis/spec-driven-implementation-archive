#ifndef PS_SOLVER_EXPORT_H
#define PS_SOLVER_EXPORT_H
#include "../physics/world.h"

/* Write current solver manifolds and impulses to path; returns 0 on success */
int ps_solver_export_trace(const ps_world *w, const char *path);

#endif
