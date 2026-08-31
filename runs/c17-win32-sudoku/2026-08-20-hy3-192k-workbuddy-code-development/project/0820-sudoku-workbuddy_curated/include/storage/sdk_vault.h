/* sdk_vault.h - encrypted local vault (docs/08 + docs/19 sections 16-22).
 *
 * Self-contained persistence layer.  Uses only Kernel32 (file ops) and
 * BCryptGenRandom (salt/nonce) plus the self-implemented crypto in
 * src/crypto.  No GDI/User32 dependency.
 *
 * Security model (docs/08):
 *   - PBKDF2-HMAC-SHA-256 (200000 iters in production) derives the key.
 *   - XChaCha20-Poly1305 authenticates + encrypts every write (fresh nonce).
 *   - Header non-secret fields are AAD.
 *   - Atomic replacement with a retained known-good backup.
 *   - Tag is verified before any plaintext is parsed.
 */
#ifndef SDK_VAULT_H
#define SDK_VAULT_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"
#include "sudoku/sdk_sudoku.h"   /* SDK_O_* origin constants */

#ifdef __cplusplus
extern "C" {
#endif

#define SDK_VAULT_MAGIC        "SDKVLT01"
#define SDK_VAULT_MAGIC_LEN    8u
#define SDK_PAYLOAD_MAGIC      "SDKPAY01"
#define SDK_PAYLOAD_MAGIC_LEN  8u

#define SDK_VAULT_HEADER_VERSION 1u
#define SDK_PAYLOAD_VERSION      1u
#define SDK_VAULT_KDF_PBKDF2     1u
#define SDK_VAULT_CIPHER_XC20P   1u

#define SDK_VAULT_SALT_LEN   16u
#define SDK_VAULT_NONCE_LEN  24u
#define SDK_VAULT_TAG_LEN    16u
#define SDK_VAULT_KEY_LEN    32u

#define SDK_VAULT_PROD_ITERATIONS 200000u
#define SDK_VAULT_TEST_ITERATIONS 1000u   /* non-performance tests only */

#define SDK_SETTINGS_LEN 16u
#define SDK_GAME_ID_LEN  16u

/* ---- in-memory model ------------------------------------------------- */

typedef struct sdk_settings {
    uint8_t theme;                 /* 0 dark, 1 light */
    uint8_t motion;                /* 0 full, 1 reduced */
    uint8_t auto_remove_peer_notes;/* 0 off, 1 on */
    uint8_t confirm_auto_solve;    /* 0 off, 1 on */
    uint8_t last_difficulty;       /* 0 easy, 1 medium, 2 hard */
} sdk_settings;

typedef struct sdk_change {
    uint8_t  cell_index;           /* 0..80 */
    uint8_t  before_value, after_value;
    uint8_t  before_origin, after_origin;
    uint16_t before_notes, after_notes;
} sdk_change;

typedef struct sdk_undo_transaction {
    uint8_t  action_kind;
    uint8_t  assisted_reason;      /* 0 none, 1 hint, 2 auto-solve */
    uint16_t change_count;
    uint64_t sequence_number;
    sdk_change *changes;           /* change_count entries */
} sdk_undo_transaction;

typedef struct sdk_game_record {
    uint8_t  id[SDK_GAME_ID_LEN];
    int      difficulty;
    uint16_t diff_rules_ver;
    uint16_t gen_format_ver;
    uint64_t gen_seed;
    uint8_t  orig[81];
    uint8_t  cur[81];
    uint16_t notes[81];
    uint8_t  origin[81];
    uint64_t active_elapsed_ms;
    int64_t  created_epoch_ms;
    int64_t  last_played_epoch_ms;
    uint8_t  paused;
    uint32_t hints_viewed;
    uint32_t hints_applied;
    uint8_t  highest_hint_tech;
    uint8_t  used_auto_solve;
    uint64_t current_generation;
    uint64_t saved_generation;
    uint32_t undo_count;
    sdk_undo_transaction *undo;
    uint32_t redo_count;
    sdk_undo_transaction *redo;
} sdk_game_record;

typedef struct sdk_completed_record {
    uint8_t  id[SDK_GAME_ID_LEN];
    int      difficulty;
    uint16_t diff_rules_ver;
    uint16_t gen_format_ver;
    uint64_t gen_seed;
    uint8_t  orig[81];
    uint8_t  grid[81];
    uint8_t  origin[81];
    uint64_t active_elapsed_ms;
    int64_t  created_epoch_ms;
    int64_t  last_played_epoch_ms;
    int64_t  completed_epoch_ms;
    uint32_t hints_viewed;
    uint32_t hints_applied;
    uint8_t  highest_hint_tech;
    uint8_t  used_auto_solve;
    uint8_t  completion_class;     /* 0 UNASSISTED, 1 HINT_ASSISTED, 2 AUTO_SOLVED */
    int      logic_score;
    int      max_technique;
    int      clue_count;
} sdk_completed_record;

typedef struct sdk_store {
    sdk_settings settings;
    uint32_t game_count;
    uint32_t game_cap;
    sdk_game_record *games;
    uint32_t completed_count;
    uint32_t completed_cap;
    sdk_completed_record *completed;
} sdk_store;

/* opaque vault handle (holds derived key + path for re-save) */
typedef struct sdk_vault sdk_vault;

/* ---- lifecycle ------------------------------------------------------- */

void     sdk_store_init(sdk_store *s);
void     sdk_store_free(sdk_store *s);
sdk_status sdk_store_add_game(sdk_store *s, const sdk_game_record *g);
sdk_status sdk_store_add_completed(sdk_store *s, const sdk_completed_record *c);
sdk_game_record *sdk_store_find_game(sdk_store *s, const uint8_t id[SDK_GAME_ID_LEN]);

/* Create a brand new vault file at `utf8_path` (UTF-8).  Refuses to overwrite
 * an existing file.  `prod` selects the iteration count (1 => 200000). */
sdk_status sdk_vault_create(const char *utf8_path, const char *password,
                            int prod, const sdk_store *store, sdk_vault **out);

/* Open + decrypt + parse an existing vault.  Returns SDK_ERR_AUTH on wrong
 * password or corrupted data (the two cannot always be distinguished, per
 * docs/08 section 11). */
sdk_status sdk_vault_open(const char *utf8_path, const char *password,
                          sdk_vault **out, sdk_store *out_store);

/* Re-encrypt the current in-memory store and atomically replace the vault
 * file, keeping a known-good backup. */
sdk_status sdk_vault_save(sdk_vault *v, const sdk_store *store);

/* Close + wipe key material. */
void sdk_vault_close(sdk_vault *v);

/* Low-level helpers used by tests / failure injection. */
sdk_status sdk_vault_serialize_store(const sdk_store *s,
                                     unsigned char **out_buf, size_t *out_len);
sdk_status sdk_vault_deserialize_store(const unsigned char *buf, size_t len,
                                       sdk_store *out);

/* Game-ID generation with collision retry (docs/19 section 22). */
sdk_status sdk_vault_new_game_id(uint8_t id[SDK_GAME_ID_LEN]);

#ifdef __cplusplus
}
#endif

#endif /* SDK_VAULT_H */
