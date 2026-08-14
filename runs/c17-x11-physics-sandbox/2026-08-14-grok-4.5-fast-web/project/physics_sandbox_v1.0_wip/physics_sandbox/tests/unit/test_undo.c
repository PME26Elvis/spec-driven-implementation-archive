#include <stdio.h>
#include "../../src/physics/world.h"
#include "../../src/scene/undo.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL\n");f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    ps_undo_stack u; ps_undo_init(&u);
    ps_body *b = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=0.5f;
    ps_body_set_shape(b,&s); ps_body_set_transform(b, ps_v2(1,2),0);
    ps_undo_push(&u, &w);
    int count1 = w.body_count;
    /* add another */
    ps_world_create_body(&w, PS_BODY_DYNAMIC);
    EXPECT(w.body_count == count1 + 1);
    EXPECT(ps_undo_pop(&u, &w) == 1);
    EXPECT(w.body_count == count1);
    printf("Undo tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
