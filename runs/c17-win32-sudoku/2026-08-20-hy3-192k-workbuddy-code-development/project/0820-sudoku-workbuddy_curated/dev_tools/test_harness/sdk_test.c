/* sdk_test.c - implementation of the shared C17 test harness.
 *
 * Result documents follow docs/19 section 26.  Summary counters are derived
 * from the case list, so the summary can never disagree with the detail
 * (docs/10 section 19 requires that consistency to be checkable).
 */
#include "test/sdk_test.h"
#include "common/sdk_win.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define SDK_TEST_MAX_CASES 4096
#define SDK_TEST_MAX_EVIDENCE 8
#define SDK_TEST_MSG_BYTES 1024

typedef enum sdk_test_status {
    SDK_TC_PASS = 0,
    SDK_TC_FAIL,
    SDK_TC_SKIP
} sdk_test_status;

struct sdk_test_case {
    const char     *name;
    const char     *requirement_ids;
    sdk_test_fn     fn;
    int             xfail;
    sdk_test_status status;
    unsigned        assertions;
    uint64_t        duration_ms;
    char            failure[SDK_TEST_MSG_BYTES];
    char           *evidence[SDK_TEST_MAX_EVIDENCE];
    size_t          evidence_count;
    wchar_t        *kept_tempdir;
};

static sdk_test_case g_cases[SDK_TEST_MAX_CASES];
static size_t        g_case_count;
static int           g_registration_overflow;

static const wchar_t *g_opt_json;
static const wchar_t *g_opt_text;
static const wchar_t *g_opt_filter;
static const wchar_t *g_opt_temp_root;
static int            g_quiet;

/* Extra options captured verbatim for suites that need their own switches. */
#define SDK_TEST_MAX_OPTS 32
static struct { const wchar_t *name; const wchar_t *value; } g_opts[SDK_TEST_MAX_OPTS];
static size_t g_opt_count;

static wchar_t *g_repo_root;
static wchar_t *g_temp_root;
static char    *g_source_commit;

/* ------------------------------------------------------------------ */
/* Registration                                                        */
/* ------------------------------------------------------------------ */

static void add_case(const char *name, const char *ids, sdk_test_fn fn, int xfail) {
    sdk_test_case *tc;
    size_t i;

    if (name == NULL || ids == NULL || ids[0] == '\0' || fn == NULL) {
        /* A case without a requirement ID would break traceability, so this is
         * treated as a hard registration error rather than being accepted. */
        g_registration_overflow = 1;
        return;
    }
    for (i = 0; i < g_case_count; ++i) {
        if (strcmp(g_cases[i].name, name) == 0) {
            g_registration_overflow = 1;   /* duplicate case name */
            return;
        }
    }
    if (g_case_count >= SDK_TEST_MAX_CASES) {
        g_registration_overflow = 1;
        return;
    }
    tc = &g_cases[g_case_count++];
    memset(tc, 0, sizeof *tc);
    tc->name = name;
    tc->requirement_ids = ids;
    tc->fn = fn;
    tc->xfail = xfail;
    tc->status = SDK_TC_PASS;
}

void sdk_test_add(const char *name, const char *requirement_ids, sdk_test_fn fn) {
    add_case(name, requirement_ids, fn, 0);
}

void sdk_test_add_xfail(const char *name, const char *requirement_ids,
                        sdk_test_fn fn) {
    add_case(name, requirement_ids, fn, 1);
}

void sdk_test_evidence(sdk_test_ctx *t, const char *path) {
    size_t n;
    char *copy;

    if (t == NULL || t->tc == NULL || path == NULL) {
        return;
    }
    if (t->tc->evidence_count >= SDK_TEST_MAX_EVIDENCE) {
        return;
    }
    n = strlen(path) + 1u;
    copy = (char *)malloc(n);
    if (copy == NULL) {
        return;
    }
    memcpy(copy, path, n);
    t->tc->evidence[t->tc->evidence_count++] = copy;
}

void sdk_test_skip(sdk_test_ctx *t, const char *reason) {
    if (t == NULL || t->tc == NULL) {
        return;
    }
    t->skipped = 1;
    snprintf(t->tc->failure, sizeof t->tc->failure, "skipped: %s",
             reason ? reason : "(no reason given)");
}

/* ------------------------------------------------------------------ */
/* Assertions                                                          */
/* ------------------------------------------------------------------ */

