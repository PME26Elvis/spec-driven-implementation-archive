/* test_crypto.c - known-answer and negative tests for the self-implemented
 * cryptographic stack.
 *
 * Normative sources:
 *   docs/08_STATE_STORAGE_AND_SECURITY.md sections 3, 4, 17, 24
 *   docs/10_TESTING_AND_EVIDENCE.md sections 4 (Storage/security), 16, 25
 *   docs/02_SCOPE_AND_TECHNICAL_BOUNDARIES.md section 3
 *
 * Every expected value below is a published constant transcribed from a
 * standards document, or is constructed inside this test file from a primitive
 * that is itself pinned to a published constant.  No expected value is produced
 * by calling the function under test (docs/08 section 17 last paragraph,
 * docs/10 section 16 and section 25).
 *
 * Vector provenance:
 *   FIPS 180-4 / NIST CAVP        SHA-256
 *   RFC 4231 section 4            HMAC-SHA-256 test cases 1-7
 *   RFC 7914 section 11           PBKDF2-HMAC-SHA-256
 *   RFC 8439 sections 2.3.2,
 *            2.4.2, 2.6.2,
 *            2.5.2, A.3, 2.8.2    ChaCha20, Poly1305, ChaCha20-Poly1305
 *   draft-irtf-cfrg-xchacha-03
 *            2.2.1, A.1/A.3.1,
 *            A.3.2.1              HChaCha20, XChaCha20, XChaCha20-Poly1305
 */
#include <stdlib.h>
#include <string.h>

#include "common/sdk_common.h"
#include "common/sdk_sha256.h"
#include "common/sdk_win.h"
#include "crypto/sdk_aead.h"
#include "crypto/sdk_chacha.h"
#include "crypto/sdk_hmac.h"
#include "crypto/sdk_pbkdf2.h"
#include "crypto/sdk_poly1305.h"
#include "test/sdk_test.h"

/* ------------------------------------------------------------------ */
/* Shared literals                                                     */
/* ------------------------------------------------------------------ */

/* RFC 8439 section 2.4.2 / draft-irtf-cfrg-xchacha A.1 plaintext. */
static const char RFC8439_SUNSCREEN[] =
    "Ladies and Gentlemen of the class of '99: If I could offer you only one "
    "tip for the future, sunscreen would be it.";
#define RFC8439_SUNSCREEN_LEN 114u

/* Decodes a lowercase hex literal into a freshly malloc'ed buffer.  Used only
 * to turn published vectors into byte arrays; it never touches the code under
 * test.  Returns NULL on malformed input so the caller can fail the case. */
static uint8_t *hexbuf(sdk_test_ctx *t, const char *hex, size_t *out_len) {
    size_t n_hex = strlen(hex);
    uint8_t *p;

    if ((n_hex % 2u) != 0u) {
        SDK_T_TRUE(t, 0 /* malformed test vector literal */);
        return NULL;
    }
    p = (uint8_t *)malloc((n_hex / 2u) ? (n_hex / 2u) : 1u);
    if (p == NULL) {
        SDK_T_TRUE(t, 0 /* out of memory decoding test vector */);
        return NULL;
    }
    if (!sdk_hex_decode(hex, n_hex, p)) {
        SDK_T_TRUE(t, 0 /* malformed test vector literal */);
        free(p);
        return NULL;
    }
    *out_len = n_hex / 2u;
    return p;
}

/* ------------------------------------------------------------------ */
/* SHA-256                                                             */
/* ------------------------------------------------------------------ */

static void sha256_expect(sdk_test_ctx *t, const void *data, size_t len,
                          const char *expect_hex) {
    uint8_t d[SDK_SHA256_DIGEST_LEN];
    sdk_sha256(data, len, d);
    sdk_test_eq_hex(t, __FILE__, __LINE__, expect_hex, d, sizeof d,
                    "sha256 digest");
}

