#include <stdio.h>
#include "../../src/physics/matrix.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL\n");f++;}}while(0)
int main(void) {
    ps_collision_matrix m;
    ps_matrix_init(&m);
    EXPECT(ps_matrix_should_collide(&m, 0x1, 0x1));
    ps_matrix_set(&m, 0, 1, false);
    EXPECT(!ps_matrix_should_collide(&m, 0x1, 0x2));
    EXPECT(ps_matrix_should_collide(&m, 0x1, 0x1));
    printf("Matrix tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
