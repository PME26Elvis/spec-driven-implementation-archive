/* test_locstat.c - unit tests for the locstat line-counting tool.
 *
 * Covers docs/03_DEV_TOOL_LOCSTAT.md section 11:
 *   empty file, single line without trailing newline, LF vs CRLF, blank/comment
 *   mix, comment symbols inside strings, nested dirs + ignore patterns,
 *   .tinyvcs exclusion, JSON config parse error, reparse point + root reparse
 *   resolution, unreadable path, stable ordering across two identical scans.
 *
 * Entry point: wmain -> sdk_test_main("unit-locstat", register_all, ...).
 */
#include "locstat_core.h"
#include "test/sdk_test.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "common/sdk_common.h"
#include "common/sdk_win.h"

/* ------------------------------------------------------------------ */
/* Filesystem fixtures helpers                                         */
/* ------------------------------------------------------------------ */

static wchar_t *join_rel(const wchar_t *base, const char *rel_utf8) {
    wchar_t *wrel = sdk_utf8_to_utf16(rel_utf8, strlen(rel_utf8), NULL);
    size_t i;
    wchar_t *out;
    if (wrel == NULL) {
        return NULL;
    }
    for (i = 0; wrel[i] != L'\0'; ++i) {
        if (wrel[i] == L'/') {
            wrel[i] = L'\\';
        }
    }
    out = sdk_wpath_join(base, wrel);
    free(wrel);
    return out;
}

/* Creates parent directories then writes content. */
static int write_tree_file(const wchar_t *base, const char *rel,
                           const char *content) {
    wchar_t *full = join_rel(base, rel);
    size_t len;
    int rc;
    wchar_t *parent;
    if (full == NULL) {
        return 0;
    }
    len = wcslen(full);
    while (len > 0 && full[len - 1] != L'\\' && full[len - 1] != L'/') {
        --len;
    }
    parent = (wchar_t *)malloc((len + 1u) * sizeof *parent);
    if (parent == NULL) {
        free(full);
        return 0;
    }
    memcpy(parent, full, len * sizeof *parent);
    parent[len] = L'\0';
    (void)sdk_mkdir_parents_w(parent, NULL);
    free(parent);
    rc = (sdk_file_write_all_w(full, (const void *)content, strlen(content),
                               NULL) == SDK_OK);
    free(full);
    return rc;
}

