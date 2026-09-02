#include "common.h"
#include "ean.h"
#include "code128.h"
#include "table.h"
#include <stdio.h>
#include <string.h>

int write_ean_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text) {
    if (module < 1 || module > 100 || height < 20 || height > 2000 || gap < 0 || gap > 2000)
        return EXIT_DOMAIN;
    size_t count = 0;
    for (size_t r = 0; r < t->nrow; r++) if (!t->rows[r][col_idx].is_null) count++;
    int max_modules = 113;
    int block_w = max_modules * module;
    int block_h = height + (text ? 20 : 0);
    int W = count ? block_w : 1;
    int H = count ? (int)(count * block_h + (count > 1 ? (count-1)*gap : 0)) : 1;
    if (count == 1) H = block_h;
    FILE *f = fopen(path, "wb");
    if (!f) return EXIT_IO;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n", W, H, W, H);
    int y = 0;
    for (size_t r = 0; r < t->nrow; r++) {
        Cell *c = &t->rows[r][col_idx];
        if (c->is_null) continue;
        size_t len; char *can = cell_canonical(c, TYPE_EAN13, &len);
        char mods[96];
        int nm = ean13_encode_modules(can, mods);
        if (nm != 95) { tt_free(can); fclose(f); return EXIT_INTERNAL; }
        int x = 11 * module;
        for (int m = 0; m < 95; m++) {
            if (mods[m] == '1') {
                fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"black\"/>\n", x, y, module, height);
            }
            x += module;
        }
        if (text) {
            fprintf(f, "<text x=\"%d\" y=\"%d\" font-size=\"14\" text-anchor=\"middle\">%s</text>\n", block_w/2, y + height + 15, can);
        }
        tt_free(can);
        y += block_h + gap;
    }
    fprintf(f, "</svg>\n");
    if (fflush(f) || fclose(f)) return EXIT_IO;
    return EXIT_OK;
}

int write_code128_svg_sheet(Table *t, int col_idx, const char *path, int module, int height, int gap, int text) {
    if (module < 1 || module > 100 || height < 20 || height > 2000 || gap < 0 || gap > 2000)
        return EXIT_DOMAIN;
    size_t count = 0;
    int max_mod = 0;
    int codes[512]; int nc;
    for (size_t r = 0; r < t->nrow; r++) {
        if (t->rows[r][col_idx].is_null) continue;
        count++;
        size_t len; char *can = cell_canonical(&t->rows[r][col_idx], TYPE_CODE128, &len);
        if (code128_encode(can, len, codes, &nc) != 0) { tt_free(can); return EXIT_DATA; }
        int mods = 20; /* quiet */
        for (int i = 0; i < nc; i++) {
            const char *pat = code128_pattern(codes[i]);
            if (!pat) { tt_free(can); return EXIT_INTERNAL; }
            for (const char *q = pat; *q; q++) mods += (*q - '0');
        }
        if (mods > max_mod) max_mod = mods;
        tt_free(can);
    }
    int block_w = max_mod * module;
    int block_h = height + (text ? 20 : 0);
    int W = count ? block_w : 1;
    int H = count ? (int)(count * block_h + (count > 1 ? (count-1)*gap : 0)) : 1;
    if (count == 1) H = block_h;
    FILE *f = fopen(path, "wb");
    if (!f) return EXIT_IO;
    fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(f, "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\" viewBox=\"0 0 %d %d\">\n", W, H, W, H);
    int y = 0;
    for (size_t r = 0; r < t->nrow; r++) {
        Cell *c = &t->rows[r][col_idx];
        if (c->is_null) continue;
        size_t len; char *can = cell_canonical(c, TYPE_CODE128, &len);
        if (code128_encode(can, len, codes, &nc) != 0) { tt_free(can); fclose(f); return EXIT_DATA; }
        int x = 10 * module;
        for (int i = 0; i < nc; i++) {
            const char *pat = code128_pattern(codes[i]);
            int bar = 1;
            for (const char *q = pat; *q; q++) {
                int w = *q - '0';
                if (bar) {
                    fprintf(f, "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"black\"/>\n", x, y, w*module, height);
                }
                x += w * module;
                bar = !bar;
            }
        }
        if (text) {
            /* escape XML specials */
            fprintf(f, "<text x=\"%d\" y=\"%d\" font-size=\"14\" text-anchor=\"middle\">", block_w/2, y + height + 15);
            for (size_t k = 0; k < len; k++) {
                char ch = can[k];
                if (ch == '&') fputs("&amp;", f);
                else if (ch == '<') fputs("&lt;", f);
                else if (ch == '>') fputs("&gt;", f);
                else fputc(ch, f);
            }
            fputs("</text>\n", f);
        }
        tt_free(can);
        y += block_h + gap;
    }
    fprintf(f, "</svg>\n");
    if (fflush(f) || fclose(f)) return EXIT_IO;
    return EXIT_OK;
}
