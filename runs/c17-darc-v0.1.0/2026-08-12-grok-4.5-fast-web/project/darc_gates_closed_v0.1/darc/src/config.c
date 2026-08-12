#define _POSIX_C_SOURCE 200809L
#include "darc_config.h"
#include "darc_sha256.h"
#include "darc_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void darc_config_defaults(darc_config_t *c) {
    memset(c, 0, sizeof(*c));
    c->chunk_min = 16384;
    c->chunk_avg = 65536;
    c->chunk_max = 262144;
    c->min_savings_bytes = 32;
    c->compression_enabled = true;
    c->parity_enabled = true;
    c->parity_data_members = 8;
    snprintf(c->format, sizeof(c->format), "text");
}

void darc_config_free(darc_config_t *c) {
    for (size_t i = 0; i < c->n_include; ++i) free(c->include_globs[i]);
    for (size_t i = 0; i < c->n_exclude; ++i) free(c->exclude_globs[i]);
    free(c->include_globs);
    free(c->exclude_globs);
    c->include_globs = c->exclude_globs = NULL;
    c->n_include = c->n_exclude = 0;
}

/* ---- Minimal JSON parser for config subset ---- */
typedef struct {
    const char *s;
    size_t i, n;
    int depth;
} jctx_t;

static void jskip(jctx_t *j) {
    while (j->i < j->n && (j->s[j->i]==' '||j->s[j->i]=='\t'||j->s[j->i]=='\n'||j->s[j->i]=='\r'))
        j->i++;
}

static int jexpect(jctx_t *j, char c) {
    jskip(j);
    if (j->i >= j->n || j->s[j->i] != c) return -1;
    j->i++;
    return 0;
}

static int jstring(jctx_t *j, char *out, size_t outcap) {
    jskip(j);
    if (j->i >= j->n || j->s[j->i] != '"') return -1;
    j->i++;
    size_t o = 0;
    while (j->i < j->n && j->s[j->i] != '"') {
        char c = j->s[j->i++];
        if (c == '\\') {
            if (j->i >= j->n) return -1;
            char e = j->s[j->i++];
            switch (e) {
                case '"': case '\\': case '/': c = e; break;
                case 'b': c = '\b'; break;
                case 'f': c = '\f'; break;
                case 'n': c = '\n'; break;
                case 'r': c = '\r'; break;
                case 't': c = '\t'; break;
                case 'u': {
                    if (j->i + 4 > j->n) return -1;
                    unsigned code = 0;
                    for (int k = 0; k < 4; ++k) {
                        char h = j->s[j->i++];
                        code <<= 4;
                        if (h >= '0' && h <= '9') code |= h - '0';
                        else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                        else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                        else return -1;
                    }
                    if (code < 0x80) c = (char)code;
                    else if (code < 0x800) {
                        if (o + 2 >= outcap) return -1;
                        out[o++] = (char)(0xC0 | (code >> 6));
                        out[o++] = (char)(0x80 | (code & 0x3F));
                        continue;
                    } else {
                        if (o + 3 >= outcap) return -1;
                        out[o++] = (char)(0xE0 | (code >> 12));
                        out[o++] = (char)(0x80 | ((code >> 6) & 0x3F));
                        out[o++] = (char)(0x80 | (code & 0x3F));
                        continue;
                    }
                    break;
                }
                default: return -1;
            }
        }
        if (o + 1 >= outcap) return -1;
        out[o++] = c;
    }
    if (j->i >= j->n || j->s[j->i] != '"') return -1;
    j->i++;
    out[o] = 0;
    return 0;
}

static int jnumber(jctx_t *j, double *out) {
    jskip(j);
    char buf[64]; size_t o = 0;
    if (j->i < j->n && (j->s[j->i]=='-'||j->s[j->i]=='+')) buf[o++] = j->s[j->i++];
    while (j->i < j->n && isdigit((unsigned char)j->s[j->i]) && o+1 < sizeof(buf))
        buf[o++] = j->s[j->i++];
    if (j->i < j->n && j->s[j->i]=='.') {
        buf[o++] = j->s[j->i++];
        while (j->i < j->n && isdigit((unsigned char)j->s[j->i]) && o+1 < sizeof(buf))
            buf[o++] = j->s[j->i++];
    }
    buf[o] = 0;
    if (o == 0) return -1;
    *out = atof(buf);
    return 0;
}

static int jbool(jctx_t *j, bool *out) {
    jskip(j);
    if (j->i + 4 <= j->n && memcmp(j->s + j->i, "true", 4) == 0) { j->i += 4; *out = true; return 0; }
    if (j->i + 5 <= j->n && memcmp(j->s + j->i, "false", 5) == 0) { j->i += 5; *out = false; return 0; }
    return -1;
}

