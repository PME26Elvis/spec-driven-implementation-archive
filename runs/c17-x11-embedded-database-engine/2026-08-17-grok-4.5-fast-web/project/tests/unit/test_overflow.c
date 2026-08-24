#include "edb/pager.h"
#include "edb/overflow.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void) {
    edb_error err;
    unlink("/tmp/ovf.edb");
    edb_pager *p = edb_pager_open("/tmp/ovf.edb", true, false, NULL, &err);
    if (!p) { printf("open fail %s\n", err.message); return 1; }
    /* 12KB payload spanning multiple pages */
    uint32_t len = 12000;
    uint8_t *src = malloc(len);
    for (uint32_t i = 0; i < len; i++) src[i] = (uint8_t)(i * 7 + 3);
    uint32_t first = 0;
    if (edb_overflow_write(p, src, len, &first, &err) != 0) {
        printf("write fail %s\n", err.message); return 1;
    }
    printf("first page %u\n", first);
    uint8_t *dst = malloc(len);
    uint32_t got = 0;
    if (edb_overflow_read(p, first, dst, len, &got, &err) != 0) {
        printf("read fail %s\n", err.message); return 1;
    }
    if (got != len || memcmp(src, dst, len) != 0) {
        printf("FAIL mismatch got=%u\n", got); return 1;
    }
    printf("PASS overflow roundtrip %u bytes\n", len);
    if (edb_overflow_free(p, first, &err) != 0) {
        printf("free fail %s\n", err.message); return 1;
    }
    printf("PASS overflow free\n");
    free(src); free(dst);
    edb_pager_close(p);
    return 0;
}
