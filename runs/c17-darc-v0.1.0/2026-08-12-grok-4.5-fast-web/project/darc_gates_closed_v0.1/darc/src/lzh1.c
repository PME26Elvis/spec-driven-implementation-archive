#include "darc_lzh1.h"
#include "darc_util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LZ77_WINDOW 32768
#define LZ77_MIN_MATCH 3
#define LZ77_MAX_MATCH 258

/* Simple LZ77: emit tokens into a growable buffer */
typedef struct {
    uint8_t *data;
    size_t len;
    size_t cap;
} buf_t;

static int buf_append(buf_t *b, const void *p, size_t n) {
    if (b->len + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n) nc *= 2;
        uint8_t *nd = realloc(b->data, nc);
        if (!nd) return -1;
        b->data = nd;
        b->cap = nc;
    }
    memcpy(b->data + b->len, p, n);
    b->len += n;
    return 0;
}

static int buf_append_u8(buf_t *b, uint8_t v) {
    return buf_append(b, &v, 1);
}

static int emit_literal(buf_t *tok, uint8_t c) {
    if (buf_append_u8(tok, 0x00) < 0) return -1;
    return buf_append_u8(tok, c);
}

static int emit_match(buf_t *tok, uint16_t dist, uint16_t len) {
    uint8_t tmp[5];
    tmp[0] = 0x01;
    darc_write_u16_le(tmp + 1, dist);
    darc_write_u16_le(tmp + 3, len);
    return buf_append(tok, tmp, 5);
}

/* Find longest match ending at pos, looking back up to window */
static void find_match(const uint8_t *data, size_t pos, size_t data_len,
                       uint16_t *best_dist, uint16_t *best_len) {
    *best_dist = 0;
    *best_len = 0;
    size_t max_look = pos > LZ77_WINDOW ? LZ77_WINDOW : pos;
    size_t max_match = data_len - pos;
    if (max_match > LZ77_MAX_MATCH) max_match = LZ77_MAX_MATCH;
    if (max_match < LZ77_MIN_MATCH) return;

    for (size_t d = 1; d <= max_look; ++d) {
        size_t len = 0;
        while (len < max_match && data[pos + len] == data[pos - d + len])
            ++len;
        if (len >= LZ77_MIN_MATCH && len > *best_len) {
            *best_len = (uint16_t)len;
            *best_dist = (uint16_t)d;
            if (len == max_match) break; /* cannot do better */
        } else if (len == *best_len && len >= LZ77_MIN_MATCH && d < *best_dist) {
            *best_dist = (uint16_t)d; /* smaller distance on tie */
        }
    }
}

static int lz77_tokenize(const uint8_t *in, size_t in_len, buf_t *tok) {
    size_t pos = 0;
    while (pos < in_len) {
        uint16_t dist = 0, mlen = 0;
        find_match(in, pos, in_len, &dist, &mlen);
        if (mlen >= LZ77_MIN_MATCH) {
            if (emit_match(tok, dist, mlen) < 0) return -1;
            pos += mlen;
        } else {
            if (emit_literal(tok, in[pos]) < 0) return -1;
            pos += 1;
        }
    }
    return 0;
}

/* Canonical Huffman */
typedef struct {
    uint32_t freq;
    int16_t left, right; /* -1 for leaf, or symbol for leaf? use separate */
    uint16_t min_sym; /* for tie-break */
    int is_leaf;
    uint8_t symbol;
} hnode_t;

