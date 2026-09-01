/* lzss.c - LZSS with 4096-byte window, min match 3, max match 18.
 *
 * Encoded stream format:
 *   - flag byte: 8 control bits (MSB first). 1 = literal, 0 = match.
 *   - literal: 1 raw byte.
 *   - match: 12-bit offset (1..4096, distance back), 5-bit length (3..18),
 *     packed as two bytes: 
 *        b0 = (offset - 1) >> 4            (8 bits)
 *        b1 = ((offset - 1) & 0xF) << 4 | (len - 3)   (4 bits offset low + 4 bits len)
 *   Decompression uses the full sliding window.
 */
#include "lzss.h"
#include "ce_common.h"

#define WIN 4096
#define MIN_MATCH 3
#define MAX_MATCH 18

/* simple hash table for speed: hash on 3 bytes */
#define HASH_BITS 14
#define HASH_SIZE (1<<HASH_BITS)

typedef struct {
    unsigned char *out;
    size_t cap, len;
} outbuf;

static void ob_reserve(outbuf *o, size_t extra){
    if(o->len + extra <= o->cap) return;
    size_t nc = o->cap ? o->cap : 64;
    while(nc < o->len + extra) nc *= 2;
    o->out = ce_realloc(o->out, nc);
    o->cap = nc;
}
static void ob_push(outbuf *o, unsigned char b){ ob_reserve(o, 1); o->out[o->len++] = b; }

static inline uint32_t h3(const unsigned char *p){
    return (uint32_t)((p[0] << 10) ^ (p[1] << 5) ^ p[2]) & (HASH_SIZE - 1);
}

unsigned char *ce_lzss_compress(const unsigned char *in, size_t in_len, size_t *out_len){
    outbuf o = {0,0,0};
    size_t i = 0;
    /* flag bytes are written with a backpatch pointer */
    size_t flag_pos = 0;
    unsigned char flag = 0; int nbits = 0;

    int32_t *head = ce_malloc(HASH_SIZE * sizeof(int32_t));
    int32_t *prev = ce_malloc((in_len ? in_len : 1) * sizeof(int32_t));
    for(int k = 0; k < HASH_SIZE; k++) head[k] = -1;

    /* append an initial dummy flag byte position */
    flag_pos = o.len;
    ob_push(&o, 0);

    while(i < in_len){
        int best_len = 0; int best_off = 0;
        if(i + MIN_MATCH <= in_len){
            uint32_t h = h3(in + i);
            int32_t cand = head[h];
            size_t max_off = i > (size_t)WIN ? (size_t)WIN : i;
            while(cand >= 0 && (size_t)(i - (size_t)cand) <= max_off){
                const unsigned char *c = in + cand;
                if(c[0] == in[i]){
                    size_t j = 1;
                    while(j < (size_t)MAX_MATCH && i + j < in_len && c[j] == in[i+j]) j++;
                    if(j >= MIN_MATCH && j > (size_t)best_len){
                        best_len = (int)j; best_off = (int)(i - (size_t)cand);
                        if(j == (size_t)MAX_MATCH) break;
                    }
                }
                cand = prev[cand];
            }
        }
        if(best_len >= MIN_MATCH){
            /* emit match */
            if(nbits == 8){ o.out[flag_pos] = flag; flag = 0; nbits = 0; flag_pos = o.len; ob_push(&o, 0); }
            flag <<= 1; nbits++;  /* 0 bit = match */
            int offm1 = best_off - 1;
            unsigned char b0 = (unsigned char)(offm1 >> 4);
            unsigned char b1 = (unsigned char)(((offm1 & 0xF) << 4) | (best_len - MIN_MATCH));
            ob_push(&o, b0); ob_push(&o, b1);
            for(int k = 0; k < best_len && i + k + MIN_MATCH <= in_len; k++){
                size_t p = i + k;
                uint32_t hh = h3(in + p);
                prev[p] = head[hh];
                head[hh] = (int32_t)p;
            }
            i += (size_t)best_len;
        } else {
            if(nbits == 8){ o.out[flag_pos] = flag; flag = 0; nbits = 0; flag_pos = o.len; ob_push(&o, 0); }
            flag = (unsigned char)((flag << 1) | 1); nbits++;  /* 1 bit = literal */
            ob_push(&o, in[i]);
            uint32_t hh = h3(in + i);
            prev[i] = head[hh];
            head[hh] = (int32_t)i;
            i++;
        }
    }
    ce_free(head);
    ce_free(prev);
    /* finalize flag */
    if(nbits > 0){ flag <<= (8 - nbits); o.out[flag_pos] = flag; }
    *out_len = o.len;
    return o.out;
}

unsigned char *ce_lzss_decompress(const unsigned char *in, size_t in_len, size_t *out_len){
    unsigned char *out = ce_malloc(1);
    size_t olen = 0, ocap = 1;
    size_t i = 0;
    while(i < in_len){
        unsigned char flag = in[i++];
        for(int b = 7; b >= 0; b--){
            if((flag >> b) & 1){
                /* literal */
                if(i >= in_len) goto malformed;
                if(olen + 1 > ocap){ ocap *= 2; out = ce_realloc(out, ocap); }
                out[olen++] = in[i++];
            } else {
                /* match */
                if(i + 1 >= in_len) goto malformed;
                int offm1 = ((int)in[i] << 4) | (in[i+1] >> 4);
                int mlen = (in[i+1] & 0xF) + MIN_MATCH;
                i += 2;
                int off = offm1 + 1;
                if(off <= 0 || (size_t)off > olen) goto malformed;
                if(olen + (size_t)mlen > ocap){
                    while(ocap < olen + (size_t)mlen) ocap *= 2;
                    out = ce_realloc(out, ocap);
                }
                for(int k = 0; k < mlen; k++){
                    out[olen] = out[olen - (size_t)off];
                    olen++;
                }
            }
            /* we may run out of flags but have trailing bits that were padding */
            if(i >= in_len) break;
        }
    }
    *out_len = olen;
    return out;
malformed:
    ce_free(out);
    return NULL;
}
