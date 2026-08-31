/* test_vault.c - unit + failure-injection tests for the encrypted vault
 * (docs/08, docs/19 sections 16-22). Covers G3/G7/G8 evidence for storage. */
#include "test/sdk_test.h"
#include "storage/sdk_vault.h"
#include "common/sdk_common.h"
#include "common/sdk_win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static const char *VAULT_PATH = "build/evidence/vault_unit.dat";
static const char *CORRUPT_PATH = "build/evidence/vault_corrupt.dat";

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
    s->settings.motion = 0;
    s->settings.auto_remove_peer_notes = 1;
    s->settings.confirm_auto_solve = 0;

    sdk_game_record g; memset(&g, 0, sizeof g);
    sdk_vault_new_game_id(g.id);
    g.difficulty = 2; g.gen_seed = 12345; g.used_auto_solve = 0;
    for (int i = 0; i < 81; ++i) { g.orig[i] = (uint8_t)((i % 9) + 1); g.cur[i] = g.orig[i]; g.origin[i] = 1; }
    g.undo_count = 1;
    g.undo = (sdk_undo_transaction *)calloc(1, sizeof(sdk_undo_transaction));
    g.undo[0].action_kind = 1; g.undo[0].assisted_reason = 0;
    g.undo[0].change_count = 2; g.undo[0].sequence_number = 7;
    g.undo[0].changes = (sdk_change *)calloc(2, sizeof(sdk_change));
    g.undo[0].changes[0].cell_index = 0; g.undo[0].changes[0].before_value = 0;
    g.undo[0].changes[0].after_value = 5; g.undo[0].changes[0].before_origin = 0;
    g.undo[0].changes[0].after_origin = 2; g.undo[0].changes[0].before_notes = 0;
    g.undo[0].changes[0].after_notes = 0;
    g.undo[0].changes[1].cell_index = 4; g.undo[0].changes[1].before_value = 3;
    g.undo[0].changes[1].after_value = 0; g.undo[0].changes[1].before_origin = 2;
    g.undo[0].changes[1].after_origin = 0; g.undo[0].changes[1].before_notes = 0;
    g.undo[0].changes[1].after_notes = 0;
    sdk_store_add_game(s, &g);
    free(g.undo[0].changes); free(g.undo);

    sdk_completed_record c; memset(&c, 0, sizeof c);
    sdk_vault_new_game_id(c.id);
    c.difficulty = 1; c.completion_class = 0; c.active_elapsed_ms = 123456;
    c.clue_count = 36; c.used_auto_solve = 0;
    for (int i = 0; i < 81; ++i) { c.orig[i] = (uint8_t)((i % 9) + 1); c.grid[i] = c.orig[i]; c.origin[i] = 1; }
    sdk_store_add_completed(s, &c);
}

/* ---- G3/G8: create -> open(wrong) -> open(right) -> save roundtrip ---- */
static void tc_create_open_roundtrip(sdk_test_ctx *t) {
    DeleteFileA(VAULT_PATH);
    sdk_store s; build_store(&s);
    sdk_vault *v = NULL;
    sdk_status st = sdk_vault_create(VAULT_PATH, "correct horse", 0, &s, &v);
    SDK_T_EQ_ST(t, SDK_OK, st);
    SDK_T_TRUE(t, v != NULL);
    sdk_vault_close(v); v = NULL;

    /* wrong password must be rejected with AUTH */
    sdk_store wrong; memset(&wrong, 0, sizeof wrong);
    st = sdk_vault_open(VAULT_PATH, "wrong password", &v, &wrong);
    SDK_T_EQ_ST(t, SDK_ERR_AUTH, st);

    /* correct password opens and parses */
    sdk_store open; memset(&open, 0, sizeof open);
    st = sdk_vault_open(VAULT_PATH, "correct horse", &v, &open);
    SDK_T_EQ_ST(t, SDK_OK, st);
    SDK_T_EQ_I(t, 1, (int)open.game_count);
    SDK_T_EQ_I(t, 1, (int)open.completed_count);
    SDK_T_EQ_I(t, 2, (int)open.games[0].difficulty);
    SDK_T_EQ_I(t, 1, (int)open.games[0].undo_count);
    SDK_T_EQ_I(t, 2, (int)open.games[0].undo[0].change_count);
    SDK_T_EQ_I(t, 0, (int)open.games[0].undo[0].changes[0].cell_index);
    SDK_T_EQ_I(t, 5, (int)open.games[0].undo[0].changes[0].after_value);
    sdk_store_free(&open);
    sdk_vault_close(v); v = NULL;

    /* re-save idempotency: open, save, reopen -> still parses */
    st = sdk_vault_open(VAULT_PATH, "correct horse", &v, &s);
    SDK_T_EQ_ST(t, SDK_OK, st);
    st = sdk_vault_save(v, &s);
    SDK_T_EQ_ST(t, SDK_OK, st);
    sdk_vault_close(v);
}

