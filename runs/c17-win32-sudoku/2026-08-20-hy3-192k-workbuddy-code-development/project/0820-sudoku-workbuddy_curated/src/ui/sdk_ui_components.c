/* sdk_ui_components.c - self-made components (docs/06 §7-§14, §19-§24). */
#include "ui/sdk_ui.h"

#include <stdlib.h>
#include <string.h>

int sdk_hit(sdk_rect r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

/* soft radial-ish glow via concentric translucent round rects (spatial falloff) */
static void _glow(sdk_fb *fb, sdk_rect r, int radius, sdk_color c) {
    for (int i = 3; i >= 1; --i) {
        sdk_rect g = { r.x - i*radius, r.y - i*radius, r.w + 2*i*radius, r.h + 2*i*radius };
        sdk_color gc = c; gc.a = (uint8_t)(c.a * (i == 1 ? 1.0 : (i == 2 ? 0.55 : 0.28)));
        sdk_fill_round_rect(fb, g, radius + 2, gc);
    }
}

static sdk_color _btn_base(sdk_ui *ui, int variant) {
    if (variant == 1) return ui->theme->accent;
    if (variant == 2) return ui->theme->danger;
    return ui->theme->surface_raised;
}
static sdk_color _btn_hover(sdk_ui *ui, int variant) {
    if (variant == 1) return ui->theme->accent_hover;
    if (variant == 2) return ui->theme->danger;
    return ui->theme->surface_raised;
}

void sdk_draw_button(sdk_ui *ui, int id, sdk_rect r, const char *label,
                     int enabled, int hovered, int pressed, int focused,
                     int variant, sdk_ripple_mgr *ripples) {
    (void)id;
    sdk_fb *fb = ui->fb;
    int elev = (hovered && enabled && !ui->reduced_motion) ? 3 : 0;
    sdk_rect dr = { r.x, r.y - elev, r.w, r.h };   /* visual lift only */

    if (focused || (hovered && enabled)) {
        sdk_color gc = (variant == 2) ? ui->theme->danger : ui->theme->focus;
        gc.a = focused ? 90 : 55;
        _glow(fb, dr, 4, gc);
    }
    /* shadow */
    sdk_color sh = ui->theme->shadow;
    sdk_fill_round_rect(fb, (sdk_rect){dr.x+2, dr.y + (pressed?1:3), dr.w, dr.h}, 10, sh);

    sdk_color base = !enabled ? ui->theme->surface_sunken
                  : (pressed ? _btn_base(ui,variant) : (hovered ? _btn_hover(ui,variant) : _btn_base(ui,variant)));
    sdk_fill_round_rect(fb, dr, 10, base);

    sdk_color txt = variant == 0 ? (enabled ? ui->theme->text_primary : ui->theme->text_secondary)
                                 : ui->theme->text_on_accent;
    if (!enabled) txt.a = 150;
    int tx = dr.x + (dr.w - sdk_text_width(label)) / 2;
    int ty = dr.y + (dr.h - SDK_FONT_H) / 2 + (pressed ? 1 : 0);
    sdk_draw_text(fb, tx, ty, label, txt);

    if (ripples && enabled)
        sdk_ripple_draw(ripples, fb, dr, 0, ui->now, ui->theme->text_on_accent, ui->reduced_motion);
    if (focused) {
        sdk_color fc = ui->theme->focus; fc.a = 220;
        sdk_stroke_round_rect(fb, (sdk_rect){dr.x-1,dr.y-1,dr.w+2,dr.h+2}, 11, 2, fc);
    }
}

void sdk_draw_icon_button(sdk_ui *ui, int id, sdk_rect r, const char *glyph,
                          int enabled, int hovered, int pressed, int focused) {
    (void)id;
    sdk_fb *fb = ui->fb;
    if (focused || hovered) {
        sdk_color gc = ui->theme->focus; gc.a = focused ? 90 : 50;
        _glow(fb, r, 3, gc);
    }
    sdk_fill_round_rect(fb, (sdk_rect){r.x, r.y + (pressed?1:0), r.w, r.h}, 8,
                        hovered ? ui->theme->surface_raised : ui->theme->surface_sunken);
    sdk_color c = enabled ? ui->theme->text_primary : ui->theme->text_secondary;
    if (!enabled) c.a = 140;
    sdk_draw_text_rect(fb, r, glyph, c, 1);
    if (focused) { sdk_color fc = ui->theme->focus; fc.a = 220;
        sdk_stroke_round_rect(fb, (sdk_rect){r.x-1,r.y-1,r.w+2,r.h+2}, 9, 2, fc); }
}

void sdk_draw_panel(sdk_ui *ui, sdk_rect r, int raised) {
    sdk_color c = raised ? ui->theme->surface_raised : ui->theme->surface;
    sdk_fill_round_rect(ui->fb, r, 14, c);
    sdk_color edge = ui->theme->text_secondary; edge.a = 40;
    sdk_stroke_round_rect(ui->fb, (sdk_rect){r.x,r.y,r.w,r.h}, 14, 1, edge);
}

void sdk_draw_toast(sdk_ui *ui, sdk_rect r, const char *msg, int kind) {
    sdk_color bg = ui->theme->surface_raised;
    sdk_fill_round_rect(ui->fb, r, 10, bg);
    sdk_color bar = kind == 2 ? ui->theme->danger : (kind == 1 ? ui->theme->success : ui->theme->accent);
    sdk_fill_round_rect(ui->fb, (sdk_rect){r.x, r.y, 5, r.h}, 2, bar);
    sdk_draw_text_rect(ui->fb, sdk_inset_ex(r,10,0,10,0), msg, ui->theme->text_primary, 0);
}

void sdk_draw_input(sdk_ui *ui, sdk_rect r, const char *text, int focused,
                    int password, const char *placeholder) {
    sdk_fb *fb = ui->fb;
    sdk_fill_round_rect(fb, r, 8, ui->theme->surface_sunken);
    sdk_color edge = focused ? ui->theme->focus : ui->theme->text_secondary;
    edge.a = focused ? 220 : 60;
    sdk_stroke_round_rect(fb, (sdk_rect){r.x,r.y,r.w,r.h}, 8, focused?2:1, edge);
    int has = text && text[0];
    if (!has && placeholder) {
        sdk_color ph = ui->theme->text_secondary; ph.a = 150;
        sdk_draw_text_rect(fb, sdk_inset_ex(r,10,0,10,0), placeholder, ph, 0);
        return;
    }
    if (password) {
        int n = (int)strlen(text);
        int dots = n > 64 ? 64 : n;
        int x = r.x + 10;
        for (int i = 0; i < dots; ++i) { sdk_draw_text(fb, x, r.y + (r.h - SDK_FONT_H)/2, "*", ui->theme->text_primary); x += SDK_FONT_W; }
    } else {
        sdk_draw_text(fb, r.x + 10, r.y + (r.h - SDK_FONT_H)/2, text, ui->theme->text_primary);
    }
}

void sdk_draw_capsule_tabs(sdk_ui *ui, sdk_rect r, const char **labels, int n,
                           double slide, int hovered_idx) {
    sdk_fb *fb = ui->fb;
    sdk_fill_round_rect(fb, r, r.h/2, ui->theme->surface_sunken);
    if (n <= 0) return;
    if (slide < 0) slide = 0; if (slide > n-1) slide = n-1;
    int cellw = r.w / n;
    int ind_w = cellw - 6;
    int ind_x = r.x + 3 + (int)(slide * cellw);
    sdk_rect ind = { ind_x, r.y + 3, ind_w, r.h - 6 };
    sdk_fill_round_rect(fb, ind, ind.h/2, ui->theme->accent);
    for (int i = 0; i < n; ++i) {
        sdk_rect cr = { r.x + i*cellw, r.y, cellw, r.h };
        sdk_color c = (i == (int)(slide + 0.5)) ? ui->theme->text_on_accent : ui->theme->text_secondary;
        if (i == hovered_idx && i != (int)(slide+0.5)) c = ui->theme->text_primary;
        sdk_draw_text_rect(fb, cr, labels[i], c, 1);
    }
}

/* ---- modal & frosted (real blur of the application's own background) ---- */
static sdk_fb *g_blur_tmp = NULL;
static void _ensure_tmp(int w, int h) {
    if (g_blur_tmp && g_blur_tmp->width == w && g_blur_tmp->height == h) return;
    if (g_blur_tmp) sdk_fb_destroy(g_blur_tmp);
    g_blur_tmp = sdk_fb_create(w, h);
}
static void _copy_region(const sdk_fb *fb, sdk_rect src, sdk_fb *tmp) {
    for (int y = 0; y < src.h; ++y)
        memcpy(tmp->pixels + (size_t)y*tmp->pitch,
               fb->pixels + (size_t)(src.y+y)*fb->pitch + (size_t)src.x*4,
               (size_t)src.w*4);
}
static void _put_region(sdk_fb *fb, sdk_rect dst, const sdk_fb *tmp) {
    for (int y = 0; y < dst.h; ++y)
        memcpy(fb->pixels + (size_t)(dst.y+y)*fb->pitch + (size_t)dst.x*4,
               tmp->pixels + (size_t)y*tmp->pitch, (size_t)dst.w*4);
}

void sdk_draw_modal_backdrop(sdk_ui *ui, sdk_rect panel,
                             double open_progress, int reduced) {
    (void)panel;
    sdk_fb *fb = ui->fb;
    double p = sdk_clamp01(open_progress);
    int radius = reduced ? 2 : (int)(p * 9.0);
    sdk_rect full = {0,0,fb->width,fb->height};
    if (radius > 0) {
        _ensure_tmp(fb->width, fb->height);
        if (g_blur_tmp) {
            sdk_box_blur(fb, g_blur_tmp, radius);
            _put_region(fb, full, g_blur_tmp);
        }
    }
    /* dim */
    sdk_color dim = ui->theme->backdrop; dim.a = (uint8_t)(dim.a * p);
    sdk_fill_rect(fb, full, dim);
}

/* frosted glass over an already-rendered region (Library nav) */
void sdk_draw_frost(sdk_ui *ui, sdk_rect region, int radius, sdk_color tint, int reduced) {
    sdk_fb *fb = ui->fb;
    sdk_rect cl = fb->_clip[fb->_clipn-1];
    sdk_rect it; if (!sdk_rect_intersect(region, cl, &it)) return;
    int rad = reduced ? (radius>0?1:0) : radius;
    _ensure_tmp(it.w, it.h);
    if (!g_blur_tmp) return;
    /* build an sdk_fb view over a fresh buffer */
    sdk_fb *srcv = sdk_fb_create(it.w, it.h);
    if (!srcv) return;
    _copy_region(fb, it, srcv);
    sdk_fb *bl = sdk_fb_create(it.w, it.h);
    if (bl) {
        if (rad > 0) sdk_box_blur(srcv, bl, rad); else _copy_region(srcv, (sdk_rect){0,0,it.w,it.h}, bl);
        /* blit blurred back into fb region (overwrite) */
        for (int y = 0; y < it.h; ++y)
            memcpy(fb->pixels + (size_t)(it.y+y)*fb->pitch + (size_t)it.x*4,
                   bl->pixels + (size_t)y*bl->pitch, (size_t)it.w*4);
        sdk_fb_destroy(bl);
    }
    sdk_fb_destroy(srcv);
    /* tint */
    if (tint.a > 0) sdk_fill_rect(fb, it, tint);
}
