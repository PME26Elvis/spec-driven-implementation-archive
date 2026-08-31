/* sdk_test.h - shared C17 test harness for every suite in this project.
 *
 * Normative source: docs/10_TESTING_AND_EVIDENCE.md sections 3, 14, 15, 21, 22
 * and docs/19_CANONICAL_FORMATS_AND_LIMITS.md section 26 (test result JSON).
 *
 * Capabilities required by docs/10 section 3 and provided here:
 *   - named test registration                       sdk_test_add
 *   - equal / not-equal assertions                  SDK_T_EQ_*  SDK_T_NE_*
 *   - memory / byte comparison                      SDK_T_EQ_MEM
 *   - expected failure                              SDK_T_ERR, sdk_test_add_xfail
 *   - temporary directory fixture                   sdk_test_tempdir
 *   - per test pass/fail and suite summary          text + JSON writers
 *   - non-zero process exit status on failure       sdk_test_main
 *   - human readable and JSON output                --text / --json
 *
 * Every case must reference at least one requirement ID from
 * docs/17_ACCEPTANCE_REQUIREMENT_CATALOG.md; the registration API makes the ID
 * list mandatory so a case cannot be added without traceability.
 */
#ifndef SDK_TEST_H
#define SDK_TEST_H

#include <stddef.h>
#include <stdint.h>

#include "common/sdk_common.h"

typedef struct sdk_test_case sdk_test_case;

/* Per-case execution context handed to the test body. */
typedef struct sdk_test_ctx {
    sdk_test_case *tc;          /* case being executed                       */
    unsigned       assertions;  /* assertions evaluated so far               */
    int            failed;      /* set once the first assertion fails        */
    int            skipped;     /* set by sdk_test_skip                      */
    wchar_t       *tempdir;     /* lazily created, owned by the harness      */
} sdk_test_ctx;

typedef void (*sdk_test_fn)(sdk_test_ctx *t);

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

/* name             stable dotted behaviour name, e.g.
 *                  "unit.crypto.sha256.nist_vectors"
 * requirement_ids  comma separated catalog IDs, e.g. "SEC-03,SEC-11"
 */
void sdk_test_add(const char *name, const char *requirement_ids, sdk_test_fn fn);

/* Registers a case that is expected to report at least one failing assertion.
 * A run with no failing assertion is reported as a suite failure
 * ("unexpected pass"), which prevents a silently disabled negative test. */
void sdk_test_add_xfail(const char *name, const char *requirement_ids,
                        sdk_test_fn fn);

/* Attaches an evidence path (screenshot, recording, report) to the running
 * case; the path is emitted in the JSON result. */
void sdk_test_evidence(sdk_test_ctx *t, const char *path);

/* Marks the running case as skipped with a reason.  Skipping a case that the
 * canonical scenario list requires is a release-gate failure, so the reason is
 * always printed and counted. */
void sdk_test_skip(sdk_test_ctx *t, const char *reason);

/* ------------------------------------------------------------------ */
/* Assertion plumbing                                                  */
/* ------------------------------------------------------------------ */

/* Records one assertion.  ok==0 marks the case failed and stores the first
 * failure message together with file and line. */
void sdk_test_assert_report(sdk_test_ctx *t, int ok, const char *file, int line,
                            const char *fmt, ...);

