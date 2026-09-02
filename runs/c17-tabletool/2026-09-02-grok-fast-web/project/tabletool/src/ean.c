#include "common.h"
#include <stdio.h>

/* EAN-13 check digit */
int ean13_check_digit(const char *d12) {
    int s = 0;
    for (int i = 0; i < 12; i++) {
        int d = d12[i] - '0';
        if (i % 2 == 0) s += d;
        else s += 3 * d;
    }
    return (10 - (s % 10)) % 10;
}

/* Validate or canonicalize. Input 12 or 13 digits. Output 13 digits in out (must be 14 bytes). */
int ean13_canonicalize(const char *s, size_t n, char *out) {
    if (n != 12 && n != 13) return -1;
    for (size_t i = 0; i < n; i++) if (s[i] < '0' || s[i] > '9') return -1;
    if (n == 12) {
        memcpy(out, s, 12);
        out[12] = (char)('0' + ean13_check_digit(s));
        out[13] = '\0';
        return 0;
    }
    char tmp[13];
    memcpy(tmp, s, 12);
    tmp[12] = '\0';
    int expected = ean13_check_digit(tmp);
    if (s[12] - '0' != expected) return -1;
    memcpy(out, s, 13);
    out[13] = '\0';
    return 0;
}

/* L patterns */
static const char *L_PAT[] = {
    "0001101","0011001","0010011","0111101","0100011",
    "0110001","0101111","0111011","0110111","0001011"
};
static const char *G_PAT[] = {
    "0100111","0110011","0011011","0100001","0011101",
    "0111001","0000101","0010001","0001001","0010111"
};
static const char *R_PAT[] = {
    "1110010","1100110","1101100","1000010","1011100",
    "1001110","1010000","1000100","1001000","1110100"
};
/* parity for first digit */
static const char *PARITY[] = {
    "LLLLLL","LLGLGG","LLGGLG","LLGGGL","LGLLGG",
    "LGGLLG","LGGGLL","LGLGLG","LGLGGL","LGGLGL"
};

/* Encode to 95-module bit string (0/1 chars). out must be at least 96 bytes. */
int ean13_encode_modules(const char *ean13, char *out) {
    if (strlen(ean13) != 13) return -1;
    int first = ean13[0] - '0';
    const char *par = PARITY[first];
    size_t pos = 0;
    /* start guard */
    memcpy(out + pos, "101", 3); pos += 3;
    /* left 6 */
    for (int i = 0; i < 6; i++) {
        int d = ean13[i + 1] - '0';
        const char *p = (par[i] == 'L') ? L_PAT[d] : G_PAT[d];
        memcpy(out + pos, p, 7); pos += 7;
    }
    /* center */
    memcpy(out + pos, "01010", 5); pos += 5;
    /* right 6 */
    for (int i = 0; i < 6; i++) {
        int d = ean13[i + 7] - '0';
        memcpy(out + pos, R_PAT[d], 7); pos += 7;
    }
    /* end */
    memcpy(out + pos, "101", 3); pos += 3;
    out[pos] = '\0';
    return (int)pos; /* should be 95 */
}
