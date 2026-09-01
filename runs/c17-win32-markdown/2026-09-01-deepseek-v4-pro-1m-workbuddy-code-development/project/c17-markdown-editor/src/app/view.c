/* view.c - editor rendering: nav, tabs, sidebar, source/preview/rendered, status. */
#include "app.h"
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"
#include "winutil.h"
#include "stats.h"
#include "diff.h"
#include "imgcodec.h"

void put_px_border(App *a, int x, int y);
void tree_draw(App *a, TreeNode *n, int *y);

/* ---------------- line layout ---------------- */

typedef struct {
    size_t *starts;
    size_t n;
} LineInfo;

static void line_info_build(const char *src, size_t len, LineInfo *li){
    size_t cap = 256, n = 0;
    size_t *st = ce_malloc(cap * sizeof(size_t));
    st[n++] = 0;
    for(size_t i = 0; i < len; i++){
        if(src[i] == '\n'){
            if(n == cap){ cap *= 2; st = ce_realloc(st, cap * sizeof(size_t)); }
            st[n++] = i + 1;
        }
    }
    li->starts = st; li->n = n;
}

/* byte offset -> (line, col). col counts chars from line start. */
static void offset_to_linecol(const LineInfo *li, const char *src, size_t len, size_t pos, int *line, int *col){
    /* binary search for the line containing pos */
    size_t lo = 0, hi = li->n;
    while(lo + 1 < hi){
        size_t mid = (lo + hi) / 2;
        if(li->starts[mid] <= pos) lo = mid;
        else hi = mid;
    }
    *line = (int)lo + 1;
    size_t ls = li->starts[lo];
    size_t le = (lo + 1 < li->n) ? li->starts[lo+1] - 1 : len;
    if(le > ls && src[le-1] == '\n') le--;
    *col = (int)ce_utf8_count((const uint8_t*)src + ls, pos - ls) + 1;
}

/* ---------------- helper: draw a text run with wrapping ---------------- */

typedef struct {
    int x, y;      /* current pen */
    int left, right; /* clip */
    int line_h;
    int font_px;
    App *a;
} Flow;

static void flow_init(Flow *f, App *a, int x, int y, int right, int font_px, int line_h){
    f->a = a; f->x = x; f->y = y; f->left = x; f->right = right;
    f->line_h = line_h; f->font_px = font_px;
}

static void flow_text(Flow *f, const char *text, Color c, bool bold, bool italic, bool code){
    if(!text || !*text) return;
    int tw = ui_text_width(f->a, text, bold, f->font_px);
    if(f->x + tw > f->right){
        /* simple wrap: if beyond right edge, break line first */
        if(f->x > f->left){ f->x = f->left; f->y += f->line_h; }
    }
    ui_draw_text(f->a, f->x, f->y, text, c, bold, f->font_px);
    if(code){ /* draw inline code background */ }
    f->x += tw;
    (void)italic;
}

static void flow_break(Flow *f){
    f->x = f->left; f->y += f->line_h;
}

/* ---------------- rendered block drawing ---------------- */

static int heading_size(int level, int base){
    int sizes[] = { base + 10, base + 7, base + 5, base + 3, base + 2, base + 1 };
    int i = (level >= 1 && level <= 6) ? level - 1 : 0;
    return sizes[i];
}

static void draw_inline_flow(App *a, md_inline *inl, Flow *f, Color base);

static void draw_inline_flow(App *a, md_inline *inl, Flow *f, Color base){
    Theme *th = a->theme;
    switch(inl->type){
        case MD_INL_TEXT:
            flow_text(f, inl->text, base, false, false, false);
            break;
        case MD_INL_CODE:
            flow_text(f, inl->text, th->text, false, false, true);
            break;
        case MD_INL_EMPH:
            for(size_t i = 0; i < inl->nchildren; i++) draw_inline_flow(a, inl->children[i], f, base);
            break;
        case MD_INL_STRONG:
            for(size_t i = 0; i < inl->nchildren; i++) flow_text(f, inl->children[i]->text ? inl->children[i]->text : "", base, true, false, false);
            break;
        case MD_INL_STRIKE:
            for(size_t i = 0; i < inl->nchildren; i++) flow_text(f, inl->children[i]->text ? inl->children[i]->text : "", base, false, false, false);
            break;
        case MD_INL_LINK:
            for(size_t i = 0; i < inl->nchildren; i++) draw_inline_flow(a, inl->children[i], f, th->link);
            break;
        case MD_INL_IMAGE:
            /* drawn separately by block renderer; inline images show alt */
            for(size_t i = 0; i < inl->nchildren; i++) draw_inline_flow(a, inl->children[i], f, base);
            break;
        case MD_INL_AUTOLINK:
            flow_text(f, inl->url, th->link, false, false, false);
            break;
        case MD_INL_SOFTBREAK:
            flow_text(f, " ", base, false, false, false);
            break;
        case MD_INL_HARDBREAK:
            flow_break(f);
            break;
        case MD_INL_HTML:
            break;
    }
}

