/* sdk_ui_font.c - 5x7 bitmap font text rendering. */
#include "ui/sdk_ui.h"
#include "ui/sdk_ui_font_data.h"

#include <string.h>

int sdk_font_height(void) { return SDK_FONT_H; }
int sdk_font_width(void)  { return SDK_FONT_W; }

int sdk_text_width(const char *s) {
    if (!s) return 0;
    return (int)strlen(s) * SDK_FONT_W;
}

/* clip-aware pixel set (font local) */
static void _set(sdk_fb *fb, int x, int y, sdk_color c) {
    sdk_rect cl = fb->_clip[fb->_clipn - 1];
    if (x < cl.x || x >= cl.x + cl.w || y < cl.y || y >= cl.y + cl.h) return;
    if (c.a >= 255) {
        uint8_t *p = fb->pixels + (size_t)y * fb->pitch + (size_t)x * 4;
        p[0] = c.b; p[1] = c.g; p[2] = c.r; p[3] = 255;
    } else if (c.a > 0) {
        uint8_t *p = fb->pixels + (size_t)y * fb->pitch + (size_t)x * 4;
        unsigned ia = c.a, sa = 255 - c.a;
        unsigned b = p[0] * sa + c.b * ia;
        unsigned g = p[1] * sa + c.g * ia;
        unsigned r = p[2] * sa + c.r * ia;
        p[0] = (uint8_t)((b + 128) / 255);
        p[1] = (uint8_t)((g + 128) / 255);
        p[2] = (uint8_t)((r + 128) / 255);
    }
}

static void _draw_glyph(sdk_fb *fb, int code, int x, int y, sdk_color c) {
    if (code < 32 || code > 126) code = 126;
    const uint8_t *g = SDK_FONT_5X7[code - 32];
    for (int row = 0; row < SDK_FONT_H; ++row) {
        uint8_t bits = g[row];
        for (int col = 0; col < SDK_FONT_W; ++col)
            if (bits & (1u << (SDK_FONT_W - 1 - col)))
                _set(fb, x + col, y + row, c);
    }
}

void sdk_draw_text(sdk_fb *fb, int x, int y, const char *s, sdk_color c) {
    if (!s) return;
    int cx = x;
    for (const char *p = s; *p; ++p) {
        int code = (unsigned char)*p;
        _draw_glyph(fb, code, cx, y, c);
        cx += SDK_FONT_W;
    }
}

void sdk_draw_text_rect(sdk_fb *fb, sdk_rect r, const char *s, sdk_color c, int align) {
    if (!s) return;
    int tw = sdk_text_width(s);
    int x;
    if (align == 1) x = r.x + (r.w - tw) / 2;
    else if (align == 2) x = r.x + r.w - tw;
    else x = r.x;
    int y = r.y + (r.h - SDK_FONT_H) / 2;
    sdk_draw_text(fb, x, y, s, c);
}
