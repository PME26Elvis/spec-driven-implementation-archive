#include "darc_sha256.h"
#include "darc_crc32c.h"
#include "darc_buzhash.h"
#include "darc_lzh1.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL %s\n", msg); fails++; } else printf("PASS %s\n", msg); } while(0)

int main(void) {
    uint8_t dig[32]; char hex[65];
    darc_sha256("", 0, dig); darc_sha256_hex(dig, hex);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")==0, "sha256 empty");
    darc_sha256("abc", 3, dig); darc_sha256_hex(dig, hex);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")==0, "sha256 abc");
    uint32_t c = darc_crc32c("123456789", 9);
    CHECK(c == 0xE3069283u, "crc32c");
    uint64_t table[256];
    darc_buzhash_table_init(table);
    CHECK(table[0] == 0xa7b77319d39f7883ULL, "buzhash T[0]");
    CHECK(table[255] == 0x9987a1cb57aa2b4fULL, "buzhash T[255]");
    size_t clen;
    uint8_t *comp = darc_lzh1_compress((const uint8_t*)"abc", 3, &clen);
    CHECK(comp && clen == 278, "lzh1 abc len");
    darc_sha256(comp, clen, dig); darc_sha256_hex(dig, hex);
    CHECK(strcmp(hex, "987e14626677b433e9a410bfd73a1b1d9ea0ad7d363a0335239a1d390f6b6cbf")==0, "lzh1 abc sha");
    size_t dlen;
    uint8_t *raw = darc_lzh1_decompress(comp, clen, 3, &dlen);
    CHECK(raw && dlen==3 && memcmp(raw,"abc",3)==0, "lzh1 roundtrip");
    free(comp); free(raw);
    printf(fails ? "SOME FAILED\n" : "ALL ALGORITHM TESTS PASSED\n");
    return fails ? 1 : 0;
}
