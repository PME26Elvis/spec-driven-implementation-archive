/* sdk_ui_base.c - easing, ripple pool, layout helpers. */
#include "ui/sdk_ui.h"
#include "common/sdk_win.h"

#include <stdlib.h>
#include <math.h>

double sdk_clamp01(double t) { if (t < 0) return 0; if (t > 1) return 1; return t; }

/* cubic-bezier(x1,y1,x2,y2) evaluator with x solving */
static double _bez(double t, double a, double b) {
    /* a = p1, b = p2 (p0=0,p3=1) */
    double c = 3*a;
    double bb = 3*(b-a) - c;
    double aa = 1 - c - bb;
    return ((aa*t + bb)*t + c)*t;
}
static double _bez_solve_x(double x, double x1, double x2) {
    double t = x;
    for (int i = 0; i < 24; ++i) {
        double xe = _bez(t, x1, x2) - x;
        if (fabs(xe) < 1e-4) return t;
        double d = (3*(1-3*x2+3*x1)*t*t + 2*(3*x2-6*x1)*t + 3*x1);
        if (fabs(d) < 1e-6) break;
        t -= xe / d;
        if (t < 0) t = 0; if (t > 1) t = 1;
    }
    return t;
}
double sdk_ease_eval(sdk_ease e, double t) {
    t = sdk_clamp01(t);
    switch (e) {
        case SDK_EASE_LINEAR: return t;
        case SDK_EASE_OUT:    return 1 - (1-t)*(1-t);                 /* quad out */
        case SDK_EASE_IN_OUT: return t < 0.5 ? 2*t*t : 1 - pow(-2*t+2,2)/2;
        case SDK_EASE_EMPHASIZED: {                                   /* spring-ish */
            double x1=0.2, x2=0.8;
            double tt = _bez_solve_x(t, x1, x2);
            double y = _bez(tt, 0.05, 0.95);
            /* mild overshoot */
            return sdk_clamp01(y + 0.06 * sin(t*3.14159) * (1-t));
        }
        default: return t;
    }
}

/* ---------------- ripple pool ---------------- */
struct sdk_ripple_mgr {
    int cap;
    int count;
    struct { int id; int x, y; uint64_t start; } *items;
};
sdk_ripple_mgr *sdk_ripple_create(int cap) {
    if (cap <= 0) cap = 1;
    sdk_ripple_mgr *m = (sdk_ripple_mgr *)calloc(1, sizeof *m);
    if (!m) return NULL;
    m->items = (void *)calloc(cap, sizeof *m->items);
    if (!m->items) { free(m); return NULL; }
    m->cap = cap;
    return m;
}
void sdk_ripple_destroy(sdk_ripple_mgr *m) {
    if (!m) return;
    free(m->items);
    free(m);
}
void sdk_ripple_spawn(sdk_ripple_mgr *m, int id, int x, int y, uint64_t now) {
    if (!m) return;
    /* reuse a slot for same id or find free */
    int slot = -1;
    for (int i = 0; i < m->count; ++i)
        if (m->items[i].id == id) { slot = i; break; }
    if (slot < 0) {
        if (m->count < m->cap) slot = m->count++;
        else slot = 0; /* overwrite oldest */
    }
    m->items[slot].id = id; m->items[slot].x = x; m->items[slot].y = y;
    m->items[slot].start = now;
}
void sdk_ripple_draw(const sdk_ripple_mgr *m, sdk_fb *fb, sdk_rect bounds, int radius,
                     uint64_t now, sdk_color tint, int reduced) {
    if (!m) return;
    sdk_clip_push(fb, bounds);
    for (int i = 0; i < m->count; ++i) {
        uint64_t dt = now - m->items[i].start;
        double dur = reduced ? 160.0 : 380.0;
        double t = (double)dt / dur;
        if (t >= 1.0) continue;
        t = sdk_clamp01(t);
        int maxr = radius > 0 ? radius : (bounds.w > bounds.h ? bounds.w : bounds.h);
        int r = (int)(maxr * t);
        double op = (t < 0.3 ? t / 0.3 : (1 - t) / 0.7);  /* fade in then out */
        sdk_color c = tint; c.a = (uint8_t)(c.a * op);
        sdk_fill_disc(fb, m->items[i].x, m->items[i].y, r, c);
    }
    sdk_clip_pop(fb);
}

/* ---------------- layout ---------------- */
sdk_rect sdk_inset(sdk_rect r, int pad) {
    return (sdk_rect){ r.x + pad, r.y + pad, r.w - 2*pad, r.h - 2*pad };
}
sdk_rect sdk_inset_ex(sdk_rect r, int l, int t, int rr, int b) {
    return (sdk_rect){ r.x + l, r.y + t, r.w - l - rr, r.h - t - b };
}

sdk_rect sdk_hstack(const sdk_rect *avail, int count, const int *sizes, int gap, sdk_align a, int *out) {
    int total = 0;
    for (int i = 0; i < count; ++i) total += sizes[i];
    total += gap * (count > 0 ? count - 1 : 0);
    int start;
    if (a == SDK_ALIGN_CENTER) start = (avail->w - total) / 2;
    else if (a == SDK_ALIGN_END) start = avail->w - total;
    else start = 0;
    int pos = start;
    sdk_rect bbox = { avail->x + start, avail->y, total, avail->h };
    for (int i = 0; i < count; ++i) {
        sdk_rect cell = { avail->x + pos, avail->y, sizes[i], avail->h };
        if (out) out[i] = 0; /* unused */
        (void)cell;
        pos += sizes[i] + gap;
    }
    return bbox;
}
sdk_rect sdk_vstack(const sdk_rect *avail, int count, const int *sizes, int gap, sdk_align a, int *out) {
    int total = 0;
    for (int i = 0; i < count; ++i) total += sizes[i];
    total += gap * (count > 0 ? count - 1 : 0);
    int start;
    if (a == SDK_ALIGN_CENTER) start = (avail->h - total) / 2;
    else if (a == SDK_ALIGN_END) start = avail->h - total;
    else start = 0;
    int pos = start;
    sdk_rect bbox = { avail->x, avail->y + start, avail->w, total };
    for (int i = 0; i < count; ++i) {
        sdk_rect cell = { avail->x, avail->y + pos, avail->w, sizes[i] };
        if (out) out[i] = 0;
        (void)cell;
        pos += sizes[i] + gap;
    }
    return bbox;
}
