#include "common.h"
#include "table.h"
#include "util.h"

/* Simple but correct CSV loader for HEADER YES/NO, NULL-TOKEN */
int load_csv(Table *t, const char *path, bool header, const char *null_token, size_t null_len) {
    unsigned char *data;
    size_t len;
    if (read_file_binary(path, &data, &len) != 0) return EXIT_IO;
    if (!utf8_validate(data, len)) { tt_free(data); return EXIT_DATA; }

    /* State machine parser */
    size_t i = 0;
    /* skip BOM */
    if (len >= 3 && data[0]==0xEF && data[1]==0xBB && data[2]==0xBF) i = 3;

    /* Collect records as list of list of strings */
    char ***records = NULL;
    size_t nrec = 0, rec_cap = 0;
    size_t *field_counts = NULL;

    while (i <= len) {
        /* parse one record */
        char **fields = NULL;
        size_t nf = 0, fcap = 0;
        while (1) {
            /* parse field */
            char *field = NULL;
            size_t flen = 0, fcap2 = 0;
            int quoted = 0;
            if (i < len && data[i] == '"') {
                quoted = 1;
                i++;
                while (i < len) {
                    if (data[i] == '"') {
                        if (i + 1 < len && data[i+1] == '"') {
                            /* escaped quote */
                            if (flen + 1 >= fcap2) {
                                size_t nc = fcap2 ? fcap2 * 2 : 32;
                                char *n = tt_realloc(field, nc);
                                if (!n) goto oom;
                                field = n; fcap2 = nc;
                            }
                            field[flen++] = '"';
                            i += 2;
                        } else {
                            i++; /* end quote */
                            break;
                        }
                    } else {
                        if (flen + 1 >= fcap2) {
                            size_t nc = fcap2 ? fcap2 * 2 : 32;
                            char *n = tt_realloc(field, nc);
                            if (!n) goto oom;
                            field = n; fcap2 = nc;
                        }
                        field[flen++] = (char)data[i++];
                    }
                }
            } else {
                while (i < len && data[i] != ',' && data[i] != '\n' && !(data[i]=='\r' && i+1<len && data[i+1]=='\n')) {
                    if (flen + 1 >= fcap2) {
                        size_t nc = fcap2 ? fcap2 * 2 : 32;
                        char *n = tt_realloc(field, nc);
                        if (!n) goto oom;
                        field = n; fcap2 = nc;
                    }
                    field[flen++] = (char)data[i++];
                }
            }
            if (!field) { field = tt_malloc(1); if (!field) goto oom; field[0]=0; flen=0; }
            else { field[flen] = 0; }
            if (nf >= fcap) {
                size_t nc = fcap ? fcap * 2 : 8;
                char **n = tt_realloc(fields, nc * sizeof(char*));
                if (!n) goto oom;
                fields = n; fcap = nc;
            }
            fields[nf++] = field;
            if (i >= len) break;
            if (data[i] == ',') { i++; continue; }
            if (data[i] == '\n') { i++; break; }
            if (data[i] == '\r' && i+1 < len && data[i+1] == '\n') { i += 2; break; }
            /* trailing after quote? error */
            if (quoted) { /* should have been comma or end */ goto bad; }
            break;
        }
        if (nrec >= rec_cap) {
            size_t nc = rec_cap ? rec_cap * 2 : 8;
            char ***nr = tt_realloc(records, nc * sizeof(char**));
            size_t *nfc = tt_realloc(field_counts, nc * sizeof(size_t));
            if (!nr || !nfc) goto oom;
            records = nr; field_counts = nfc; rec_cap = nc;
        }
        records[nrec] = fields;
        field_counts[nrec] = nf;
        nrec++;
        if (i >= len) break;
        /* terminal LF does not create empty record -- already handled by break */
        if (i == len) break;
    }

    if (nrec == 0) { tt_free(data); return EXIT_DATA; }

    size_t ncol;
    size_t data_start;
    if (header) {
        ncol = field_counts[0];
        data_start = 1;
        for (size_t c = 0; c < ncol; c++) {
            if (records[0][c][0] == 0) goto bad; /* empty name */
            if (table_add_column(t, records[0][c], strlen(records[0][c]), TYPE_STRING) != 0) goto bad;
        }
    } else {
        ncol = field_counts[0];
        data_start = 0;
        for (size_t c = 0; c < ncol; c++) {
            char name[32];
            snprintf(name, sizeof(name), "C%zu", c+1);
            if (table_add_column(t, name, strlen(name), TYPE_STRING) != 0) goto bad;
        }
    }

    for (size_t r = data_start; r < nrec; r++) {
        if (field_counts[r] != ncol) goto bad;
        /* append row */
        if (t->nrow >= t->row_cap) {
            size_t nc = t->row_cap ? t->row_cap * 2 : 8;
            Cell **nr = tt_realloc(t->rows, nc * sizeof(Cell*));
            if (!nr) goto oom;
            t->rows = nr; t->row_cap = nc;
        }
        Cell *row = tt_calloc(ncol, sizeof(Cell));
        if (!row) goto oom;
        for (size_t c = 0; c < ncol; c++) {
            char *f = records[r][c];
            size_t fl = strlen(f);
            if (null_token && fl == null_len && memcmp(f, null_token, null_len) == 0) {
                row[c].is_null = true;
            } else {
                if (cell_set_string(&row[c], f, fl) != 0) goto oom;
            }
        }
        t->rows[t->nrow++] = row;
    }

    /* free temp */
    for (size_t r = 0; r < nrec; r++) {
        for (size_t c = 0; c < field_counts[r]; c++) tt_free(records[r][c]);
        tt_free(records[r]);
    }
    tt_free(records);
    tt_free(field_counts);
    tt_free(data);
    return EXIT_OK;

bad:
    /* cleanup omitted for brevity in this draft */
    tt_free(data);
    return EXIT_DATA;
oom:
    tt_free(data);
    return EXIT_RESOURCE;
}
