#include "ui.h"
#include <string.h>

void ps_button_draw(ps_framebuffer *fb, ps_button *btn) {
    uint32_t bg = btn->pressed ? 0xFF4a90d9 : (btn->hovered ? 0xFF3a5a8a : 0xFF2a3a5a);
    uint32_t border = 0xFF88aacc;
    ps_fb_fill_rect(fb, btn->x, btn->y, btn->w, btn->h, bg);
    /* border */
    for (int i = 0; i < btn->w; i++) {
        ps_fb_set_pixel(fb, btn->x + i, btn->y, border);
        ps_fb_set_pixel(fb, btn->x + i, btn->y + btn->h - 1, border);
    }
    for (int i = 0; i < btn->h; i++) {
        ps_fb_set_pixel(fb, btn->x, btn->y + i, border);
        ps_fb_set_pixel(fb, btn->x + btn->w - 1, btn->y + i, border);
    }
    if (btn->label) {
        int tx = btn->x + 6;
        int ty = btn->y + (btn->h - 7) / 2;
        ps_fb_draw_text(fb, tx, ty, btn->label, 0xFFeeeeee);
    }
}

bool ps_button_hit(ps_button *btn, int mx, int my) {
    return mx >= btn->x && mx < btn->x + btn->w &&
           my >= btn->y && my < btn->y + btn->h;
}

void ps_button_set_pressed(ps_button *btn, bool v) {
    btn->pressed = v;
}
