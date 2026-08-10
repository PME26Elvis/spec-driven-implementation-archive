#ifndef MDEDIT_DIFF_H
#define MDEDIT_DIFF_H

#include "mdedit/core.h"

typedef enum {
    MD_DIFF_EQUAL,
    MD_DIFF_INSERT,
    MD_DIFF_DELETE
} MdDiffKind;

typedef struct {
    MdDiffKind kind;
    size_t a_start;
    size_t a_count;
    size_t b_start;
    size_t b_count;
} MdDiffHunk;

typedef struct {
    MdDiffHunk *hunks;
    size_t count;
    size_t cap;
} MdDiff;

void md_diff_init(MdDiff *diff);
void md_diff_free(MdDiff *diff);
bool md_diff_lines(const char *a, size_t a_len, const char *b, size_t b_len,
                   MdDiff *out, char *error, size_t error_cap);
bool md_diff_tokens(const char *a, size_t a_len, const char *b, size_t b_len,
                    MdDiff *out, char *error, size_t error_cap);
bool md_delta_encode(const char *base, size_t base_len,
                     const char *target, size_t target_len, MdBytes *out,
                     char *error, size_t error_cap);
bool md_delta_apply(const char *base, size_t base_len,
                    const uint8_t *delta, size_t delta_len, MdBuf *out,
                    char *error, size_t error_cap);
bool md_lzss_compress(const uint8_t *input, size_t len, MdBytes *out);
bool md_lzss_decompress(const uint8_t *input, size_t len,
                        size_t expected_len, MdBytes *out,
                        char *error, size_t error_cap);

#endif

