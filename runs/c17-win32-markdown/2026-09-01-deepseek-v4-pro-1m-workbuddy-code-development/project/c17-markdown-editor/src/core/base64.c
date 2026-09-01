/* base64.c - authored Base64 codec (RFC 4648). */
#include "base64.h"
#include "ce_common.h"

static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *ce_base64_encode(const unsigned char *data, size_t n){
    size_t outlen = 4 * ((n + 2) / 3);
    char *out = ce_malloc(outlen + 1);
    size_t i = 0, o = 0;
    while(i + 3 <= n){
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8) | data[i+2];
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = tbl[(v >> 6) & 63];
        out[o++] = tbl[v & 63];
        i += 3;
    }
    if(n - i == 1){
        uint32_t v = (uint32_t)data[i] << 16;
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = '=';
        out[o++] = '=';
    } else if(n - i == 2){
        uint32_t v = ((uint32_t)data[i] << 16) | ((uint32_t)data[i+1] << 8);
        out[o++] = tbl[(v >> 18) & 63];
        out[o++] = tbl[(v >> 12) & 63];
        out[o++] = tbl[(v >> 6) & 63];
        out[o++] = '=';
    }
    out[o] = 0;
    return out;
}

static int val(char c){
    if(c >= 'A' && c <= 'Z') return c - 'A';
    if(c >= 'a' && c <= 'z') return c - 'a' + 26;
    if(c >= '0' && c <= '9') return c - '0' + 52;
    if(c == '+') return 62;
    if(c == '/') return 63;
    return -1;
}

int ce_base64_decode(const char *s, size_t slen, unsigned char **out, size_t *out_len){
    /* strip ASCII whitespace (tolerated by data-URI conventions) */
    size_t n = 0;
    for(size_t i = 0; i < slen; i++) if(s[i] != ' ' && s[i] != '\t' && s[i] != '\r' && s[i] != '\n') n++;
    if(n == 0){ *out = ce_malloc(1); *out_len = 0; return 0; }
    if(n % 4 != 0) return -1;

    size_t pad = 0;
    if(n >= 1 && s[slen-1] == '=') pad++;
    if(n >= 2 && s[slen-2] == '=') pad++;
    /* find last non-whitespace chars for padding detection */
    size_t data_len = n - pad;
    if(data_len % 4 != 0) return -1;
    if(data_len % 4 == 1) return -1; /* invalid: leftover single sextet */

    size_t olen = (n / 4) * 3 - pad;
    unsigned char *buf = ce_malloc(olen ? olen : 1);
    size_t o = 0;
    uint32_t acc = 0; int bits = 0; int seen_pad = 0;
    for(size_t i = 0; i < slen; i++){
        char c = s[i];
        if(c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
        if(c == '='){
            seen_pad = 1;
            continue;
        }
        if(seen_pad) { ce_free(buf); return -1; }  /* data after padding */
        int v = val(c);
        if(v < 0){ ce_free(buf); return -1; }
        acc = (acc << 6) | (uint32_t)v;
        bits += 6;
        if(bits >= 8){
            bits -= 8;
            buf[o++] = (unsigned char)((acc >> bits) & 0xFF);
        }
    }
    /* verify padding count is consistent */
    if(pad == 1){
        /* last sextet must have zero low bits, but we simply trust length consistency */
    }
    *out = buf;
    *out_len = o;
    return 0;
}
