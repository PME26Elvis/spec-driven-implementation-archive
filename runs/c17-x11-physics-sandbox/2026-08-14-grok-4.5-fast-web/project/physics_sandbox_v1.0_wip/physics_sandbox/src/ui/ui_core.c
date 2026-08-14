#include "ui_core.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void ps_ui_init(ps_ui_state *ui) {
    memset(ui, 0, sizeof(*ui));
    ui->active_nav = 0;
    ui->capsule_x = 20.0f;
    ui->capsule_target_x = 20.0f;
    ui->hot_widget = -1;
    ui->selected_body_id = -1;
}

int ps_ui_add_panel(ps_ui_state *ui, int x, int y, int w, int h, const char *title, bool frosted) {
    if (ui->panel_count >= PS_UI_MAX_PANELS) return -1;
    ps_panel *p = &ui->panels[ui->panel_count];
    p->id = ui->panel_count;
    p->x = x; p->y = y; p->w = w; p->h = h;
    p->visible = true;
    p->frosted = frosted;
    p->bg_color = frosted ? 0xCC1a1a2e : 0xFF16213e;
    if (title) snprintf(p->title, sizeof(p->title), "%s", title);
    else p->title[0] = 0;
    return ui->panel_count++;
}

int ps_ui_add_button(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label) {
    if (ui->widget_count >= PS_UI_MAX_WIDGETS) return -1;
    ps_widget *wdt = &ui->widgets[ui->widget_count];
    memset(wdt, 0, sizeof(*wdt));
    wdt->type = PS_W_BUTTON;
    wdt->id = ui->widget_count;
    wdt->x = x; wdt->y = y; wdt->w = w; wdt->h = h;
    wdt->visible = true; wdt->enabled = true;
    wdt->parent_panel = panel;
    if (label) snprintf(wdt->label, sizeof(wdt->label), "%s", label);
    return ui->widget_count++;
}

int ps_ui_add_slider(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label, float minv, float maxv, float val) {
    if (ui->widget_count >= PS_UI_MAX_WIDGETS) return -1;
    ps_widget *wdt = &ui->widgets[ui->widget_count];
    memset(wdt, 0, sizeof(*wdt));
    wdt->type = PS_W_SLIDER;
    wdt->id = ui->widget_count;
    wdt->x = x; wdt->y = y; wdt->w = w; wdt->h = h;
    wdt->visible = true; wdt->enabled = true;
    wdt->parent_panel = panel;
    wdt->min_v = minv; wdt->max_v = maxv; wdt->value = val;
    if (label) snprintf(wdt->label, sizeof(wdt->label), "%s", label);
    return ui->widget_count++;
}

int ps_ui_add_checkbox(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *label, bool checked) {
    if (ui->widget_count >= PS_UI_MAX_WIDGETS) return -1;
    ps_widget *wdt = &ui->widgets[ui->widget_count];
    memset(wdt, 0, sizeof(*wdt));
    wdt->type = PS_W_CHECKBOX;
    wdt->id = ui->widget_count;
    wdt->x = x; wdt->y = y; wdt->w = w; wdt->h = h;
    wdt->visible = true; wdt->enabled = true;
    wdt->parent_panel = panel;
    wdt->active = checked;
    if (label) snprintf(wdt->label, sizeof(wdt->label), "%s", label);
    return ui->widget_count++;
}

int ps_ui_add_label(ps_ui_state *ui, int panel, int x, int y, int w, int h, const char *text) {
    if (ui->widget_count >= PS_UI_MAX_WIDGETS) return -1;
    ps_widget *wdt = &ui->widgets[ui->widget_count];
    memset(wdt, 0, sizeof(*wdt));
    wdt->type = PS_W_LABEL;
    wdt->id = ui->widget_count;
    wdt->x = x; wdt->y = y; wdt->w = w; wdt->h = h;
    wdt->visible = true; wdt->enabled = true;
    wdt->parent_panel = panel;
    if (text) snprintf(wdt->label, sizeof(wdt->label), "%s", text);
    return ui->widget_count++;
}

void ps_ui_set_nav(ps_ui_state *ui, int index) {
    ui->active_nav = index;
    /* capsule targets: approx positions for 4 nav items */
    float targets[] = { 30.f, 130.f, 250.f, 400.f };
    if (index >= 0 && index < 4) ui->capsule_target_x = targets[index];
}

void ps_ui_show_modal(ps_ui_state *ui, const char *title, const char *msg) {
    ui->modal_visible = 1;
    ui->modal_result = 0;
    if (title) snprintf(ui->modal_title, sizeof(ui->modal_title), "%s", title);
    if (msg) snprintf(ui->modal_message, sizeof(ui->modal_message), "%s", msg);
}