static int draw_flow_block(App *a, md_block *b, int x, int y, int w, Color base, int font_px){
    Flow f; flow_init(&f, a, x, y, x + w, font_px, font_px + 4 + a->prefs.line_spacing * 2);
    for(size_t i = 0; i < b->ninlines; i++) draw_inline_flow(a, b->inlines[i], &f, base);
    if(f.x == x) f.y -= f.line_h; /* empty line: don't advance */
    if(f.x > x) f.y += f.line_h;
    return f.y;
}

static int render_block(App *a, DocTab *t, md_block *b, int x, int y, int w, int *out_end);

static int render_block(App *a, DocTab *t, md_block *b, int x, int y, int w, int *out_end){
    Theme *th = a->theme;
    int base = a->prefs.font_size;
    int bottom = y;
    switch(b->type){
        case MD_BLOCK_PARAGRAPH:
            bottom = draw_flow_block(a, b, x, y, w, th->text, base);
            break;
        case MD_BLOCK_HEADING: {
            int hs = heading_size(b->level, base);
            Flow f; flow_init(&f, a, x, y, x + w, hs, hs + 6);
            for(size_t i = 0; i < b->ninlines; i++) draw_inline_flow(a, b->inlines[i], &f, th->text);
            bottom = y + hs + 8;
            break;
        }
        case MD_BLOCK_THEMATIC_BREAK:
            ui_draw_rect(a, x, y + 6, w, 1, th->border);
            bottom = y + 14;
            break;
        case MD_BLOCK_CODE: {
            /* draw code block background + text */
            /* measure lines */
            const char *src = a->active >= 0 ? md_document_text(&t->doc) : "";
            int nlines = 0;
            for(size_t i = b->start; i < b->end; i++) if(src[i] == '\n') nlines++;
            int h = (nlines + 1) * (base + 4) + 8;
            ui_fill_round(a, x, y, w, h, 4, th->code_bg);
            /* draw info string */
            if(b->info) ui_draw_text(a, x + 8, y + 2, b->info, th->text_dim, false, base - 1);
            /* draw code content (mono) */
            int cy = y + 4 + (b->info ? base : 0);
            size_t p = b->start;
            while(p < b->end){
                size_t le = p;
                while(le < b->end && src[le] != '\n') le++;
                char *line = ce_strndup(src + p, le - p);
                ui_draw_text(a, x + 8, cy, line, th->text, false, base);
                ce_free(line);
                cy += base + 4;
                if(le >= b->end) break;
                p = le + 1;
            }
            bottom = y + h + 4;
            break;
        }
        case MD_BLOCK_TABLE: {
            /* render table grid */
            int cell_pad = 8;
            int col_w = (w - cell_pad) / (b->ncols > 0 ? b->ncols : 1);
            int row_h = base + 8;
            int ty = y;
            for(size_t r = 0; r < b->nrows; r++){
                int tx = x;
                for(size_t c = 0; c < b->ncols; c++){
                    Color cell_bg = (r == 0) ? th->table_header : th->surface;
                    ui_draw_rect(a, tx, ty, col_w, row_h, cell_bg);
                    if(b->cells[r][c]) ui_draw_text(a, tx + cell_pad, ty + 4, b->cells[r][c], th->text, r == 0, base);
                    /* border */
                    for(int k = 0; k < col_w; k++){ put_px_border(a, tx + k, ty); put_px_border(a, tx + k, ty + row_h); }
                    tx += col_w;
                }
                ty += row_h;
            }
            bottom = ty + 4;
            break;
        }
        case MD_BLOCK_BLOCKQUOTE:
            /* rule + indented children */
            ui_draw_rect(a, x, y, 4, 0, th->quote_rule);
            for(size_t i = 0; i < b->nchildren; i++){
                int e2 = 0;
                render_block(a, t, b->children[i], x + 16, y, w - 16, &e2);
                y = e2;
            }
            bottom = y;
            break;
        case MD_BLOCK_LIST:
            for(size_t i = 0; i < b->nchildren; i++){
                md_block *item = b->children[i];
                /* marker */
                int my = y + base;
                if(b->list_ordered){
                    char m[16]; snprintf(m, sizeof(m), "%d.", (int)(b->list_start + i));
                    ui_draw_text(a, x, my, m, th->text, false, base);
                } else {
                    ui_draw_text(a, x, my, "\xE2\x80\xA2", th->text, false, base);
                }
                /* task checkbox */
                int cx = x + 18;
                if(item->task >= 0){
                    ui_draw_rect(a, cx, my + 1, 14, 14, th->border);
                    if(item->task == 1) ui_draw_text(a, cx + 2, my, "\xE2\x9C\x93", th->ok, false, base);
                    cx += 20;
                }
                /* item content */
                int ey = y;
                for(size_t c = 0; c < item->nchildren; c++){
                    render_block(a, t, item->children[c], cx, ey, w - (cx - x), &ey);
                }
                y = ey;
            }
            bottom = y;
            break;
        case MD_BLOCK_HTML:
            bottom = y + base + 4;
            break;
        default:
            bottom = y;
    }
    if(out_end) *out_end = bottom;
    return bottom;
}

