/* test_batch.c - algorithm batch + stress (docs/10 sections 11,12; G6).
 * Generates >=50 distinct puzzles per difficulty, verifies uniqueness,
 * logic-solvability, classification and band invariants; then exercises the
 * crypto AEAD, LZSS codec and framebuffer under load. */
#include "test/sdk_test.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"
#include "common/sdk_win.h"
#include "common/sdk_crc32.h"
#include "common/sdk_lzss.h"
#include "crypto/sdk_aead.h"
#include "ui/sdk_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- G6.1: >=50 distinct seeds per difficulty --------------------------- */
static void tc_gen_50_per_difficulty(sdk_test_ctx *t) {
    sdk_difficulty reqs[3] = { SDK_DIFF_EASY, SDK_DIFF_MEDIUM, SDK_DIFF_HARD };
    const char *names[3] = { "EASY", "MEDIUM", "HARD" };

    for (int di = 0; di < 3; ++di) {
        int got = 0, attempts = 0;
        int clue_min = 999, clue_max = 0, score_min = 999999, score_max = 0;
        while (got < 50 && attempts < 400) {
            sdk_gen_params p;
            sdk_gen_params_default(&p, reqs[di]);
            p.fixed_seed = 1;                       /* explicit, distinct seed */
            p.seed = (uint64_t)(91000 + di * 1000000 + attempts);
            p.wall_guard_ms = 30000;

            sdk_board puzzle, sol;
            sdk_gen_report rep;
            memset(&sol, 0, sizeof sol);
            sdk_generate(&p, &puzzle, &sol, NULL, &rep);
            ++attempts;
            if (!rep.accepted) continue;

            /* independent uniqueness check (docs/10 section 22) */
            int cnt = 0;
            SDK_T_TRUE(t, sdk_count_solutions(&puzzle, 2, &cnt));
            SDK_T_EQ_I(t, cnt, 1);

            /* independent logic-solvability + classification */
            sdk_board out;
            sdk_logic_trace tr;
            SDK_T_TRUE(t, sdk_logic_solve(&puzzle, &out, &tr));
            SDK_T_TRUE(t, tr.solved);
            SDK_T_TRUE(t, !tr.stalled);

            sdk_difficulty_result dr;
            sdk_classify_difficulty(&puzzle, &dr);
            SDK_T_EQ_I(t, (int)dr.difficulty, (int)reqs[di]);

            if (rep.clue_count < clue_min) clue_min = rep.clue_count;
            if (rep.clue_count > clue_max) clue_max = rep.clue_count;
            if (rep.logic_score < score_min) score_min = rep.logic_score;
            if (rep.logic_score > score_max) score_max = rep.logic_score;

            if (reqs[di] == SDK_DIFF_EASY) {
                SDK_T_TRUE(t, rep.clue_count >= 36 && rep.clue_count <= 49);
                SDK_T_TRUE(t, rep.logic_score >= 20 && rep.logic_score <= 120);
                SDK_T_TRUE(t, dr.max_technique <= SDK_TECH_HIDDEN_SINGLE);
            } else if (reqs[di] == SDK_DIFF_MEDIUM) {
                SDK_T_TRUE(t, rep.clue_count >= 30 && rep.clue_count <= 40);
                SDK_T_TRUE(t, rep.logic_score >= 70 && rep.logic_score <= 260);
                SDK_T_TRUE(t, dr.tech_counts[SDK_TECH_NAKED_TRIPLE] == 0 &&
                                dr.tech_counts[SDK_TECH_HIDDEN_TRIPLE] == 0);
            } else {
                SDK_T_TRUE(t, rep.clue_count >= 24 && rep.clue_count <= 34);
                SDK_T_TRUE(t, rep.logic_score >= 180 && rep.logic_score <= 520);
                int t7t8 = dr.tech_counts[SDK_TECH_NAKED_TRIPLE] +
                           dr.tech_counts[SDK_TECH_HIDDEN_TRIPLE];
                int t5t6 = dr.tech_counts[SDK_TECH_NAKED_PAIR] +
                           dr.tech_counts[SDK_TECH_HIDDEN_PAIR];
                SDK_T_TRUE(t, t7t8 > 0 || (t5t6 >= 3 && rep.logic_score >= 220));
            }

            /* the published solution must be a complete, valid grid */
            sdk_validation v;
            sdk_validate(&sol, &v);
            SDK_T_EQ_I(t, 1, (int)v.valid_complete);

            ++got;
        }
        SDK_T_EQ_I(t, got, 50);
        printf("  [%s] collected 50 in %d attempts; clue[%d..%d] score[%d..%d]\n",
               names[di], attempts, clue_min, clue_max, score_min, score_max);
    }
}

