#include <stdio.h>
#include <string.h>
#include "../../src/physics/world.h"
#include "../../src/diagnostics/solver_export.h"
static int f=0,t=0;
#define EXPECT(c) do{t++; if(!(c)){fprintf(stderr,"FAIL\n");f++;}}while(0)
int main(void) {
    ps_world w; ps_world_init(&w);
    ps_body *a = ps_world_create_body(&w, PS_BODY_DYNAMIC);
    ps_body *b = ps_world_create_body(&w, PS_BODY_STATIC);
    ps_shape s={0}; s.type=PS_SHAPE_CIRCLE; s.density=1; s.data.circle.radius=1;
    ps_body_set_shape(a,&s); ps_body_set_transform(a, ps_v2(0,0),0);
    ps_shape fs={0}; fs.type=PS_SHAPE_RECTANGLE; fs.data.rectangle.hx=5; fs.data.rectangle.hy=1;
    ps_body_set_shape(b,&fs); ps_body_set_transform(b, ps_v2(0,2),0);
    for (int i=0;i<30;i++) ps_world_step(&w, 1.f/60.f);
    EXPECT(ps_solver_export_trace(&w, "/tmp/solver_trace.txt") == 0);
    FILE *fp = fopen("/tmp/solver_trace.txt", "r");
    EXPECT(fp != NULL);
    if (fp) {
        char buf[128];
        int has = 0;
        while (fgets(buf, sizeof(buf), fp)) if (strstr(buf, "manifold_count")) has = 1;
        fclose(fp);
        EXPECT(has);
    }
    printf("Solver export tests: %d run, %d failed\n", t, f);
    return f?1:0;
}