void sdk_test_assert_report(sdk_test_ctx *t, int ok, const char *file, int line,
                            const char *fmt, ...) {
    va_list ap;
    char detail[SDK_TEST_MSG_BYTES];

    if (t == NULL) {
        return;
    }
    ++t->assertions;
    if (ok) {
        return;
    }
    va_start(ap, fmt);
    vsnprintf(detail, sizeof detail, fmt, ap);
    va_end(ap);

    if (!t->failed) {
        const char *base = file;
        const char *p;
        for (p = file; *p != '\0'; ++p) {
            if (*p == '\\' || *p == '/') {
                base = p + 1;
            }
        }
        snprintf(t->tc->failure, sizeof t->tc->failure, "%s:%d: %s",
                 base, line, detail);
    }
    t->failed = 1;
}

void sdk_test_eq_str(sdk_test_ctx *t, const char *file, int line,
                     const char *expected, const char *actual,
                     const char *expr) {
    int ok;

    if (expected == NULL || actual == NULL) {
        ok = (expected == actual);
    } else {
        ok = (strcmp(expected, actual) == 0);
    }
    sdk_test_assert_report(t, ok, file, line,
                           "%s: expected \"%s\", got \"%s\"",
                           expr,
                           expected ? expected : "(null)",
                           actual ? actual : "(null)");
}

static void hex_window(char *out, size_t out_cap, const unsigned char *p,
                       size_t n, size_t from) {
    static const char digits[] = "0123456789abcdef";
    size_t i = 0;
    size_t written = 0;

    if (out_cap == 0) {
        return;
    }
    for (i = from; i < n && written + 3u < out_cap; ++i) {
        out[written++] = digits[(p[i] >> 4) & 0x0fu];
        out[written++] = digits[p[i] & 0x0fu];
    }
    out[written] = '\0';
}

void sdk_test_eq_mem(sdk_test_ctx *t, const char *file, int line,
                     const void *expected, const void *actual, size_t n,
                     const char *expr) {
    const unsigned char *e = (const unsigned char *)expected;
    const unsigned char *a = (const unsigned char *)actual;
    size_t i;

    if (e == NULL || a == NULL) {
        sdk_test_assert_report(t, e == a, file, line,
                               "%s: NULL buffer in comparison", expr);
        return;
    }
    for (i = 0; i < n; ++i) {
        if (e[i] != a[i]) {
            char ehex[97];
            char ahex[97];
            size_t from = (i > 8u) ? (i - 8u) : 0u;
            hex_window(ehex, sizeof ehex, e, n, from);
            hex_window(ahex, sizeof ahex, a, n, from);
            sdk_test_assert_report(t, 0, file, line,
                                   "%s: first difference at byte %zu "
                                   "(expected 0x%02x, got 0x%02x); "
                                   "expected[%zu..]=%s actual[%zu..]=%s",
                                   expr, i, e[i], a[i], from, ehex, from, ahex);
            return;
        }
    }
    sdk_test_assert_report(t, 1, file, line, "%s", expr);
}

void sdk_test_ne_mem(sdk_test_ctx *t, const char *file, int line,
                     const void *a, const void *b, size_t n, const char *expr) {
    int differs = 0;
    size_t i;
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;

    if (pa == NULL || pb == NULL) {
        sdk_test_assert_report(t, pa != pb, file, line,
                               "%s: NULL buffer in comparison", expr);
        return;
    }
    for (i = 0; i < n; ++i) {
        if (pa[i] != pb[i]) {
            differs = 1;
            break;
        }
    }
    sdk_test_assert_report(t, differs, file, line,
                           "%s: buffers are identical over %zu bytes", expr, n);
}

void sdk_test_eq_hex(sdk_test_ctx *t, const char *file, int line,
                     const char *expected_hex, const void *actual, size_t n,
                     const char *expr) {
    unsigned char *expected;

    if (expected_hex == NULL) {
        sdk_test_assert_report(t, 0, file, line, "%s: null expected hex", expr);
        return;
    }
    if (strlen(expected_hex) != n * 2u) {
        sdk_test_assert_report(t, 0, file, line,
                               "%s: expected hex length %zu does not match "
                               "%zu bytes", expr, strlen(expected_hex), n);
        return;
    }
    expected = (unsigned char *)malloc(n ? n : 1u);
    if (expected == NULL) {
        sdk_test_assert_report(t, 0, file, line, "%s: out of memory", expr);
        return;
    }
    if (!sdk_hex_decode(expected_hex, n * 2u, expected)) {
        sdk_test_assert_report(t, 0, file, line,
                               "%s: malformed expected hex literal", expr);
        free(expected);
        return;
    }
    sdk_test_eq_mem(t, file, line, expected, actual, n, expr);
    free(expected);
}

