#include "common.h"
#include <stdio.h>
#include <string.h>

/* Code 128 patterns 0-106 */
static const char *C128_PAT[] = {
"212222","222122","222221","121223","121322","131222","122213","122312","132212","221213",
"221312","231212","112232","122132","122231","113222","123122","123221","223211","221132",
"221231","213212","223112","312131","311222","321122","321221","312212","322112","322211",
"212123","212321","232121","111323","131123","131321","112313","132113","132311","211313",
"231113","231311","112133","112331","132131","113123","113321","133121","313121","211331",
"231131","213113","213311","213131","311123","311321","331121","312113","312311","332111",
"314111","221411","431111","111224","111422","121124","121421","141122","141221","112214",
"112412","122114","122411","142112","142211","241211","221114","413111","241112","134111",
"111242","121142","121241","114212","124112","124211","411212","421112","421211","212141",
"214121","412121","111143","111341","131141","114113","114311","411113","411311","113141",
"114131","311141","411131","211412","211214","211232","2331112"
};

#define START_B 104
#define START_C 105
#define CODE_B 100
#define CODE_C 99
#define STOP 106

/* Optimal encoding using DP. Returns number of codewords (excluding checksum/stop), fills codes[]. */
int code128_encode(const char *payload, size_t n, int *codes, int *n_codes) {
    if (n == 0 || n > 256) return -1;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)payload[i];
        if (c < 32 || c > 126) return -1;
    }
    /* DP: cost[i][set] = min pre-checksum codewords to encode payload[i..] starting in set (0=B,1=C) */
    /* For simplicity and correctness, use recursion with memo for small n<=256 */
    /* We implement a practical DP. */
    const int INF = 100000;
    int cost[257][2];
    int choice[257][2]; /* 0 = take B run of len, 1 = take C run of pairs, encoded as length */
    for (int i = 0; i <= (int)n; i++) { cost[i][0] = cost[i][1] = INF; }
    cost[n][0] = cost[n][1] = 0;

    for (int i = (int)n - 1; i >= 0; i--) {
        /* from B */
        /* take at least 1 B */
        int c = 1 + cost[i+1][0];
        if (c < cost[i][0]) { cost[i][0] = c; choice[i][0] = 1; /* len 1 B */ }
        for (int len = 2; i + len <= (int)n; len++) {
            c = len + cost[i+len][0];
            if (c < cost[i][0]) { cost[i][0] = c; choice[i][0] = len; }
        }
        /* switch to C if even digits */
        if (i + 2 <= (int)n && payload[i] >= '0' && payload[i] <= '9' &&
            payload[i+1] >= '0' && payload[i+1] <= '9') {
            int pairs = 1;
            int j = i + 2;
            while (j + 1 < (int)n && payload[j] >= '0' && payload[j] <= '9' &&
                   payload[j+1] >= '0' && payload[j+1] <= '9') {
                pairs++; j += 2;
            }
            /* switch cost + pairs + continue in C */
            c = 1 /*switch*/ + pairs + cost[i + pairs*2][1];
            if (c < cost[i][0]) { cost[i][0] = c; choice[i][0] = -pairs; /* negative means switch C */ }
        }
        /* from C */
        if (i + 2 <= (int)n && payload[i] >= '0' && payload[i] <= '9' &&
            payload[i+1] >= '0' && payload[i+1] <= '9') {
            int pairs = 1;
            int j = i + 2;
            while (j + 1 < (int)n && payload[j] >= '0' && payload[j] <= '9' &&
                   payload[j+1] >= '0' && payload[j+1] <= '9') {
                pairs++; j += 2;
            }
            c = pairs + cost[i + pairs*2][1];
            if (c < cost[i][1]) { cost[i][1] = c; choice[i][1] = pairs; }
        }
        /* switch to B */
        c = 1 + cost[i][0]; /* switch then from B cost, but cost[i][0] already includes data from i */
        /* careful: cost[i][0] is data from i in B */
        c = 1 + cost[i][0];
        if (c < cost[i][1]) { cost[i][1] = c; choice[i][1] = -1; /* switch B */ }
    }

    /* Choose start */
    int start_set;
    int total;
    if (cost[0][0] <= cost[0][1]) {
        start_set = 0;
        total = 1 + cost[0][0]; /* start + data/switches */
    } else {
        start_set = 1;
        total = 1 + cost[0][1];
    }
    /* Tie-break: fewer switches, Start B over C, lex smaller -- simplified: prefer B if equal cost */
    if (cost[0][0] == cost[0][1]) start_set = 0;

    /* Reconstruct */
    int pos = 0;
    int set = start_set;
    codes[pos++] = (set == 0) ? START_B : START_C;
    int i = 0;
    while (i < (int)n) {
        int ch = choice[i][set];
        if (set == 0) {
            if (ch > 0) {
                for (int k = 0; k < ch; k++) {
                    codes[pos++] = (unsigned char)payload[i+k] - 32;
                }
                i += ch;
            } else {
                /* switch to C */
                codes[pos++] = CODE_C;
                set = 1;
                int pairs = -ch;
                for (int k = 0; k < pairs; k++) {
                    int v = (payload[i] - '0') * 10 + (payload[i+1] - '0');
                    codes[pos++] = v;
                    i += 2;
                }
            }
        } else {
            if (ch > 0) {
                for (int k = 0; k < ch; k++) {
                    int v = (payload[i] - '0') * 10 + (payload[i+1] - '0');
                    codes[pos++] = v;
                    i += 2;
                }
            } else {
                codes[pos++] = CODE_B;
                set = 0;
            }
        }
    }
    /* checksum */
    int sum = codes[0];
    for (int k = 1; k < pos; k++) {
        sum += k * codes[k];
    }
    codes[pos++] = sum % 103;
    codes[pos++] = STOP;
    *n_codes = pos;
    return 0;
}

/* Get module pattern string for a code value */
const char *code128_pattern(int v) {
    if (v < 0 || v > 106) return NULL;
    return C128_PAT[v];
}
