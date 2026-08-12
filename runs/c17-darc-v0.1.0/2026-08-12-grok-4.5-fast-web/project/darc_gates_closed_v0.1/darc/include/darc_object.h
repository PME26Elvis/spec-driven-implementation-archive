#ifndef DARC_OBJECT_H
#define DARC_OBJECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "darc_sha256.h"

#define DARC_TYPE_CHUNK    1
#define DARC_TYPE_FILE     2
#define DARC_TYPE_TREE     3
#define DARC_TYPE_SNAPSHOT 4
#define DARC_TYPE_PARITY   5

#define DARC_CODEC_RAW  0
#define DARC_CODEC_LZH1 1

typedef uint8_t darc_cid_t[DARC_SHA256_DIGEST_SIZE];

/* Domain-separated CID: SHA256("DARC\0" || type || version_u16_le || payload) */
void darc_cid_compute(uint8_t type, uint16_t version, const void *payload, size_t plen, darc_cid_t out);
void darc_cid_hex(const darc_cid_t cid, char out[65]);
int darc_cid_from_hex(const char *hex, darc_cid_t out);

/* Frame an object for storage. Returns allocated buffer (caller frees). */
uint8_t *darc_object_frame(uint8_t type, uint8_t codec,
                           const void *uncompressed, size_t uncomp_len,
                           const void *stored, size_t stored_len,
                           size_t *out_len);

/* Parse frame. On success fills type/codec/uncomp_len and returns pointer to payload inside buf (not owned). */
int darc_object_unframe(const uint8_t *buf, size_t buf_len,
                        uint8_t *type, uint8_t *codec,
                        size_t *uncomp_len, size_t *stored_len,
                        const uint8_t **payload);

/* Write object to path atomically via tmp+rename. Returns 0 on success. */
int darc_object_write_file(const char *path, const uint8_t *framed, size_t framed_len);

/* Read entire file into allocated buffer. */
uint8_t *darc_read_file(const char *path, size_t *out_len);

#endif
