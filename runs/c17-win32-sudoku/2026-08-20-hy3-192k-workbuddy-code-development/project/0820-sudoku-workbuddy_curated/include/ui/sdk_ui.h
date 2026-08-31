/* sdk_ui.h - self-made software renderer / UI engine (docs/06, docs/20).
 * No native controls; single framebuffer; GDI is used ONLY for final blit. */
#ifndef SDK_UI_H
#define SDK_UI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* framebuffer & color                                                */
/* ------------------------------------------------------------------ */
/* embedded bitmap font dimensions (see ui/sdk_ui_font_data.h) */
#define SDK_FONT_W 5
#define SDK_FONT_H 7

typedef struct {
    uint8_t b, g, r, a;   /* BGRA8, top-down */
} sdk_color;

typedef struct {
    int x, y, w, h;
} sdk_rect;

typedef struct sdk_fb {
    int width, height;
    int pitch;                 /* bytes per row (>= width*4) */
    uint8_t *pixels;           /* width*height*4, owned */
    int   owns;                /* 1 if pixels owned by this fb */
    sdk_rect _clip[16];        /* nested clip stack (internal) */
    int      _clipn;
} sdk_fb;

sdk_fb  *sdk_fb_create(int w, int h);
void     sdk_fb_destroy(sdk_fb *fb);
int      sdk_fb_resize(sdk_fb *fb, int w, int h);   /* safe realloc; keeps old on fail */
sdk_color sdk_color_make(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
sdk_color sdk_color_from_u32(uint32_t rgba);        /* 0xRRGGBBAA */
uint32_t  sdk_color_to_u32(sdk_color c);

/* clipping (nested clips take intersection) */
void sdk_clip_push(sdk_fb *fb, sdk_rect r);
void sdk_clip_pop(sdk_fb *fb);
sdk_rect sdk_clip_current(const sdk_fb *fb);

/* blending & primitives */
void sdk_clear(sdk_fb *fb, sdk_color c);
void sdk_fill_rect(sdk_fb *fb, sdk_rect r, sdk_color c);
void sdk_stroke_rect(sdk_fb *fb, sdk_rect r, int thickness, sdk_color c);
void sdk_fill_round_rect(sdk_fb *fb, sdk_rect r, int radius, sdk_color c);
void sdk_stroke_round_rect(sdk_fb *fb, sdk_rect r, int radius, int thickness, sdk_color c);
void sdk_fill_disc(sdk_fb *fb, int cx, int cy, int rad, sdk_color c);
void sdk_stroke_disc(sdk_fb *fb, int cx, int cy, int rad, int thickness, sdk_color c);
void sdk_line(sdk_fb *fb, int x0, int y0, int x1, int y1, sdk_color c);
void sdk_gradient_v(sdk_fb *fb, sdk_rect r, sdk_color top, sdk_color bottom);
void sdk_gradient_h(sdk_fb *fb, sdk_rect r, sdk_color left, sdk_color right);
/* alpha-mask composite: mask is 0..255 per pixel over (r), src over dest */
void sdk_mask_blit(sdk_fb *fb, sdk_rect r, const uint8_t *mask, int mask_pitch, sdk_color tint);

/* separable box blur of src region into dst (both w*h). radius 0..N. */
void sdk_box_blur(const sdk_fb *src, sdk_fb *dst, int radius);

/* write framebuffer to 32-bit BMP (top-down). returns 0 on ok. */
int  sdk_write_bmp(const sdk_fb *fb, const char *path_utf8);

/* ------------------------------------------------------------------ */
/* bitmap font (5x7, embedded)                                        */
/* ------------------------------------------------------------------ */
int  sdk_font_height(void);                 /* 7 */
int  sdk_font_width(void);                  /* 5 */
int  sdk_text_width(const char *s);
void sdk_draw_text(sdk_fb *fb, int x, int y, const char *s, sdk_color c);
void sdk_draw_text_rect(sdk_fb *fb, sdk_rect r, const char *s, sdk_color c, int align); /* align: 0 left,1 center,2 right; v-center */

/* ------------------------------------------------------------------ */
/* easing & animation clock                                           */
/* ------------------------------------------------------------------ */
typedef enum { SDK_EASE_LINEAR, SDK_EASE_OUT, SDK_EASE_IN_OUT, SDK_EASE_EMPHASIZED } sdk_ease;
double sdk_ease_eval(sdk_ease e, double t);            /* t in 0..1 -> 0..1 */
double sdk_clamp01(double t);

/* monotonic clock (ms) - platform provides, here a fallback impl */
uint64_t sdk_monotonic_ms(void);

/* ripple pool (capped) - per-button transient feedback */
typedef struct sdk_ripple_mgr sdk_ripple_mgr;
sdk_ripple_mgr *sdk_ripple_create(int cap);
void sdk_ripple_destroy(sdk_ripple_mgr *m);
void sdk_ripple_spawn(sdk_ripple_mgr *m, int id, int x, int y, uint64_t now);
/* draws ripples clipped to given round-rect, returns nothing */
void sdk_ripple_draw(const sdk_ripple_mgr *m, sdk_fb *fb, sdk_rect bounds, int radius,
                     uint64_t now, sdk_color tint, int reduced);

/* ------------------------------------------------------------------ */
/* layout helpers                                                     */
/* ------------------------------------------------------------------ */
typedef enum { SDK_ALIGN_START, SDK_ALIGN_CENTER, SDK_ALIGN_END, SDK_ALIGN_STRETCH } sdk_align;
sdk_rect sdk_hstack(const sdk_rect *avail, int count, const int *sizes, int gap, sdk_align a, int *out);
sdk_rect sdk_vstack(const sdk_rect *avail, int count, const int *sizes, int gap, sdk_align a, int *out);
sdk_rect sdk_inset(sdk_rect r, int pad);
sdk_rect sdk_inset_ex(sdk_rect r, int l, int t, int rr, int b);
bool sdk_rect_contains(sdk_rect r, int x, int y);
bool sdk_rect_intersect(sdk_rect a, sdk_rect b, sdk_rect *out);

/* ------------------------------------------------------------------ */
/* theme tokens (docs/06 §23, docs/20)                                */
/* ------------------------------------------------------------------ */
typedef enum { SDK_THEME_DARK, SDK_THEME_LIGHT } sdk_theme_kind;
typedef struct {
    sdk_theme_kind kind;
    sdk_color surface, surface_raised, surface_sunken;
    sdk_color text_primary, text_secondary, text_on_accent;
    sdk_color accent, accent_hover, danger, success, warning, focus;
    sdk_color given_fg, player_fg, note_fg, conflict_fg;
    sdk_color sel_bg, peer_bg, same_bg;
    sdk_color backdrop;          /* modal backdrop tint */
    sdk_color shadow;
} sdk_theme;
void sdk_theme_load(sdk_theme_kind k, sdk_theme *out);

/* ------------------------------------------------------------------ */
/* combined draw context                                              */
/* ------------------------------------------------------------------ */
typedef struct {
    sdk_fb      *fb;
    const sdk_theme *theme;
    int mouse_x, mouse_y;        /* client coords */
    int mouse_down;              /* button currently held */
    int keyboard_focus;          /* current focus id */
    uint64_t now;                /* monotonic ms */
    int reduced_motion;          /* 1 to shorten/cancel motion */
} sdk_ui;

/* draw a button with hover elevation, border glow, and ripple clipped to shape.
 * variant: 0 normal, 1 accent, 2 danger. */
void sdk_draw_button(sdk_ui *ui, int id, sdk_rect r, const char *label,
                     int enabled, int hovered, int pressed, int focused,
                     int variant, sdk_ripple_mgr *ripples);
void sdk_draw_icon_button(sdk_ui *ui, int id, sdk_rect r, const char *glyph,
                          int enabled, int hovered, int pressed, int focused);
void sdk_draw_panel(sdk_ui *ui, sdk_rect r, int raised);
void sdk_draw_toast(sdk_ui *ui, sdk_rect r, const char *msg, int kind);
/* text/password field: mask!=0 renders bullets */
void sdk_draw_input(sdk_ui *ui, sdk_rect r, const char *text, int focused,
                    int password, const char *placeholder);
/* capsule tabs with animated sliding indicator (ind 0..n-1, slide 0..n-1 float) */
void sdk_draw_capsule_tabs(sdk_ui *ui, sdk_rect r, const char **labels, int n,
                           double slide, int hovered_idx);
/* modal: dims+blurs backdrop (radius from progress), draws framed panel.
 * caller draws content inside `panel` after this returns. */
void sdk_draw_modal_backdrop(sdk_ui *ui, sdk_rect panel, double open_progress,
                             int reduced);

int  sdk_hit(sdk_rect r, int x, int y);

#ifdef __cplusplus
}
#endif
#endif /* SDK_UI_H */