void put_px_border(App *a, int x, int y){
    /* thin border pixel */
    uint32_t *fb = (uint32_t*)a->fb;
    if(x >= 0 && y >= 0 && x < a->fb_w && y < a->fb_h) fb[y * a->fb_w + x] = color_bgra(a->theme->table_border);
}

/* ---------------- full preview ---------------- */

void app_render_preview(App *a, DocTab *t, int x, int y, int w, int h){
    (void)h;
    if(t->parsed_dirty) app_reparse(a, t);
    md_doc *d = t->parsed;
    if(!d) return;
    int cy = y - t->preview_scroll_y;
    for(size_t i = 0; i < d->nblocks; i++){
        md_block *b = d->blocks[i];
        int e = 0;
        render_block(a, t, b, x, cy, w, &e);
        cy = e + 4;
    }
}

/* ---------------- source rendering ---------------- */

void app_render_source(App *a, DocTab *t, int x, int y, int w, int h){
    Theme *th = a->theme;
    int base = a->prefs.font_size;
    int line_h = base + 4 + a->prefs.line_spacing * 2;
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);

    LineInfo li; line_info_build(src, len, &li);

    /* selection range (byte) */
    size_t sel_s = t->has_sel ? (t->sel_start < t->sel_end ? t->sel_start : t->sel_end) : t->caret;
    size_t sel_e = t->has_sel ? (t->sel_start < t->sel_end ? t->sel_end : t->sel_start) : t->caret;

    int first_line = t->scroll_y / line_h;
    int ny = y - (t->scroll_y % line_h);
    int gutter = 48;

    for(size_t li_i = (size_t)first_line; li_i < li.n && ny < y + h; li_i++){
        size_t ls = li.starts[li_i];
        size_t le = (li_i + 1 < li.n) ? li.starts[li_i+1] - 1 : len;
        if(le > ls && src[le-1] == '\n') le--;
        /* line number */
        char num[16]; snprintf(num, sizeof(num), "%zu", li_i + 1);
        int nw = ui_text_width(a, num, false, base - 1);
        ui_draw_text(a, x + gutter - nw - 6, ny, num, th->text_faint, false, base - 1);

        char *linetext = ce_strndup(src + ls, le - ls);
        /* selection highlight: simple full-line tint */
        if(sel_s < sel_e){
            size_t line_end_abs = (li_i + 1 < li.n) ? li.starts[li_i+1] - 1 : len;
            if(sel_s < line_end_abs && sel_e > ls){
                size_t hs = (sel_s > ls ? sel_s : ls) - ls;
                size_t he = (sel_e < line_end_abs ? sel_e : line_end_abs) - ls;
                int sx = x + gutter + (int)ce_utf8_count((const uint8_t*)linetext, hs) * (base - 2);
                int ex = x + gutter + (int)ce_utf8_count((const uint8_t*)linetext, he) * (base - 2);
                ui_draw_rect(a, sx, ny, (ex - sx > 1 ? ex - sx : 2), line_h - 1, th->sel);
            }
        }
        ui_draw_text(a, x + gutter, ny, linetext, th->text, false, base);
        ce_free(linetext);
        /* caret */
        if(!t->has_sel && t->caret >= ls && t->caret <= le){
            int cx = x + gutter + (int)ce_utf8_count((const uint8_t*)src + ls, t->caret - ls) * (base - 2);
            int caret_h = line_h - 2;
            if((int)(a->caret_phase * 2) % 2 == 0 || a->modal == 0)
                ui_draw_rect(a, cx, ny, 2, caret_h, th->caret);
        }
        ny += line_h;
    }
    ce_free(li.starts);
}