/* Build code lengths from frequencies */
static void build_code_lengths(const uint32_t freq[256], uint8_t lengths[256]) {
    memset(lengths, 0, 256);
    int nactive = 0;
    for (int i = 0; i < 256; ++i) if (freq[i]) nactive++;

    if (nactive == 0) return;
    if (nactive == 1) {
        for (int i = 0; i < 256; ++i) if (freq[i]) { lengths[i] = 1; return; }
    }

    /* Simple priority queue of nodes using array (for 256 symbols, max ~511 nodes) */
    hnode_t nodes[512];
    int nnodes = 0;
    int leaves[256];
    int nleaves = 0;

    for (int i = 0; i < 256; ++i) {
        if (freq[i]) {
            nodes[nnodes].freq = freq[i];
            nodes[nnodes].left = -1;
            nodes[nnodes].right = -1;
            nodes[nnodes].min_sym = (uint16_t)i;
            nodes[nnodes].is_leaf = 1;
            nodes[nnodes].symbol = (uint8_t)i;
            leaves[nleaves++] = nnodes;
            nnodes++;
        }
    }

    /* Use a simple selection for min each time (n=256, fine) */
    while (nleaves > 1) {
        /* Find two smallest */
        int a = 0, b = 1;
        if (nodes[leaves[b]].freq < nodes[leaves[a]].freq ||
            (nodes[leaves[b]].freq == nodes[leaves[a]].freq &&
             nodes[leaves[b]].min_sym < nodes[leaves[a]].min_sym)) {
            int t = a; a = b; b = t;
        }
        for (int i = 2; i < nleaves; ++i) {
            int idx = leaves[i];
            if (nodes[idx].freq < nodes[leaves[a]].freq ||
                (nodes[idx].freq == nodes[leaves[a]].freq &&
                 nodes[idx].min_sym < nodes[leaves[a]].min_sym)) {
                b = a;
                a = i;
            } else if (nodes[idx].freq < nodes[leaves[b]].freq ||
                       (nodes[idx].freq == nodes[leaves[b]].freq &&
                        nodes[idx].min_sym < nodes[leaves[b]].min_sym)) {
                b = i;
            }
        }
        int ia = leaves[a], ib = leaves[b];
        /* Create parent */
        nodes[nnodes].freq = nodes[ia].freq + nodes[ib].freq;
        nodes[nnodes].left = (int16_t)ia;
        nodes[nnodes].right = (int16_t)ib;
        nodes[nnodes].min_sym = nodes[ia].min_sym < nodes[ib].min_sym ?
                                nodes[ia].min_sym : nodes[ib].min_sym;
        nodes[nnodes].is_leaf = 0;
        /* Remove a and b, add parent */
        if (a > b) { int t = a; a = b; b = t; }
        leaves[b] = leaves[nleaves - 1];
        nleaves--;
        leaves[a] = leaves[nleaves - 1];
        nleaves--;
        leaves[nleaves++] = nnodes;
        nnodes++;
    }

    /* Traverse to get depths */
    int stack[512];
    int depths[512];
    int sp = 0;
    stack[sp] = leaves[0];
    depths[sp] = 0;
    sp++;
    while (sp > 0) {
        sp--;
        int idx = stack[sp];
        int d = depths[sp];
        if (nodes[idx].is_leaf) {
            lengths[nodes[idx].symbol] = (uint8_t)d;
        } else {
            stack[sp] = nodes[idx].left;
            depths[sp] = d + 1;
            sp++;
            stack[sp] = nodes[idx].right;
            depths[sp] = d + 1;
            sp++;
        }
    }
}

/* Assign canonical codes */
static void assign_canonical(const uint8_t lengths[256], uint32_t codes[256], uint8_t *max_len) {
    memset(codes, 0, 256 * sizeof(uint32_t));
    *max_len = 0;
    int bl_count[33] = {0};
    for (int i = 0; i < 256; ++i) {
        if (lengths[i]) {
            bl_count[lengths[i]]++;
            if (lengths[i] > *max_len) *max_len = lengths[i];
        }
    }
    uint32_t next_code[33] = {0};
    uint32_t code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= 32; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }
    for (int i = 0; i < 256; ++i) {
        if (lengths[i]) {
            codes[i] = next_code[lengths[i]]++;
        }
    }
}

/* Bit writer */
typedef struct {
    buf_t b;
    uint8_t cur;
    int bits;
} bitwriter_t;

static void bw_init(bitwriter_t *bw) {
    memset(bw, 0, sizeof(*bw));
}

static int bw_write_bits(bitwriter_t *bw, uint32_t code, int nbits) {
    for (int i = nbits - 1; i >= 0; --i) {
        if (code & (1u << i))
            bw->cur |= (1u << (7 - bw->bits));
        bw->bits++;
        if (bw->bits == 8) {
            if (buf_append_u8(&bw->b, bw->cur) < 0) return -1;
            bw->cur = 0;
            bw->bits = 0;
        }
    }
    return 0;
}

static int bw_finish(bitwriter_t *bw) {
    if (bw->bits > 0) {
        if (buf_append_u8(&bw->b, bw->cur) < 0) return -1;
    }
    return 0;
}

uint8_t *darc_lzh1_compress(const uint8_t *in, size_t in_len, size_t *out_len) {
    buf_t tok = {0};
    if (lz77_tokenize(in, in_len, &tok) < 0) {
        free(tok.data);
        return NULL;
    }

    uint32_t freq[256] = {0};
    for (size_t i = 0; i < tok.len; ++i)
        freq[tok.data[i]]++;

    uint8_t lengths[256];
    build_code_lengths(freq, lengths);

    uint32_t codes[256];
    uint8_t max_len;
    assign_canonical(lengths, codes, &max_len);

    bitwriter_t bw;
    bw_init(&bw);
    for (size_t i = 0; i < tok.len; ++i) {
        uint8_t s = tok.data[i];
        if (bw_write_bits(&bw, codes[s], lengths[s]) < 0) {
            free(tok.data);
            free(bw.b.data);
            return NULL;
        }
    }
    if (bw_finish(&bw) < 0) {
        free(tok.data);
        free(bw.b.data);
        return NULL;
    }

    /* Build LZH1 payload */
    size_t payload_len = 4 + 8 + 256 + 8 + bw.b.len;
    uint8_t *out = malloc(payload_len);
    if (!out) {
        free(tok.data);
        free(bw.b.data);
        return NULL;
    }
    size_t off = 0;
    memcpy(out + off, "LZH1", 4); off += 4;
    darc_write_u64_le(out + off, tok.len); off += 8;
    memcpy(out + off, lengths, 256); off += 256;
    darc_write_u64_le(out + off, bw.b.len); off += 8;
    if (bw.b.len) memcpy(out + off, bw.b.data, bw.b.len);

    free(tok.data);
    free(bw.b.data);
    *out_len = payload_len;
    return out;
}


