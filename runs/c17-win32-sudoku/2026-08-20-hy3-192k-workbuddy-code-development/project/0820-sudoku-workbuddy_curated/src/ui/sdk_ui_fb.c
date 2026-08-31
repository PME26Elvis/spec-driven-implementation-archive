/* sdk_ui_fb.c - framebuffer, pixel ops, primitives, clipping, blur, BMP.
 * All drawing respects a nested clip stack (intersection). */
#include "ui/sdk_ui.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---------------- framebuffer ---------------- */
sdk_fb *sdk_fb_create(int w, int h) {
    if (w <= 0 || h <= 0) return NULL;
    /* overflow check */
    if ((size_t)w > (SIZE_MAX / 4) / (size_t)h) return NULL;
    sdk_fb *fb = (sdk_fb *)calloc(1, sizeof *fb);
    if (!fb) return NULL;
    size_t need = (size_t)w * 4 * (size_t)h;
    fb->pixels = (uint8_t *)malloc(need);
    if (!fb->pixels) { free(fb); return NULL; }
    fb->width = w; fb->height = h;
    fb->pitch = w * 4;
    fb->owns = 1;
    sdk_rect full = {0,0,w,h};
    fb->_clip[0] = full; fb->_clipn = 1;
    return fb;
}
void sdk_fb_destroy(sdk_fb *fb) {
    if (!fb) return;
    if (fb->owns && fb->pixels) free(fb->pixels);
    free(fb);
}
int sdk_fb_resize(sdk_fb *fb, int w, int h) {
    if (w <= 0 || h <= 0) return 0;
    if ((size_t)w > (SIZE_MAX / 4) / (size_t)h) return 0;
    size_t need = (size_t)w * 4 * (size_t)h;
    uint8_t *np = (uint8_t *)malloc(need);
    if (!np) return 0;               /* keep old buffer intact */
    free(fb->pixels);
    fb->pixels = np;
    fb->width = w; fb->height = h; fb->pitch = w * 4;
    sdk_rect full = {0,0,w,h};
    fb->_clip[0] = full; fb->_clipn = 1;
    return 1;
}

sdk_color sdk_color_make(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    sdk_color c; c.r = r; c.g = g; c.b = b; c.a = a; return c;
}
sdk_color sdk_color_from_u32(uint32_t rgba) {
    sdk_color c;
    c.r = (uint8_t)((rgba >> 24) & 0xFF);
    c.g = (uint8_t)((rgba >> 16) & 0xFF);
    c.b = (uint8_t)((rgba >> 8) & 0xFF);
    c.a = (uint8_t)(rgba & 0xFF);
    return c;
}
uint32_t sdk_color_to_u32(sdk_color c) {
    return ((uint32_t)c.r << 24) | ((uint32_t)c.g << 16) | ((uint32_t)c.b << 8) | c.a;
}

/* ---------------- clip ---------------- */
static sdk_rect _normalize(sdk_rect r) {
    if (r.w < 0) { r.x += r.w; r.w = -r.w; }
    if (r.h < 0) { r.y += r.h; r.h = -r.h; }
    return r;
}
void sdk_clip_push(sdk_fb *fb, sdk_rect r) {
    r = _normalize(r);
    sdk_rect cur = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(cur, r, &it)) it = (sdk_rect){0,0,0,0};
    if (fb->_clipn < 16) fb->_clip[fb->_clipn++] = it;
    else fb->_clip[15] = it;
}
void sdk_clip_pop(sdk_fb *fb) {
    if (fb->_clipn > 1) fb->_clipn--;
}
sdk_rect sdk_clip_current(const sdk_fb *fb) {
    return fb->_clip[fb->_clipn - 1];
}
bool sdk_rect_contains(sdk_rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}
bool sdk_rect_intersect(sdk_rect a, sdk_rect b, sdk_rect *out) {
    a = _normalize(a); b = _normalize(b);
    int x0 = a.x > b.x ? a.x : b.x;
    int y0 = a.y > b.y ? a.y : b.y;
    int x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    int y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0) { *out = (sdk_rect){0,0,0,0}; return false; }
    out->x = x0; out->y = y0; out->w = x1 - x0; out->h = y1 - y0;
    return true;
}

/* ---------------- pixel ---------------- */
static inline void _set_px(sdk_fb *fb, int x, int y, sdk_color c) {
    uint8_t *p = fb->pixels + (size_t)y * fb->pitch + (size_t)x * 4;
    if (c.a >= 255) {
        p[0] = c.b; p[1] = c.g; p[2] = c.r; p[3] = 255;
    } else if (c.a > 0) {
        unsigned ia = c.a, sa = 255 - c.a;
        unsigned b = p[0] * sa + c.b * ia;
        unsigned g = p[1] * sa + c.g * ia;
        unsigned r = p[2] * sa + c.r * ia;
        p[0] = (uint8_t)((b + 128) / 255);
        p[1] = (uint8_t)((g + 128) / 255);
        p[2] = (uint8_t)((r + 128) / 255);
    }
}

