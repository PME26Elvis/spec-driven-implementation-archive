#define _POSIX_C_SOURCE 200809L
#include "darc_object.h"
#include "darc_crc32c.h"
#include "darc_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

void darc_cid_compute(uint8_t type, uint16_t version, const void *payload, size_t plen, darc_cid_t out) {
    darc_sha256_ctx ctx;
    darc_sha256_init(&ctx);
    darc_sha256_update(&ctx, "DARC\0", 5);
    darc_sha256_update(&ctx, &type, 1);
    uint8_t ver[2];
    darc_write_u16_le(ver, version);
    darc_sha256_update(&ctx, ver, 2);
    if (plen && payload)
        darc_sha256_update(&ctx, payload, plen);
    darc_sha256_final(&ctx, out);
}

void darc_cid_hex(const darc_cid_t cid, char out[65]) {
    darc_sha256_hex(cid, out);
}

int darc_cid_from_hex(const char *hex, darc_cid_t out) {
    if (!hex || strlen(hex) != 64) return -1;
    for (int i = 0; i < 32; ++i) {
        unsigned int b;
        if (sscanf(hex + i*2, "%02x", &b) != 1) return -1;
        out[i] = (uint8_t)b;
    }
    return 0;
}

uint8_t *darc_object_frame(uint8_t type, uint8_t codec,
                           const void *uncompressed, size_t uncomp_len,
                           const void *stored, size_t stored_len,
                           size_t *out_len) {
    (void)uncompressed;
    size_t total = 8 + 1 + 1 + 2 + 8 + 8 + 4 + stored_len + 4;
    uint8_t *buf = malloc(total);
    if (!buf) return NULL;
    size_t off = 0;
    memcpy(buf + off, "DARCOBJ1", 8); off += 8;
    buf[off++] = type;
    buf[off++] = codec;
    buf[off++] = 0; buf[off++] = 0; /* reserved */
    darc_write_u64_le(buf + off, uncomp_len); off += 8;
    darc_write_u64_le(buf + off, stored_len); off += 8;
    /* header_crc covers object_type through stored_len (20 bytes starting at type) */
    uint32_t hcrc = darc_crc32c(buf + 8, 20);
    darc_write_u32_le(buf + off, hcrc); off += 4;
    if (stored_len && stored)
        memcpy(buf + off, stored, stored_len);
    off += stored_len;
    uint32_t pcrc = darc_crc32c(stored, stored_len);
    darc_write_u32_le(buf + off, pcrc); off += 4;
    *out_len = off;
    return buf;
}

int darc_object_unframe(const uint8_t *buf, size_t buf_len,
                        uint8_t *type, uint8_t *codec,
                        size_t *uncomp_len, size_t *stored_len,
                        const uint8_t **payload) {
    if (buf_len < 8 + 20 + 4 + 4) return -1;
    if (memcmp(buf, "DARCOBJ1", 8) != 0) return -1;
    *type = buf[8];
    *codec = buf[9];
    *uncomp_len = (size_t)darc_read_u64_le(buf + 12);
    *stored_len = (size_t)darc_read_u64_le(buf + 20);
    uint32_t hcrc_stored = darc_read_u32_le(buf + 28);
    uint32_t hcrc = darc_crc32c(buf + 8, 20);
    if (hcrc != hcrc_stored) return -2; /* header corrupt */
    if (buf_len < 32 + *stored_len + 4) return -1;
    *payload = buf + 32;
    uint32_t pcrc_stored = darc_read_u32_le(buf + 32 + *stored_len);
    uint32_t pcrc = darc_crc32c(*payload, *stored_len);
    if (pcrc != pcrc_stored) return -3; /* payload corrupt */
    return 0;
}

int darc_object_write_file(const char *path, const uint8_t *framed, size_t framed_len) {
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp.%d", path, (int)getpid());
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    size_t written = 0;
    while (written < framed_len) {
        ssize_t n = write(fd, framed + written, framed_len - written);
        if (n < 0) { close(fd); unlink(tmp); return -1; }
        written += (size_t)n;
    }
    if (fsync(fd) < 0) { close(fd); unlink(tmp); return -1; }
    close(fd);
    if (rename(tmp, path) < 0) { unlink(tmp); return -1; }
    return 0;
}

uint8_t *darc_read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) { free(buf); fclose(f); return NULL; }
    fclose(f);
    *out_len = (size_t)sz;
    return buf;
}
