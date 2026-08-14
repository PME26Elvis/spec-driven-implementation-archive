#include "solver_export.h"
#include <stdio.h>

int ps_solver_export_trace(const ps_world *w, const char *path) {
    if (!w || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "# Solver trace export\n");
    fprintf(f, "velocity_iterations=%d\n", w->velocity_iterations);
    fprintf(f, "position_iterations=%d\n", w->position_iterations);
    fprintf(f, "manifold_count=%d\n", w->solver.manifold_count);
    for (int i = 0; i < w->solver.manifold_count; i++) {
        const ps_manifold *m = &w->solver.manifolds[i];
        fprintf(f, "manifold[%d] bodies=%u,%u normal=(%.4f,%.4f) points=%d friction=%.3f restitution=%.3f\n",
                i,
                m->body_a ? m->body_a->id : 0,
                m->body_b ? m->body_b->id : 0,
                m->normal.x, m->normal.y,
                m->point_count, m->friction, m->restitution);
        for (int p = 0; p < m->point_count; p++) {
            fprintf(f, "  point[%d] world=(%.4f,%.4f) sep=%.5f n_imp=%.5f t_imp=%.5f\n",
                    p, m->points[p].world_point.x, m->points[p].world_point.y,
                    m->points[p].separation,
                    m->points[p].normal_impulse, m->points[p].tangent_impulse);
        }
    }
    fclose(f);
    return 0;
}