static inline int _in_clip(const sdk_fb *fb, int x, int y) {
    sdk_rect c = fb->_clip[fb->_clipn - 1];
    return x >= c.x && x < c.x + c.w && y >= c.y && y < c.y + c.h;
}

void sdk_clear(sdk_fb *fb, sdk_color c) {
    /* clear ignores clip (full frame wipe at frame start) */
    for (int y = 0; y < fb->height; ++y)
        for (int x = 0; x < fb->width; ++x) {
            uint8_t *p = fb->pixels + (size_t)y * fb->pitch + (size_t)x * 4;
            p[0] = c.b; p[1] = c.g; p[2] = c.r; p[3] = 255;
        }
}

void sdk_fill_rect(sdk_fb *fb, sdk_rect r, sdk_color c) {
    r = _normalize(r);
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(r, cl, &it)) return;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x)
            _set_px(fb, x, y, c);
}

void sdk_stroke_rect(sdk_fb *fb, sdk_rect r, int t, sdk_color c) {
    if (t <= 0) return;
    r = _normalize(r);
    sdk_fill_rect(fb, (sdk_rect){r.x, r.y, r.w, t}, c);
    sdk_fill_rect(fb, (sdk_rect){r.x, r.y + r.h - t, r.w, t}, c);
    sdk_fill_rect(fb, (sdk_rect){r.x, r.y, t, r.h}, c);
    sdk_fill_rect(fb, (sdk_rect){r.x + r.w - t, r.y, t, r.h}, c);
}

void sdk_fill_round_rect(sdk_fb *fb, sdk_rect r, int radius, sdk_color c) {
    r = _normalize(r);
    if (radius <= 0) { sdk_fill_rect(fb, r, c); return; }
    int maxr = (r.w < r.h ? r.w : r.h) / 2;
    if (radius > maxr) radius = maxr;
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(r, cl, &it)) return;
    int rr2 = radius * radius;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            int inside = 1;
            /* corner distance checks */
            if (x < r.x + radius && y < r.y + radius) {
                int dx = (r.x + radius) - x, dy = (r.y + radius) - y;
                if (dx*dx + dy*dy > rr2) inside = 0;
            } else if (x >= r.x + r.w - radius && y < r.y + radius) {
                int dx = x - (r.x + r.w - radius), dy = (r.y + radius) - y;
                if (dx*dx + dy*dy > rr2) inside = 0;
            } else if (x < r.x + radius && y >= r.y + r.h - radius) {
                int dx = (r.x + radius) - x, dy = y - (r.y + r.h - radius);
                if (dx*dx + dy*dy > rr2) inside = 0;
            } else if (x >= r.x + r.w - radius && y >= r.y + r.h - radius) {
                int dx = x - (r.x + r.w - radius), dy = y - (r.y + r.h - radius);
                if (dx*dx + dy*dy > rr2) inside = 0;
            }
            if (inside) _set_px(fb, x, y, c);
        }
}

void sdk_stroke_round_rect(sdk_fb *fb, sdk_rect r, int radius, int t, sdk_color c) {
    if (t <= 0) return;
    /* stroke by sampling the ring between outer and inner rounded rects */
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(r, cl, &it)) return;
    int rr2 = radius * radius;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            int in_outer = 1, in_inner = 1;
            /* outer test */
            if (x < r.x + radius && y < r.y + radius) {
                int dx = (r.x + radius) - x, dy = (r.y + radius) - y;
                if (dx*dx + dy*dy > rr2) in_outer = 0;
            } else if (x >= r.x + r.w - radius && y < r.y + radius) {
                int dx = x - (r.x + r.w - radius), dy = (r.y + radius) - y;
                if (dx*dx + dy*dy > rr2) in_outer = 0;
            } else if (x < r.x + radius && y >= r.y + r.h - radius) {
                int dx = (r.x + radius) - x, dy = y - (r.y + r.h - radius);
                if (dx*dx + dy*dy > rr2) in_outer = 0;
            } else if (x >= r.x + r.w - radius && y >= r.y + r.h - radius) {
                int dx = x - (r.x + r.w - radius), dy = y - (r.y + r.h - radius);
                if (dx*dx + dy*dy > rr2) in_outer = 0;
            }
            /* inner test (radius reduced by t at inner boundary) */
            sdk_rect ri = { r.x + t, r.y + t, r.w - 2*t, r.h - 2*t };
            int rirad = radius - t; if (rirad < 0) rirad = 0;
            int rri2 = rirad * rirad;
            if (x >= ri.x && x < ri.x + ri.w && y >= ri.y && y < ri.y + ri.h) {
                if (rirad > 0) {
                    if (x < ri.x + rirad && y < ri.y + rirad) {
                        int dx = (ri.x + rirad) - x, dy = (ri.y + rirad) - y;
                        if (dx*dx + dy*dy > rri2) in_inner = 0;
                    } else if (x >= ri.x + ri.w - rirad && y < ri.y + rirad) {
                        int dx = x - (ri.x + ri.w - rirad), dy = (ri.y + rirad) - y;
                        if (dx*dx + dy*dy > rri2) in_inner = 0;
                    } else if (x < ri.x + rirad && y >= ri.y + ri.h - rirad) {
                        int dx = (ri.x + rirad) - x, dy = y - (ri.y + ri.h - rirad);
                        if (dx*dx + dy*dy > rri2) in_inner = 0;
                    } else if (x >= ri.x + ri.w - rirad && y >= ri.y + ri.h - rirad) {
                        int dx = x - (ri.x + ri.w - rirad), dy = y - (ri.y + ri.h - rirad);
                        if (dx*dx + dy*dy > rri2) in_inner = 0;
                    }
                }
            } else in_inner = 0;
            if (in_outer && !in_inner) _set_px(fb, x, y, c);
        }
}

