#include "edb/xchacha20_poly1305.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t key[32], nonce[24], pt[64], ct[64], out[64], tag[16];
    memset(key, 0x11, 32);
    memset(nonce, 0x22, 24);
    for (int i = 0; i < 64; i++) pt[i] = (uint8_t)i;
    if (!edb_xchacha20_poly1305_encrypt(key, nonce, NULL, 0, pt, 64, ct, tag)) {
        printf("FAIL enc\n"); return 1;
    }
    if (!edb_xchacha20_poly1305_decrypt(key, nonce, NULL, 0, ct, 64, tag, out)) {
        printf("FAIL dec\n"); return 1;
    }
    if (memcmp(out, pt, 64) != 0) { printf("FAIL mismatch\n"); return 1; }
    tag[0] ^= 0x01;
    if (edb_xchacha20_poly1305_decrypt(key, nonce, NULL, 0, ct, 64, tag, out)) {
        printf("FAIL tamper should fail\n"); return 1;
    }
    printf("PASS aead encrypt/decrypt/tamper\n");
    return 0;
}