#define SDK_T_TRUE(t, cond)                                                   \
    sdk_test_assert_report((t), (cond) ? 1 : 0, __FILE__, __LINE__,           \
                           "expected true: %s", #cond)

#define SDK_T_FALSE(t, cond)                                                  \
    sdk_test_assert_report((t), (cond) ? 0 : 1, __FILE__, __LINE__,           \
                           "expected false: %s", #cond)

#define SDK_T_EQ_I(t, expected, actual)                                       \
    do {                                                                      \
        long long sdk__e = (long long)(expected);                             \
        long long sdk__a = (long long)(actual);                               \
        sdk_test_assert_report((t), sdk__e == sdk__a, __FILE__, __LINE__,     \
                               "%s == %s: expected %lld, got %lld",           \
                               #expected, #actual, sdk__e, sdk__a);           \
    } while (0)

#define SDK_T_NE_I(t, unexpected, actual)                                     \
    do {                                                                      \
        long long sdk__e = (long long)(unexpected);                           \
        long long sdk__a = (long long)(actual);                               \
        sdk_test_assert_report((t), sdk__e != sdk__a, __FILE__, __LINE__,     \
                               "%s != %s: both are %lld",                     \
                               #unexpected, #actual, sdk__a);                 \
    } while (0)

#define SDK_T_EQ_U(t, expected, actual)                                       \
    do {                                                                      \
        unsigned long long sdk__e = (unsigned long long)(expected);           \
        unsigned long long sdk__a = (unsigned long long)(actual);             \
        sdk_test_assert_report((t), sdk__e == sdk__a, __FILE__, __LINE__,     \
                               "%s == %s: expected %llu, got %llu",           \
                               #expected, #actual, sdk__e, sdk__a);           \
    } while (0)

/* Status comparison prints the symbolic names so a failure is readable. */
#define SDK_T_EQ_ST(t, expected, actual)                                      \
    do {                                                                      \
        sdk_status sdk__e = (expected);                                       \
        sdk_status sdk__a = (actual);                                         \
        sdk_test_assert_report((t), sdk__e == sdk__a, __FILE__, __LINE__,     \
                               "status: expected %s, got %s",                 \
                               sdk_status_name(sdk__e),                       \
                               sdk_status_name(sdk__a));                      \
    } while (0)

/* Asserts that a status is any non-OK value (expected failure form). */
#define SDK_T_ERR(t, actual)                                                  \
    do {                                                                      \
        sdk_status sdk__a = (actual);                                         \
        sdk_test_assert_report((t), sdk__a != SDK_OK, __FILE__, __LINE__,     \
                               "expected an error status, got %s",            \
                               sdk_status_name(sdk__a));                      \
    } while (0)

#define SDK_T_OK(t, actual) SDK_T_EQ_ST((t), SDK_OK, (actual))

/* String comparison; NULL is handled explicitly rather than crashing. */
void sdk_test_eq_str(sdk_test_ctx *t, const char *file, int line,
                     const char *expected, const char *actual,
                     const char *expr);
#define SDK_T_EQ_STR(t, expected, actual)                                     \
    sdk_test_eq_str((t), __FILE__, __LINE__, (expected), (actual), #actual)

/* Byte comparison; on mismatch the first differing offset and a bounded hex
 * window of both buffers are reported. */
void sdk_test_eq_mem(sdk_test_ctx *t, const char *file, int line,
                     const void *expected, const void *actual, size_t n,
                     const char *expr);
#define SDK_T_EQ_MEM(t, expected, actual, n)                                  \
    sdk_test_eq_mem((t), __FILE__, __LINE__, (expected), (actual), (n), #actual)

void sdk_test_ne_mem(sdk_test_ctx *t, const char *file, int line,
                     const void *a, const void *b, size_t n, const char *expr);
#define SDK_T_NE_MEM(t, a, b, n)                                              \
    sdk_test_ne_mem((t), __FILE__, __LINE__, (a), (b), (n), #a " vs " #b)

/* Compares a byte buffer against a lowercase hex literal.  This is the primary
 * form for cryptographic known-answer tests: the expected value is a fixed
 * published constant, never recomputed by the code under test. */
void sdk_test_eq_hex(sdk_test_ctx *t, const char *file, int line,
                     const char *expected_hex, const void *actual, size_t n,
                     const char *expr);
#define SDK_T_EQ_HEX(t, expected_hex, actual, n)                              \
    sdk_test_eq_hex((t), __FILE__, __LINE__, (expected_hex), (actual), (n),   \
                    #actual)

/* ------------------------------------------------------------------ */
/* Fixtures                                                            */
/* ------------------------------------------------------------------ */

/* Returns a per-case temporary directory (created on first call).  The root is
 * <repo>\build\tmp by default so that no test writes outside the work volume;
 * --temp-root overrides it.  Directories of passing cases are removed, and
 * directories of failing cases are kept and printed (docs/10 section 15). */
const wchar_t *sdk_test_tempdir(sdk_test_ctx *t);

/* Absolute path of the repository root, discovered by walking up from the
 * executable location looking for build.cmd + docs.  Used to locate fixtures. */
const wchar_t *sdk_test_repo_root(void);

/* Joins repo_root + relative UTF-8 path, returning a heap wide string the
 * caller frees with free(). */
wchar_t *sdk_test_repo_path(const char *relative_utf8);

/* ------------------------------------------------------------------ */
/* Suite entry point                                                   */
/* ------------------------------------------------------------------ */

/* register_all is called once to populate the case table.  Recognised options:
 *   --json <path>    write the docs/19 section 26 result document
 *   --text <path>    write the human readable report (also printed to stdout)
 *   --filter <sub>   run only cases whose name contains <sub>
 *   --list           list registered case names and exit 0
 *   --temp-root <p>  override the temporary fixture root
 *   --quiet          suppress per-case stdout lines
 * Returns 0 when every executed case passed, 1 otherwise. */
int sdk_test_main(const char *suite_name, void (*register_all)(void),
                  int argc, wchar_t **argv);

/* Suite-level extra options are made available to suites that need them
 * (for example the E2E runner's --app). Returns NULL when absent. */
const wchar_t *sdk_test_option(const wchar_t *name);

#endif /* SDK_TEST_H */
