#ifndef PS_MATRIX_H
#define PS_MATRIX_H
#include <stdint.h>
#include <stdbool.h>

#define PS_NUM_CATEGORIES 16

typedef struct {
    /* matrix[i][j] = true means category i collides with category j */
    bool collide[PS_NUM_CATEGORIES][PS_NUM_CATEGORIES];
} ps_collision_matrix;

void ps_matrix_init(ps_collision_matrix *m);
bool ps_matrix_should_collide(const ps_collision_matrix *m, uint16_t cat_a, uint16_t cat_b);
void ps_matrix_set(ps_collision_matrix *m, int a, int b, bool v);

#endif