static void tc_sha256_published_vectors(sdk_test_ctx *t) {
    /* FIPS 180-4 appendix B and the NIST SHAVS short-message set. */
    sha256_expect(t, "", 0,
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    sha256_expect(t, "abc", 3,
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    sha256_expect(t, "a", 1,
        "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb");
    sha256_expect(t,
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    sha256_expect(t,
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
        "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu", 112,
        "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    /* 448-bit boundary: exactly one byte short of needing a second block. */
    sha256_expect(t,
        "message digest", 14,
        "f7846f55cf23e14eebeab5b4e1550cad5b509e3348fbc4efa3a1413d393cb650");
    sha256_expect(t,
        "abcdefghijklmnopqrstuvwxyz", 26,
        "71c480df93d6ae2f1efad1447c66c9525e316218cf51fc8d9ed832f2daf18b73");
    sha256_expect(t,
        "12345678901234567890123456789012345678901234567890"
        "123456789012345678901234567890", 80,
        "f371bc4a311f2b009eef952dd83ca80e2b60026c8e935592d0f9c308453c813e");
}

static void tc_sha256_one_million_a(sdk_test_ctx *t) {
    /* FIPS 180-4 appendix B.3: one million repetitions of 'a'. */
    sdk_sha256_ctx c;
    uint8_t d[SDK_SHA256_DIGEST_LEN];
    char chunk[1000];
    int i;

    memset(chunk, 'a', sizeof chunk);
    sdk_sha256_init(&c);
    for (i = 0; i < 1000; ++i) {
        sdk_sha256_update(&c, chunk, sizeof chunk);
    }
    sdk_sha256_final(&c, d);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        d, sizeof d, "sha256 of 1e6 'a'");
}

static void tc_sha256_incremental_matches_oneshot(sdk_test_ctx *t) {
    /* The published one-shot digest above pins the value; here the streaming
     * path must reproduce it for every chunk boundary from 1 to 64 bytes so a
     * block-buffering bug cannot hide behind aligned inputs. */
    static const char msg[] =
        "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmno"
        "ijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    const size_t len = 112u;
    size_t step;

    for (step = 1; step <= 64u; ++step) {
        sdk_sha256_ctx c;
        uint8_t d[SDK_SHA256_DIGEST_LEN];
        size_t off = 0;

        sdk_sha256_init(&c);
        while (off < len) {
            size_t n = len - off;
            if (n > step) {
                n = step;
            }
            sdk_sha256_update(&c, msg + off, n);
            off += n;
        }
        sdk_sha256_final(&c, d);
        sdk_test_eq_hex(t, __FILE__, __LINE__,
            "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1",
            d, sizeof d, "streamed sha256 digest");
        if (t->failed) {
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA-256 (RFC 4231)                                             */
/* ------------------------------------------------------------------ */

static void hmac_expect(sdk_test_ctx *t, const char *key_hex,
                        const char *data_hex, const char *expect_hex) {
    uint8_t mac[SDK_HMAC_SHA256_MAC_LEN];
    size_t klen = 0, dlen = 0;
    uint8_t *key = hexbuf(t, key_hex, &klen);
    uint8_t *data = hexbuf(t, data_hex, &dlen);

    if (key != NULL && data != NULL) {
        sdk_hmac_sha256(key, klen, data, dlen, mac);
        sdk_test_eq_hex(t, __FILE__, __LINE__, expect_hex, mac, sizeof mac,
                        "hmac-sha256 mac");
    }
    free(key);
    free(data);
}

static void tc_hmac_sha256_rfc4231(sdk_test_ctx *t) {
    /* Case 1: 20-byte key. */
    hmac_expect(t, "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b",
                "4869205468657265",
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    /* Case 2: short key, "Jefe". */
    hmac_expect(t, "4a656665",
                "7768617420646f2079612077616e7420666f72206e6f7468696e673f",
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    /* Case 3: 20-byte 0xaa key, 50 bytes of 0xdd. */
    hmac_expect(t, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                "dddddddddddddddddddddddddddddddddddddddddddddddddd"
                "dddddddddddddddddddddddddddddddddddddddddddddddddd",
        "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    /* Case 4: 25-byte incrementing key, 50 bytes of 0xcd. */
    hmac_expect(t, "0102030405060708090a0b0c0d0e0f10111213141516171819",
                "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd"
                "cdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcdcd",
        "82558a389a443c0ea4cc819899f2083a85f0faa3e578f8077a2e3ff46729665b");
    /* Case 5: the RFC truncates to 128 bits; the full MAC is asserted here so
     * no information is discarded. */
    hmac_expect(t, "0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c0c",
                "546573742057697468205472756e636174696f6e",
        "a3b6167473100ee06e0c796c2955552bfa6f7c0a6a8aef8b93f860aab0cd20c5");
    /* Case 6: 131-byte key, longer than the 64-byte block, so the key must be
     * hashed first. */
    hmac_expect(t,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaa",
        "54657374205573696e67204c6172676572205468616e20426c6f636b2d53697a"
        "65204b6579202d2048617368204b6579204669727374",
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    /* Case 7: 131-byte key with a long message. */
    hmac_expect(t,
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaa",
        "5468697320697320612074657374207573696e672061206c6172676572207468"
        "616e20626c6f636b2d73697a65206b657920616e642061206c61726765722074"
        "68616e20626c6f636b2d73697a6520646174612e20546865206b6579206e6565"
        "647320746f20626520686173686564206265666f7265206265696e6720757365"
        "642062792074686520484d414320616c676f726974686d2e",
        "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2");
}

static void tc_hmac_sha256_boundary_keys(sdk_test_ctx *t) {
    /* Keys of exactly 63, 64 and 65 bytes exercise the pad / hash-key branch
     * boundary.  The 64-byte case must equal the RFC 4231 case 6 style
     * "no pre-hash" path, so the expected values are pinned independently by
     * building the HMAC definition by hand from KAT-verified SHA-256:
     *     HMAC(K, m) = H((K0 ^ opad) || H((K0 ^ ipad) || m))
     * The hand construction lives only in this test file (docs/10 section 25,
     * "simple reference implementation that exists only in test source"). */
    static const size_t klens[] = {0u, 1u, 63u, 64u, 65u, 128u, 129u};
    size_t i;

    for (i = 0; i < sizeof klens / sizeof klens[0]; ++i) {
        size_t klen = klens[i];
        uint8_t key[200];
        uint8_t k0[SDK_HMAC_SHA256_BLOCK_LEN];
        uint8_t pad[SDK_HMAC_SHA256_BLOCK_LEN];
        uint8_t inner[SDK_SHA256_DIGEST_LEN];
        uint8_t expect[SDK_SHA256_DIGEST_LEN];
        uint8_t got[SDK_HMAC_SHA256_MAC_LEN];
        const char *msg = "boundary key length probe";
        size_t mlen = strlen(msg);
        sdk_sha256_ctx c;
        size_t j;

        for (j = 0; j < klen; ++j) {
            key[j] = (uint8_t)(0x40u + (j & 0x1fu));
        }

        /* Reference K0 per FIPS 198-1: hash when longer than the block,
         * otherwise right-pad with zeros. */
        memset(k0, 0, sizeof k0);
        if (klen > SDK_HMAC_SHA256_BLOCK_LEN) {
            sdk_sha256(key, klen, k0);
        } else if (klen > 0) {
            memcpy(k0, key, klen);
        }

        for (j = 0; j < sizeof pad; ++j) {
            pad[j] = (uint8_t)(k0[j] ^ 0x36u);
        }
        sdk_sha256_init(&c);
        sdk_sha256_update(&c, pad, sizeof pad);
        sdk_sha256_update(&c, msg, mlen);
        sdk_sha256_final(&c, inner);

        for (j = 0; j < sizeof pad; ++j) {
            pad[j] = (uint8_t)(k0[j] ^ 0x5cu);
        }
        sdk_sha256_init(&c);
        sdk_sha256_update(&c, pad, sizeof pad);
        sdk_sha256_update(&c, inner, sizeof inner);
        sdk_sha256_final(&c, expect);

        sdk_hmac_sha256(key, klen, msg, mlen, got);
        SDK_T_EQ_MEM(t, expect, got, sizeof expect);
        if (t->failed) {
            return;
        }
    }
}

static void tc_hmac_sha256_incremental(sdk_test_ctx *t) {
    /* RFC 4231 case 7 message, fed one byte at a time. */
    static const char key_hex[] =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaa";
    static const char msg[] =
        "This is a test using a larger than block-size key and a larger than "
        "block-size data. The key needs to be hashed before being used by the "
        "HMAC algorithm.";
    size_t klen = 0, i;
    uint8_t *key = hexbuf(t, key_hex, &klen);
    sdk_hmac_sha256_ctx c;
    uint8_t mac[SDK_HMAC_SHA256_MAC_LEN];

    if (key == NULL) {
        return;
    }
    sdk_hmac_sha256_init(&c, key, klen);
    for (i = 0; i < strlen(msg); ++i) {
        sdk_hmac_sha256_update(&c, msg + i, 1u);
    }
    sdk_hmac_sha256_final(&c, mac);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2",
        mac, sizeof mac, "incremental hmac");
    free(key);
}

static void tc_hmac_sha256_precomputed_key_matches(sdk_test_ctx *t) {
    /* The precomputed-key form is what PBKDF2 uses 200,000 times; it must be
     * bit-identical to the one-shot form pinned above by RFC 4231 case 2. */
    sdk_hmac_sha256_key k;
    uint8_t mac[SDK_HMAC_SHA256_MAC_LEN];
    const char *key = "Jefe";
    const char *msg = "what do ya want for nothing?";

    sdk_hmac_sha256_key_init(&k, key, strlen(key));
    sdk_hmac_sha256_with_key(&k, msg, strlen(msg), mac);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        mac, sizeof mac, "precomputed-key hmac");
    /* Reusable across messages without state leaking between calls. */
    sdk_hmac_sha256_with_key(&k, msg, strlen(msg), mac);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
        mac, sizeof mac, "precomputed-key hmac, second call");
    sdk_hmac_sha256_key_free(&k);
}

/* ------------------------------------------------------------------ */
/* PBKDF2-HMAC-SHA-256                                                 */
/* ------------------------------------------------------------------ */

static void pbkdf2_expect(sdk_test_ctx *t, const void *p, size_t plen,
                          const void *s, size_t slen, uint32_t c,
                          size_t dklen, const char *expect_hex) {
    uint8_t *dk = (uint8_t *)malloc(dklen);
    if (dk == NULL) {
        SDK_T_TRUE(t, 0 /* out of memory */);
        return;
    }
    SDK_T_OK(t, sdk_pbkdf2_hmac_sha256(p, plen, s, slen, c, dk, dklen));
    sdk_test_eq_hex(t, __FILE__, __LINE__, expect_hex, dk, dklen,
                    "pbkdf2 derived key");
    free(dk);
}

static void tc_pbkdf2_rfc7914_vectors(sdk_test_ctx *t) {
    /* RFC 7914 section 11, PBKDF2-HMAC-SHA-256 vectors. */
    pbkdf2_expect(t, "passwd", 6, "salt", 4, 1u, 64u,
        "55ac046e56e3089fec1691c22544b605f94185216dde0465e68b9d57c20dacbc"
        "49ca9cccf179b645991664b39d77ef317c71b845b1e30bd509112041d3a19783");
    pbkdf2_expect(t, "Password", 8, "NaCl", 4, 80000u, 64u,
        "4ddcd8f60b98be21830cee5ef22701f9641a4418d04c0414aeff08876b34ab56"
        "a1d425a1225833549adb841b51c9b3176a272bdebba1d078478f62b397f33c8d");
}

static void tc_pbkdf2_iteration_progression(sdk_test_ctx *t) {
    /* Widely published PBKDF2-HMAC-SHA-256 counterparts of the RFC 6070
     * inputs.  Different iteration counts must produce different keys, and
     * each key is pinned. */
    pbkdf2_expect(t, "password", 8, "salt", 4, 1u, 32u,
        "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    pbkdf2_expect(t, "password", 8, "salt", 4, 2u, 32u,
        "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
    pbkdf2_expect(t, "password", 8, "salt", 4, 4096u, 32u,
        "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");
}

static void tc_pbkdf2_partial_final_block(sdk_test_ctx *t) {
    /* dkLen 40 is not a multiple of the 32-byte MAC length, so the final block
     * must be truncated rather than padded. */
    pbkdf2_expect(t, "passwordPASSWORDpassword", 24,
                  "saltSALTsaltSALTsaltSALTsaltSALTsalt", 36, 4096u, 40u,
        "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1"
        "c635518c7dac47e9");
}

static void tc_pbkdf2_embedded_nul(sdk_test_ctx *t) {
    /* Password and salt are byte strings, not C strings; an embedded NUL must
     * not truncate either input. */
    pbkdf2_expect(t, "pass\0word", 9u, "sa\0lt", 5u, 4096u, 16u,
        "89b69d0516f829893c696226650a8687");
}

static void tc_pbkdf2_first_block_equals_hmac(sdk_test_ctx *t) {
    /* Independent construction: for c == 1 the PBKDF2 output block i is
     * exactly HMAC(P, S || INT_BE32(i)).  HMAC is already pinned to RFC 4231,
     * so this checks the PBKDF2 framing without reusing PBKDF2 itself. */
    const char *pw = "engineering-boundary";
    const char *salt = "sixteen-byte-salt";
    uint8_t block[SDK_HMAC_SHA256_MAC_LEN * 2u];
    uint8_t dk[SDK_HMAC_SHA256_MAC_LEN * 2u];
    uint8_t input[64];
    size_t slen = strlen(salt);
    uint32_t i;

    for (i = 1; i <= 2u; ++i) {
        memcpy(input, salt, slen);
        input[slen + 0] = (uint8_t)(i >> 24);
        input[slen + 1] = (uint8_t)(i >> 16);
        input[slen + 2] = (uint8_t)(i >> 8);
        input[slen + 3] = (uint8_t)i;
        sdk_hmac_sha256(pw, strlen(pw), input, slen + 4u,
                        block + (i - 1u) * SDK_HMAC_SHA256_MAC_LEN);
    }
    SDK_T_OK(t, sdk_pbkdf2_hmac_sha256(pw, strlen(pw), salt, slen, 1u,
                                       dk, sizeof dk));
    SDK_T_EQ_MEM(t, block, dk, sizeof block);
}

static void tc_pbkdf2_production_iteration_count(sdk_test_ctx *t) {
    /* docs/08 section 3: production vaults use exactly 200,000 iterations and
     * the value must not be lowered. */
    SDK_T_EQ_U(t, 200000u, SDK_PBKDF2_PRODUCTION_ITERATIONS);
}

static void tc_pbkdf2_rejects_bad_arguments(sdk_test_ctx *t) {
    uint8_t dk[32];
    SDK_T_EQ_ST(t, SDK_ERR_USAGE,
                sdk_pbkdf2_hmac_sha256("p", 1u, "s", 1u, 0u, dk, sizeof dk));
    SDK_T_EQ_ST(t, SDK_ERR_USAGE,
                sdk_pbkdf2_hmac_sha256("p", 1u, "s", 1u, 1u, dk, 0u));
    SDK_T_EQ_ST(t, SDK_ERR_USAGE,
                sdk_pbkdf2_hmac_sha256("p", 1u, "s", 1u, 1u, NULL, sizeof dk));
}

/* ------------------------------------------------------------------ */
/* ChaCha20 (RFC 8439)                                                 */
/* ------------------------------------------------------------------ */

static void tc_chacha20_block_function_rfc8439(sdk_test_ctx *t) {
    /* RFC 8439 section 2.3.2. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t nonce[SDK_CHACHA20_NONCE_LEN];
    uint8_t out[SDK_CHACHA20_BLOCK_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", &n);
    uint8_t *v = hexbuf(t, "000000090000004a00000000", &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    sdk_chacha20_block(key, 1u, nonce, out);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "10f1e7e4d13b5915500fdd1fa32071c4"
        "c7d1f4c733c068030422aa9ac3d46c4e"
        "d2826446079faa0914c2d705d98b02a2"
        "b5129cd1de164eb9cbd083e8a2503c4e",
        out, sizeof out, "chacha20 block keystream");
    free(k);
    free(v);
}

static void tc_chacha20_stream_rfc8439(sdk_test_ctx *t) {
    /* RFC 8439 section 2.4.2: 114-byte plaintext, counter starts at 1. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t nonce[SDK_CHACHA20_NONCE_LEN];
    uint8_t ct[RFC8439_SUNSCREEN_LEN];
    uint8_t back[RFC8439_SUNSCREEN_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", &n);
    uint8_t *v = hexbuf(t, "000000000000004a00000000", &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);

    SDK_T_EQ_U(t, RFC8439_SUNSCREEN_LEN, strlen(RFC8439_SUNSCREEN));
    sdk_chacha20_xor(key, 1u, nonce, (const unsigned char *)RFC8439_SUNSCREEN,
                     ct, RFC8439_SUNSCREEN_LEN);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "6e2e359a2568f98041ba0728dd0d6981"
        "e97e7aec1d4360c20a27afccfd9fae0b"
        "f91b65c5524733ab8f593dabcd62b357"
        "1639d624e65152ab8f530c359f0861d8"
        "07ca0dbf500d6a6156a38e088a22b65e"
        "52bc514d16ccf806818ce91ab7793736"
        "5af90bbf74a35be6b40b8eedf2785e42"
        "874d",
        ct, sizeof ct, "chacha20 ciphertext");

    /* Involution: applying the same keystream returns the plaintext. */
    sdk_chacha20_xor(key, 1u, nonce, ct, back, sizeof ct);
    SDK_T_EQ_MEM(t, RFC8439_SUNSCREEN, back, RFC8439_SUNSCREEN_LEN);
    free(k);
    free(v);
}

static void tc_chacha20_poly1305_key_generation_rfc8439(sdk_test_ctx *t) {
    /* RFC 8439 section 2.6.2: the AEAD one-time Poly1305 key is the first 32
     * bytes of the counter-0 block. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t nonce[SDK_CHACHA20_NONCE_LEN];
    uint8_t block[SDK_CHACHA20_BLOCK_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", &n);
    uint8_t *v = hexbuf(t, "000000000001020304050607", &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    sdk_chacha20_block(key, 0u, nonce, block);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "8ad5a08b905f81cc815040274ab29471"
        "a833b637e3fd0da508dbb8e2fdd1a646",
        block, 32u, "poly1305 one-time key");
    free(k);
    free(v);
}

static void tc_chacha20_chunked_matches_contiguous(sdk_test_ctx *t) {
    /* Splitting a stream across calls must advance the block counter exactly;
     * the contiguous result is pinned by the RFC 8439 section 2.4.2 vector. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t nonce[SDK_CHACHA20_NONCE_LEN];
    uint8_t ct[RFC8439_SUNSCREEN_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", &n);
    uint8_t *v = hexbuf(t, "000000000000004a00000000", &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);

    /* Bytes 0..63 come from counter 1, bytes 64..113 from counter 2. */
    sdk_chacha20_xor(key, 1u, nonce, (const unsigned char *)RFC8439_SUNSCREEN,
                     ct, 64u);
    sdk_chacha20_xor(key, 2u, nonce,
                     (const unsigned char *)RFC8439_SUNSCREEN + 64,
                     ct + 64, RFC8439_SUNSCREEN_LEN - 64u);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "6e2e359a2568f98041ba0728dd0d6981"
        "e97e7aec1d4360c20a27afccfd9fae0b"
        "f91b65c5524733ab8f593dabcd62b357"
        "1639d624e65152ab8f530c359f0861d8"
        "07ca0dbf500d6a6156a38e088a22b65e"
        "52bc514d16ccf806818ce91ab7793736"
        "5af90bbf74a35be6b40b8eedf2785e42"
        "874d",
        ct, sizeof ct, "chunked chacha20 ciphertext");
    free(k);
    free(v);
}

/* ------------------------------------------------------------------ */
/* HChaCha20 and XChaCha20                                             */
/* ------------------------------------------------------------------ */

static void tc_hchacha20_draft_vector(sdk_test_ctx *t) {
    /* draft-irtf-cfrg-xchacha-03 section 2.2.1. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t nonce[SDK_HCHACHA20_NONCE_LEN];
    uint8_t out[SDK_HCHACHA20_OUT_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f", &n);
    uint8_t *v = hexbuf(t, "000000090000004a0000000031415927", &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    sdk_hchacha20(key, nonce, out);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "82413b4227b27bfed30e42508a877d73"
        "a0f9e4d58a74a853c12ec41326d3ecdc",
        out, sizeof out, "hchacha20 subkey");
    free(k);
    free(v);
}

static void tc_xchacha20_keystream_draft_vector(sdk_test_ctx *t) {
    /* draft-irtf-cfrg-xchacha-03 section A.3.2.1 (block counter 0).  The
     * XChaCha20 stream is HChaCha20 followed by ChaCha20 with the derived
     * subkey and a 4-zero-byte prefixed nonce; asserting the published
     * keystream validates that composition. */
    uint8_t key[SDK_CHACHA20_KEY_LEN];
    uint8_t iv24[SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t subkey[SDK_HCHACHA20_OUT_LEN];
    uint8_t nonce12[SDK_CHACHA20_NONCE_LEN];
    uint8_t zeros[304];
    uint8_t ks[304];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", &n);
    uint8_t *v = hexbuf(t, "404142434445464748494a4b4c4d4e4f5051525354555658",
                        &n);

    if (k == NULL || v == NULL) {
        free(k);
        free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(iv24, v, sizeof iv24);

    sdk_hchacha20(key, iv24, subkey);
    memset(nonce12, 0, sizeof nonce12);
    memcpy(nonce12 + 4, iv24 + 16, 8u);
    memset(zeros, 0, sizeof zeros);
    sdk_chacha20_xor(subkey, 0u, nonce12, zeros, ks, sizeof zeros);

    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "1131ce9a2a20ae0d67c8935c7789fa1025c9e5bb720fb96f11354fb97af0bd9a"
        "adec0863ba60cac8582c48f86cdfc48edd46a48642c5de62ccf11c7b21bf337d"
        "29624b4b1b140ace53740e405b2168540fd7d630c1f536fecd722fc3cddba7f4"
        "cca98cf9e47e5e64d115450f9b125b54449ff76141ca620a1f9cfcab2a1a8a25"
        "5e766a5266b878846120ea64ad99aa479471e63befcbd37cd1c22a221fe46221"
        "5cf32c74895bf505863ccddd48f62916dc6521f1ec50a5ae08903aa259d9bf60"
        "7cd8026fba548604f1b6072d91bc91243a5b845f7fd171b02edc5a0a84cf28dd"
        "241146bc376e3f48df5e7fee1d11048c190a3d3deb0feb64b42d9c6fdeee290f"
        "a0e6ae2c26c0249ea8c181f7e2ffd100cbe5fd3c4f8271d62b15330cb8fdcf00"
        "b3df507ca8c924f7017b7e712d15a2eb",
        ks, sizeof ks, "xchacha20 keystream");
    free(k);
    free(v);
}

/* ------------------------------------------------------------------ */
/* Poly1305 (RFC 8439)                                                 */
/* ------------------------------------------------------------------ */

static void poly_expect(sdk_test_ctx *t, const char *key_hex,
                        const void *msg, size_t mlen, const char *tag_hex) {
    uint8_t tag[SDK_POLY1305_TAG_LEN];
    size_t klen = 0;
    uint8_t *key = hexbuf(t, key_hex, &klen);

    if (key == NULL) {
        return;
    }
    SDK_T_EQ_U(t, SDK_POLY1305_KEY_LEN, klen);
    sdk_poly1305(key, msg, mlen, tag);
    sdk_test_eq_hex(t, __FILE__, __LINE__, tag_hex, tag, sizeof tag,
                    "poly1305 tag");
    free(key);
}

static void tc_poly1305_rfc8439_section_2_5_2(sdk_test_ctx *t) {
    static const char msg[] = "Cryptographic Forum Research Group";
    poly_expect(t,
        "85d6be7857556d337f4452fe42d506a8"
        "0103808afb0db2fd4abff6af4149f51b",
        msg, 34u, "a8061dc1305136c6c22b8baf0c0127a9");
}

static void tc_poly1305_rfc8439_appendix_a3(sdk_test_ctx *t) {
    /* Test vector #1: all-zero key and 64 zero bytes yield an all-zero tag. */
    uint8_t zeros[64];
    memset(zeros, 0, sizeof zeros);
    poly_expect(t,
        "0000000000000000000000000000000000000000000000000000000000000000",
        zeros, sizeof zeros, "00000000000000000000000000000000");

    /* Test vector #2: r == 0 so the tag is exactly s. */
    {
        static const char msg[] =
            "Any submission to the IETF intended by the Contributor for "
            "publication as all or part of an IETF Internet-Draft or RFC and "
            "any statement made within the context of an IETF activity is "
            "considered an \"IETF Contribution\". Such statements include "
            "oral statements in IETF sessions, as well as written and "
            "electronic communications made at any time or place, which are "
            "addressed to";
        SDK_T_EQ_U(t, 375u, strlen(msg));
        poly_expect(t,
            "00000000000000000000000000000000"
            "36e5f6b5c5e06070f0efca96227a863e",
            msg, strlen(msg), "36e5f6b5c5e06070f0efca96227a863e");

        /* Test vector #3: s == 0, same message. */
        poly_expect(t,
            "36e5f6b5c5e06070f0efca96227a863e"
            "00000000000000000000000000000000",
            msg, strlen(msg), "f3477e7cd95417af89a6b8794c310cf0");
    }
}

static void tc_poly1305_incremental_matches_oneshot(sdk_test_ctx *t) {
    /* Streaming across every chunk size from 1 to 34 bytes must reproduce the
     * RFC 8439 section 2.5.2 tag. */
    static const char msg[] = "Cryptographic Forum Research Group";
    size_t klen = 0, step;
    uint8_t *key = hexbuf(t,
        "85d6be7857556d337f4452fe42d506a8"
        "0103808afb0db2fd4abff6af4149f51b", &klen);

    if (key == NULL) {
        return;
    }
    for (step = 1; step <= 34u; ++step) {
        sdk_poly1305_ctx c;
        uint8_t tag[SDK_POLY1305_TAG_LEN];
        size_t off = 0;

        sdk_poly1305_init(&c, key);
        while (off < 34u) {
            size_t n = 34u - off;
            if (n > step) {
                n = step;
            }
            sdk_poly1305_update(&c, (const unsigned char *)msg + off, n);
            off += n;
        }
        sdk_poly1305_final(&c, tag);
        sdk_test_eq_hex(t, __FILE__, __LINE__,
                        "a8061dc1305136c6c22b8baf0c0127a9", tag, sizeof tag,
                        "streamed poly1305 tag");
        if (t->failed) {
            break;
        }
    }
    free(key);
}

static void tc_poly1305_empty_message(sdk_test_ctx *t) {
    /* With an empty message the tag is s mod 2^128, which for these key bytes
     * is the second half of the key. */
    poly_expect(t,
        "0000000000000000000000000000000036e5f6b5c5e06070f0efca96227a863e",
        "", 0u, "36e5f6b5c5e06070f0efca96227a863e");
}

/* ------------------------------------------------------------------ */
/* AEAD construction cross-check against RFC 8439 section 2.8.2        */
/* ------------------------------------------------------------------ */

/* Reference AEAD framing written only for the tests (docs/10 section 25).
 * It composes ChaCha20 and Poly1305, both already pinned to published
 * vectors, using the RFC 8439 section 2.8 MAC input layout:
 *     AAD || pad16 || ciphertext || pad16 || le64(aadlen) || le64(ctlen)
 * The result is compared against the published IETF ChaCha20-Poly1305 vector,
 * which validates the exact framing that production XChaCha20-Poly1305 reuses.
 */
static void ref_aead_chacha20poly1305(const uint8_t key[32],
                                      const uint8_t nonce[12],
                                      const uint8_t *aad, size_t aadlen,
                                      const uint8_t *pt, size_t ptlen,
                                      uint8_t *ct, uint8_t tag[16]) {
    uint8_t block[SDK_CHACHA20_BLOCK_LEN];
    uint8_t polykey[SDK_POLY1305_KEY_LEN];
    sdk_poly1305_ctx pc;
    static const uint8_t zero16[16] = {0};
    uint8_t lens[16];
    size_t i;
    uint64_t a = (uint64_t)aadlen;
    uint64_t c = (uint64_t)ptlen;

    sdk_chacha20_block(key, 0u, nonce, block);
    memcpy(polykey, block, sizeof polykey);
    sdk_chacha20_xor(key, 1u, nonce, pt, ct, ptlen);

    sdk_poly1305_init(&pc, polykey);
    sdk_poly1305_update(&pc, aad, aadlen);
    if ((aadlen % 16u) != 0u) {
        sdk_poly1305_update(&pc, zero16, 16u - (aadlen % 16u));
    }
    sdk_poly1305_update(&pc, ct, ptlen);
    if ((ptlen % 16u) != 0u) {
        sdk_poly1305_update(&pc, zero16, 16u - (ptlen % 16u));
    }
    for (i = 0; i < 8u; ++i) {
        lens[i] = (uint8_t)(a >> (8u * i));
        lens[8u + i] = (uint8_t)(c >> (8u * i));
    }
    sdk_poly1305_update(&pc, lens, sizeof lens);
    sdk_poly1305_final(&pc, tag);
}

static void tc_aead_framing_matches_rfc8439_2_8_2(sdk_test_ctx *t) {
    uint8_t key[32], nonce[12], aad[12];
    uint8_t ct[RFC8439_SUNSCREEN_LEN];
    uint8_t tag[16];
    size_t n = 0;
    uint8_t *k = hexbuf(t,
        "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f", &n);
    uint8_t *v = hexbuf(t, "070000004041424344454647", &n);
    uint8_t *a = hexbuf(t, "50515253c0c1c2c3c4c5c6c7", &n);

    if (k == NULL || v == NULL || a == NULL) {
        free(k); free(v); free(a);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    memcpy(aad, a, sizeof aad);

    ref_aead_chacha20poly1305(key, nonce, aad, sizeof aad,
                              (const uint8_t *)RFC8439_SUNSCREEN,
                              RFC8439_SUNSCREEN_LEN, ct, tag);
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "d31a8d34648e60db7b86afbc53ef7ec2"
        "a4aded51296e08fea9e2b5a736ee62d6"
        "3dbea45e8ca9671282fafb69da92728b"
        "1a71de0a9e060b2905d6a5b67ecd3b36"
        "92ddbd7f2d778b8c9803aee328091b58"
        "fab324e4fad675945585808b4831d7bc"
        "3ff4def08e4b7a9de576d26586cec64b"
        "6116",
        ct, sizeof ct, "rfc8439 aead ciphertext");
    sdk_test_eq_hex(t, __FILE__, __LINE__,
        "1ae10b594f09e26a7e902ecbd0600691", tag, sizeof tag,
        "rfc8439 aead tag");
    free(k); free(v); free(a);
}

/* ------------------------------------------------------------------ */
/* XChaCha20-Poly1305 AEAD                                             */
/* ------------------------------------------------------------------ */

/* draft-irtf-cfrg-xchacha-03 appendix A.1 / A.3.1. */
#define XAEAD_KEY_HEX \
    "808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f"
#define XAEAD_NONCE_HEX "404142434445464748494a4b4c4d4e4f5051525354555657"
#define XAEAD_AAD_HEX   "50515253c0c1c2c3c4c5c6c7"
#define XAEAD_CT_HEX \
    "bd6d179d3e83d43b9576579493c0e939572a1700252bfaccbed2902c21396cbb" \
    "731c7f1b0b4aa6440bf3a82f4eda7e39ae64c6708c54c216cb96b72e1213b452" \
    "2f8c9ba40db5d945b11b69b982c1bb9e3f3fac2bc369488f76b2383565d3fff9" \
    "21f9664c97637da9768812f615c68b13b52e"
#define XAEAD_TAG_HEX   "c0875924c1c7987947deafd8780acf49"
#define XAEAD_POLYKEY_HEX \
    "7b191f80f361f099094f6f4b8fb97df847cc6873a8f2b190dd73807183f907d5"

static void tc_xaead_draft_vector_encrypt(sdk_test_ctx *t) {
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t aad[12];
    uint8_t ct[RFC8439_SUNSCREEN_LEN];
    uint8_t tag[SDK_XCHACHA20POLY1305_TAG_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t, XAEAD_KEY_HEX, &n);
    uint8_t *v = hexbuf(t, XAEAD_NONCE_HEX, &n);
    uint8_t *a = hexbuf(t, XAEAD_AAD_HEX, &n);

    if (k == NULL || v == NULL || a == NULL) {
        free(k); free(v); free(a);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    memcpy(aad, a, sizeof aad);

    SDK_T_OK(t, sdk_xchacha20poly1305_encrypt(
        key, nonce, aad, sizeof aad,
        (const unsigned char *)RFC8439_SUNSCREEN, RFC8439_SUNSCREEN_LEN,
        ct, tag));
    sdk_test_eq_hex(t, __FILE__, __LINE__, XAEAD_CT_HEX, ct, sizeof ct,
                    "xchacha20-poly1305 ciphertext");
    sdk_test_eq_hex(t, __FILE__, __LINE__, XAEAD_TAG_HEX, tag, sizeof tag,
                    "xchacha20-poly1305 tag");
    free(k); free(v); free(a);
}

static void tc_xaead_draft_vector_decrypt(sdk_test_ctx *t) {
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t pt[RFC8439_SUNSCREEN_LEN];
    size_t n = 0, ctlen = 0, taglen = 0, aadlen = 0;
    uint8_t *k = hexbuf(t, XAEAD_KEY_HEX, &n);
    uint8_t *v = hexbuf(t, XAEAD_NONCE_HEX, &n);
    uint8_t *a = hexbuf(t, XAEAD_AAD_HEX, &aadlen);
    uint8_t *ct = hexbuf(t, XAEAD_CT_HEX, &ctlen);
    uint8_t *tag = hexbuf(t, XAEAD_TAG_HEX, &taglen);

    if (k == NULL || v == NULL || a == NULL || ct == NULL || tag == NULL) {
        free(k); free(v); free(a); free(ct); free(tag);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(nonce, v, sizeof nonce);
    SDK_T_EQ_U(t, RFC8439_SUNSCREEN_LEN, ctlen);
    SDK_T_EQ_U(t, SDK_XCHACHA20POLY1305_TAG_LEN, taglen);

    SDK_T_OK(t, sdk_xchacha20poly1305_decrypt(key, nonce, a, aadlen,
                                              ct, ctlen, tag, pt));
    SDK_T_EQ_MEM(t, RFC8439_SUNSCREEN, pt, RFC8439_SUNSCREEN_LEN);
    free(k); free(v); free(a); free(ct); free(tag);
}

static void tc_xaead_poly_key_is_derived_not_reused(sdk_test_ctx *t) {
    /* draft appendix A.1 also publishes the derived Poly1305 key.  Deriving it
     * through HChaCha20 + ChaCha20 confirms the subkey and inner-nonce layout
     * that production uses, and proves the vault key itself is never used
     * directly as the MAC key. */
    uint8_t key[32], iv24[24], subkey[32], nonce12[12];
    uint8_t block[SDK_CHACHA20_BLOCK_LEN];
    size_t n = 0;
    uint8_t *k = hexbuf(t, XAEAD_KEY_HEX, &n);
    uint8_t *v = hexbuf(t, XAEAD_NONCE_HEX, &n);

    if (k == NULL || v == NULL) {
        free(k); free(v);
        return;
    }
    memcpy(key, k, sizeof key);
    memcpy(iv24, v, sizeof iv24);
    sdk_hchacha20(key, iv24, subkey);
    memset(nonce12, 0, sizeof nonce12);
    memcpy(nonce12 + 4, iv24 + 16, 8u);
    sdk_chacha20_block(subkey, 0u, nonce12, block);
    sdk_test_eq_hex(t, __FILE__, __LINE__, XAEAD_POLYKEY_HEX, block, 32u,
                    "derived poly1305 key");
    SDK_T_NE_MEM(t, key, block, 32u);
    free(k); free(v);
}

static void tc_xaead_round_trip_lengths(sdk_test_ctx *t) {
    /* Lengths around the 64-byte block boundary, plus empty plaintext and
     * empty AAD, which the vault uses for the smallest possible payload. */
    static const size_t lens[] = {0u, 1u, 15u, 16u, 17u, 63u, 64u, 65u,
                                  127u, 128u, 129u, 1000u};
    size_t i;
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN];

    SDK_T_OK(t, sdk_random_bytes(key, sizeof key));
    SDK_T_OK(t, sdk_random_bytes(nonce, sizeof nonce));

    for (i = 0; i < sizeof lens / sizeof lens[0]; ++i) {
        size_t len = lens[i];
        uint8_t *pt = (uint8_t *)malloc(len ? len : 1u);
        uint8_t *ct = (uint8_t *)malloc(len ? len : 1u);
        uint8_t *back = (uint8_t *)malloc(len ? len : 1u);
        uint8_t tag[SDK_XCHACHA20POLY1305_TAG_LEN];
        size_t aadlen = (i % 3u == 0u) ? 0u : (i * 7u);
        uint8_t aad[128];
        size_t j;

        if (pt == NULL || ct == NULL || back == NULL) {
            SDK_T_TRUE(t, 0 /* out of memory */);
            free(pt); free(ct); free(back);
            return;
        }
        for (j = 0; j < len; ++j) {
            pt[j] = (uint8_t)(j * 31u + i);
        }
        for (j = 0; j < aadlen && j < sizeof aad; ++j) {
            aad[j] = (uint8_t)(0xa0u + j);
        }
        if (aadlen > sizeof aad) {
            aadlen = sizeof aad;
        }

        SDK_T_OK(t, sdk_xchacha20poly1305_encrypt(key, nonce, aad, aadlen,
                                                  pt, len, ct, tag));
        if (len > 0) {
            SDK_T_NE_MEM(t, pt, ct, len);
        }
        SDK_T_OK(t, sdk_xchacha20poly1305_decrypt(key, nonce, aad, aadlen,
                                                  ct, len, tag, back));
        if (len > 0) {
            SDK_T_EQ_MEM(t, pt, back, len);
        }
        free(pt); free(ct); free(back);
        if (t->failed) {
            return;
        }
    }
}

/* Builds the published vector once so negative cases can mutate a copy. */
typedef struct xaead_fixture {
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t aad[12];
    uint8_t ct[RFC8439_SUNSCREEN_LEN];
    uint8_t tag[SDK_XCHACHA20POLY1305_TAG_LEN];
} xaead_fixture;

static int xaead_fixture_load(sdk_test_ctx *t, xaead_fixture *f) {
    size_t n = 0;
    uint8_t *k = hexbuf(t, XAEAD_KEY_HEX, &n);
    uint8_t *v = hexbuf(t, XAEAD_NONCE_HEX, &n);
    uint8_t *a = hexbuf(t, XAEAD_AAD_HEX, &n);
    uint8_t *c = hexbuf(t, XAEAD_CT_HEX, &n);
    uint8_t *g = hexbuf(t, XAEAD_TAG_HEX, &n);
    int ok = 0;

    if (k != NULL && v != NULL && a != NULL && c != NULL && g != NULL) {
        memcpy(f->key, k, sizeof f->key);
        memcpy(f->nonce, v, sizeof f->nonce);
        memcpy(f->aad, a, sizeof f->aad);
        memcpy(f->ct, c, sizeof f->ct);
        memcpy(f->tag, g, sizeof f->tag);
        ok = 1;
    }
    free(k); free(v); free(a); free(c); free(g);
    return ok;
}

/* Asserts that decryption fails and that not one plaintext byte was written
 * into the caller buffer (docs/08 sections 4 and 24). */
static void expect_auth_failure(sdk_test_ctx *t, const xaead_fixture *f,
                                const char *what) {
    uint8_t out[RFC8439_SUNSCREEN_LEN];
    uint8_t canary[RFC8439_SUNSCREEN_LEN];
    sdk_status st;

    memset(out, 0x5a, sizeof out);
    memset(canary, 0x5a, sizeof canary);
    st = sdk_xchacha20poly1305_decrypt(f->key, f->nonce, f->aad, sizeof f->aad,
                                       f->ct, sizeof f->ct, f->tag, out);
    sdk_test_assert_report(t, st == SDK_ERR_AUTH, __FILE__, __LINE__,
                           "%s: expected SDK_ERR_AUTH, got %s", what,
                           sdk_status_name(st));
    sdk_test_eq_mem(t, __FILE__, __LINE__, canary, out, sizeof out,
                    "plaintext buffer must remain untouched");
}

static void tc_xaead_rejects_wrong_key(sdk_test_ctx *t) {
    xaead_fixture f;
    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    f.key[0] ^= 0x01u;
    expect_auth_failure(t, &f, "single-bit key change");
    f.key[0] ^= 0x01u;
    f.key[31] ^= 0x80u;
    expect_auth_failure(t, &f, "high-bit change in last key byte");
}

static void tc_xaead_rejects_modified_nonce(sdk_test_ctx *t) {
    xaead_fixture f;
    size_t i;
    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    /* Both halves matter: bytes 0..15 feed HChaCha20, bytes 16..23 the inner
     * ChaCha20 nonce. */
    for (i = 0; i < SDK_XCHACHA20POLY1305_NONCE_LEN; ++i) {
        f.nonce[i] ^= 0x01u;
        expect_auth_failure(t, &f, "nonce bit flip");
        f.nonce[i] ^= 0x01u;
        if (t->failed) {
            return;
        }
    }
}

static void tc_xaead_rejects_modified_aad(sdk_test_ctx *t) {
    xaead_fixture f;
    size_t i;
    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    for (i = 0; i < sizeof f.aad; ++i) {
        f.aad[i] ^= 0x80u;
        expect_auth_failure(t, &f, "aad bit flip");
        f.aad[i] ^= 0x80u;
        if (t->failed) {
            return;
        }
    }
}

static void tc_xaead_rejects_modified_ciphertext(sdk_test_ctx *t) {
    xaead_fixture f;
    static const size_t offsets[] = {0u, 1u, 63u, 64u, 112u, 113u};
    size_t i;
    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    for (i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) {
        f.ct[offsets[i]] ^= 0x01u;
        expect_auth_failure(t, &f, "ciphertext bit flip");
        f.ct[offsets[i]] ^= 0x01u;
        if (t->failed) {
            return;
        }
    }
}

static void tc_xaead_rejects_modified_tag(sdk_test_ctx *t) {
    xaead_fixture f;
    size_t i;
    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    for (i = 0; i < SDK_XCHACHA20POLY1305_TAG_LEN; ++i) {
        f.tag[i] ^= 0x01u;
        expect_auth_failure(t, &f, "tag bit flip");
        f.tag[i] ^= 0x01u;
        if (t->failed) {
            return;
        }
    }
    /* An all-zero tag must not be accepted either. */
    memset(f.tag, 0, sizeof f.tag);
    expect_auth_failure(t, &f, "zeroed tag");
}

static void tc_xaead_rejects_truncated_ciphertext(sdk_test_ctx *t) {
    /* Shortening the ciphertext changes the length field in the MAC input, so
     * the tag must fail even though the retained bytes are unmodified. */
    xaead_fixture f;
    uint8_t out[RFC8439_SUNSCREEN_LEN];
    uint8_t canary[RFC8439_SUNSCREEN_LEN];

    if (!xaead_fixture_load(t, &f)) {
        return;
    }
    memset(out, 0x5a, sizeof out);
    memset(canary, 0x5a, sizeof canary);
    SDK_T_EQ_ST(t, SDK_ERR_AUTH,
                sdk_xchacha20poly1305_decrypt(f.key, f.nonce, f.aad,
                                              sizeof f.aad, f.ct,
                                              sizeof f.ct - 1u, f.tag, out));
    SDK_T_EQ_MEM(t, canary, out, sizeof out);
}

static void tc_xaead_fresh_nonce_changes_output(sdk_test_ctx *t) {
    /* docs/08 sections 4 and 17: each write uses a fresh 24-byte nonce from
     * BCryptGenRandom, so repeated writes of identical plaintext must differ
     * in nonce, ciphertext and tag. */
    enum { ROUNDS = 64 };
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN];
    uint8_t nonces[ROUNDS][SDK_XCHACHA20POLY1305_NONCE_LEN];
    uint8_t cts[ROUNDS][32];
    uint8_t tags[ROUNDS][SDK_XCHACHA20POLY1305_TAG_LEN];
    static const uint8_t plain[32] = {0};
    int i, j;

    SDK_T_OK(t, sdk_random_bytes(key, sizeof key));
    for (i = 0; i < ROUNDS; ++i) {
        SDK_T_OK(t, sdk_random_bytes(nonces[i], sizeof nonces[i]));
        SDK_T_OK(t, sdk_xchacha20poly1305_encrypt(key, nonces[i], NULL, 0u,
                                                  plain, sizeof plain,
                                                  cts[i], tags[i]));
    }
    for (i = 0; i < ROUNDS; ++i) {
        for (j = i + 1; j < ROUNDS; ++j) {
            SDK_T_NE_MEM(t, nonces[i], nonces[j], sizeof nonces[0]);
            SDK_T_NE_MEM(t, cts[i], cts[j], sizeof cts[0]);
            SDK_T_NE_MEM(t, tags[i], tags[j], sizeof tags[0]);
            if (t->failed) {
                return;
            }
        }
    }
}

static void tc_xaead_rejects_oversized_input(sdk_test_ctx *t) {
    uint8_t key[SDK_XCHACHA20POLY1305_KEY_LEN] = {0};
    uint8_t nonce[SDK_XCHACHA20POLY1305_NONCE_LEN] = {0};
    uint8_t tag[SDK_XCHACHA20POLY1305_TAG_LEN];
    uint8_t small[1];

    /* Length beyond the canonical vault ciphertext limit must be refused
     * before any allocation or keystream work. */
    SDK_T_EQ_ST(t, SDK_ERR_LIMIT,
                sdk_xchacha20poly1305_encrypt(key, nonce, NULL, 0u, small,
                                              (size_t)SDK_LIMIT_VAULT_CIPHERTEXT
                                              + 1u, small, tag));
}

/* ------------------------------------------------------------------ */
/* Constant-time comparison and wiping                                 */
/* ------------------------------------------------------------------ */

static void tc_ct_equal_semantics(sdk_test_ctx *t) {
    uint8_t a[16], b[16];
    size_t i;

    memset(a, 0x11, sizeof a);
    memcpy(b, a, sizeof b);
    SDK_T_EQ_I(t, 1, sdk_ct_equal(a, b, sizeof a));
    /* A difference in any single position, including the last, must be seen;
     * an early-exit comparison that stops at the first byte would pass the
     * first case and fail here (docs/08 section 24). */
    for (i = 0; i < sizeof a; ++i) {
        b[i] ^= 0x01u;
        SDK_T_EQ_I(t, 0, sdk_ct_equal(a, b, sizeof a));
        b[i] ^= 0x01u;
    }
    SDK_T_EQ_I(t, 1, sdk_ct_equal(a, b, 0u));
}

static void tc_secure_wipe_clears_buffer(sdk_test_ctx *t) {
    uint8_t buf[64];
    uint8_t zero[64];
    size_t i;

    for (i = 0; i < sizeof buf; ++i) {
        buf[i] = (uint8_t)(i + 1u);
    }
    memset(zero, 0, sizeof zero);
    sdk_secure_wipe(buf, sizeof buf);
    SDK_T_EQ_MEM(t, zero, buf, sizeof buf);
}

/* ------------------------------------------------------------------ */
/* CSPRNG                                                             */
/* ------------------------------------------------------------------ */

static void tc_random_bytes_is_csprng(sdk_test_ctx *t) {
    /* docs/26 section 16: BCryptGenRandom is the only permitted RNG and there
     * is no weak fallback.  This is a smoke test for liveness, not a
     * statistical certification: repeated 32-byte draws must differ and must
     * not be all-zero. */
    enum { ROUNDS = 32 };
    uint8_t draws[ROUNDS][32];
    uint8_t zero[32];
    int i, j;

    memset(zero, 0, sizeof zero);
    for (i = 0; i < ROUNDS; ++i) {
        SDK_T_OK(t, sdk_random_bytes(draws[i], sizeof draws[i]));
        SDK_T_NE_MEM(t, zero, draws[i], sizeof zero);
    }
    for (i = 0; i < ROUNDS; ++i) {
        for (j = i + 1; j < ROUNDS; ++j) {
            SDK_T_NE_MEM(t, draws[i], draws[j], sizeof draws[0]);
        }
    }
    /* Zero-length request is a no-op success so callers need no special case. */
    SDK_T_OK(t, sdk_random_bytes(draws[0], 0u));
}

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

static void register_all(void) {
    sdk_test_add("unit.crypto.sha256.published_vectors", "SEC-01,SEC-02,VCS-02",
                 tc_sha256_published_vectors);
    sdk_test_add("unit.crypto.sha256.one_million_a", "SEC-01,VCS-02",
                 tc_sha256_one_million_a);
    sdk_test_add("unit.crypto.sha256.incremental_matches_oneshot",
                 "SEC-01,VCS-02", tc_sha256_incremental_matches_oneshot);

    sdk_test_add("unit.crypto.hmac_sha256.rfc4231_cases", "SEC-01",
                 tc_hmac_sha256_rfc4231);
    sdk_test_add("unit.crypto.hmac_sha256.key_length_boundaries", "SEC-01",
                 tc_hmac_sha256_boundary_keys);
    sdk_test_add("unit.crypto.hmac_sha256.incremental_update", "SEC-01",
                 tc_hmac_sha256_incremental);
    sdk_test_add("unit.crypto.hmac_sha256.precomputed_key_form", "SEC-01",
                 tc_hmac_sha256_precomputed_key_matches);

    sdk_test_add("unit.crypto.pbkdf2.rfc7914_vectors", "SEC-01",
                 tc_pbkdf2_rfc7914_vectors);
    sdk_test_add("unit.crypto.pbkdf2.iteration_progression", "SEC-01",
                 tc_pbkdf2_iteration_progression);
    sdk_test_add("unit.crypto.pbkdf2.partial_final_block", "SEC-01",
                 tc_pbkdf2_partial_final_block);
    sdk_test_add("unit.crypto.pbkdf2.embedded_nul_not_truncated", "SEC-01",
                 tc_pbkdf2_embedded_nul);
    sdk_test_add("unit.crypto.pbkdf2.first_block_equals_hmac", "SEC-01",
                 tc_pbkdf2_first_block_equals_hmac);
    sdk_test_add("unit.crypto.pbkdf2.production_iteration_count", "SEC-01",
                 tc_pbkdf2_production_iteration_count);
    sdk_test_add("unit.crypto.pbkdf2.rejects_bad_arguments", "SEC-01,FMT-07",
                 tc_pbkdf2_rejects_bad_arguments);

    sdk_test_add("unit.crypto.chacha20.block_function_rfc8439", "SEC-02",
                 tc_chacha20_block_function_rfc8439);
    sdk_test_add("unit.crypto.chacha20.stream_rfc8439", "SEC-02",
                 tc_chacha20_stream_rfc8439);
    sdk_test_add("unit.crypto.chacha20.poly1305_key_generation", "SEC-02",
                 tc_chacha20_poly1305_key_generation_rfc8439);
    sdk_test_add("unit.crypto.chacha20.chunked_matches_contiguous", "SEC-02",
                 tc_chacha20_chunked_matches_contiguous);

    sdk_test_add("unit.crypto.hchacha20.draft_vector", "SEC-02",
                 tc_hchacha20_draft_vector);
    sdk_test_add("unit.crypto.xchacha20.keystream_draft_vector", "SEC-02",
                 tc_xchacha20_keystream_draft_vector);

    sdk_test_add("unit.crypto.poly1305.rfc8439_2_5_2", "SEC-02",
                 tc_poly1305_rfc8439_section_2_5_2);
    sdk_test_add("unit.crypto.poly1305.rfc8439_appendix_a3", "SEC-02",
                 tc_poly1305_rfc8439_appendix_a3);
    sdk_test_add("unit.crypto.poly1305.incremental_matches_oneshot", "SEC-02",
                 tc_poly1305_incremental_matches_oneshot);
    sdk_test_add("unit.crypto.poly1305.empty_message", "SEC-02",
                 tc_poly1305_empty_message);

    sdk_test_add("unit.crypto.aead.framing_matches_rfc8439_2_8_2", "SEC-02",
                 tc_aead_framing_matches_rfc8439_2_8_2);
    sdk_test_add("unit.crypto.xaead.draft_vector_encrypt", "SEC-02",
                 tc_xaead_draft_vector_encrypt);
    sdk_test_add("unit.crypto.xaead.draft_vector_decrypt", "SEC-02",
                 tc_xaead_draft_vector_decrypt);
    sdk_test_add("unit.crypto.xaead.poly_key_is_derived", "SEC-02,SEC-06",
                 tc_xaead_poly_key_is_derived_not_reused);
    sdk_test_add("unit.crypto.xaead.round_trip_lengths", "SEC-02",
                 tc_xaead_round_trip_lengths);
    sdk_test_add("unit.crypto.xaead.rejects_wrong_key", "SEC-02,SEC-10",
                 tc_xaead_rejects_wrong_key);
    sdk_test_add("unit.crypto.xaead.rejects_modified_nonce", "SEC-02,SEC-10",
                 tc_xaead_rejects_modified_nonce);
    sdk_test_add("unit.crypto.xaead.rejects_modified_aad", "SEC-02,SEC-10",
                 tc_xaead_rejects_modified_aad);
    sdk_test_add("unit.crypto.xaead.rejects_modified_ciphertext",
                 "SEC-02,SEC-10", tc_xaead_rejects_modified_ciphertext);
    sdk_test_add("unit.crypto.xaead.rejects_modified_tag", "SEC-02,SEC-10",
                 tc_xaead_rejects_modified_tag);
    sdk_test_add("unit.crypto.xaead.rejects_truncated_ciphertext",
                 "SEC-02,SEC-03,FMT-07", tc_xaead_rejects_truncated_ciphertext);
    sdk_test_add("unit.crypto.xaead.fresh_nonce_changes_output",
                 "SEC-02,WIN-13", tc_xaead_fresh_nonce_changes_output);
    sdk_test_add("unit.crypto.xaead.rejects_oversized_input",
                 "SEC-03,FMT-07", tc_xaead_rejects_oversized_input);

    sdk_test_add("unit.crypto.constant_time.tag_comparison", "SEC-10",
                 tc_ct_equal_semantics);
    sdk_test_add("unit.crypto.secure_wipe.clears_buffer", "SEC-06",
                 tc_secure_wipe_clears_buffer);
    sdk_test_add("unit.crypto.csprng.bcrypt_gen_random", "WIN-13",
                 tc_random_bytes_is_csprng);
}

int wmain(int argc, wchar_t **argv) {
    return sdk_test_main("unit-crypto", register_all, argc, argv);
}