static int make_tree_dir(const wchar_t *base, const char *rel) {
    wchar_t *full = join_rel(base, rel);
    int rc;
    if (full == NULL) {
        return 0;
    }
    rc = (sdk_mkdir_parents_w(full, NULL) == SDK_OK);
    free(full);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Pure lexical / physical-line tests                                  */
/* ------------------------------------------------------------------ */

static void test_empty_file(sdk_test_ctx *t) {
    loc_lexical lex = loc_analyze_c(NULL, 0);
    SDK_T_EQ_U(t, 0u, loc_physical_lines(NULL, 0));
    SDK_T_EQ_U(t, 0u, lex.physical_lines);
    SDK_T_EQ_U(t, 0u, lex.blank_lines);
    SDK_T_EQ_U(t, 0u, lex.comment_only_lines);
    SDK_T_EQ_U(t, 0u, lex.code_lines);
    SDK_T_EQ_U(t, 0u, lex.mixed_code_comment_lines);
}

static void test_single_line_no_newline(sdk_test_ctx *t) {
    const char *src = "int x = 1;";
    loc_lexical lex = loc_analyze_c((const uint8_t *)src, strlen(src));
    SDK_T_EQ_U(t, 1u, loc_physical_lines((const uint8_t *)src, strlen(src)));
    SDK_T_EQ_U(t, 1u, lex.code_lines);
    SDK_T_EQ_U(t, 0u, lex.blank_lines);
    SDK_T_EQ_U(t, 0u, lex.comment_only_lines);
}

static void test_lf_vs_crlf(sdk_test_ctx *t) {
    const char *lf = "a\nb\nc\n";
    const char *crlf = "a\r\nb\r\nc\r\n";
    const char *mixed = "a\nb\r\nc";
    const char *cronly = "a\rb\rc";
    const char *crlf_noclose = "a\r\nb"; /* CRLF not at very end counts as 2 */
    SDK_T_EQ_U(t, 3u, loc_physical_lines((const uint8_t *)lf, strlen(lf)));
    SDK_T_EQ_U(t, 3u, loc_physical_lines((const uint8_t *)crlf, strlen(crlf)));
    /* A CRLF pair is one terminator; final byte not a terminator adds one. */
    SDK_T_EQ_U(t, 3u, loc_physical_lines((const uint8_t *)mixed, strlen(mixed)));
    SDK_T_EQ_U(t, 3u, loc_physical_lines((const uint8_t *)cronly, strlen(cronly)));
    SDK_T_EQ_U(t, 2u,
              loc_physical_lines((const uint8_t *)crlf_noclose,
                                 strlen(crlf_noclose)));
}

static void test_blank_comment_mix(sdk_test_ctx *t) {
    /* blank, comment_only, code, mixed (code then //), block only, block+mixed */
    const char *src =
        "\n"
        "// comment only\n"
        "int x = 1;\n"
        "int y = 2; // trailing comment\n"
        "/* block only */\n"
        "int z = 3; /* inline */\n"
        "/* unterminated at end\n"
        "still comment\n";
    loc_lexical lex = loc_analyze_c((const uint8_t *)src, strlen(src));
    SDK_T_EQ_U(t, 1u, lex.blank_lines);
    SDK_T_EQ_U(t, 4u, lex.comment_only_lines); /* // c, block only, +2 in block */
    SDK_T_EQ_U(t, 1u, lex.code_lines);         /* int x */
    SDK_T_EQ_U(t, 2u, lex.mixed_code_comment_lines); /* y, z */
}

static void test_comment_in_strings(sdk_test_ctx *t) {
    const char *src =
        "char *s = \"// not a comment\";\n"
        "char c = '\"';\n"
        "char q = '\\'';\n"
        "url = \"http://example.com\"; /* real comment */\n";
    loc_lexical lex = loc_analyze_c((const uint8_t *)src, strlen(src));
    /* No comment_only lines: the // inside the string is not a comment. The
     * only real comment is the trailing one on the last line, making that line
     * mixed, not comment_only. */
    SDK_T_EQ_U(t, 0u, lex.comment_only_lines);
    SDK_T_EQ_U(t, 3u, lex.code_lines); /* 3 pure-code lines */
    SDK_T_EQ_U(t, 1u, lex.mixed_code_comment_lines);
}

/* ------------------------------------------------------------------ */
/* Config parser tests                                                 */
/* ------------------------------------------------------------------ */

static void test_json_config_parse_error(sdk_test_ctx *t) {
    loc_config cfg;
    loc_json_err err;
    int rc;

    /* malformed JSON: trailing comma */
    rc = loc_config_parse("{\n  \"max_file_bytes\": 10,\n", 26, &cfg, &err);
    SDK_T_NE_I(t, SDK_OK, rc);
    SDK_T_TRUE(t, err.line > 0 || err.column > 0 || err.msg[0] != '\0');

    /* unknown key */
    {
        const char *bad = "{\"bogus_key\": 1}";
        loc_config c2;
        rc = loc_config_parse(bad, strlen(bad), &c2, &err);
        SDK_T_NE_I(t, SDK_OK, rc);
        loc_config_free(&c2);
    }

    /* follow_reparse_points = true is unsupported */
    {
        const char *bad = "{\"follow_reparse_points\": true}";
        loc_config c2;
        memset(&err, 0, sizeof err);
        rc = loc_config_parse(bad, strlen(bad), &c2, &err);
        SDK_T_NE_I(t, SDK_OK, rc);
        loc_config_free(&c2);
    }

    /* max_file_bytes out of range */
    {
        const char *bad = "{\"max_file_bytes\": 0}";
        loc_config c2;
        memset(&err, 0, sizeof err);
        rc = loc_config_parse(bad, strlen(bad), &c2, &err);
        SDK_T_NE_I(t, SDK_OK, rc);
        loc_config_free(&c2);
    }

    /* valid config parses */
    {
        const char *good =
            "{\"include_extensions\":[\".c\"],\"exclude_paths\":[\"build/**\"],"
            "\"categories\":{\"source\":[\".c\"]},\"max_file_bytes\":1024,"
            "\"follow_reparse_points\":false}";
        loc_config c2;
        memset(&err, 0, sizeof err);
        rc = loc_config_parse(good, strlen(good), &c2, &err);
        SDK_T_EQ_I(t, SDK_OK, rc);
        SDK_T_EQ_U(t, 1u, (unsigned long long)c2.include_count);
        SDK_T_EQ_U(t, 1u, (unsigned long long)c2.exclude_paths_count);
        SDK_T_EQ_U(t, 1u, (unsigned long long)c2.categories_count);
        SDK_T_EQ_U(t, 1024u, (unsigned long long)c2.max_file_bytes);
        loc_config_free(&c2);
    }
}

/* ------------------------------------------------------------------ */
/* Exclusion / category pure tests                                     */
/* ------------------------------------------------------------------ */

static void test_reparse_exclusion_pure(sdk_test_ctx *t) {
    loc_config cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.max_file_bytes = 67108864u;

    SDK_T_EQ_I(t, LOC_EXCLUDE_REPARSE,
               loc_entry_excluded(&cfg, 1, "link/x.c", "x.c", ".c", 0, 1, 10));
    SDK_T_EQ_I(t, LOC_EXCLUDE_REPARSE,
               loc_entry_excluded(&cfg, 1, "junction", "junction", "", 1, 1, 0));
    /* non-reparse file is not excluded */
    SDK_T_EQ_I(t, LOC_EXCLUDE_NONE,
               loc_entry_excluded(&cfg, 1, "src/x.c", "x.c", ".c", 0, 0, 10));
    loc_config_free(&cfg);
}

static void test_category_priority(sdk_test_ctx *t) {
    loc_config cfg;
    memset(&cfg, 0, sizeof cfg);
    {
        /* config defines a "source" category */
        cfg.categories = (loc_category_rule *)calloc(1, sizeof *cfg.categories);
        cfg.categories_count = 1;
        cfg.categories[0].name = _strdup("source");
        cfg.categories[0].extensions = (char **)malloc(sizeof(char *));
        cfg.categories[0].extensions[0] = _strdup(".c");
        cfg.categories[0].count = 1;
    }

    /* test path rule beats config source for .c/.h */
    SDK_T_EQ_STR(t, "tests",
                 loc_classify_category(&cfg, "tests/foo.c", "foo.c", ".c"));
    /* plain .c -> config source (priority 2) */
    SDK_T_EQ_STR(t, "source",
                 loc_classify_category(&cfg, "src/foo.c", "foo.c", ".c"));
    /* .md -> builtin docs */
    SDK_T_EQ_STR(t, "docs",
                 loc_classify_category(&cfg, "doc/readme.md", "readme.md",
                                      ".md"));
    /* .json -> builtin config */
    SDK_T_EQ_STR(t, "config",
                 loc_classify_category(&cfg, "cfg/a.json", "a.json", ".json"));
    /* unknown -> unclassified */
    SDK_T_EQ_STR(t, "unclassified",
                 loc_classify_category(&cfg, "x/odd.xyz", "odd.xyz", ".xyz"));

    loc_config_free(&cfg);
}

/* ------------------------------------------------------------------ */
/* Integration: nested dirs + ignore patterns + .tinyvcs                */
/* ------------------------------------------------------------------ */

static void test_nested_and_ignore(sdk_test_ctx *t) {
    const wchar_t *base = sdk_test_tempdir(t);
    wchar_t *cfg_path;
    loc_options opts;
    loc_report rep;
    size_t i;
    int saw_secret = 0, saw_ignored = 0, saw_log = 0, saw_main = 0;
    int rc;

    SDK_T_TRUE(t, base != NULL);

    /* Write the config file outside the scan root. */
    cfg_path = join_rel(base, "scanconfig.json");
    {
        const char *cfg =
            "{\n"
            "  \"include_extensions\": [],\n"
            "  \"exclude_extensions\": [\".log\"],\n"
            "  \"exclude_paths\": [\"secret/**\", \"ignored_dir/**\"],\n"
            "  \"categories\": {\n"
            "    \"source\": [\".c\", \".h\"],\n"
            "    \"docs\": [\".md\", \".txt\"]\n"
            "  },\n"
            "  \"max_file_bytes\": 67108864,\n"
            "  \"follow_reparse_points\": false\n"
            "}";
        sdk_file_write_all_w(cfg_path, cfg, strlen(cfg), NULL);
    }

    /* Build the scan root tree. */
    SDK_T_TRUE(t, make_tree_dir(base, "tree"));
    SDK_T_TRUE(t, write_tree_file(base, "tree/main.c", "int main(){}\n"));
    SDK_T_TRUE(t, write_tree_file(base, "tree/secret/key.c",
                                  "int secret(){}\n"));
    SDK_T_TRUE(t, write_tree_file(base, "tree/ignored_dir/notes.md",
                                  "# notes\n"));
    SDK_T_TRUE(t, write_tree_file(base, "tree/debug.log", "noise\n"));

    memset(&opts, 0, sizeof opts);
    opts.root = join_rel(base, "tree");
    opts.config_path = cfg_path;

    memset(&rep, 0, sizeof rep);
    loc_report_init(&rep);
    rc = loc_scan(&opts, &rep);
    SDK_T_EQ_I(t, SDK_EXIT_OK, rc);
    SDK_T_TRUE(t, rep.scanned);

    for (i = 0; i < rep.files_count; ++i) {
        const char *p = rep.files[i].path;
        if (strcmp(p, "main.c") == 0) {
            saw_main = 1;
        }
        if (strncmp(p, "secret/", 7) == 0) {
            saw_secret = 1;
        }
        if (strncmp(p, "ignored_dir/", 12) == 0) {
            saw_ignored = 1;
        }
        if (strcmp(p, "debug.log") == 0) {
            saw_log = 1;
        }
    }
    SDK_T_TRUE(t, saw_main);       /* included */
    SDK_T_FALSE(t, saw_secret);    /* excluded by config path */
    SDK_T_FALSE(t, saw_ignored);   /* excluded by config path */
    SDK_T_FALSE(t, saw_log);       /* excluded by config ext */

    /* The excluded[] list must record the reasons. */
    {
        int found_secret = 0, found_ignored = 0, found_log = 0;
        for (i = 0; i < rep.excluded_count; ++i) {
            const char *p = rep.excluded[i].path;
            const char *r = loc_exclude_reason_str(rep.excluded[i].reason);
            if (strncmp(p, "secret/", 7) == 0 &&
                strcmp(r, "config_path") == 0) {
                found_secret = 1;
            }
            if (strncmp(p, "ignored_dir/", 12) == 0 &&
                strcmp(r, "config_path") == 0) {
                found_ignored = 1;
            }
            if (strcmp(p, "debug.log") == 0 &&
                strcmp(r, "config_ext") == 0) {
                found_log = 1;
            }
        }
        SDK_T_TRUE(t, found_secret);
        SDK_T_TRUE(t, found_ignored);
        SDK_T_TRUE(t, found_log);
    }

    loc_report_free(&rep);
    free((void *)opts.root);
    free(cfg_path);
}

static void test_tinyvcs_exclusion(sdk_test_ctx *t) {
    const wchar_t *base = sdk_test_tempdir(t);
    loc_options opts;
    loc_report rep;
    size_t i;
    int saw_tinyvcs = 0, saw_src = 0;
    int rc;

    SDK_T_TRUE(t, base != NULL);
    SDK_T_TRUE(t, make_tree_dir(base, "proj/.tinyvcs/objects"));
    SDK_T_TRUE(t, write_tree_file(base, "proj/.tinyvcs/objects/aa/bbbbb",
                                 "blob data\n"));
    SDK_T_TRUE(t, write_tree_file(base, "proj/src/app.c", "int main(){}\n"));

    memset(&opts, 0, sizeof opts);
    opts.root = join_rel(base, "proj");
    /* default config + default excludes active (no --no-default-excludes) */

    memset(&rep, 0, sizeof rep);
    loc_report_init(&rep);
    rc = loc_scan(&opts, &rep);
    SDK_T_EQ_I(t, SDK_EXIT_OK, rc);
    SDK_T_TRUE(t, rep.scanned);

    for (i = 0; i < rep.files_count; ++i) {
        const char *p = rep.files[i].path;
        if (strncmp(p, ".tinyvcs/", 9) == 0) {
            saw_tinyvcs = 1;
        }
        if (strcmp(p, "src/app.c") == 0) {
            saw_src = 1;
        }
    }
    SDK_T_FALSE(t, saw_tinyvcs); /* .tinyvcs must be excluded */
    SDK_T_TRUE(t, saw_src);

    {
        int found = 0;
        for (i = 0; i < rep.excluded_count; ++i) {
            if (strncmp(rep.excluded[i].path, ".tinyvcs", 8) == 0) {
                found = 1;
            }
        }
        SDK_T_TRUE(t, found);
    }

    loc_report_free(&rep);
    free((void *)opts.root);
}

/* ------------------------------------------------------------------ */
/* Reparse point + root reparse resolution                             */
/* ------------------------------------------------------------------ */

static void test_reparse_point_and_root(sdk_test_ctx *t) {
    /* Pure exclusion (docs/19 section 4.2): reparse points are excluded and
     * reported with reason=reparse_point, never followed. */
    test_reparse_exclusion_pure(t);

    /* Root reparse resolution uses sdk_final_directory_path_w; on a normal
     * directory it resolves to the canonical path (exercises the same API the
     * scanner calls when the root itself is a reparse point). */
    {
        const wchar_t *base = sdk_test_tempdir(t);
        wchar_t *sub = join_rel(base, "reproot");
        wchar_t *resolved;
        SDK_T_TRUE(t, make_tree_dir(base, "reproot"));
        resolved = sdk_final_directory_path_w(sub);
        SDK_T_TRUE(t, resolved != NULL);
        free(resolved);
        free(sub);
    }
}

/* ------------------------------------------------------------------ */
/* Unreadable path                                                    */
/* ------------------------------------------------------------------ */

static void test_unreadable_path(sdk_test_ctx *t) {
    const wchar_t *base = sdk_test_tempdir(t);
    wchar_t *file_path;
    HANDLE h = INVALID_HANDLE_VALUE;
    loc_options opts;
    loc_report rep;
    int rc;
    size_t i;
    int saw_error = 0;

    SDK_T_TRUE(t, base != NULL);
    SDK_T_TRUE(t, make_tree_dir(base, "ur"));
    file_path = join_rel(base, "ur/locked.txt");
    SDK_T_TRUE(t, sdk_file_write_all_w(file_path, "data\n", 5, NULL) == SDK_OK);

    /* Open exclusively (no sharing) so the scanner's read fails. */
    h = CreateFileW(file_path, GENERIC_READ, 0, NULL, OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL, NULL);
    SDK_T_TRUE(t, h != INVALID_HANDLE_VALUE);

    /* Without --fail-on-error: scan succeeds, error recorded. */
    memset(&opts, 0, sizeof opts);
    opts.root = join_rel(base, "ur");
    memset(&rep, 0, sizeof rep);
    loc_report_init(&rep);
    rc = loc_scan(&opts, &rep);
    SDK_T_EQ_I(t, SDK_EXIT_OK, rc);
    for (i = 0; i < rep.errors_count; ++i) {
        if (strstr(rep.errors[i], "unreadable") != NULL) {
            saw_error = 1;
        }
    }
    SDK_T_TRUE(t, saw_error);
    loc_report_free(&rep);
    free((void *)opts.root);

    /* With --fail-on-error: scan fails with I/O error. */
    memset(&opts, 0, sizeof opts);
    opts.root = join_rel(base, "ur");
    opts.fail_on_error = 1;
    memset(&rep, 0, sizeof rep);
    loc_report_init(&rep);
    rc = loc_scan(&opts, &rep);
    SDK_T_EQ_I(t, SDK_EXIT_IO, rc);
    loc_report_free(&rep);
    free((void *)opts.root);

    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
    free(file_path);
}

/* ------------------------------------------------------------------ */
/* Stable ordering across two identical scans                          */
/* ------------------------------------------------------------------ */

static wchar_t *build_stable_tree(const wchar_t *base) {
    if (!make_tree_dir(base, "stable/src")) return NULL;
    if (!make_tree_dir(base, "stable/tests")) return NULL;
    if (!make_tree_dir(base, "stable/docs")) return NULL;
    write_tree_file(base, "stable/src/a.c", "int a(){}\n");
    write_tree_file(base, "stable/src/b.c", "int b(){}\n");
    write_tree_file(base, "stable/src/sub/c.c", "int c(){}\n");
    write_tree_file(base, "stable/tests/t1.c", "int t1(){}\n");
    write_tree_file(base, "stable/docs/readme.md", "# readme\n");
    return join_rel(base, "stable");
}

static void test_stable_ordering(sdk_test_ctx *t) {
    const wchar_t *base = sdk_test_tempdir(t);
    wchar_t *root;
    loc_options opts1, opts2;
    loc_report rep1, rep2;
    size_t i;

    SDK_T_TRUE(t, base != NULL);
    root = build_stable_tree(base);
    SDK_T_TRUE(t, root != NULL);

    memset(&opts1, 0, sizeof opts1);
    opts1.root = root;
    memset(&rep1, 0, sizeof rep1);
    loc_report_init(&rep1);
    SDK_T_EQ_I(t, SDK_EXIT_OK, loc_scan(&opts1, &rep1));

    memset(&opts2, 0, sizeof opts2);
    opts2.root = _wcsdup(root);
    memset(&rep2, 0, sizeof rep2);
    loc_report_init(&rep2);
    SDK_T_EQ_I(t, SDK_EXIT_OK, loc_scan(&opts2, &rep2));

    SDK_T_EQ_U(t, (unsigned long long)rep1.files_count,
               (unsigned long long)rep2.files_count);
    SDK_T_EQ_U(t, (unsigned long long)rep1.totals.physical_lines,
               (unsigned long long)rep2.totals.physical_lines);
    for (i = 0; i < rep1.files_count; ++i) {
        SDK_T_EQ_STR(t, rep1.files[i].path, rep2.files[i].path);
        SDK_T_EQ_STR(t, rep1.files[i].category, rep2.files[i].category);
    }

    loc_report_free(&rep1);
    loc_report_free(&rep2);
    /* opts1.root aliases `root`; free it exactly once. */
    free((void *)opts1.root);
    free((void *)opts2.root);
}

/* ------------------------------------------------------------------ */
/* Registration                                                       */
/* ------------------------------------------------------------------ */

static void register_all(void) {
    sdk_test_add("locstat.empty_file", "docs03-11-empty",
                 test_empty_file);
    sdk_test_add("locstat.single_line_no_newline", "docs03-11-single-line",
                 test_single_line_no_newline);
    sdk_test_add("locstat.lf_vs_crlf", "docs03-11-lf-crlf", test_lf_vs_crlf);
    sdk_test_add("locstat.blank_comment_mix", "docs03-11-blank-comment",
                 test_blank_comment_mix);
    sdk_test_add("locstat.comment_in_strings", "docs03-11-string-comment",
                 test_comment_in_strings);
    sdk_test_add("locstat.json_config_parse_error", "docs03-11-json-error",
                 test_json_config_parse_error);
    sdk_test_add("locstat.category_priority", "docs19-4.5-category",
                 test_category_priority);
    sdk_test_add("locstat.nested_and_ignore", "docs03-11-ignore",
                 test_nested_and_ignore);
    sdk_test_add("locstat.tinyvcs_exclusion", "docs03-11-tinyvcs",
                 test_tinyvcs_exclusion);
    sdk_test_add("locstat.reparse_point_and_root", "docs03-11-reparse",
                 test_reparse_point_and_root);
    sdk_test_add("locstat.unreadable_path", "docs03-11-unreadable",
                 test_unreadable_path);
    sdk_test_add("locstat.stable_ordering", "docs03-11-stable",
                 test_stable_ordering);
}

int wmain(int argc, wchar_t *argv[]) {
    return sdk_test_main("unit-locstat", register_all, argc, argv);
}
