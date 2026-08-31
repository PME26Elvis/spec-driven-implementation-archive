/* test_failure.c - failure injection (docs/10 section 11; G7).
 * Vault corruption (truncation / bit-flip / wrong password / .bak recovery)
 * and generator wall-guard exhaustion (no hang). */
#include "test/sdk_test.h"
#include "storage/sdk_vault.h"
#include "sudoku/sdk_sudoku.h"
#include "common/sdk_common.h"
#include "common/sdk_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static const char *VAULT_PATH = "build/evidence/failure_vault.dat";
static const char *BAK_PATH   = "build/evidence/failure_vault.dat.bak";
static const char *CORRUPT    = "build/evidence/failure_corrupt.dat";

static unsigned char *read_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *buf = (unsigned char *)malloc((size_t)sz ? (size_t)sz : 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    *out_len = rd;
    return buf;
}
static int write_file(const char *path, const unsigned char *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fwrite(buf, 1, len, f);
    fclose(f);
    return 0;
}

static void build_store(sdk_store *s) {
    sdk_store_init(s);
    s->settings.theme = 1;
    s->settings.last_difficulty = 2;
    sdk_completed_record c; memset(&c, 0, sizeof c);
    sdk_vault_new_game_id(c.id);
    c.difficulty = 1; c.completion_class = 0; c.active_elapsed_ms = 123456;
    c.clue_count = 36; c.used_auto_solve = 0;
    for (int i = 0; i < 81; ++i) { c.orig[i]=(uint8_t)((i%9)+1); c.grid[i]=c.orig[i]; c.origin[i]=1; }
    sdk_store_add_completed(s, &c);
}

/* ---- G7: corruption of the on-disk vault is rejected; valid data untouched */
static void tc_vault_corruption_modes(sdk_test_ctx *t) {
    DeleteFileA(VAULT_PATH); DeleteFileA(CORRUPT);
    sdk_store s; build_store(&s);
    sdk_vault *v = NULL;
    SDK_T_EQ_ST(t, SDK_OK, sdk_vault_create(VAULT_PATH, "pw", 0, &s, &v));
    sdk_vault_close(v);

    size_t len = 0;
    unsigned char *buf = read_file(VAULT_PATH, &len);
    SDK_T_TRUE(t, buf && len > 64);

    /* truncated header */
    unsigned char *t1 = (unsigned char *)malloc(40);
    memcpy(t1, buf, 40); write_file(CORRUPT, t1, 40); free(t1);
    sdk_store o1; memset(&o1, 0, sizeof o1); v = NULL;
    SDK_T_TRUE(t, sdk_vault_open(CORRUPT, "pw", &v, &o1) != SDK_OK);
    SDK_T_EQ_I(t, 0, (int)o1.game_count);   /* nothing parsed into caller */
    SDK_T_EQ_I(t, 0, (int)o1.completed_count);
    sdk_store_free(&o1);

    /* truncated ciphertext (length mismatch) */
    unsigned char *t2 = (unsigned char *)malloc(len - 20);
    memcpy(t2, buf, len - 20); write_file(CORRUPT, t2, len - 20); free(t2);
    sdk_store o2; memset(&o2, 0, sizeof o2); v = NULL;
    SDK_T_TRUE(t, sdk_vault_open(CORRUPT, "pw", &v, &o2) != SDK_OK);
    sdk_store_free(&o2);

    /* flipped authentication tag */
    unsigned char *t3 = (unsigned char *)malloc(len);
    memcpy(t3, buf, len); t3[len - 1] ^= 0xFF; write_file(CORRUPT, t3, len); free(t3);
    sdk_store o3; memset(&o3, 0, sizeof o3); v = NULL;
    SDK_T_EQ_ST(t, SDK_ERR_AUTH, sdk_vault_open(CORRUPT, "pw", &v, &o3));
    sdk_store_free(&o3);

    /* flipped header AAD */
    unsigned char *t4 = (unsigned char *)malloc(len);
    memcpy(t4, buf, len); t4[6] ^= 0xFF; write_file(CORRUPT, t4, len); free(t4);
    sdk_store o4; memset(&o4, 0, sizeof o4); v = NULL;
    SDK_T_TRUE(t, sdk_vault_open(CORRUPT, "pw", &v, &o4) != SDK_OK);
    sdk_store_free(&o4);

    /* wrong password on a pristine file */
    sdk_store o5; memset(&o5, 0, sizeof o5); v = NULL;
    SDK_T_EQ_ST(t, SDK_ERR_AUTH, sdk_vault_open(VAULT_PATH, "nope", &v, &o5));
    SDK_T_EQ_I(t, 0, (int)o5.completed_count);
    sdk_store_free(&o5);

    free(buf); DeleteFileA(CORRUPT);
}

