#include "scene_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int ps_scene_save_json(const ps_world *w, const char *path) {
    if (!w || !path) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\n  \"version\": 1,\n  \"gravity\": [%.6f, %.6f],\n  \"bodies\": [\n",
            w->gravity.x, w->gravity.y);
    for (int i = 0; i < w->body_count; i++) {
        const ps_body *b = &w->bodies[i];
        const char *type = b->type == PS_BODY_STATIC ? "static" :
                           b->type == PS_BODY_KINEMATIC ? "kinematic" : "dynamic";
        fprintf(f, "    {\"id\": %u, \"type\": \"%s\", \"pos\": [%.6f, %.6f], \"angle\": %.6f,",
                b->id, type, b->xf.p.x, b->xf.p.y, ps_rot2_angle(b->xf.q));
        if (b->shape.type == PS_SHAPE_CIRCLE) {
            fprintf(f, " \"shape\": {\"type\": \"circle\", \"radius\": %.6f, \"density\": %.6f, \"friction\": %.6f, \"restitution\": %.6f}}",
                    b->shape.data.circle.radius, b->shape.density, b->shape.friction, b->shape.restitution);
        } else {
            fprintf(f, " \"shape\": {\"type\": \"rectangle\", \"hx\": %.6f, \"hy\": %.6f, \"density\": %.6f, \"friction\": %.6f, \"restitution\": %.6f}}",
                    b->shape.data.rectangle.hx, b->shape.data.rectangle.hy,
                    b->shape.density, b->shape.friction, b->shape.restitution);
        }
        if (i + 1 < w->body_count) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    return 0;
}

/* Minimal loader: only supports the format we write (very simple tokenizer) */
static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',') p++;
    return p;
}

static const char *parse_number(const char *p, float *out) {
    p = skip_ws(p);
    char *end;
    *out = strtof(p, &end);
    return end;
}

int ps_scene_load_json(ps_world *w, const char *path) {
    if (!w || !path) return -1;
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = 0;
    fclose(f);

    w->body_count = 0;
    w->joint_count = 0;
    w->next_id = 1;
    ps_bvh_init(&w->broadphase);
    ps_cache_init(&w->contact_cache);

    const char *p = buf;
    /* gravity */
    p = strstr(p, "\"gravity\"");
    if (p) {
        p = strchr(p, '[');
        if (p) {
            float gx, gy;
            p = parse_number(p+1, &gx);
            p = parse_number(p, &gy);
            w->gravity = ps_v2(gx, gy);
        }
    }
    /* bodies - very naive: find each "type" and following fields */
    p = buf;
    while ((p = strstr(p, "\"type\"")) != NULL) {
        p += 6;
        p = skip_ws(p);
        if (*p != ':') continue;
        p = skip_ws(p+1);
        if (*p != '"') continue;
        p++;
        ps_body_type btype = PS_BODY_DYNAMIC;
        if (strncmp(p, "static", 6) == 0) btype = PS_BODY_STATIC;
        else if (strncmp(p, "kinematic", 9) == 0) btype = PS_BODY_KINEMATIC;

        /* find pos */
        const char *pp = strstr(p, "\"pos\"");
        if (!pp || pp > p + 200) { p++; continue; }
        pp = strchr(pp, '[');
        float px=0, py=0, angle=0;
        if (pp) {
            pp = parse_number(pp+1, &px);
            pp = parse_number(pp, &py);
        }
        const char *pa = strstr(p, "\"angle\"");
        if (pa && pa < p + 300) {
            pa = strchr(pa, ':');
            if (pa) parse_number(pa+1, &angle);
        }

        ps_body *b = ps_world_create_body(w, btype);
        if (!b) break;
        ps_shape s = {0};
        s.density = 1.0f; s.friction = 0.3f; s.restitution = 0.1f;
        const char *ps = strstr(p, "\"shape\"");
        if (ps && ps < p + 400) {
            if (strstr(ps, "\"circle\"") && strstr(ps, "\"circle\"") < ps + 100) {
                s.type = PS_SHAPE_CIRCLE;
                const char *pr = strstr(ps, "\"radius\"");
                if (pr) { pr = strchr(pr, ':'); if (pr) parse_number(pr+1, &s.data.circle.radius); }
            } else {
                s.type = PS_SHAPE_RECTANGLE;
                const char *ph = strstr(ps, "\"hx\"");
                if (ph) { ph = strchr(ph, ':'); if (ph) parse_number(ph+1, &s.data.rectangle.hx); }
                ph = strstr(ps, "\"hy\"");
                if (ph) { ph = strchr(ph, ':'); if (ph) parse_number(ph+1, &s.data.rectangle.hy); }
            }
            const char *pd = strstr(ps, "\"density\"");
            if (pd && pd < ps + 200) { pd = strchr(pd, ':'); if (pd) parse_number(pd+1, &s.density); }
        }
        if (s.type == PS_SHAPE_CIRCLE && s.data.circle.radius < 1e-4f) s.data.circle.radius = 0.5f;
        if (s.type == PS_SHAPE_RECTANGLE && s.data.rectangle.hx < 1e-4f) {
            s.data.rectangle.hx = 0.5f; s.data.rectangle.hy = 0.5f;
        }
        ps_body_set_shape(b, &s);
        ps_body_set_transform(b, ps_v2(px, py), angle);
        p += 10;
    }
    free(buf);
    return 0;
}
