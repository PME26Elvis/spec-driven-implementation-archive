#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "../src/utf8.h"
#include "../src/ean.h"
#include "../src/mem.h"
#include "../src/url.h"

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d\n", __FILE__, __LINE__); fails++; } } while(0)

int main(void) {
    /* UTF-8 */
    CHECK(utf8_validate((unsigned char*)"abc", 3));
    CHECK(utf8_validate((unsigned char*)"中文", 6));
    CHECK(utf8_validate((unsigned char*)"\xF0\x9F\x98\x80", 4)); /* emoji */
    CHECK(!utf8_validate((unsigned char*)"\x80", 1));
    CHECK(!utf8_validate((unsigned char*)"\xC0\x80", 2));
    CHECK(!utf8_validate((unsigned char*)"\xED\xA0\x80", 3)); /* surrogate */
    size_t f = utf8_find((unsigned char*)"資料結構", 12, (unsigned char*)"資料", 6, 0);
    CHECK(f == 0);
    f = utf8_find((unsigned char*)"Hello", 5, (unsigned char*)"hello", 5, 1);
    CHECK(f == 0); /* ASCII-insensitive */
    f = utf8_find((unsigned char*)"Hello", 5, (unsigned char*)"hello", 5, 0);
    CHECK(f == (size_t)-1); /* sensitive */

    /* EAN */
    char out[14];
    CHECK(ean13_canonicalize("400638133393", 12, out) == 0);
    CHECK(strcmp(out, "4006381333931") == 0);
    CHECK(ean13_canonicalize("4006381333932", 13, out) != 0);
    CHECK(ean13_canonicalize("5901234123457", 13, out) == 0);
    char mods[96];
    CHECK(ean13_encode_modules("4006381333931", mods) == 95);

    /* URL */
    char *u = NULL; size_t ul = 0;
    CHECK(url_canonicalize("HTTP://Example.COM", 18, &u, &ul) == 0);
    CHECK(u && strcmp(u, "http://example.com/") == 0);
    free(u); u = NULL;
    CHECK(url_canonicalize("https://example.com/%ZZ", 23, &u, &ul) != 0);

    if (fails == 0) { printf("All unit tests passed\n"); return 0; }
    printf("%d failures\n", fails);
    return 1;
}