void ps_fb_box_blur(ps_framebuffer *fb, int x, int y, int w, int h, int radius) {
    if (radius < 1 || !fb->pixels) return;
    int W = fb->width, H = fb->height;
    /* simple separable box blur into a temp buffer for the region */
    uint32_t *tmp = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
    if (!tmp) return;
    /* horizontal */
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int r=0,g=0,b=0,a=0,c=0;
            for (int k = -radius; k <= radius; k++) {
                int xx = x + i + k;
                int yy = y + j;
                if (xx < 0 || xx >= W || yy < 0 || yy >= H) continue;
                uint32_t pix = fb->pixels[yy * W + xx];
                a += (pix >> 24) & 0xff;
                r += (pix >> 16) & 0xff;
                g += (pix >> 8) & 0xff;
                b += pix & 0xff;
                c++;
            }
            if (c < 1) c = 1;
            tmp[j * w + i] = ((a/c)<<24) | ((r/c)<<16) | ((g/c)<<8) | (b/c);
        }
    }
    /* vertical back to fb */
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            int r=0,g=0,b=0,a=0,c=0;
            for (int k = -radius; k <= radius; k++) {
                int yy = j + k;
                if (yy < 0 || yy >= h) continue;
                uint32_t pix = tmp[yy * w + i];
                a += (pix >> 24) & 0xff;
                r += (pix >> 16) & 0xff;
                g += (pix >> 8) & 0xff;
                b += pix & 0xff;
                c++;
            }
            if (c < 1) c = 1;
            int xx = x + i, yy = y + j;
            if (xx >= 0 && xx < W && yy >= 0 && yy < H)
                fb->pixels[yy * W + xx] = ((a/c)<<24) | ((r/c)<<16) | ((g/c)<<8) | (b/c);
        }
    }
    free(tmp);
}

static bool hit(ps_widget *w, int mx, int my) {
    return mx >= w->x && mx < w->x + w->w && my >= w->y && my < w->y + w->h;
}

void ps_ui_handle_mouse(ps_ui_state *ui, int mx, int my, int down, int up) {
    if (ui->modal_visible) {
        /* simple modal buttons at bottom of modal */
        int mw = 320, mh = 140;
        int mx0 = (/*assume*/ 400) - mw/2; /* will be centered in draw */
        (void)mx0;
        if (down) {
            /* approximate: any click in lower half = OK for simplicity */
            ui->modal_result = 1;
            ui->modal_visible = 0;
        }
        return;
    }
    ui->hot_widget = -1;
    for (int i = ui->widget_count - 1; i >= 0; i--) {
        ps_widget *w = &ui->widgets[i];
        if (!w->visible || !w->enabled) continue;
        if (hit(w, mx, my)) {
            ui->hot_widget = i;
            w->hovered = true;
            if (down) {
                w->pressed = true;
                if (w->type == PS_W_CHECKBOX) w->active = !w->active;
                if (w->type == PS_W_SLIDER) {
                    float t = (float)(mx - w->x) / (float)w->w;
                    if (t < 0) t = 0; if (t > 1) t = 1;
                    w->value = w->min_v + t * (w->max_v - w->min_v);
                }
                if (w->type == PS_W_NAV_ITEM) {
                    ps_ui_set_nav(ui, w->id); /* reuse id or store index in value */
                }
            }
            if (up) w->pressed = false;
            break;
        } else {
            w->hovered = false;
            if (up) w->pressed = false;
        }
    }
    /* continuous slider drag */
    if (ui->hot_widget >= 0 && ui->widgets[ui->hot_widget].type == PS_W_SLIDER &&
        ui->widgets[ui->hot_widget].pressed) {
        ps_widget *w = &ui->widgets[ui->hot_widget];
        float t = (float)(mx - w->x) / (float)w->w;
        if (t < 0) t = 0; if (t > 1) t = 1;
        w->value = w->min_v + t * (w->max_v - w->min_v);
    }
}

void ps_ui_update(ps_ui_state *ui, float dt) {
    /* animate capsule */
    float dx = ui->capsule_target_x - ui->capsule_x;
    ui->capsule_x += dx * fminf(1.0f, dt * 12.0f);
}