/* ------------------------------------------------------------------ */
/* Repository root and fixtures                                        */
/* ------------------------------------------------------------------ */

static int dir_has_marker(const wchar_t *dir) {
    wchar_t *probe;
    sdk_fileinfo fi;
    int ok = 0;

    probe = sdk_wpath_join(dir, L"build.cmd");
    if (probe != NULL) {
        if (sdk_stat_w(probe, &fi) == SDK_OK && fi.exists && !fi.is_directory) {
            ok = 1;
        }
        free(probe);
    }
    if (!ok) {
        return 0;
    }
    probe = sdk_wpath_join(dir, L"include");
    ok = 0;
    if (probe != NULL) {
        if (sdk_stat_w(probe, &fi) == SDK_OK && fi.exists && fi.is_directory) {
            ok = 1;
        }
        free(probe);
    }
    return ok;
}

/* Walks upward from "start" (which is consumed) looking for the repository
 * marker. Returns the matching directory on success (ownership transferred to
 * the caller) or NULL after freeing the working copy. */
static wchar_t *walk_up_for_marker(wchar_t *start) {
    size_t len;

    if (start == NULL) {
        return NULL;
    }
    for (;;) {
        if (dir_has_marker(start)) {
            return start;
        }
        len = wcslen(start);
        while (len > 0 && start[len - 1] != L'\\' && start[len - 1] != L'/') {
            --len;
        }
        if (len <= 1) {
            break;
        }
        start[len - 1] = L'\0';
        if (wcslen(start) < 3) {
            break;
        }
    }
    free(start);
    return NULL;
}

const wchar_t *sdk_test_repo_root(void) {
    wchar_t *found;

    if (g_repo_root != NULL) {
        return g_repo_root;
    }

    /* Start from the executable directory so the suite can be started from any
     * working directory, then fall back to the working directory itself. */
    found = walk_up_for_marker(sdk_module_dir_w());
    if (found == NULL) {
        found = walk_up_for_marker(sdk_getcwd_w());
    }
    if (found == NULL) {
        found = sdk_getcwd_w();
    }
    g_repo_root = found;
    return g_repo_root;
}

wchar_t *sdk_test_repo_path(const char *relative_utf8) {
    const wchar_t *root = sdk_test_repo_root();
    wchar_t *rel;
    wchar_t *joined;
    size_t i;

    if (root == NULL || relative_utf8 == NULL) {
        return NULL;
    }
    rel = sdk_utf8_to_utf16(relative_utf8, strlen(relative_utf8), NULL);
    if (rel == NULL) {
        return NULL;
    }
    for (i = 0; rel[i] != L'\0'; ++i) {
        if (rel[i] == L'/') {
            rel[i] = L'\\';
        }
    }
    joined = sdk_wpath_join(root, rel);
    free(rel);
    return joined;
}

static const wchar_t *temp_root(void) {
    if (g_temp_root != NULL) {
        return g_temp_root;
    }
    if (g_opt_temp_root != NULL) {
        g_temp_root = sdk_wcsdup_n(g_opt_temp_root, wcslen(g_opt_temp_root));
    } else {
        g_temp_root = sdk_test_repo_path("build/tmp");
    }
    if (g_temp_root != NULL) {
        (void)sdk_mkdir_parents_w(g_temp_root, NULL);
    }
    return g_temp_root;
}

const wchar_t *sdk_test_tempdir(sdk_test_ctx *t) {
    const wchar_t *root;
    wchar_t leaf[160];
    static unsigned counter;
    unsigned char rnd[6];
    char rndhex[13];

    if (t == NULL) {
        return NULL;
    }
    if (t->tempdir != NULL) {
        return t->tempdir;
    }
    root = temp_root();
    if (root == NULL) {
        return NULL;
    }
    if (sdk_random_bytes(rnd, sizeof rnd) != SDK_OK) {
        /* A fixture directory name is not a security value, but the project
         * forbids weak fallbacks for RNG-dependent operations, so failing here
         * is reported to the caller as "no fixture available". */
        return NULL;
    }
    sdk_hex_encode(rnd, sizeof rnd, rndhex);
    ++counter;
    _snwprintf_s(leaf, sizeof leaf / sizeof leaf[0], _TRUNCATE,
                 L"case_%04u_%hs", counter, rndhex);

    t->tempdir = sdk_wpath_join(root, leaf);
    if (t->tempdir == NULL) {
        return NULL;
    }
    if (sdk_mkdir_parents_w(t->tempdir, NULL) != SDK_OK) {
        free(t->tempdir);
        t->tempdir = NULL;
        return NULL;
    }
    return t->tempdir;
}