/* ---- G7: failure injection on the on-disk ciphertext ---- */
static void tc_failure_injection(sdk_test_ctx *t) {
    DeleteFileA(VAULT_PATH);
    sdk_store s; build_store(&s);
    sdk_vault *v = NULL;
    sdk_status st = sdk_vault_create(VAULT_PATH, "pw", 0, &s, &v);
    SDK_T_EQ_ST(t, SDK_OK, st);
    sdk_vault_close(v);

    size_t len = 0;
    unsigned char *buf = read_file(VAULT_PATH, &len);
    SDK_T_TRUE(t, buf != NULL);
    SDK_T_TRUE(t, len > 64);

    /* 1) truncated header */
    unsigned char *t1 = (unsigned char *)malloc(40);
    memcpy(t1, buf, 40);
    write_file(CORRUPT_PATH, t1, 40); free(t1);
    sdk_store o1; memset(&o1, 0, sizeof o1); v = NULL;
    st = sdk_vault_open(CORRUPT_PATH, "pw", &v, &o1);
    SDK_T_TRUE(t, st != SDK_OK);
    sdk_store_free(&o1);

    /* 2) truncated ciphertext (drop the last 20 bytes, breaking length) */
    unsigned char *t2 = (unsigned char *)malloc(len - 20);
    memcpy(t2, buf, len - 20);
    write_file(CORRUPT_PATH, t2, len - 20); free(t2);
    sdk_store o2; memset(&o2, 0, sizeof o2); v = NULL;
    st = sdk_vault_open(CORRUPT_PATH, "pw", &v, &o2);
    SDK_T_TRUE(t, st != SDK_OK);   /* DATA (length) or AUTH (tag) both reject */
    sdk_store_free(&o2);

    /* 3) modified authentication tag (last 16 bytes) */
    unsigned char *t3 = (unsigned char *)malloc(len);
    memcpy(t3, buf, len);
    t3[len - 1] ^= 0xFF;
    write_file(CORRUPT_PATH, t3, len); free(t3);
    sdk_store o3; memset(&o3, 0, sizeof o3); v = NULL;
    st = sdk_vault_open(CORRUPT_PATH, "pw", &v, &o3);
    SDK_T_EQ_ST(t, SDK_ERR_AUTH, st);
    sdk_store_free(&o3);

    /* 4) modified header AAD (flip cipher byte) -> tag verification fails */
    unsigned char *t4 = (unsigned char *)malloc(len);
    memcpy(t4, buf, len);
    t4[6] ^= 0xFF;   /* header region consumed as AAD */
    write_file(CORRUPT_PATH, t4, len); free(t4);
    sdk_store o4; memset(&o4, 0, sizeof o4); v = NULL;
    st = sdk_vault_open(CORRUPT_PATH, "pw", &v, &o4);
    SDK_T_TRUE(t, st != SDK_OK);
    sdk_store_free(&o4);

    /* 5) wrong password on a pristine file */
    sdk_store o5; memset(&o5, 0, sizeof o5); v = NULL;
    st = sdk_vault_open(VAULT_PATH, "not-the-pw", &v, &o5);
    SDK_T_EQ_ST(t, SDK_ERR_AUTH, st);
    sdk_store_free(&o5);

    free(buf);
    DeleteFileA(CORRUPT_PATH);
}

/* ---- G3: serialize/deserialize determinism ---- */
static void tc_serialize_deterministic(sdk_test_ctx *t) {
    sdk_store a; build_store(&a);
    unsigned char *b1 = NULL; size_t l1 = 0;
    sdk_vault_serialize_store(&a, &b1, &l1);
    SDK_T_TRUE(t, b1 != NULL && l1 > 0);
    sdk_store b; memset(&b, 0, sizeof b);
    sdk_status st = sdk_vault_deserialize_store(b1, l1, &b);
    SDK_T_EQ_ST(t, SDK_OK, st);
    unsigned char *b2 = NULL; size_t l2 = 0;
    sdk_vault_serialize_store(&b, &b2, &l2);
    SDK_T_EQ_MEM(t, b1, b2, l1);
    free(b1); free(b2); sdk_store_free(&b);
}

static void register_all(void) {
    sdk_test_add("vault-create-open-roundtrip", "STA-03,STA-04,VUL-01",
                 tc_create_open_roundtrip);
    sdk_test_add("vault-failure-injection", "G7,VUL-02,VUL-03",
                 tc_failure_injection);
    sdk_test_add("vault-serialize-deterministic", "G3,STA-03",
                 tc_serialize_deterministic);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("unit-vault", register_all, argc, argv);
}