static int apply_key(darc_config_t *c, const char *key, const char *sval, double nval, int is_num, int is_bool, bool bval) {
    if (strcmp(key, "chunk_min") == 0 || strcmp(key, "chunking.min") == 0) {
        if (is_num) c->chunk_min = (uint64_t)nval; return 0;
    }
    if (strcmp(key, "chunk_avg") == 0 || strcmp(key, "chunking.avg") == 0) {
        if (is_num) c->chunk_avg = (uint64_t)nval; return 0;
    }
    if (strcmp(key, "chunk_max") == 0 || strcmp(key, "chunking.max") == 0) {
        if (is_num) c->chunk_max = (uint64_t)nval; return 0;
    }
    if (strcmp(key, "min_savings_bytes") == 0 || strcmp(key, "compression.min_savings_bytes") == 0) {
        if (is_num) c->min_savings_bytes = (uint64_t)nval; return 0;
    }
    if (strcmp(key, "compression") == 0 || strcmp(key, "compression.enabled") == 0) {
        if (is_bool) c->compression_enabled = bval; return 0;
    }
    if (strcmp(key, "parity") == 0 || strcmp(key, "parity.enabled") == 0) {
        if (is_bool) c->parity_enabled = bval; return 0;
    }
    if (strcmp(key, "format") == 0) {
        if (sval) snprintf(c->format, sizeof(c->format), "%s", sval); return 0;
    }
    if (strcmp(key, "quiet") == 0) { if (is_bool) c->quiet = bval; return 0; }
    if (strcmp(key, "verbose") == 0) { if (is_bool) c->verbose = bval; return 0; }
    /* unknown keys: ignore for forward compatibility or could error */
    return 0;
}

static int jparse_value(jctx_t *j, darc_config_t *c, const char *key);

static int jparse_object(jctx_t *j, darc_config_t *c, const char *prefix) {
    if (j->depth > 64) return -1;
    j->depth++;
    if (jexpect(j, '{') != 0) { j->depth--; return -1; }
    jskip(j);
    if (j->i < j->n && j->s[j->i] == '}') { j->i++; j->depth--; return 0; }
    while (1) {
        char key[256];
        if (jstring(j, key, sizeof(key)) != 0) { j->depth--; return -1; }
        if (jexpect(j, ':') != 0) { j->depth--; return -1; }
        char fullkey[512];
        if (prefix && prefix[0])
            snprintf(fullkey, sizeof(fullkey), "%s.%s", prefix, key);
        else
            snprintf(fullkey, sizeof(fullkey), "%s", key);
        if (jparse_value(j, c, fullkey) != 0) { j->depth--; return -1; }
        jskip(j);
        if (j->i < j->n && j->s[j->i] == ',') { j->i++; continue; }
        if (j->i < j->n && j->s[j->i] == '}') { j->i++; break; }
        j->depth--; return -1;
    }
    j->depth--;
    return 0;
}

static int jparse_value(jctx_t *j, darc_config_t *c, const char *key) {
    jskip(j);
    if (j->i >= j->n) return -1;
    char ch = j->s[j->i];
    if (ch == '{') return jparse_object(j, c, key);
    if (ch == '[') {
        /* skip arrays for now (include/exclude) */
        j->i++;
        int depth = 1;
        while (j->i < j->n && depth) {
            if (j->s[j->i]=='[') depth++;
            else if (j->s[j->i]==']') depth--;
            else if (j->s[j->i]=='"') {
                j->i++;
                while (j->i < j->n && j->s[j->i] != '"') {
                    if (j->s[j->i]=='\\') j->i++;
                    j->i++;
                }
            }
            j->i++;
        }
        return 0;
    }
    if (ch == '"') {
        char s[512];
        if (jstring(j, s, sizeof(s)) != 0) return -1;
        return apply_key(c, key, s, 0, 0, 0, false);
    }
    if (ch == 't' || ch == 'f') {
        bool b;
        if (jbool(j, &b) != 0) return -1;
        return apply_key(c, key, NULL, 0, 0, 1, b);
    }
    if (ch == 'n') {
        if (j->i + 4 <= j->n && memcmp(j->s+j->i, "null", 4)==0) { j->i += 4; return 0; }
        return -1;
    }
    if (ch == '-' || isdigit((unsigned char)ch)) {
        double n;
        if (jnumber(j, &n) != 0) return -1;
        return apply_key(c, key, NULL, n, 1, 0, false);
    }
    return -1;
}

int darc_config_load_json(const char *path, darc_config_t *c) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz < 0 || sz > 4*1024*1024) { fclose(f); return -1; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return -1; }
    fclose(f);
    buf[sz] = 0;
    /* strip BOM */
    size_t start = 0;
    if (sz >= 3 && (unsigned char)buf[0]==0xEF && (unsigned char)buf[1]==0xBB && (unsigned char)buf[2]==0xBF)
        start = 3;
    jctx_t j = { buf + start, 0, (size_t)sz - start, 0 };
    int rc = jparse_object(&j, c, "");
    jskip(&j);
    if (j.i != j.n) rc = -1; /* trailing garbage */
    free(buf);
    return rc;
}