/* ------------------------------------------------------------------ */
/* Source commit discovery (docs/11 section 15)                        */
/* ------------------------------------------------------------------ */

static char *read_small_text(const wchar_t *path) {
    uint8_t *data = NULL;
    size_t len = 0;
    char *out;

    if (sdk_file_read_all_w(path, 64u * 1024u, &data, &len, NULL) != SDK_OK) {
        return NULL;
    }
    out = (char *)malloc(len + 1u);
    if (out == NULL) {
        free(data);
        return NULL;
    }
    memcpy(out, data, len);
    out[len] = '\0';
    free(data);
    return out;
}

static void trim_ws(char *s) {
    size_t n;
    size_t i = 0;

    if (s == NULL) {
        return;
    }
    n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t')) {
        s[--n] = '\0';
    }
    while (s[i] == ' ' || s[i] == '\t') {
        ++i;
    }
    if (i > 0) {
        memmove(s, s + i, n - i + 1u);
    }
}

static const char *source_commit(void) {
    wchar_t *head_path;
    char *head;
    char *ref;
    wchar_t *ref_path;
    char *commit;

    if (g_source_commit != NULL) {
        return g_source_commit;
    }
    /* Explicit override lets the release-gate scripts pin the commit. */
    g_source_commit = sdk_getenv_utf8(L"SDK_SOURCE_COMMIT");
    if (g_source_commit != NULL && g_source_commit[0] != '\0') {
        trim_ws(g_source_commit);
        return g_source_commit;
    }
    free(g_source_commit);
    g_source_commit = NULL;

    head_path = sdk_test_repo_path(".tinyvcs/HEAD");
    if (head_path == NULL) {
        goto unknown;
    }
    head = read_small_text(head_path);
    free(head_path);
    if (head == NULL) {
        goto unknown;
    }
    trim_ws(head);
    if (strncmp(head, "ref: ", 5) == 0) {
        char rel[512];
        ref = head + 5;
        snprintf(rel, sizeof rel, ".tinyvcs/%s", ref);
        ref_path = sdk_test_repo_path(rel);
        free(head);
        if (ref_path == NULL) {
            goto unknown;
        }
        commit = read_small_text(ref_path);
        free(ref_path);
        if (commit == NULL) {
            goto unknown;
        }
        trim_ws(commit);
        g_source_commit = commit;
        return g_source_commit;
    }
    g_source_commit = head;
    return g_source_commit;

unknown:
    g_source_commit = (char *)malloc(8u);
    if (g_source_commit != NULL) {
        memcpy(g_source_commit, "unknown", 8u);
    }
    return g_source_commit ? g_source_commit : "unknown";
}

/* ------------------------------------------------------------------ */
/* Reporting                                                           */
/* ------------------------------------------------------------------ */

static void json_escape(sdk_buf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");

    sdk_buf_append_u8(b, '"');
    for (; *p != '\0'; ++p) {
        switch (*p) {
        case '"':  sdk_buf_append(b, "\\\"", 2); break;
        case '\\': sdk_buf_append(b, "\\\\", 2); break;
        case '\n': sdk_buf_append(b, "\\n", 2); break;
        case '\r': sdk_buf_append(b, "\\r", 2); break;
        case '\t': sdk_buf_append(b, "\\t", 2); break;
        default:
            if (*p < 0x20u) {
                sdk_buf_appendf(b, "\\u%04x", (unsigned)*p);
            } else {
                sdk_buf_append_u8(b, *p);
            }
            break;
        }
    }
    sdk_buf_append_u8(b, '"');
}