void sdk_fill_disc(sdk_fb *fb, int cx, int cy, int rad, sdk_color c) {
    if (rad <= 0) return;
    sdk_rect box = { cx - rad, cy - rad, rad*2 + 1, rad*2 + 1 };
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(box, cl, &it)) return;
    int r2 = rad * rad;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy <= r2) _set_px(fb, x, y, c);
        }
}
void sdk_stroke_disc(sdk_fb *fb, int cx, int cy, int rad, int t, sdk_color c) {
    if (rad <= 0 || t <= 0) return;
    sdk_rect box = { cx - rad, cy - rad, rad*2 + 1, rad*2 + 1 };
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(box, cl, &it)) return;
    int r2 = rad * rad, ri2 = (rad - t) * (rad - t);
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            int dx = x - cx, dy = y - cy;
            int d2 = dx*dx + dy*dy;
            if (d2 <= r2 && d2 > ri2) _set_px(fb, x, y, c);
        }
}

void sdk_line(sdk_fb *fb, int x0, int y0, int x1, int y1, sdk_color c) {
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    for (;;) {
        if (_in_clip(fb, x0, y0)) _set_px(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* gradient: vertical/horizontal linear, per-pixel lerp */
static sdk_color _lerp(sdk_color a, sdk_color b, double t) {
    sdk_color c;
    c.r = (uint8_t)(a.r + (b.r - a.r) * t);
    c.g = (uint8_t)(a.g + (b.g - a.g) * t);
    c.b = (uint8_t)(a.b + (b.b - a.b) * t);
    c.a = (uint8_t)(a.a + (b.a - a.a) * t);
    return c;
}
static void _grad(sdk_fb *fb, sdk_rect r, sdk_color a, sdk_color b, int vertical) {
    r = _normalize(r);
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(r, cl, &it)) return;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            double t = vertical ? (double)(y - r.y) / (double)r.h : (double)(x - r.x) / (double)r.w;
            if (t < 0) t = 0; if (t > 1) t = 1;
            _set_px(fb, x, y, _lerp(a, b, t));
        }
}
void sdk_gradient_v(sdk_fb *fb, sdk_rect r, sdk_color top, sdk_color bottom) { _grad(fb, r, top, bottom, 1); }
void sdk_gradient_h(sdk_fb *fb, sdk_rect r, sdk_color left, sdk_color right) { _grad(fb, r, left, right, 0); }

void sdk_mask_blit(sdk_fb *fb, sdk_rect r, const uint8_t *mask, int mask_pitch, sdk_color tint) {
    r = _normalize(r);
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    sdk_rect it;
    if (!sdk_rect_intersect(r, cl, &it)) return;
    for (int y = it.y; y < it.y + it.h; ++y)
        for (int x = it.x; x < it.x + it.w; ++x) {
            int m = mask[(y - r.y) * mask_pitch + (x - r.x)];
            if (m <= 0) continue;
            sdk_color c = tint;
            c.a = (uint8_t)((int)c.a * m / 255);
            _set_px(fb, x, y, c);
        }
}

/* ---------------- box blur (separable) ---------------- */
void sdk_box_blur(const sdk_fb *src, sdk_fb *dst, int radius) {
    if (radius <= 0 || !src || !dst) return;
    int w = src->width, h = src->height;
    if (dst->width != w || dst->height != h) return;
    /* horizontal then vertical using a temp buffer */
    sdk_fb *tmp = sdk_fb_create(w, h);
    if (!tmp) return;
    int div = radius * 2 + 1;     /* every window contributes exactly div samples */
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sr=0,sg=0,sb=0,sa=0;
            for (int k = -radius; k <= radius; ++k) {
                int xx = x + k; if (xx < 0) xx = 0; if (xx >= w) xx = w-1;
                const uint8_t *p = src->pixels + (size_t)y*src->pitch + (size_t)xx*4;
                sr+=p[2]; sg+=p[1]; sb+=p[0]; sa+=p[3];
            }
            uint8_t *q = tmp->pixels + (size_t)y*tmp->pitch + (size_t)x*4;
            q[2]= (uint8_t)(sr/div); q[1]=(uint8_t)(sg/div); q[0]=(uint8_t)(sb/div); q[3]=(uint8_t)(sa/div);
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int sr=0,sg=0,sb=0,sa=0;
            for (int k = -radius; k <= radius; ++k) {
                int yy = y + k; if (yy < 0) yy = 0; if (yy >= h) yy = h-1;
                const uint8_t *p = tmp->pixels + (size_t)yy*tmp->pitch + (size_t)x*4;
                sr+=p[2]; sg+=p[1]; sb+=p[0]; sa+=p[3];
            }
            uint8_t *q = dst->pixels + (size_t)y*dst->pitch + (size_t)x*4;
            q[2]= (uint8_t)(sr/div); q[1]=(uint8_t)(sg/div); q[0]=(uint8_t)(sb/div); q[3]=(uint8_t)(sa/div);
        }
    }
    sdk_fb_destroy(tmp);
}

