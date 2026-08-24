#include "edb/sha256.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int failures = 0;

static void expect_hex(const char *name, const uint8_t *got, const char *expected_hex) {
    char buf[65];
    for (int i = 0; i < 32; i++) sprintf(buf + i*2, "%02x", got[i]);
    if (strcmp(buf, expected_hex) != 0) {
        fprintf(stderr, "FAIL %s\n  got: %s\n  exp: %s\n", name, buf, expected_hex);
        failures++;
    } else {
        printf("PASS %s\n", name);
    }
}

int main(void) {
    uint8_t hash[32];

    /* empty */
    edb_sha256((const uint8_t*)"", 0, hash);
    expect_hex("empty", hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    /* "abc" */
    edb_sha256((const uint8_t*)"abc", 3, hash);
    expect_hex("abc", hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    /* longer */
    const char *s = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    edb_sha256((const uint8_t*)s, strlen(s), hash);
    expect_hex("long", hash, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

    if (failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("All SHA-256 known-answer tests passed.\n");
    return 0;
}
