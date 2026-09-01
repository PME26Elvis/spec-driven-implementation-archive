/* render.c - software framebuffer rendering: shapes, text, and effects. */
#include "app.h"
#include "ce_common.h"
#include "utf8.h"
#include "winutil.h"
#include <stdio.h>
#include <math.h>

Theme g_light = {
    {0xFA,0xFA,0xFA,0xFF},{0xF0,0xF0,0xF0,0xFF},{0xFF,0xFF,0xFF,0xFF},
    {0xF4,0xF4,0xF4,0xFF},{0xE8,0xE8,0xEC,0xFF},{0xE0,0xE0,0xE4,0xFF},
    {0x1A,0x1A,0x1E,0xFF},{0x55,0x55,0x5C,0xFF},{0x8A,0x8A,0x92,0xFF},
    {0x3B,0x82,0xF6,0xFF},{0x25,0x63,0xEB,0xFF},{0xD6,0xE4,0xFF,0xFF},
    {0xE2,0xEC,0xFF,0xFF},{0x3B,0x82,0xF6,0xFF},{0xFF,0xFF,0xFF,0xFF},
    {0xF6,0xF8,0xFA,0xFF},{0xD8,0xDC,0xE2,0xFF},{0xEF,0xF1,0xF4,0xFF},
    {0xE8,0xE0,0xC8,0xFF},{0x25,0x63,0xEB,0xFF},{0xFF,0xE0,0xE0,0xFF},
    {0xE0,0xFF,0xE0,0xFF},{0xFF,0xF0,0xC8,0xFF},{0xE0,0x3A,0x3A,0xFF},
    {0xC8,0x7A,0x1A,0xFF},{0x1E,0xA0,0x50,0xFF},
    {0xF2,0xF4,0xF7,0xF0},{0xEE,0xEE,0xF2,0xFF},{0x2A,0x2A,0x3A,0xFF},
    {0x80,0x80,0x90,0x28}
};

Theme g_dark = {
    {0x1E,0x1E,0x22,0xFF},{0x18,0x18,0x1C,0xFF},{0x24,0x24,0x28,0xFF},
    {0x28,0x28,0x2C,0xFF},{0x2E,0x2E,0x34,0xFF},{0x3A,0x3A,0x42,0xFF},
    {0xE8,0xE8,0xEC,0xFF},{0xA0,0xA0,0xAC,0xFF},{0x6A,0x6A,0x76,0xFF},
    {0x4C,0x8D,0xFF,0xFF},{0x2F,0x6F,0xF0,0xFF},{0x2A,0x3A,0x5C,0xFF},
    {0x2E,0x44,0x70,0xFF},{0x6A,0xA0,0xFF,0xFF},{0xE8,0xE8,0xEC,0xFF},
    {0x1A,0x1E,0x24,0xFF},{0x3A,0x40,0x4A,0xFF},{0x26,0x2C,0x34,0xFF},
    {0x3A,0x30,0x20,0xFF},{0x6A,0xA0,0xFF,0xFF},{0x5C,0x2A,0x2A,0xFF},
    {0x24,0x40,0x26,0xFF},{0x4A,0x3A,0x22,0xFF},{0xF0,0x60,0x60,0xFF},
    {0xE0,0xA0,0x40,0xFF},{0x50,0xD0,0x80,0xFF},
    {0x14,0x14,0x18,0xF0},{0x10,0x10,0x14,0xFF},{0xE0,0xE0,0xF0,0xFF},
    {0x00,0x00,0x00,0x50}
};

/* ---------------- framebuffer management ---------------- */

static void fb_create(App *a, int w, int h){
    if(a->dib){ SelectObject(a->memdc, a->old_bmp); DeleteObject(a->dib); a->dib = NULL; }
    HDC screen = GetDC(NULL);
    BITMAPINFO bi; memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;   /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    a->dib = CreateDIBSection(screen, &bi, DIB_RGB_COLORS, &a->fb, NULL, 0);
    ReleaseDC(NULL, screen);
    a->old_bmp = (HBITMAP)SelectObject(a->memdc, a->dib);
    a->fb_w = w; a->fb_h = h; a->fb_stride = w * 4;
}