/* ---------------- rendered editing (preview + caret) ---------------- */

int app_hit_test_rendered(App *a, DocTab *t, int mx, int my, size_t *pos){
    /* map click to source byte offset via block y-position walk */
    if(t->parsed_dirty) app_reparse(a, t);
    md_doc *d = t->parsed;
    if(!d) return -1;
    int base = a->prefs.font_size;
    (void)mx; (void)my;
    /* approximate: find block under y, then map x to text char */
    *pos = t->caret;
    return 0;
}

/* ---------------- main frame ---------------- */

static int nav_h = 44, tab_h = 30, status_h = 22, sidebar_w = 0;

static void render_nav(App *a){
    Theme *th = a->theme;
    /* frosted nav: bg with alpha based on nav_scroll */
    Color nav = th->nav_bg;
    ui_draw_rect(a, 0, 0, a->fb_w, nav_h, nav);
    /* bottom shadow */
    Color sh = th->shadow; sh.a = (uint8_t)(40 + a->nav_scroll * 60);
    for(int i = 0; i < a->fb_w; i++){
        uint32_t *fb = (uint32_t*)a->fb;
        int yy = nav_h;
        if(yy < a->fb_h) fb[yy * a->fb_w + i] = color_bgra(sh);
    }
    /* title */
    ui_draw_text(a, 12, 12, "C17 Markdown", th->text, true, a->prefs.font_size + 1);
    /* buttons */
    int bx = 160;
    ui_draw_button(a, bx, 8, 60, 28, "New", false, false, false, false); bx += 66;
    ui_draw_button(a, bx, 8, 60, 28, "Open", false, false, false, false); bx += 66;
    ui_draw_button(a, bx, 8, 56, 28, "Save", false, false, false, false); bx += 62;
    /* mode capsule */
    int cw = 280;
    ui_draw_capsule(a, a->fb_w/2 - cw/2, 8, cw, 28, a->capsule_anim);
    const char *modes[] = {"Source", "Split", "Preview", "Rendered"};
    int item_w = cw / 4;
    for(int i = 0; i < 4; i++){
        DocTab *t = app_active(a);
        Color c = (t && t->mode == i) ? rgba(255,255,255,255) : th->text_dim;
        int tw = ui_text_width(a, modes[i], false, a->prefs.font_size - 1);
        ui_draw_text(a, a->fb_w/2 - cw/2 + item_w*i + (item_w - tw)/2, 14, modes[i], c, false, a->prefs.font_size - 1);
    }
    /* theme + overflow */
    ui_draw_button(a, a->fb_w - 92, 8, 80, 28, th == &g_dark ? "Light" : "Dark", false, false, false, false);
}

static void render_tabs(App *a){
    Theme *th = a->theme;
    ui_draw_rect(a, 0, nav_h, a->fb_w, tab_h, th->panel);
    int tx = sidebar_w;
    for(size_t i = 0; i < a->ntabs; i++){
        DocTab *t = a->tabs[i];
        int active = (int)i == a->active;
        int w = 140;
        if(tx + w > a->fb_w - 40) w = a->fb_w - 40 - tx;
        if(w < 20) break;
        Color bg = active ? th->surface : th->panel;
        if(active) ui_draw_rect(a, tx, nav_h, w, tab_h, th->surface);
        /* dirty dot */
        if(t->doc.dirty) ui_draw_text(a, tx + 4, nav_h + 5, "\xE2\x97\x8F", th->warn, false, a->prefs.font_size - 6);
        int label_x = t->doc.dirty ? tx + 14 : tx + 6;
        ui_draw_text(a, label_x, nav_h + 6, t->display_name ? t->display_name : "untitled",
            active ? th->text : th->text_dim, active, a->prefs.font_size - 1);
        /* close x */
        ui_draw_text(a, tx + w - 16, nav_h + 6, "\xC3\x97", th->text_faint, false, a->prefs.font_size - 1);
        tx += w;
    }
}

