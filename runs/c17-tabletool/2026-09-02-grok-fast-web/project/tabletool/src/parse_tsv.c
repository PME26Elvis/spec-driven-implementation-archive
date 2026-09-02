#include "common.h"
#include "table.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

int load_tsv(Table *t, const char *path, bool header, const char *null_token, size_t null_len) {
    unsigned char *data; size_t len;
    if (read_file_binary(path, &data, &len) != 0) return EXIT_IO;
    if (!utf8_validate(data, len)) { tt_free(data); return EXIT_DATA; }
    size_t i = 0;
    if (len >= 3 && data[0]==0xEF && data[1]==0xBB && data[2]==0xBF) i = 3;
    char ***records = NULL; size_t nrec = 0, rec_cap = 0; size_t *fcounts = NULL;
    while (i <= len) {
        char **fields = NULL; size_t nf = 0, fcap = 0;
        while (1) {
            char *field = NULL; size_t fl = 0, fcap2 = 0;
            while (i < len && data[i] != '\t' && data[i] != '\n' && !(data[i]=='\r' && i+1<len && data[i+1]=='\n')) {
                if (data[i] == '\\') {
                    i++;
                    if (i >= len) { tt_free(data); return EXIT_DATA; }
                    char esc = data[i++];
                    char out;
                    if (esc == '\\') out = '\\';
                    else if (esc == 't') out = '\t';
                    else if (esc == 'n') out = '\n';
                    else if (esc == 'r') out = '\r';
                    else { tt_free(data); return EXIT_DATA; }
                    if (fl + 1 >= fcap2) { size_t nc = fcap2 ? fcap2*2 : 32; char *n = tt_realloc(field, nc); if (!n) { tt_free(data); return EXIT_RESOURCE; } field = n; fcap2 = nc; }
                    field[fl++] = out;
                } else {
                    if (fl + 1 >= fcap2) { size_t nc = fcap2 ? fcap2*2 : 32; char *n = tt_realloc(field, nc); if (!n) { tt_free(data); return EXIT_RESOURCE; } field = n; fcap2 = nc; }
                    field[fl++] = (char)data[i++];
                }
            }
            if (!field) { field = tt_malloc(1); field[0]=0; fl=0; }
            else field[fl]=0;
            if (nf >= fcap) { size_t nc = fcap ? fcap*2 : 8; char **n = tt_realloc(fields, nc*sizeof(char*)); if (!n) { tt_free(data); return EXIT_RESOURCE; } fields = n; fcap = nc; }
            fields[nf++] = field;
            if (i >= len) break;
            if (data[i] == '\t') { i++; continue; }
            if (data[i] == '\n') { i++; break; }
            if (data[i] == '\r') { i += 2; break; }
            break;
        }
        if (nrec >= rec_cap) {
            size_t nc = rec_cap ? rec_cap*2 : 8;
            char ***nr = tt_realloc(records, nc*sizeof(char**));
            size_t *nfc = tt_realloc(fcounts, nc*sizeof(size_t));
            if (!nr || !nfc) { tt_free(data); return EXIT_RESOURCE; }
            records = nr; fcounts = nfc; rec_cap = nc;
        }
        records[nrec] = fields; fcounts[nrec] = nf; nrec++;
        if (i >= len) break;
    }
    if (nrec == 0) { tt_free(data); return EXIT_DATA; }
    size_t ncol, data_start;
    if (header) {
        ncol = fcounts[0]; data_start = 1;
        for (size_t c = 0; c < ncol; c++) {
            if (records[0][c][0]==0) { tt_free(data); return EXIT_DATA; }
            if (table_add_column(t, records[0][c], strlen(records[0][c]), TYPE_STRING) != 0) { tt_free(data); return EXIT_DATA; }
        }
    } else {
        ncol = fcounts[0]; data_start = 0;
        for (size_t c = 0; c < ncol; c++) {
            char name[32]; snprintf(name, sizeof(name), "C%zu", c+1);
            table_add_column(t, name, strlen(name), TYPE_STRING);
        }
    }
    for (size_t r = data_start; r < nrec; r++) {
        if (fcounts[r] != ncol) { tt_free(data); return EXIT_DATA; }
        if (t->nrow >= t->row_cap) {
            size_t nc = t->row_cap ? t->row_cap*2 : 8;
            Cell **nr = tt_realloc(t->rows, nc*sizeof(Cell*));
            if (!nr) { tt_free(data); return EXIT_RESOURCE; }
            t->rows = nr; t->row_cap = nc;
        }
        Cell *row = tt_calloc(ncol, sizeof(Cell));
        for (size_t c = 0; c < ncol; c++) {
            char *f = records[r][c]; size_t fl = strlen(f);
            if (null_token && fl == null_len && memcmp(f, null_token, null_len)==0)
                row[c].is_null = true;
            else
                cell_set_string(&row[c], f, fl);
        }
        t->rows[t->nrow++] = row;
    }
    for (size_t r = 0; r < nrec; r++) {
        for (size_t c = 0; c < fcounts[r]; c++) tt_free(records[r][c]);
        tt_free(records[r]);
    }
    tt_free(records); tt_free(fcounts); tt_free(data);
    return EXIT_OK;
}