/* ---- crypto AEAD round trip + tamper (docs/08, G8) ---------------------- */
static void tc_crypto_roundtrip(sdk_test_ctx *t) {
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t tag[SDK_XCHACHA20POLY1305_TAG_LEN];

    for (int i = 0; i < 100; ++i) {
        size_t n = (size_t)(16 + (i * 37) % 240);
        uint8_t *pt = (uint8_t *)malloc(n ? n : 1);
        uint8_t *ct = (uint8_t *)malloc(n ? n : 1);
        uint8_t *out = (uint8_t *)malloc(n ? n : 1);
        SDK_T_TRUE(t, pt && ct && out);

        sdk_random_bytes(pt, n);
        sdk_random_bytes(key, sizeof key);
        sdk_random_bytes(nonce, sizeof nonce);

        SDK_T_EQ_ST(t,
            sdk_xchacha20poly1305_encrypt(key, nonce, NULL, 0, pt, n, ct, tag),
            SDK_OK);
        SDK_T_EQ_ST(t,
            sdk_xchacha20poly1305_decrypt(key, nonce, NULL, 0, ct, n, tag, out),
            SDK_OK);
        SDK_T_EQ_MEM(t, pt, out, n);

        /* ciphertext tamper -> authentication failure, no plaintext written */
        uint8_t *out2 = (uint8_t *)malloc(n ? n : 1);
        memset(out2, 0xCD, n);
        ct[0] ^= 0x01;
        SDK_T_EQ_ST(t,
            sdk_xchacha20poly1305_decrypt(key, nonce, NULL, 0, ct, n, tag, out2),
            SDK_ERR_AUTH);
        SDK_T_EQ_MEM(t, out2, (const uint8_t *)"\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD",
                     (n < 8 ? n : 8));
        ct[0] ^= 0x01;  /* restore */

        /* tag tamper -> authentication failure */
        tag[sizeof tag - 1] ^= 0x01;
        SDK_T_EQ_ST(t,
            sdk_xchacha20poly1305_decrypt(key, nonce, NULL, 0, ct, n, tag, out),
            SDK_ERR_AUTH);
        tag[sizeof tag - 1] ^= 0x01;  /* restore */

        free(pt); free(ct); free(out); free(out2);
    }
}

/* ---- LZSS codec round trip (empty / random / repeated) ----------------- */
static void tc_lzss_roundtrip(sdk_test_ctx *t) {
    for (int i = 0; i < 120; ++i) {
        size_t n = (size_t)(1 + (i * 53) % 600);
        uint8_t *src = (uint8_t *)malloc(n);
        SDK_T_TRUE(t, src);
        for (size_t j = 0; j < n; ++j) {
            if (i % 4 == 0)      src[j] = 0;                 /* empty-ish */
            else if (i % 4 == 1) src[j] = (uint8_t)(j % 7);  /* repeated */
            else if (i % 4 == 2) src[j] = (uint8_t)(j & 0x0F);
            else                 src[j] = (uint8_t)((i * 31 + j * 7) & 0xFF);
        }

        size_t cap = sdk_lzss_max_compressed_size(n);
        uint8_t *comp = (uint8_t *)malloc(cap ? cap : 1);
        uint8_t *dec = (uint8_t *)malloc(n ? n : 1);
        SDK_T_TRUE(t, comp && dec);

        size_t clen = 0;
        SDK_T_EQ_ST(t, sdk_lzss_compress(src, n, comp, cap, &clen), SDK_OK);
        SDK_T_EQ_ST(t, sdk_lzss_decompress(comp, clen, dec, n), SDK_OK);
        SDK_T_EQ_MEM(t, src, dec, n);

        free(src); free(comp); free(dec);
    }
}

/* ---- framebuffer stress (create / fill / destroy loop) ----------------- */
static void tc_fb_stress(sdk_test_ctx *t) {
    for (int i = 0; i < 1000; ++i) {
        int w = 200 + (int)((i * 131) % 900);
        int h = 150 + (int)((i * 71) % 700);
        sdk_fb *fb = sdk_fb_create(w, h);
        SDK_T_TRUE(t, fb != NULL);
        sdk_rect r = { 0, 0, w, h };
        sdk_fill_rect(fb, r, sdk_color_from_u32((uint32_t)(0x10203040 + (i % 16) * 0x01010100)));
        sdk_fb_destroy(fb);
    }
}

static void register_all(void) {
    sdk_test_add("batch-gen-50-per-difficulty", "G6.1,DIF-01,DIF-02,DIF-03,DIF-04,DIF-05,BAT-01",
                 tc_gen_50_per_difficulty);
    sdk_test_add("batch-crypto-roundtrip", "G8,CRY-01,CRY-02,CRY-03",
                 tc_crypto_roundtrip);
    sdk_test_add("batch-lzss-roundtrip", "G8,LZS-01,LZS-02",
                 tc_lzss_roundtrip);
    sdk_test_add("batch-fb-stress", "G9,FB-01,FB-02",
                 tc_fb_stress);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("batch", register_all, argc, argv);
}
