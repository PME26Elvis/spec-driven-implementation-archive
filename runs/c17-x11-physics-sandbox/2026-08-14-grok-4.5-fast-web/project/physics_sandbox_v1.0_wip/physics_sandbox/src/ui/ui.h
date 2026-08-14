#ifndef PS_UI_H
#define PS_UI_H
#include "../render/framebuffer.h"
#include <stdbool.h>

typedef struct {
    int x, y, w, h;
    const char *label;
    bool pressed;
    bool hovered;
} ps_button;

void ps_button_draw(ps_framebuffer *fb, ps_button *btn);
bool ps_button_hit(ps_button *btn, int mx, int my);
void ps_button_set_pressed(ps_button *btn, bool v);

#endif