/* ---- G7: .bak retains a valid prior version after a corrupt write ------- */
static void tc_vault_bak_recovery(sdk_test_ctx *t) {
    DeleteFileA(VAULT_PATH); DeleteFileA(BAK_PATH);
    sdk_store s; build_store(&s);
    sdk_vault *v = NULL;
    SDK_T_EQ_ST(t, SDK_OK, sdk_vault_create(VAULT_PATH, "pw", 0, &s, &v));
    SDK_T_EQ_ST(t, SDK_OK, sdk_vault_save(v, &s));   /* establishes .bak */
    sdk_vault_close(v);

    /* corrupt the current file (simulating a torn write) */
    size_t len = 0;
    unsigned char *buf = read_file(VAULT_PATH, &len);
    SDK_T_TRUE(t, buf && len > 64);
    buf[10] ^= 0xFF; buf[len / 2] ^= 0xFF;
    write_file(VAULT_PATH, buf, len);
    free(buf);

    /* the corrupted current is rejected cleanly */
    sdk_store o; memset(&o, 0, sizeof o); v = NULL;
    SDK_T_TRUE(t, sdk_vault_open(VAULT_PATH, "pw", &v, &o) != SDK_OK);
    sdk_store_free(&o);

    /* the retained backup is still a valid, parseable prior version */
    sdk_store b; memset(&b, 0, sizeof b); v = NULL;
    SDK_T_EQ_ST(t, SDK_OK, sdk_vault_open(BAK_PATH, "pw", &v, &b));
    SDK_T_EQ_I(t, 1, (int)b.completed_count);
    sdk_store_free(&b);
    sdk_vault_close(v);
}

/* ---- G7: generator wall-guard bounds the search; never hangs ----------- */
static void tc_generator_guard(sdk_test_ctx *t) {
    sdk_gen_params p;
    sdk_gen_params_default(&p, SDK_DIFF_HARD);
    p.fixed_seed = 0;
    p.wall_guard_ms = 1;            /* force the outer loop to stop ASAP */

    sdk_board b, s; sdk_gen_report r;
    uint64_t t0 = sdk_monotonic_ms();
    sdk_generate(&p, &b, &s, NULL, &r);
    uint64_t dt = sdk_monotonic_ms() - t0;

    /* A single HARD attempt's adjust is the latency floor (~1-6s); the guard
     * must still bound the overall search to well under the normal 30s window
     * and never loop forever. */
    SDK_T_TRUE(t, dt < 15000);                      /* no hang */
    SDK_T_TRUE(t, r.accepted == 0 || r.accepted == 1);
    if (!r.accepted) {
        SDK_T_TRUE(t, r.reject_reason != NULL && r.reject_reason[0] != '\0');
    }
}

static void register_all(void) {
    sdk_test_add("failure-vault-corruption-modes", "G7,VUL-02,VUL-03,STA-06",
                 tc_vault_corruption_modes);
    sdk_test_add("failure-vault-bak-recovery", "G7,VUL-04,STA-07",
                 tc_vault_bak_recovery);
    sdk_test_add("failure-generator-guard", "G9.1,GEN-05",
                 tc_generator_guard);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("failure", register_all, argc, argv);
}
