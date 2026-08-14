#ifndef PS_FRAMEBUFFER_H
#define PS_FRAMEBUFFER_H

#include <stdint.h>
#include <stddef.h>

typedef struct ps_framebuffer {
    int width;
    int height;
    uint32_t *pixels; /* ARGB or XRGB 32-bit */
} ps_framebuffer;

int ps_fb_init(ps_framebuffer *fb, int w, int h);
void ps_fb_free(ps_framebuffer *fb);
void ps_fb_clear(ps_framebuffer *fb, uint32_t color);
void ps_fb_set_pixel(ps_framebuffer *fb, int x, int y, uint32_t color);
void ps_fb_fill_rect(ps_framebuffer *fb, int x, int y, int w, int h, uint32_t color);
void ps_fb_draw_circle(ps_framebuffer *fb, int cx, int cy, int r, uint32_t color);
void ps_fb_draw_line(ps_framebuffer *fb, int x0, int y0, int x1, int y1, uint32_t color);

#endif /* PS_FRAMEBUFFER_H */

void ps_fb_draw_char(ps_framebuffer *fb, int x, int y, char c, uint32_t color);
void ps_fb_draw_text(ps_framebuffer *fb, int x, int y, const char *text, uint32_t color);
