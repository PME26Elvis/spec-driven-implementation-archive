#include "common.h"
#include "table.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

static int is_blank_line(const char *s, size_t n) {
    for (size_t i = 0; i < n; i++) if (s[i] != ' ' && s[i] != '\t') return 0;
    return 1;
}

int load_markdown(Table *t, const char *path, const char *null_token, size_t null_len) {
    if (null_token && null_len > 0 && (null_token[0]==' ' || null_token[null_len-1]==' '))
        return EXIT_DATA; /* Markdown NULL-TOKEN boundary space */
    unsigned char *data; size_t len;
    if (read_file_binary(path, &data, &len) != 0) return EXIT_IO;
    if (!utf8_validate(data, len)) { tt_free(data); return EXIT_DATA; }
    size_t i = 0;
    if (len >= 3 && data[0]==0xEF && data[1]==0xBB && data[2]==0xBF) i = 3;
    /* collect non-blank lines */
    char **lines = NULL; size_t nlines = 0, lcap = 0;
    while (i < len) {
        size_t ls = i;
        while (i < len && data[i] != '\n' && !(data[i]=='\r' && i+1<len && data[i+1]=='\n')) i++;
        size_t le = i;
        if (i < len) { if (data[i]=='\r') i+=2; else i++; }
        if (is_blank_line((char*)data+ls, le-ls)) continue;
        if (nlines >= lcap) {
            size_t nc = lcap ? lcap*2 : 8;
            char **nl = tt_realloc(lines, nc*sizeof(char*));
            if (!nl) { tt_free(data); return EXIT_RESOURCE; }
            lines = nl; lcap = nc;
        }
        lines[nlines] = tt_strndup((char*)data+ls, le-ls);
        nlines++;
    }
    tt_free(data);
    if (nlines < 2) { /* need header + separator */ 
        for (size_t k=0;k<nlines;k++) tt_free(lines[k]); tt_free(lines);
        return EXIT_DATA;
    }
    /* parse header row */
    /* simplified tokenizer: split on |, trim spaces, decode escapes */
    #define MAXC 64
    char *hcells[MAXC]; size_t hcnt = 0;
    {
        char *row = lines[0];
        size_t rl = strlen(row);
        /* strip leading/trailing | framing */
        size_t a = 0, b = rl;
        while (a < b && row[a]==' ') a++;
        if (a < b && row[a]=='|') a++;
        while (b > a && row[b-1]==' ') b--;
        if (b > a && row[b-1]=='|') b--;
        size_t p = a;
        while (p < b && hcnt < MAXC) {
            size_t cs = p;
            while (p < b) {
                if (row[p]=='\\' && p+1 < b) { p += 2; continue; }
                if (row[p]=='|') break;
                p++;
            }
            size_t ce = p;
            while (cs < ce && row[cs]==' ') cs++;
            while (ce > cs && row[ce-1]==' ') ce--;
            /* decode escapes */
            char *cell = tt_malloc(ce-cs+1); size_t cl = 0;
            for (size_t k = cs; k < ce; k++) {
                if (row[k]=='\\' && k+1 < ce) {
                    char e = row[++k];
                    if (e=='|' || e=='\\') cell[cl++]=e;
                    else if (e=='n') cell[cl++]='\n';
                    else if (e=='r') cell[cl++]='\r';
                    else if (e=='t') cell[cl++]='\t';
                    else { tt_free(cell); goto bad; }
                } else cell[cl++]=row[k];
            }
            cell[cl]=0;
            hcells[hcnt++] = cell;
            if (p < b && row[p]=='|') p++;
        }
    }
    if (hcnt == 0) goto bad;
    for (size_t c = 0; c < hcnt; c++) {
        if (hcells[c][0]==0) goto bad;
        if (table_add_column(t, hcells[c], strlen(hcells[c]), TYPE_STRING) != 0) goto bad;
        tt_free(hcells[c]);
    }
    /* skip separator line 1 */
    /* data rows from 2 */
    for (size_t r = 2; r < nlines; r++) {
        char *row = lines[r];
        size_t rl = strlen(row);
        size_t a = 0, b = rl;
        while (a < b && row[a]==' ') a++;
        if (a < b && row[a]=='|') a++;
        while (b > a && row[b-1]==' ') b--;
        if (b > a && row[b-1]=='|') b--;
        Cell *crow = tt_calloc(hcnt, sizeof(Cell));
        size_t p = a, ci = 0;
        while (p < b && ci < hcnt) {
            size_t cs = p;
            while (p < b) {
                if (row[p]=='\\' && p+1 < b) { p += 2; continue; }
                if (row[p]=='|') break;
                p++;
            }
            size_t ce = p;
            while (cs < ce && row[cs]==' ') cs++;
            while (ce > cs && row[ce-1]==' ') ce--;
            char *cell = tt_malloc(ce-cs+1); size_t cl = 0;
            for (size_t k = cs; k < ce; k++) {
                if (row[k]=='\\' && k+1 < ce) {
                    char e = row[++k];
                    if (e=='|' || e=='\\') cell[cl++]=e;
                    else if (e=='n') cell[cl++]='\n';
                    else if (e=='r') cell[cl++]='\r';
                    else if (e=='t') cell[cl++]='\t';
                    else { tt_free(cell); goto bad; }
                } else cell[cl++]=row[k];
            }
            cell[cl]=0;
            if (null_token && cl == null_len && memcmp(cell, null_token, null_len)==0) {
                crow[ci].is_null = true; tt_free(cell);
            } else {
                cell_set_string_owned(&crow[ci], cell, cl);
            }
            ci++;
            if (p < b && row[p]=='|') p++;
        }
        if (ci != hcnt) { /* free crow */ goto bad; }
        table_append_row(t, crow);
    }
    for (size_t k=0;k<nlines;k++) tt_free(lines[k]); tt_free(lines);
    return EXIT_OK;
bad:
    for (size_t k=0;k<nlines;k++) tt_free(lines[k]); tt_free(lines);
    return EXIT_DATA;
}
