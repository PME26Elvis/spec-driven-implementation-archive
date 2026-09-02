#include "common.h"
#include "table.h"
#include "util.h"
#include <stdio.h>

/* Write restricted Markdown. Rejects leading/trailing space on names/cells. */
int write_markdown(Table *t, const char *path, const char *null_token, size_t null_len) {
    /* validate no boundary spaces */
    for (size_t c = 0; c < t->ncol; c++) {
        if (t->cols[c].name_len > 0 &&
            (t->cols[c].name[0] == ' ' || t->cols[c].name[t->cols[c].name_len-1] == ' '))
            return EXIT_DOMAIN;
    }
    for (size_t r = 0; r < t->nrow; r++) {
        for (size_t c = 0; c < t->ncol; c++) {
            if (t->rows[r][c].is_null) continue;
            size_t len;
            char *can = cell_canonical(&t->rows[r][c], t->cols[c].type, &len);
            if (!can) return EXIT_RESOURCE;
            if (len > 0 && (can[0] == ' ' || can[len-1] == ' ')) {
                tt_free(can);
                return EXIT_DOMAIN;
            }
            tt_free(can);
        }
    }
    if (null_token && null_len > 0 &&
        (null_token[0] == ' ' || null_token[null_len-1] == ' '))
        return EXIT_DOMAIN;

    /* check NULL presence vs token */
    bool has_null = false;
    for (size_t r = 0; r < t->nrow && !has_null; r++)
        for (size_t c = 0; c < t->ncol; c++)
            if (t->rows[r][c].is_null) { has_null = true; break; }
    if (has_null && !null_token) return EXIT_DOMAIN;

    /* ambiguity check */
    if (null_token) {
        for (size_t r = 0; r < t->nrow; r++) {
            for (size_t c = 0; c < t->ncol; c++) {
                if (t->rows[r][c].is_null) continue;
                size_t len;
                char *can = cell_canonical(&t->rows[r][c], t->cols[c].type, &len);
                if (len == null_len && memcmp(can, null_token, null_len) == 0) {
                    tt_free(can);
                    return EXIT_DOMAIN;
                }
                tt_free(can);
            }
        }
    }

    FILE *f = fopen(path, "wb");
    if (!f) return EXIT_IO;

    /* header */
    fputc('|', f);
    for (size_t c = 0; c < t->ncol; c++) {
        fputc(' ', f);
        /* escape | \ n r t */
        const char *nm = t->cols[c].name;
        size_t nl = t->cols[c].name_len;
        for (size_t i = 0; i < nl; i++) {
            char ch = nm[i];
            if (ch == '\\' || ch == '|' || ch == '\n' || ch == '\r' || ch == '\t') {
                fputc('\\', f);
                if (ch == '\n') fputc('n', f);
                else if (ch == '\r') fputc('r', f);
                else if (ch == '\t') fputc('t', f);
                else fputc(ch, f);
            } else fputc(ch, f);
        }
        fputs(" |", f);
    }
    fputc('\n', f);
    /* separator */
    fputc('|', f);
    for (size_t c = 0; c < t->ncol; c++) fputs(" --- |", f);
    fputc('\n', f);
    /* data */
    for (size_t r = 0; r < t->nrow; r++) {
        fputc('|', f);
        for (size_t c = 0; c < t->ncol; c++) {
            fputc(' ', f);
            if (t->rows[r][c].is_null) {
                fwrite(null_token, 1, null_len, f);
            } else {
                size_t len;
                char *can = cell_canonical(&t->rows[r][c], t->cols[c].type, &len);
                for (size_t i = 0; i < len; i++) {
                    char ch = can[i];
                    if (ch == '\\' || ch == '|' || ch == '\n' || ch == '\r' || ch == '\t') {
                        fputc('\\', f);
                        if (ch == '\n') fputc('n', f);
                        else if (ch == '\r') fputc('r', f);
                        else if (ch == '\t') fputc('t', f);
                        else fputc(ch, f);
                    } else fputc(ch, f);
                }
                tt_free(can);
            }
            fputs(" |", f);
        }
        fputc('\n', f);
    }
    if (fflush(f) != 0 || fclose(f) != 0) return EXIT_IO;
    return EXIT_OK;
}
