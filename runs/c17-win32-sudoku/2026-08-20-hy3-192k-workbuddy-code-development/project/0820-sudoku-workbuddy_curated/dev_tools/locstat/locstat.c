/* locstat.c - CLI entry point for the locstat line-counting tool.
 *
 * Normative contracts: docs/03_DEV_TOOL_LOCSTAT.md and
 * docs/19_CANONICAL_FORMATS_AND_LIMITS.md sections 3-5, 13.
 *
 * Where the two disagree, docs/19 wins.
 */
#include "locstat_core.h"

#include <stdio.h>
#include <string.h>

#include "common/sdk_common.h"
#include "common/sdk_win.h"

static void print_usage(FILE *out) {
    fprintf(out,
        "locstat - count lines per category across a source tree\n"
        "\n"
        "usage: locstat [options] <root-path>\n"
        "\n"
        "options:\n"
        "  --config <path>        JSON configuration file (default: built-in)\n"
        "  --json <path>          also write a machine-readable JSON report\n"
        "                          (use '-' for stdout; summary goes to stderr)\n"
        "  --category <name>      only show the named category\n"
        "  --fail-on-error        non-zero exit when a file cannot be read\n"
        "  --no-default-excludes  disable the built-in exclude list\n"
        "  --help                 show this help and exit 0\n"
        "\n"
        "exit codes: 0 ok, 2 usage, 3 malformed config/data, 4 I/O,\n"
        "            5 verify, 70 internal\n");
}

/* Known category names for --category validation (docs/19 section 4.1). */
static int is_known_category(const wchar_t *name) {
    return (wcscmp(name, L"source") == 0 || wcscmp(name, L"tests") == 0 ||
            wcscmp(name, L"docs") == 0 || wcscmp(name, L"config") == 0);
}

int wmain(int argc, wchar_t *argv[]) {
    loc_options opts;
    loc_report rep;
    int rc = SDK_EXIT_OK;
    int have_root = 0;
    size_t i;

    memset(&opts, 0, sizeof opts);

    for (i = 1; i < (size_t)argc; ++i) {
        const wchar_t *a = argv[i];
        const wchar_t *val = ((i + 1u) < (size_t)argc) ? argv[i + 1] : NULL;

        if (wcscmp(a, L"--help") == 0) {
            print_usage(stdout);
            return SDK_EXIT_OK;
        }
        if (wcscmp(a, L"--config") == 0) {
            if (val == NULL) {
                fprintf(stderr, "[locstat] --config requires a value\n");
                print_usage(stderr);
                return SDK_EXIT_USAGE;
            }
            opts.config_path = val;
            ++i;
            continue;
        }
        if (wcscmp(a, L"--json") == 0) {
            if (val == NULL) {
                fprintf(stderr, "[locstat] --json requires a value\n");
                print_usage(stderr);
                return SDK_EXIT_USAGE;
            }
            opts.json_path = val;
            ++i;
            continue;
        }
        if (wcscmp(a, L"--category") == 0) {
            if (val == NULL) {
                fprintf(stderr, "[locstat] --category requires a value\n");
                print_usage(stderr);
                return SDK_EXIT_USAGE;
            }
            if (!is_known_category(val)) {
                fprintf(stderr, "[locstat] unknown --category: %ls\n", val);
                print_usage(stderr);
                return SDK_EXIT_USAGE;
            }
            {
                char *u8 = sdk_utf16_to_utf8(val, wcslen(val), NULL);
                opts.category_filter = u8; /* leaked on success; tiny */
            }
            ++i;
            continue;
        }
        if (wcscmp(a, L"--fail-on-error") == 0) {
            opts.fail_on_error = 1;
            continue;
        }
        if (wcscmp(a, L"--no-default-excludes") == 0) {
            opts.no_default_excludes = 1;
            continue;
        }
        if (a[0] == L'-' && a[1] == L'-') {
            fprintf(stderr, "[locstat] unknown option: %ls\n", a);
            print_usage(stderr);
            return SDK_EXIT_USAGE;
        }
        /* positional: ROOT (exactly one) */
        if (have_root) {
            fprintf(stderr, "[locstat] multiple roots given; exactly one "
                            "required\n");
            print_usage(stderr);
            return SDK_EXIT_USAGE;
        }
        opts.root = a;
        have_root = 1;
    }

    if (!have_root) {
        fprintf(stderr, "[locstat] missing root path argument\n");
        print_usage(stderr);
        return SDK_EXIT_USAGE;
    }

    loc_report_init(&rep);
    rc = loc_scan(&opts, &rep);
    if (rc != SDK_EXIT_OK) {
        /* Errors already printed by loc_scan (config/root). Do not emit a
         * partial report; any failure must not return 0. */
        loc_report_free(&rep);
        return rc;
    }

    {
        int e = loc_emit(&rep, &opts);
        if (e != SDK_EXIT_OK) {
            rc = e;
        }
    }
    loc_report_free(&rep);
    return rc;
}