/* ---- Decompress ---- */

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
    uint8_t cur;
    int bits_left;
} bitreader_t;

static void br_init(bitreader_t *br, const uint8_t *data, size_t len) {
    br->data = data;
    br->len = len;
    br->pos = 0;
    br->cur = 0;
    br->bits_left = 0;
}

static int br_read_bit(bitreader_t *br) {
    if (br->bits_left == 0) {
        if (br->pos >= br->len) return -1;
        br->cur = br->data[br->pos++];
        br->bits_left = 8;
    }
    int bit = (br->cur >> 7) & 1;
    br->cur <<= 1;
    br->bits_left--;
    return bit;
}

/* Decode one symbol using lengths + canonical codes via table or walk */
static int huffman_decode_symbol(bitreader_t *br, const uint8_t lengths[256],
                                 const uint32_t codes[256], uint8_t max_len) {
    uint32_t code = 0;
    for (int len = 1; len <= max_len; ++len) {
        int b = br_read_bit(br);
        if (b < 0) return -1;
        code = (code << 1) | (uint32_t)b;
        for (int s = 0; s < 256; ++s) {
            if (lengths[s] == (uint8_t)len && codes[s] == code)
                return s;
        }
    }
    return -1; /* invalid */
}

static int lz77_decode(const uint8_t *tok, size_t tok_len, uint8_t *out, size_t out_cap, size_t *out_len) {
    size_t o = 0;
    size_t i = 0;
    while (i < tok_len) {
        if (tok[i] == 0x00) {
            if (i + 1 >= tok_len) return -1;
            if (o >= out_cap) return -1;
            out[o++] = tok[i + 1];
            i += 2;
        } else if (tok[i] == 0x01) {
            if (i + 5 > tok_len) return -1;
            uint16_t dist = darc_read_u16_le(tok + i + 1);
            uint16_t mlen = darc_read_u16_le(tok + i + 3);
            if (dist < 1 || dist > LZ77_WINDOW || mlen < 3 || mlen > LZ77_MAX_MATCH) return -1;
            if (o < dist) return -1;
            if (o + mlen > out_cap) return -1;
            for (uint16_t k = 0; k < mlen; ++k)
                out[o + k] = out[o - dist + k];
            o += mlen;
            i += 5;
        } else {
            return -1;
        }
    }
    *out_len = o;
    return 0;
}

uint8_t *darc_lzh1_decompress(const uint8_t *in, size_t in_len, size_t expected_raw_len, size_t *out_len) {
    if (in_len < 4 + 8 + 256 + 8) return NULL;
    if (memcmp(in, "LZH1", 4) != 0) return NULL;
    size_t off = 4;
    uint64_t token_bytes_len = darc_read_u64_le(in + off); off += 8;
    if (token_bytes_len > (1ULL << 30)) return NULL;
    const uint8_t *lengths = in + off; off += 256;
    uint64_t bitstream_len = darc_read_u64_le(in + off); off += 8;
    if (off + bitstream_len > in_len) return NULL;
    const uint8_t *bitstream = in + off;

    /* Rebuild canonical codes from lengths */
    uint32_t codes[256];
    uint8_t max_len = 0;
    assign_canonical(lengths, codes, &max_len);

    /* Decode bitstream to token bytes */
    uint8_t *tok = NULL;
    size_t tok_cap = 0, tok_len = 0;
    if (token_bytes_len > 0) {
        tok = malloc((size_t)token_bytes_len);
        if (!tok) return NULL;
        bitreader_t br;
        br_init(&br, bitstream, (size_t)bitstream_len);
        for (uint64_t i = 0; i < token_bytes_len; ++i) {
            int s = huffman_decode_symbol(&br, lengths, codes, max_len);
            if (s < 0) { free(tok); return NULL; }
            tok[tok_len++] = (uint8_t)s;
        }
    }

    /* LZ77 expand */
    size_t raw_cap = expected_raw_len ? expected_raw_len : (token_bytes_len * 2 + 64);
    if (raw_cap < 1) raw_cap = 1;
    uint8_t *raw = malloc(raw_cap);
    if (!raw) { free(tok); return NULL; }
    size_t raw_len = 0;
    if (token_bytes_len == 0) {
        raw_len = 0;
    } else {
        if (lz77_decode(tok, tok_len, raw, raw_cap, &raw_len) < 0) {
            free(tok); free(raw); return NULL;
        }
    }
    free(tok);

    if (expected_raw_len && raw_len != expected_raw_len) {
        free(raw); return NULL;
    }
    *out_len = raw_len;
    return raw;
}
