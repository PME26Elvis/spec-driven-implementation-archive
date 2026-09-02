#include "util.h"

int read_file_binary(const char *path, unsigned char **out, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return -1; }
    unsigned char *buf = tt_malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (n != (size_t)sz) { tt_free(buf); return -1; }
    buf[n] = 0;
    *out = buf;
    *out_len = n;
    return 0;
}

int write_file_binary(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    if (len && fwrite(data, 1, len, f) != len) { fclose(f); return -1; }
    if (fflush(f) != 0) { fclose(f); return -1; }
    if (fclose(f) != 0) return -1;
    return 0;
}

char *report_escape(const char *s, size_t n) {
    /* rough upper bound */
    size_t cap = n * 4 + 1;
    char *out = tt_malloc(cap);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\\') { out[j++]='\\'; out[j++]='\\'; }
        else if (c == '\n') { out[j++]='\\'; out[j++]='n'; }
        else if (c == '\r') { out[j++]='\\'; out[j++]='r'; }
        else if (c == '\t') { out[j++]='\\'; out[j++]='t'; }
        else if (c < 0x20 || c == 0x7F) {
            int nn = snprintf(out + j, cap - j, "\\x%02X", c);
            j += (size_t)nn;
        } else {
            out[j++] = (char)c;
        }
    }
    out[j] = 0;
    return out;
}