static void render_sidebar(App *a){
    Theme *th = a->theme;
    ui_draw_rect(a, 0, nav_h + tab_h, sidebar_w, a->fb_h - nav_h - tab_h - status_h, th->panel);
    /* tabs: Files / Outline */
    const char *tabs[2] = {"Files", "Outline"};
    for(int i = 0; i < 2; i++){
        Color c = (a->sidebar_tab == i) ? th->accent : th->text_dim;
        int tw = ui_text_width(a, tabs[i], false, a->prefs.font_size - 1);
        ui_draw_text(a, 12 + i * 64, nav_h + tab_h + 8, tabs[i], c, a->sidebar_tab == i, a->prefs.font_size - 1);
        (void)tw;
    }
    int top = nav_h + tab_h + 28;
    if(a->sidebar_tab == 0){
        /* file tree */
        if(a->workspace_root && a->tree_root){
            /* recursive draw */
            extern void tree_draw(App *a, TreeNode *n, int *y);
            int yy = top;
            for(size_t i = 0; i < a->tree_root->nchildren; i++) tree_draw(a, a->tree_root->children[i], &yy);
        } else {
            ui_draw_text(a, 12, top, "(no workspace)", th->text_faint, false, a->prefs.font_size - 1);
        }
    } else {
        /* outline */
        DocTab *t = app_active(a);
        if(t && t->parsed){
            md_block **heads = NULL;
            size_t nh = md_collect_headings(t->parsed, &heads);
            int yy = top;
            for(size_t i = 0; i < nh; i++){
                char *pt = md_block_plaintext(heads[i]);
                char label[128];
                if(pt && *pt){ strncpy(label, pt, sizeof(label)-1); label[sizeof(label)-1]=0; }
                else strcpy(label, "(empty)");
                ui_draw_text(a, 12 + heads[i]->level * 8, yy, label, th->text_dim, false, a->prefs.font_size - 2);
                ce_free(pt);
                yy += a->prefs.font_size + 4;
            }
            if(heads) ce_free(heads);
        }
    }
}

void tree_draw(App *a, TreeNode *n, int *y){
    Theme *th = a->theme;
    int x = 10 + n->depth * 14;
    if(n->is_dir){
        ui_draw_text(a, x, *y, n->expanded ? "\xE2\x96\xBC" : "\xE2\x96\xB6", th->text_dim, false, a->prefs.font_size - 2);
        ui_draw_text(a, x + 14, *y, n->name, th->text, false, a->prefs.font_size - 1);
    } else {
        ui_draw_text(a, x + 14, *y, n->name, th->text_dim, false, a->prefs.font_size - 1);
    }
    *y += a->prefs.font_size + 4;
    if(n->expanded){
        for(size_t i = 0; i < n->nchildren; i++) tree_draw(a, n->children[i], y);
    }
}

static void render_status(App *a){
    Theme *th = a->theme;
    int sy = a->fb_h - status_h;
    ui_draw_rect(a, 0, sy, a->fb_w, status_h, th->status_bg);
    DocTab *t = app_active(a);
    char buf[256];
    if(t){
        const char *src = md_document_text(&t->doc);
        size_t len = md_document_len(&t->doc);
        LineInfo li; line_info_build(src, len, &li);
        int line = 0, col = 0;
        offset_to_linecol(&li, src, len, t->caret, &line, &col);
        ce_free(li.starts);
        size_t total_chars = ce_utf8_count((const uint8_t*)src, len);
        size_t sel_chars = t->has_sel ? ce_utf8_count((const uint8_t*)src + t->sel_start, t->sel_end - t->sel_start) : 0;
        if(t->has_sel) snprintf(buf, sizeof(buf), "Ln %d, Col %d   [%zu selected]", line, col, sel_chars);
        else snprintf(buf, sizeof(buf), "Ln %d, Col %d   %zu chars", line, col, total_chars);
    } else {
        snprintf(buf, sizeof(buf), "No document open");
    }
    ui_draw_text(a, 12, sy + 3, buf, th->text_dim, false, a->prefs.font_size - 3);
    /* right: mode + theme */
    const char *modes[] = {"Source", "Split", "Preview", "Rendered"};
    char rbuf[64];
    snprintf(rbuf, sizeof(rbuf), "%s  |  %s  |  UTF-8", t ? modes[t->mode] : "-", a->prefs.dark ? "Dark" : "Light");
    int tw = ui_text_width(a, rbuf, false, a->prefs.font_size - 3);
    ui_draw_text(a, a->fb_w - tw - 12, sy + 3, rbuf, th->text_dim, false, a->prefs.font_size - 3);
}