/* ---------------- BMP output ---------------- */
int sdk_write_bmp(const sdk_fb *fb, const char *path_utf8) {
    if (!fb || !path_utf8) return -1;
    /* Use Win32 wide path for long-path/Unicode safety */
    wchar_t wpath[4096];
    int n = MultiByteToWideChar(CP_UTF8, 0, path_utf8, -1, wpath, 4096);
    if (n <= 0) return -1;
    HANDLE fh = CreateFileW(wpath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (fh == INVALID_HANDLE_VALUE) return -1;
    int w = fb->width, h = fb->height;
    int row_bytes = ((w * 4 + 3) / 4) * 4;     /* 4-byte aligned */
    int pix_size = row_bytes * h;
    int headers = 54;
    uint8_t hdr[54];
    memset(hdr, 0, sizeof hdr);
    hdr[0]='B'; hdr[1]='M';
    uint32_t fs = (uint32_t)(headers + pix_size);
    hdr[2]=(uint8_t)(fs&0xff); hdr[3]=(uint8_t)((fs>>8)&0xff); hdr[4]=(uint8_t)((fs>>16)&0xff); hdr[5]=(uint8_t)((fs>>24)&0xff);
    hdr[10]=54;
    hdr[14]=40; /* DIB header size */
    hdr[18]=(uint8_t)(w&0xff); hdr[19]=(uint8_t)((w>>8)&0xff); hdr[20]=(uint8_t)((w>>16)&0xff); hdr[21]=(uint8_t)((w>>24)&0xff);
    /* biHeight negative => top-down */
    uint32_t hh = (uint32_t)h;
    hdr[22]=(uint8_t)((-(int32_t)hh)&0xff); hdr[23]=(uint8_t)(((-(int32_t)hh)>>8)&0xff);
    hdr[24]=(uint8_t)(((-(int32_t)hh)>>16)&0xff); hdr[25]=(uint8_t)(((-(int32_t)hh)>>24)&0xff);
    hdr[26]=1; /* planes */
    hdr[28]=32; /* bpp */
    /* compression 0, no resolution, all colors */
    uint8_t *rowbuf = (uint8_t *)malloc(row_bytes > 0 ? row_bytes : 1);
    if (!rowbuf) { CloseHandle(fh); return -1; }
    DWORD wn = 0;
    int ok = 1;
    if (!WriteFile(fh, hdr, 54, &wn, NULL) || wn != 54) ok = 0;
    if (ok) {
        for (int y = 0; y < h; ++y) {
            memset(rowbuf, 0, row_bytes);
            const uint8_t *src = fb->pixels + (size_t)y * fb->pitch;
            for (int x = 0; x < w; ++x) {
                rowbuf[x*4+0] = src[x*4+0];
                rowbuf[x*4+1] = src[x*4+1];
                rowbuf[x*4+2] = src[x*4+2];
                rowbuf[x*4+3] = src[x*4+3];
            }
            if (!WriteFile(fh, rowbuf, (DWORD)row_bytes, &wn, NULL) || wn != (DWORD)row_bytes) { ok = 0; break; }
        }
    }
    free(rowbuf);
    CloseHandle(fh);
    return ok ? 0 : -1;
}
