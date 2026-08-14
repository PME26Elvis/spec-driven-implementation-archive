#include "framebuffer.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

int ps_fb_init(ps_framebuffer *fb, int w, int h) {
    if (w <= 0 || h <= 0) return -1;
    fb->width = w;
    fb->height = h;
    fb->pixels = (uint32_t *)calloc((size_t)w * h, sizeof(uint32_t));
    return fb->pixels ? 0 : -1;
}

void ps_fb_free(ps_framebuffer *fb) {
    free(fb->pixels);
    fb->pixels = NULL;
    fb->width = fb->height = 0;
}

void ps_fb_clear(ps_framebuffer *fb, uint32_t color) {
    if (!fb->pixels) return;
    size_t n = (size_t)fb->width * fb->height;
    for (size_t i = 0; i < n; i++) fb->pixels[i] = color;
}

void ps_fb_set_pixel(ps_framebuffer *fb, int x, int y, uint32_t color) {
    if (x < 0 || y < 0 || x >= fb->width || y >= fb->height) return;
    fb->pixels[y * fb->width + x] = color;
}

void ps_fb_fill_rect(ps_framebuffer *fb, int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            ps_fb_set_pixel(fb, x + i, y + j, color);
        }
    }
}

void ps_fb_draw_circle(ps_framebuffer *fb, int cx, int cy, int r, uint32_t color) {
    if (r <= 0) return;
    int x = r, y = 0;
    int err = 0;
    while (x >= y) {
        ps_fb_set_pixel(fb, cx + x, cy + y, color);
        ps_fb_set_pixel(fb, cx + y, cy + x, color);
        ps_fb_set_pixel(fb, cx - y, cy + x, color);
        ps_fb_set_pixel(fb, cx - x, cy + y, color);
        ps_fb_set_pixel(fb, cx - x, cy - y, color);
        ps_fb_set_pixel(fb, cx - y, cy - x, color);
        ps_fb_set_pixel(fb, cx + y, cy - x, color);
        ps_fb_set_pixel(fb, cx + x, cy - y, color);
        y++;
        err += 1 + 2 * y;
        if (2 * (err - x) + 1 > 0) {
            x--;
            err += 1 - 2 * x;
        }
    }
}

void ps_fb_draw_line(ps_framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        ps_fb_set_pixel(fb, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Minimal 5x7 bitmap font for digits and basic letters (ASCII subset) */
static const unsigned char font5x7[][7] = {
    /* space */ {0,0,0,0,0,0,0},
    /* 0 */ {0x0e,0x11,0x13,0x15,0x19,0x11,0x0e},
    /* 1 */ {0x04,0x0c,0x04,0x04,0x04,0x04,0x0e},
    /* 2 */ {0x0e,0x11,0x01,0x02,0x04,0x08,0x1f},
    /* 3 */ {0x0e,0x11,0x01,0x06,0x01,0x11,0x0e},
    /* 4 */ {0x02,0x06,0x0a,0x12,0x1f,0x02,0x02},
    /* 5 */ {0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e},
    /* 6 */ {0x06,0x08,0x10,0x1e,0x11,0x11,0x0e},
    /* 7 */ {0x1f,0x01,0x02,0x04,0x08,0x08,0x08},
    /* 8 */ {0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e},
    /* 9 */ {0x0e,0x11,0x11,0x0f,0x01,0x02,0x0c},
};

static int glyph_index(char c) {
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    return 0;
}

void ps_fb_draw_char(ps_framebuffer *fb, int x, int y, char c, uint32_t color) {
    int idx = glyph_index(c);
    for (int row = 0; row < 7; row++) {
        unsigned char bits = font5x7[idx][row];
        for (int col = 0; col < 5; col++) {
            if (bits & (1 << (4 - col)))
                ps_fb_set_pixel(fb, x + col, y + row, color);
        }
    }
}

void ps_fb_draw_text(ps_framebuffer *fb, int x, int y, const char *text, uint32_t color) {
    while (*text) {
        ps_fb_draw_char(fb, x, y, *text, color);
        x += 6;
        text++;
    }
}