/* Splits "A,B , C" into a JSON array of trimmed strings. */
static void json_id_array(sdk_buf *b, const char *ids) {
    const char *p = ids;
    int first = 1;

    sdk_buf_append_u8(b, '[');
    while (*p != '\0') {
        const char *start;
        const char *end;
        char tmp[128];
        size_t n;

        while (*p == ' ' || *p == ',' || *p == '\t') {
            ++p;
        }
        if (*p == '\0') {
            break;
        }
        start = p;
        while (*p != '\0' && *p != ',') {
            ++p;
        }
        end = p;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
            --end;
        }
        n = (size_t)(end - start);
        if (n >= sizeof tmp) {
            n = sizeof tmp - 1u;
        }
        memcpy(tmp, start, n);
        tmp[n] = '\0';
        if (!first) {
            sdk_buf_append_u8(b, ',');
        }
        json_escape(b, tmp);
        first = 0;
    }
    sdk_buf_append_u8(b, ']');
}

static const char *status_text(sdk_test_status s) {
    switch (s) {
    case SDK_TC_PASS: return "passed";
    case SDK_TC_FAIL: return "failed";
    case SDK_TC_SKIP: return "skipped";
    default:          return "unknown";
    }
}

static int write_utf8_file(const wchar_t *path, const sdk_buf *b) {
    wchar_t *full;
    sdk_status st;

    if (path == NULL) {
        return 1;
    }
    full = sdk_full_path_w(path);
    if (full == NULL) {
        return 0;
    }
    /* Ensure the parent directory exists so that a fresh checkout works. */
    {
        size_t len = wcslen(full);
        while (len > 0 && full[len - 1] != L'\\' && full[len - 1] != L'/') {
            --len;
        }
        if (len > 1) {
            wchar_t *parent = sdk_wcsdup_n(full, len - 1u);
            if (parent != NULL) {
                (void)sdk_mkdir_parents_w(parent, NULL);
                free(parent);
            }
        }
    }
    st = sdk_file_write_all_w(full, b->data, b->len, NULL);
    free(full);
    return st == SDK_OK;
}

/* ------------------------------------------------------------------ */
/* Option parsing                                                      */
/* ------------------------------------------------------------------ */

const wchar_t *sdk_test_option(const wchar_t *name) {
    size_t i;
    for (i = 0; i < g_opt_count; ++i) {
        if (wcscmp(g_opts[i].name, name) == 0) {
            return g_opts[i].value;
        }
    }
    return NULL;
}

