#include "common.h"
#include "table.h"
#include "util.h"
#include <stdio.h>

static int needs_quote(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        if (c == ',' || c == '"' || c == '\n' || c == '\r') return 1;
    }
    return 0;
}

static void write_field(FILE *f, const char *s, size_t n) {
    if (needs_quote(s, n)) {
        fputc('"', f);
        for (size_t i = 0; i < n; i++) {
            if (s[i] == '"') fputc('"', f);
            fputc(s[i], f);
        }
        fputc('"', f);
    } else {
        fwrite(s, 1, n, f);
    }
}

int write_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len) {
    bool has_null = false;
    for (size_t r = 0; r < t->nrow && !has_null; r++)
        for (size_t c = 0; c < t->ncol; c++)
            if (t->rows[r][c].is_null) { has_null = true; break; }
    if (has_null && !null_token) return EXIT_DOMAIN;
    if (null_token) {
        for (size_t r = 0; r < t->nrow; r++) {
            for (size_t c = 0; c < t->ncol; c++) {
                if (t->rows[r][c].is_null) continue;
                size_t len; char *can = cell_canonical(&t->rows[r][c], t->cols[c].type, &len);
                if (len == null_len && memcmp(can, null_token, null_len) == 0) {
                    tt_free(can); return EXIT_DOMAIN;
                }
                tt_free(can);
            }
        }
    }
    FILE *f = fopen(path, "wb");
    if (!f) return EXIT_IO;
    if (header) {
        for (size_t c = 0; c < t->ncol; c++) {
            if (c) fputc(',', f);
            write_field(f, t->cols[c].name, t->cols[c].name_len);
        }
        fputc('\n', f);
    }
    for (size_t r = 0; r < t->nrow; r++) {
        for (size_t c = 0; c < t->ncol; c++) {
            if (c) fputc(',', f);
            if (t->rows[r][c].is_null) {
                write_field(f, null_token, null_len);
            } else {
                size_t len; char *can = cell_canonical(&t->rows[r][c], t->cols[c].type, &len);
                write_field(f, can, len);
                tt_free(can);
            }
        }
        fputc('\n', f);
    }
    if (fflush(f) || fclose(f)) return EXIT_IO;
    return EXIT_OK;
}