/* ---- Minimal YAML subset (indentation maps, scalars, # comments) ---- */
static char *yaml_strip_comment(char *line) {
    int in_s = 0, in_d = 0;
    for (char *p = line; *p; ++p) {
        if (*p == '\'' && !in_d) in_s = !in_s;
        else if (*p == '"' && !in_s) in_d = !in_d;
        else if (*p == '#' && !in_s && !in_d) { *p = 0; break; }
    }
    /* rtrim */
    size_t n = strlen(line);
    while (n && (line[n-1]==' '||line[n-1]=='\t'||line[n-1]=='\n'||line[n-1]=='\r'))
        line[--n] = 0;
    return line;
}

int darc_config_load_yaml(const char *path, darc_config_t *c) {
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[1024];
    char prefix[256] = "";
    int base_indent = -1;
    while (fgets(line, sizeof(line), f)) {
        yaml_strip_comment(line);
        /* ltrim count indent */
        int indent = 0;
        char *p = line;
        while (*p == ' ') { indent++; p++; }
        while (*p == '\t') { indent += 2; p++; }
        if (*p == 0) continue;
        if (base_indent < 0) base_indent = indent;
        /* key: value */
        char *colon = strchr(p, ':');
        if (!colon) continue;
        *colon = 0;
        char *key = p;
        /* rtrim key */
        size_t kn = strlen(key);
        while (kn && (key[kn-1]==' '||key[kn-1]=='\t')) key[--kn] = 0;
        char *val = colon + 1;
        while (*val == ' ' || *val == '\t') val++;
        if (*val == 0) {
            /* nested section start */
            snprintf(prefix, sizeof(prefix), "%s", key);
            continue;
        }
        /* unquote */
        if ((*val == '"' || *val == '\'') && val[strlen(val)-1] == *val) {
            val[strlen(val)-1] = 0;
            val++;
        }
        char fullkey[512];
        if (indent > base_indent && prefix[0])
            snprintf(fullkey, sizeof(fullkey), "%s.%s", prefix, key);
        else
            snprintf(fullkey, sizeof(fullkey), "%s", key);

        if (strcmp(val, "true") == 0)
            apply_key(c, fullkey, NULL, 0, 0, 1, true);
        else if (strcmp(val, "false") == 0)
            apply_key(c, fullkey, NULL, 0, 0, 1, false);
        else if (strcmp(val, "null") == 0 || strcmp(val, "~") == 0)
            ;
        else {
            char *end = NULL;
            double n = strtod(val, &end);
            if (end && end != val && *end == 0)
                apply_key(c, fullkey, NULL, n, 1, 0, false);
            else
                apply_key(c, fullkey, val, 0, 0, 0, false);
        }
    }
    fclose(f);
    return 0;
}

int darc_config_load(const char *path, darc_config_t *c) {
    size_t n = strlen(path);
    if (n >= 5 && strcmp(path + n - 5, ".json") == 0)
        return darc_config_load_json(path, c);
    if ((n >= 5 && strcmp(path + n - 5, ".yaml") == 0) ||
        (n >= 4 && strcmp(path + n - 4, ".yml") == 0))
        return darc_config_load_yaml(path, c);
    return -2; /* E_CONFIG_FORMAT */
}

int darc_config_validate_file(const char *path) {
    darc_config_t c;
    darc_config_defaults(&c);
    int rc = darc_config_load(path, &c);
    darc_config_free(&c);
    return rc;
}

void darc_config_compute_hashes(darc_config_t *c) {
    /* deterministic normalized form for hashing */
    char buf[512];
    snprintf(buf, sizeof(buf),
        "{\"chunk_min\":%llu,\"chunk_avg\":%llu,\"chunk_max\":%llu,"
        "\"min_savings_bytes\":%llu,\"compression\":%s,\"parity\":%s}",
        (unsigned long long)c->chunk_min,
        (unsigned long long)c->chunk_avg,
        (unsigned long long)c->chunk_max,
        (unsigned long long)c->min_savings_bytes,
        c->compression_enabled ? "true" : "false",
        c->parity_enabled ? "true" : "false");
    darc_sha256(buf, strlen(buf), c->config_hash);
    /* profile subset */
    snprintf(buf, sizeof(buf),
        "{\"chunk_min\":%llu,\"chunk_avg\":%llu,\"chunk_max\":%llu}",
        (unsigned long long)c->chunk_min,
        (unsigned long long)c->chunk_avg,
        (unsigned long long)c->chunk_max);
    darc_sha256(buf, strlen(buf), c->profile_hash);
}