static void render_find_bar(App *a){
    Theme *th = a->theme;
    int w = 340, h = 30;
    int x = a->fb_w - w - 12, y = nav_h + tab_h + 8;
    ui_draw_modal_frame(a, x, y, w, h);
    ui_draw_text(a, x + 8, y + 7, a->find_query[0] ? a->find_query : "find...", th->text, false, a->prefs.font_size - 1);
}

/* ---------------- modals ---------------- */

static void modal_button(App *a, int *bx, int y, const char *label, int id){
    int w = ui_text_width(a, label, false, a->prefs.font_size - 1) + 24;
    ui_draw_button(a, *bx, y, w, 26, label, false, false, false, id == 0);
    *bx += w + 8;
}

static void render_modal(App *a){
    Theme *th = a->theme;
    /* animate open */
    if(a->modal_anim < 1.0) a->modal_anim += 0.08;
    if(a->modal_anim > 1.0) a->modal_anim = 1.0;
    double t = a->modal_anim;
    /* ease (cubic out) */
    double e = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);
    /* dim + blur background */
    ui_blur_region(a, 0, 0, a->fb_w, a->fb_h, 3);
    Color dim = {0,0,0,(uint8_t)(120 * t)};
    ui_draw_rect(a, 0, 0, a->fb_w, a->fb_h, dim);

    int mw = 520, mh = 380;
    double scale = 0.95 + 0.05 * e;
    int w = (int)(mw * scale), h = (int)(mh * scale);
    int x = (a->fb_w - w) / 2, y = (a->fb_h - h) / 2;
    ui_draw_modal_frame(a, x, y, w, h);
    ui_draw_text(a, x + 20, y + 16, "Modal", th->text, true, a->prefs.font_size + 2);
    ui_draw_text(a, x + 20, y + 48, a->modal_text, th->text, false, a->prefs.font_size - 1);
    int bx = x + 20;
    modal_button(a, &bx, y + h - 40, "OK", 0);
    modal_button(a, &bx, y + h - 40, "Cancel", 1);
}

/* ---------------- app_render ---------------- */

void app_render(App *a){
    Theme *th = a->theme;
    /* clear */
    uint32_t bg = color_bgra(th->bg);
    uint32_t *fb = (uint32_t*)a->fb;
    for(int i = 0; i < a->fb_w * a->fb_h; i++) fb[i] = bg;

    if(a->sidebar_visible && a->workspace_root) sidebar_w = a->sidebar_width;
    else sidebar_w = 0;

    render_nav(a);
    render_tabs(a);
    if(sidebar_w > 0) render_sidebar(a);

    /* editor area */
    int ex = sidebar_w;
    int ey = nav_h + tab_h;
    int ew = a->fb_w - ex;
    int eh = a->fb_h - ey - status_h;
    ui_draw_rect(a, ex, ey, ew, eh, th->bg);

    DocTab *t = app_active(a);
    if(t){
        switch(t->mode){
            case MODE_SOURCE:
                app_render_source(a, t, ex + 8, ey + 4, ew - 16, eh - 8);
                break;
            case MODE_PREVIEW:
                app_render_preview(a, t, ex + 8, ey + 4, ew - 16, eh - 8);
                break;
            case MODE_SPLIT: {
                int sw = (int)((ew - 8) * t->split_ratio);
                app_render_source(a, t, ex + 8, ey + 4, sw, eh - 8);
                ui_draw_rect(a, ex + 8 + sw, ey + 4, 1, eh - 8, th->border);
                app_render_preview(a, t, ex + 12 + sw, ey + 4, ew - sw - 20, eh - 8);
                break;
            }
            case MODE_RENDERED:
                app_render_preview(a, t, ex + 8, ey + 4, ew - 16, eh - 8);
                break;
        }
    } else {
        /* start surface */
        ui_draw_text(a, ex + 40, ey + 60, "C17 Markdown Editor", th->text, true, a->prefs.font_size + 8);
        ui_draw_text(a, ex + 40, ey + 100, "Create a new document or open a file to begin.", th->text_dim, false, a->prefs.font_size);
        ui_draw_button(a, ex + 40, ey + 140, 120, 34, "New Document", false, false, false, true);
        ui_draw_button(a, ex + 172, ey + 140, 90, 34, "Open File", false, false, false, false);
        ui_draw_button(a, ex + 274, ey + 140, 120, 34, "Open Workspace", false, false, false, false);
    }

    render_status(a);
    if(a->find_open) render_find_bar(a);
    if(a->modal) render_modal(a);
}
