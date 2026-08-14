#include "matrix.h"
#include <string.h>

void ps_matrix_init(ps_collision_matrix *m) {
    /* default: all collide with all */
    for (int i = 0; i < PS_NUM_CATEGORIES; i++)
        for (int j = 0; j < PS_NUM_CATEGORIES; j++)
            m->collide[i][j] = true;
}

bool ps_matrix_should_collide(const ps_collision_matrix *m, uint16_t cat_a, uint16_t cat_b) {
    /* cat bits may have multiple; check any pair */
    for (int i = 0; i < PS_NUM_CATEGORIES; i++) {
        if (!(cat_a & (1u << i))) continue;
        for (int j = 0; j < PS_NUM_CATEGORIES; j++) {
            if (!(cat_b & (1u << j))) continue;
            if (m->collide[i][j]) return true;
        }
    }
    return false;
}

void ps_matrix_set(ps_collision_matrix *m, int a, int b, bool v) {
    if (a >= 0 && a < PS_NUM_CATEGORIES && b >= 0 && b < PS_NUM_CATEGORIES) {
        m->collide[a][b] = v;
        m->collide[b][a] = v; /* symmetric */
    }
}