int sdk_test_main(const char *suite_name, void (*register_all)(void),
                  int argc, wchar_t **argv) {
    size_t i;
    size_t total = 0, passed = 0, failed = 0, skipped = 0;
    uint64_t suite_start_mono;
    int64_t  suite_start_wall;
    uint64_t suite_ms;
    sdk_buf text;
    sdk_buf json;
    int list_only = 0;
    int exit_code;

    for (i = 1; i < (size_t)argc; ++i) {
        const wchar_t *a = argv[i];
        const wchar_t *val = ((i + 1u) < (size_t)argc) ? argv[i + 1] : NULL;

        if (wcscmp(a, L"--json") == 0 && val != NULL) {
            g_opt_json = val; ++i;
        } else if (wcscmp(a, L"--text") == 0 && val != NULL) {
            g_opt_text = val; ++i;
        } else if (wcscmp(a, L"--filter") == 0 && val != NULL) {
            g_opt_filter = val; ++i;
        } else if (wcscmp(a, L"--temp-root") == 0 && val != NULL) {
            g_opt_temp_root = val; ++i;
        } else if (wcscmp(a, L"--quiet") == 0) {
            g_quiet = 1;
        } else if (wcscmp(a, L"--list") == 0) {
            list_only = 1;
        } else if (a[0] == L'-' && a[1] == L'-' && val != NULL && val[0] != L'-') {
            if (g_opt_count < SDK_TEST_MAX_OPTS) {
                g_opts[g_opt_count].name = a;
                g_opts[g_opt_count].value = val;
                ++g_opt_count;
            }
            ++i;
        } else {
            fwprintf(stderr, L"[%hs] unknown option: %ls\n", suite_name, a);
            return SDK_EXIT_USAGE;
        }
    }

    register_all();

    if (g_registration_overflow) {
        fwprintf(stderr,
                 L"[%hs] test registration failed: duplicate name, missing "
                 L"requirement ID, or table overflow\n", suite_name);
        return SDK_EXIT_INTERNAL;
    }
    if (g_case_count == 0) {
        fwprintf(stderr, L"[%hs] no test cases registered\n", suite_name);
        return SDK_EXIT_INTERNAL;
    }

    if (list_only) {
        for (i = 0; i < g_case_count; ++i) {
            printf("%s [%s]\n", g_cases[i].name, g_cases[i].requirement_ids);
        }
        return SDK_EXIT_OK;
    }

    sdk_buf_init(&text);
    suite_start_wall = sdk_now_epoch_ms();
    suite_start_mono = sdk_monotonic_ms();

    sdk_buf_appendf(&text, "suite: %s\n", suite_name);
    sdk_buf_appendf(&text, "source_commit: %s\n", source_commit());
    sdk_buf_appendf(&text, "started_epoch_ms: %lld\n", (long long)suite_start_wall);
    sdk_buf_append_cstr(&text, "----------------------------------------"
                               "----------------------------------------\n");

    for (i = 0; i < g_case_count; ++i) {
        sdk_test_case *tc = &g_cases[i];
        sdk_test_ctx ctx;
        uint64_t t0;

        if (g_opt_filter != NULL) {
            char *needle = sdk_utf16_to_utf8(g_opt_filter,
                                             wcslen(g_opt_filter), NULL);
            int match = (needle != NULL && strstr(tc->name, needle) != NULL);
            free(needle);
            if (!match) {
                continue;
            }
        }

        memset(&ctx, 0, sizeof ctx);
        ctx.tc = tc;

        /* Fault injection state must never leak across cases. */
        sdk_fault_reset();

        t0 = sdk_monotonic_ms();
        tc->fn(&ctx);
        tc->duration_ms = sdk_monotonic_ms() - t0;
        tc->assertions = ctx.assertions;

        sdk_fault_reset();

        if (ctx.skipped) {
            tc->status = SDK_TC_SKIP;
        } else if (tc->xfail) {
            if (ctx.failed) {
                tc->status = SDK_TC_PASS;
                /* The recorded message is the expected failure detail; keep it
                 * for the report but do not treat it as a suite failure. */
            } else {
                tc->status = SDK_TC_FAIL;
                snprintf(tc->failure, sizeof tc->failure,
                         "expected-failure case passed unexpectedly");
            }
        } else {
            tc->status = ctx.failed ? SDK_TC_FAIL : SDK_TC_PASS;
        }

        if (ctx.tempdir != NULL) {
            if (tc->status == SDK_TC_FAIL) {
                tc->kept_tempdir = ctx.tempdir;   /* kept for diagnosis */
            } else {
                (void)sdk_remove_tree_w(ctx.tempdir);
                free(ctx.tempdir);
            }
            ctx.tempdir = NULL;
        }

        ++total;
        switch (tc->status) {
        case SDK_TC_PASS: ++passed; break;
        case SDK_TC_FAIL: ++failed; break;
        default:          ++skipped; break;
        }

        sdk_buf_appendf(&text, "%-7s %-70s %5llu asserts %6llums\n",
                        status_text(tc->status), tc->name,
                        (unsigned long long)tc->assertions,
                        (unsigned long long)tc->duration_ms);
        if (tc->status != SDK_TC_PASS && tc->failure[0] != '\0') {
            sdk_buf_appendf(&text, "        %s\n", tc->failure);
        }
        if (tc->kept_tempdir != NULL) {
            char *u8 = sdk_utf16_to_utf8(tc->kept_tempdir,
                                         wcslen(tc->kept_tempdir), NULL);
            sdk_buf_appendf(&text, "        fixture retained: %s\n",
                            u8 ? u8 : "(unprintable)");
            free(u8);
        }
        if (!g_quiet) {
            printf("%-7s %s\n", status_text(tc->status), tc->name);
            if (tc->status != SDK_TC_PASS && tc->failure[0] != '\0') {
                printf("        %s\n", tc->failure);
            }
            fflush(stdout);
        }
    }

    suite_ms = sdk_monotonic_ms() - suite_start_mono;
    exit_code = (failed == 0) ? SDK_EXIT_OK : 1;

    sdk_buf_append_cstr(&text, "----------------------------------------"
                               "----------------------------------------\n");
    sdk_buf_appendf(&text,
                    "total %zu  passed %zu  failed %zu  skipped %zu  "
                    "duration_ms %llu  exit_status %d\n",
                    total, passed, failed, skipped,
                    (unsigned long long)suite_ms, exit_code);

    /* JSON document, docs/19 section 26. */
    sdk_buf_init(&json);
    sdk_buf_append_cstr(&json, "{\n  \"schema_version\": 1,\n  \"suite\": ");
    json_escape(&json, suite_name);
    sdk_buf_append_cstr(&json, ",\n  \"source_commit\": ");
    json_escape(&json, source_commit());
    sdk_buf_appendf(&json, ",\n  \"started_epoch_ms\": %lld",
                    (long long)suite_start_wall);
    sdk_buf_appendf(&json, ",\n  \"duration_ms\": %llu",
                    (unsigned long long)suite_ms);
    sdk_buf_appendf(&json, ",\n  \"total\": %zu", total);
    sdk_buf_appendf(&json, ",\n  \"passed\": %zu", passed);
    sdk_buf_appendf(&json, ",\n  \"failed\": %zu", failed);
    sdk_buf_appendf(&json, ",\n  \"skipped\": %zu", skipped);
    sdk_buf_appendf(&json, ",\n  \"exit_status\": %d", exit_code);
    sdk_buf_append_cstr(&json, ",\n  \"cases\": [");
    {
        int first = 1;
        for (i = 0; i < g_case_count; ++i) {
            sdk_test_case *tc = &g_cases[i];
            size_t k;

            if (tc->duration_ms == 0 && tc->assertions == 0 &&
                tc->status == SDK_TC_PASS && g_opt_filter != NULL) {
                /* Not executed because of --filter; excluded from the document
                 * so the summary stays consistent with the case list. */
                char *needle = sdk_utf16_to_utf8(g_opt_filter,
                                                 wcslen(g_opt_filter), NULL);
                int match = (needle != NULL && strstr(tc->name, needle) != NULL);
                free(needle);
                if (!match) {
                    continue;
                }
            }
            if (!first) {
                sdk_buf_append_u8(&json, ',');
            }
            first = 0;
            sdk_buf_append_cstr(&json, "\n    {\n      \"name\": ");
            json_escape(&json, tc->name);
            sdk_buf_append_cstr(&json, ",\n      \"requirement_ids\": ");
            json_id_array(&json, tc->requirement_ids);
            sdk_buf_append_cstr(&json, ",\n      \"status\": ");
            json_escape(&json, status_text(tc->status));
            sdk_buf_appendf(&json, ",\n      \"duration_ms\": %llu",
                            (unsigned long long)tc->duration_ms);
            sdk_buf_appendf(&json, ",\n      \"assertion_count\": %llu",
                            (unsigned long long)tc->assertions);
            sdk_buf_append_cstr(&json, ",\n      \"failure_message\": ");
            if (tc->status == SDK_TC_PASS || tc->failure[0] == '\0') {
                sdk_buf_append_cstr(&json, "null");
            } else {
                json_escape(&json, tc->failure);
            }
            sdk_buf_append_cstr(&json, ",\n      \"evidence_paths\": [");
            for (k = 0; k < tc->evidence_count; ++k) {
                if (k > 0) {
                    sdk_buf_append_u8(&json, ',');
                }
                json_escape(&json, tc->evidence[k]);
            }
            sdk_buf_append_cstr(&json, "]\n    }");
        }
    }
    sdk_buf_append_cstr(&json, "\n  ]\n}\n");

    if (g_opt_text != NULL && !write_utf8_file(g_opt_text, &text)) {
        fwprintf(stderr, L"[%hs] failed to write text report\n", suite_name);
        exit_code = SDK_EXIT_IO;
    }
    if (g_opt_json != NULL && !write_utf8_file(g_opt_json, &json)) {
        fwprintf(stderr, L"[%hs] failed to write JSON report\n", suite_name);
        exit_code = SDK_EXIT_IO;
    }

    printf("\n%.*s", (int)text.len > 0 ? 0 : 0, "");
    printf("total %zu  passed %zu  failed %zu  skipped %zu  duration_ms %llu\n",
           total, passed, failed, skipped, (unsigned long long)suite_ms);

    /* Release resources so that leak checking of the harness itself is clean. */
    for (i = 0; i < g_case_count; ++i) {
        size_t k;
        for (k = 0; k < g_cases[i].evidence_count; ++k) {
            free(g_cases[i].evidence[k]);
        }
        free(g_cases[i].kept_tempdir);
    }
    sdk_buf_free(&text);
    sdk_buf_free(&json);
    free(g_repo_root);
    g_repo_root = NULL;
    free(g_temp_root);
    g_temp_root = NULL;
    free(g_source_commit);
    g_source_commit = NULL;

    return exit_code;
}
