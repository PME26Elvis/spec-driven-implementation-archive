/* sdk_lzss.c - canonical LZSS implementation (docs/19 section 13). */

#include "common/sdk_lzss.h"

#include <string.h>

size_t sdk_lzss_max_compressed_size(size_t in_len) {
    /* Every group of 8 literals costs 1 flag byte + 8 payload bytes. */
    size_t groups = (in_len + 7u) / 8u;
    return in_len + groups + 1u;
}

/* Finds the best match for the window ending at `pos`.
 * Scans candidates from the nearest offset towards the oldest so that
 * equal-length matches resolve to the nearest distance. */
static void find_match(const uint8_t *in, size_t in_len, size_t pos,
                       size_t *best_len, size_t *best_dist) {
    size_t max_len = in_len - pos;
    if (max_len > SDK_LZSS_MAX_MATCH) {
        max_len = SDK_LZSS_MAX_MATCH;
    }
    *best_len = 0;
    *best_dist = 0;
    if (max_len < SDK_LZSS_MIN_MATCH) {
        return;
    }

    size_t max_dist = pos < (size_t)SDK_LZSS_WINDOW ? pos : (size_t)SDK_LZSS_WINDOW;
    for (size_t dist = 1; dist <= max_dist; ++dist) {
        size_t start = pos - dist;
        size_t len = 0;
        while (len < max_len && in[start + len] == in[pos + len]) {
            ++len;
        }
        if (len > *best_len) {
            *best_len = len;
            *best_dist = dist;
            if (len == max_len) {
                break; /* cannot improve further */
            }
        }
    }
    if (*best_len < SDK_LZSS_MIN_MATCH) {
        *best_len = 0;
        *best_dist = 0;
    }
}

sdk_status sdk_lzss_compress(const uint8_t *in, size_t in_len,
                             uint8_t *out, size_t out_cap, size_t *out_len) {
    size_t op = 0;
    size_t pos = 0;
    size_t flag_index = 0;   /* position of the current flag byte in `out` */
    unsigned token_in_group = 8;  /* forces a fresh flag byte on first token */

    if (!out_len) {
        return SDK_ERR_USAGE;
    }
    *out_len = 0;

    while (pos < in_len) {
        if (token_in_group == 8) {
            if (op + 1 > out_cap) {
                return SDK_ERR_LIMIT;
            }
            flag_index = op;
            out[op++] = 0;
            token_in_group = 0;
        }

        size_t mlen = 0, mdist = 0;
        find_match(in, in_len, pos, &mlen, &mdist);

        if (mlen >= SDK_LZSS_MIN_MATCH) {
            if (op + 2 > out_cap) {
                return SDK_ERR_LIMIT;
            }
            uint16_t word = (uint16_t)(((uint16_t)(mdist - 1) << 4) |
                                       (uint16_t)(mlen - SDK_LZSS_MIN_MATCH));
            sdk_put_u16le(out + op, word);
            op += 2;
            pos += mlen;
            /* flag bit stays 0 for a match */
        } else {
            if (op + 1 > out_cap) {
                return SDK_ERR_LIMIT;
            }
            out[flag_index] |= (uint8_t)(1u << token_in_group);
            out[op++] = in[pos++];
        }
        ++token_in_group;
    }

    *out_len = op;
    return SDK_OK;
}

sdk_status sdk_lzss_decompress(const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t expected_len) {
    size_t ip = 0;
    size_t op = 0;

    while (op < expected_len) {
        if (ip >= in_len) {
            return SDK_ERR_DATA; /* truncated: no flag byte */
        }
        uint8_t flags = in[ip++];
        for (unsigned bit = 0; bit < 8u; ++bit) {
            if (op == expected_len) {
                /* All remaining flag bits of this group must be zero and no
                 * further token bytes may follow. */
                for (unsigned rest = bit; rest < 8u; ++rest) {
                    if (flags & (1u << rest)) {
                        return SDK_ERR_DATA;
                    }
                }
                if (ip != in_len) {
                    return SDK_ERR_DATA; /* trailing garbage */
                }
                return SDK_OK;
            }
            if (flags & (1u << bit)) {
                if (ip + 1 > in_len) {
                    return SDK_ERR_DATA;
                }
                out[op++] = in[ip++];
            } else {
                if (ip + 2 > in_len) {
                    return SDK_ERR_DATA;
                }
                uint16_t word = sdk_get_u16le(in + ip);
                ip += 2;
                size_t dist = (size_t)((word >> 4) & 0x0FFFu) + 1u;
                size_t len = (size_t)(word & 0x000Fu) + SDK_LZSS_MIN_MATCH;
                if (dist > op) {
                    return SDK_ERR_DATA; /* references bytes never produced */
                }
                if (len > expected_len - op) {
                    return SDK_ERR_DATA; /* overruns declared length */
                }
                size_t src = op - dist;
                for (size_t k = 0; k < len; ++k) {
                    out[op + k] = out[src + k];
                }
                op += len;
            }
        }
    }

    if (ip != in_len) {
        return SDK_ERR_DATA; /* trailing garbage */
    }
    return SDK_OK;
}
