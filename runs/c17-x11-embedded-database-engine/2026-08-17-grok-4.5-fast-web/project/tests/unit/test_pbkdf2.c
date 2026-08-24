#include "edb/pbkdf2.h"
#include <stdio.h>
#include <string.h>
int main(void) {
    uint8_t out[32], out2[32];
    const char *pw = "secret";
    uint8_t salt[16] = {0};
    if (edb_pbkdf2_hmac_sha256((const uint8_t*)pw, 6, salt, 16, 1000, out, 32) != 0) {
        printf("FAIL pbkdf2\n"); return 1;
    }
    if (edb_pbkdf2_hmac_sha256((const uint8_t*)pw, 6, salt, 16, 1000, out2, 32) != 0) return 1;
    if (memcmp(out, out2, 32) != 0) { printf("FAIL nondeterministic\n"); return 1; }
    uint8_t out3[32];
    edb_pbkdf2_hmac_sha256((const uint8_t*)"other", 5, salt, 16, 1000, out3, 32);
    if (memcmp(out, out3, 32) == 0) { printf("FAIL same key different pw\n"); return 1; }
    printf("PASS pbkdf2 deterministic\n");
    return 0;
}