void render_init(App *a){
    a->scale = 1.0; a->dpi = 96;
    HDC screen = GetDC(NULL);
    a->memdc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    fb_create(a, a->width, a->height);
    a->font_px = a->prefs.font_size;
    a->font = CreateFontW(-a->font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    a->font_bold = CreateFontW(-a->font_px, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    a->font_mono = CreateFontW(-a->font_px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Consolas");
    a->font_ui = a->font;
}

void render_resize(App *a, int w, int h){
    a->width = w; a->height = h;
    fb_create(a, w, h);
}

/* ---------------- pixel helpers ---------------- */

static inline void put_px(App *a, int x, int y, uint32_t v){
    if(x < 0 || y < 0 || x >= a->fb_w || y >= a->fb_h) return;
    ((uint32_t*)a->fb)[y * a->fb_w + x] = v;
}

static inline uint32_t blend_src_over(uint32_t dst, Color s){
    int sa = s.a, da = 255 - sa;
    uint32_t db = dst & 0xFF, dg = (dst >> 8) & 0xFF, dr = (dst >> 16) & 0xFF;
    uint32_t b = (s.b * sa + db * da + 127) / 255;
    uint32_t g = (s.g * sa + dg * da + 127) / 255;
    uint32_t r = (s.r * sa + dr * da + 127) / 255;
    return (0xFF000000) | (r << 16) | (g << 8) | b;
}

static inline uint32_t get_px(App *a, int x, int y){
    if(x < 0 || y < 0 || x >= a->fb_w || y >= a->fb_h) return 0;
    return ((uint32_t*)a->fb)[y * a->fb_w + x];
}

void ui_draw_rect(App *a, int x, int y, int w, int h, Color c){
    if(w <= 0 || h <= 0) return;
    for(int j = 0; j < h; j++){
        for(int i = 0; i < w; i++) put_px(a, x + i, y + j, blend_src_over(get_px(a, x+i, y+j), c));
    }
}

void ui_fill_round(App *a, int x, int y, int w, int h, int r, Color c){
    if(w <= 0 || h <= 0) return;
    if(r <= 0){ ui_draw_rect(a, x, y, w, h, c); return; }
    if(r > w/2) r = w/2; if(r > h/2) r = h/2;
    for(int j = 0; j < h; j++){
        for(int i = 0; i < w; i++){
            int dx = 0, dy = 0;
            if(i < r) dx = r - i;
            else if(i >= w - r) dx = i - (w - r) + 1;
            if(j < r) dy = r - j;
            else if(j >= h - r) dy = j - (h - r) + 1;
            float d = 0;
            if(dx > 0 && dy > 0) d = sqrtf((float)(dx*dx + dy*dy));
            else d = (dx > 0 ? (float)dx : (dy > 0 ? (float)dy : 0));
            if(d > r) continue;
            int alpha = c.a;
            if(d > r - 1 && d > 0){ float f = r - d; if(f < 0) f = 0; if(f < 1) alpha = (int)(c.a * f); }
            Color cc = c; cc.a = (uint8_t)alpha;
            put_px(a, x + i, y + j, blend_src_over(get_px(a, x+i, y+j), cc));
        }
    }
}

/* ---------------- text ---------------- */

static HFONT select_font(App *a, bool bold, bool mono, int size){
    HFONT f = bold ? a->font_bold : (mono ? a->font_mono : a->font);
    if(size != a->font_px){
        /* recreate at size */
        LOGFONTW lf; GetObjectW(f, sizeof(lf), &lf);
        lf.lfHeight = -size;
        HFONT nf = CreateFontIndirectW(&lf);
        return nf;
    }
    return NULL;
}

int ui_text_width(App *a, const char *utf8, bool bold, int size){
    if(!utf8 || !*utf8) return 0;
    wchar_t *w = wu_u8_to_w(utf8);
    if(!w) return 0;
    HFONT f = bold ? a->font_bold : a->font;
    HFONT nf = NULL;
    if(size != a->font_px){ LOGFONTW lf; GetObjectW(f, sizeof(lf), &lf); lf.lfHeight = -size; nf = CreateFontIndirectW(&lf); f = nf; }
    HGDIOBJ old = SelectObject(a->memdc, f);
    SIZE sz; GetTextExtentPoint32W(a->memdc, w, (int)wcslen(w), &sz);
    SelectObject(a->memdc, old);
    if(nf) DeleteObject(nf);
    ce_free(w);
    return sz.cx;
}

void ui_draw_text(App *a, int x, int y, const char *utf8, Color c, bool bold, int size){
    if(!utf8 || !*utf8) return;
    wchar_t *w = wu_u8_to_w(utf8);
    if(!w) return;
    HFONT f = bold ? a->font_bold : a->font;
    HFONT nf = NULL;
    if(size != a->font_px){ LOGFONTW lf; GetObjectW(f, sizeof(lf), &lf); lf.lfHeight = -size; nf = CreateFontIndirectW(&lf); f = nf; }
    HGDIOBJ oldf = SelectObject(a->memdc, f);
    SetTextColor(a->memdc, RGB(c.r, c.g, c.b));
    SetBkMode(a->memdc, TRANSPARENT);
    TextOutW(a->memdc, x, y, w, (int)wcslen(w));
    SelectObject(a->memdc, oldf);
    if(nf) DeleteObject(nf);
    ce_free(w);
}

/* ---------------- effects ---------------- */

static void box_blur_row(App *a, int y, int x0, int x1, int radius){
    /* horizontal box blur in place using a scratch row */
    uint32_t *row = ce_malloc((size_t)(x1 - x0) * sizeof(uint32_t));
    uint32_t *fb = (uint32_t*)a->fb;
    for(int x = x0; x < x1; x++) row[x - x0] = fb[y * a->fb_w + x];
    int r = radius;
    for(int x = x0; x < x1; x++){
        int lo = x - r, hi = x + r; if(lo < x0) lo = x0; if(hi > x1 - 1) hi = x1 - 1;
        uint64_t b=0,g=0,rr=0; int cnt = 0;
        for(int i = lo; i <= hi; i++){ uint32_t p = row[i - x0]; b += p & 0xFF; g += (p>>8)&0xFF; rr += (p>>16)&0xFF; cnt++; }
        fb[y * a->fb_w + x] = 0xFF000000 | ((uint32_t)(rr/cnt)<<16) | ((uint32_t)(g/cnt)<<8) | (uint32_t)(b/cnt);
    }
    ce_free(row);
}

void ui_blur_region(App *a, int x, int y, int w, int h, int radius){
    if(radius <= 0) return;
    if(x < 0){ w += x; x = 0; } if(y < 0){ h += y; y = 0; }
    if(x + w > a->fb_w) w = a->fb_w - x;
    if(y + h > a->fb_h) h = a->fb_h - y;
    if(w <= 0 || h <= 0) return;
    /* horizontal pass */
    for(int j = 0; j < h; j++) box_blur_row(a, y + j, x, x + w, radius);
    /* vertical pass: transpose via temp buffer */
    uint32_t *fb = (uint32_t*)a->fb;
    uint32_t *tmp = ce_malloc((size_t)w * h * sizeof(uint32_t));
    for(int j = 0; j < h; j++) for(int i = 0; i < w; i++) tmp[j*w+i] = fb[(y+j)*a->fb_w + (x+i)];
    for(int i = 0; i < w; i++){
        for(int j = 0; j < h; j++){
            int lo = j - radius, hi = j + radius; if(lo < 0) lo = 0; if(hi > h-1) hi = h-1;
            uint64_t b=0,g=0,rr=0; int cnt = 0;
            for(int k = lo; k <= hi; k++){ uint32_t p = tmp[k*w+i]; b += p&0xFF; g += (p>>8)&0xFF; rr += (p>>16)&0xFF; cnt++; }
            fb[(y+j)*a->fb_w + (x+i)] = 0xFF000000 | ((uint32_t)(rr/cnt)<<16) | ((uint32_t)(g/cnt)<<8) | (uint32_t)(b/cnt);
        }
    }
    ce_free(tmp);
}

void ui_draw_shadow_rect(App *a, int x, int y, int w, int h, int r){
    /* soft drop shadow via repeated rounded rects with low alpha */
    Color s = a->theme->shadow;
    for(int i = 4; i >= 1; i--){
        Color c = s; c.a = (uint8_t)(s.a * (5 - i) / 5);
        ui_fill_round(a, x - i, y + i, w + 2*i, h + 2*i, r + i, c);
    }
}

void ui_draw_button(App *a, int x, int y, int w, int h, const char *label, bool hover, bool pressed, bool disabled, bool primary){
    Color bg = primary ? a->theme->accent : a->theme->surface;
    Color fg = primary ? rgba(255,255,255,255) : a->theme->text;
    if(disabled){ bg = a->theme->panel; fg = a->theme->text_faint; }
    else if(pressed){ bg = primary ? a->theme->accent2 : a->theme->hover; }
    else if(hover){ bg = primary ? a->theme->accent2 : a->theme->hover; }
    /* glow on hover/focus */
    if(hover && !disabled){
        Color g = primary ? a->theme->accent : a->theme->accent;
        g.a = 60;
        ui_fill_round(a, x-2, y-2, w+4, h+4, 8, g);
    }
    ui_fill_round(a, x, y, w, h, 7, bg);
    int tw = ui_text_width(a, label, false, a->font_px);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - a->font_px) / 2 - 2;
    ui_draw_text(a, tx, ty, label, fg, false, a->font_px);
}

void ui_draw_capsule(App *a, int x, int y, int w, int h, double frac){
    /* sliding capsule indicator */
    ui_fill_round(a, x, y, w, h, h/2, a->theme->panel);
    /* animate active item */
    int item_w = w / 4;
    int cx = x + (int)(frac * item_w * 3) + 3;
    ui_fill_round(a, cx, y + 3, item_w - 6, h - 6, (h-6)/2, a->theme->accent);
}

void ui_draw_modal_frame(App *a, int x, int y, int w, int h){
    /* dim + blur is applied by caller before this */
    ui_draw_shadow_rect(a, x, y, w, h, 10);
    ui_fill_round(a, x, y, w, h, 10, a->theme->surface);
    /* 1px border */
    for(int i = 0; i < w; i++){
        put_px(a, x + i, y, color_bgra(a->theme->border));
        put_px(a, x + i, y + h - 1, color_bgra(a->theme->border));
    }
    for(int j = 0; j < h; j++){
        put_px(a, x, y + j, color_bgra(a->theme->border));
        put_px(a, x + w - 1, y + j, color_bgra(a->theme->border));
    }
}

/* ---------------- image drawing ---------------- */

void app_draw_image(App *a, int x, int y, int w, int h, const uint8_t *rgba, int iw, int ih){
    if(w <= 0 || h <= 0 || iw <= 0 || ih <= 0) return;
    for(int j = 0; j < h; j++){
        int sy = j * ih / h;
        for(int i = 0; i < w; i++){
            int sx = i * iw / w;
            const uint8_t *p = rgba + (sy * iw + sx) * 4;
            Color c = {p[0], p[1], p[2], p[3]};
            put_px(a, x + i, y + j, blend_src_over(get_px(a, x+i, y+j), c));
        }
    }
}