static void draw_widget(ps_framebuffer *fb, ps_widget *w) {
    if (!w->visible) return;
    switch (w->type) {
    case PS_W_BUTTON: {
        uint32_t bg = w->pressed ? 0xFF4a90d9 : (w->hovered ? 0xFF3a5a8a : 0xFF2a3a5a);
        ps_fb_fill_rect(fb, w->x, w->y, w->w, w->h, bg);
        /* border */
        for (int i=0;i<w->w;i++){ ps_fb_set_pixel(fb,w->x+i,w->y,0xFF88aacc); ps_fb_set_pixel(fb,w->x+i,w->y+w->h-1,0xFF88aacc); }
        for (int i=0;i<w->h;i++){ ps_fb_set_pixel(fb,w->x,w->y+i,0xFF88aacc); ps_fb_set_pixel(fb,w->x+w->w-1,w->y+i,0xFF88aacc); }
        ps_fb_draw_text(fb, w->x+6, w->y+(w->h-7)/2, w->label, 0xFFeeeeee);
        break;
    }
    case PS_W_SLIDER: {
        ps_fb_draw_text(fb, w->x, w->y - 10, w->label, 0xFFaaccff);
        ps_fb_fill_rect(fb, w->x, w->y + w->h/2 - 2, w->w, 4, 0xFF333355);
        float t = (w->value - w->min_v) / (w->max_v - w->min_v + 1e-6f);
        int kx = w->x + (int)(t * (w->w - 8));
        ps_fb_fill_rect(fb, kx, w->y, 8, w->h, 0xFF4a90d9);
        break;
    }
    case PS_W_CHECKBOX: {
        ps_fb_fill_rect(fb, w->x, w->y, 14, 14, 0xFF222244);
        if (w->active) ps_fb_fill_rect(fb, w->x+3, w->y+3, 8, 8, 0xFF4a90d9);
        ps_fb_draw_text(fb, w->x + 20, w->y + 3, w->label, 0xFFcccccc);
        break;
    }
    case PS_W_LABEL:
        ps_fb_draw_text(fb, w->x, w->y, w->label, 0xFFaaaaaa);
        break;
    default: break;
    }
}

void ps_ui_draw(ps_ui_state *ui, ps_framebuffer *fb) {
    /* top nav bar */
    int nav_h = 36;
    ps_fb_fill_rect(fb, 0, 0, fb->width, nav_h, 0xEE0d0d1a);
    /* frosted-ish top */
    /* capsule */
    int cap_w = 90, cap_h = 26;
    int cy = 5;
    int cx = (int)ui->capsule_x;
    ps_fb_fill_rect(fb, cx, cy, cap_w, cap_h, 0xFF3a5a9a);
    /* nav labels */
    const char *navs[] = {"SANDBOX", "SCENES", "DIAG", "ABOUT"};
    int nx[] = {35, 135, 255, 405};
    for (int i = 0; i < 4; i++) {
        uint32_t col = (i == ui->active_nav) ? 0xFFffffff : 0xFF8899aa;
        ps_fb_draw_text(fb, nx[i], 12, navs[i], col);
    }

    /* panels */
    for (int pi = 0; pi < ui->panel_count; pi++) {
        ps_panel *p = &ui->panels[pi];
        if (!p->visible) continue;
        if (p->frosted) {
            ps_fb_box_blur(fb, p->x, p->y, p->w, p->h, 2);
            /* semi-transparent overlay */
            for (int j = 0; j < p->h; j++) {
                for (int i = 0; i < p->w; i++) {
                    int xx = p->x + i, yy = p->y + j;
                    if (xx < 0 || yy < 0 || xx >= fb->width || yy >= fb->height) continue;
                    uint32_t pix = fb->pixels[yy * fb->width + xx];
                    int r = ((pix>>16)&0xff)*3/4 + 20;
                    int g = ((pix>>8)&0xff)*3/4 + 20;
                    int b = (pix&0xff)*3/4 + 40;
                    fb->pixels[yy * fb->width + xx] = 0xFF000000 | (r<<16)|(g<<8)|b;
                }
            }
        } else {
            ps_fb_fill_rect(fb, p->x, p->y, p->w, p->h, p->bg_color);
        }
        if (p->title[0]) ps_fb_draw_text(fb, p->x + 8, p->y + 6, p->title, 0xFFaaccff);
        /* border */
        for (int i=0;i<p->w;i++){ ps_fb_set_pixel(fb,p->x+i,p->y,0xFF445566); ps_fb_set_pixel(fb,p->x+i,p->y+p->h-1,0xFF445566); }
        for (int i=0;i<p->h;i++){ ps_fb_set_pixel(fb,p->x,p->y+i,0xFF445566); ps_fb_set_pixel(fb,p->x+p->w-1,p->y+i,0xFF445566); }
    }

    /* widgets */
    for (int i = 0; i < ui->widget_count; i++) {
        draw_widget(fb, &ui->widgets[i]);
    }

    /* modal */
    if (ui->modal_visible) {
        int mw = 340, mh = 150;
        int mx = (fb->width - mw) / 2;
        int my = (fb->height - mh) / 2;
        ps_fb_box_blur(fb, mx, my, mw, mh, 3);
        ps_fb_fill_rect(fb, mx, my, mw, mh, 0xEE1a1a2e);
        for (int i=0;i<mw;i++){ ps_fb_set_pixel(fb,mx+i,my,0xFF6688aa); ps_fb_set_pixel(fb,mx+i,my+mh-1,0xFF6688aa); }
        ps_fb_draw_text(fb, mx+16, my+16, ui->modal_title, 0xFFffffff);
        ps_fb_draw_text(fb, mx+16, my+50, ui->modal_message, 0xFFcccccc);
        /* OK button */
        ps_fb_fill_rect(fb, mx+mw/2-40, my+mh-40, 80, 28, 0xFF4a90d9);
        ps_fb_draw_text(fb, mx+mw/2-10, my+mh-32, "OK", 0xFFffffff);
    }
}
