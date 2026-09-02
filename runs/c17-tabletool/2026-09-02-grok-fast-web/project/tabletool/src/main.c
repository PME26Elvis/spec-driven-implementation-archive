#include "common.h"
#include "util.h"
#include "script.h"
#include <stdio.h>
#include <string.h>

static void print_help(void) {
    puts("tabletool --script <path> --report <path>");
    puts("tabletool --help");
    puts("tabletool --version");
    puts("Required options for processing: --script, --report");
}

static void print_version(void) {
    puts("tabletool 1.0.1");
}

int main(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) { print_help(); return EXIT_OK; }
        if (strcmp(argv[1], "--version") == 0) { print_version(); return EXIT_OK; }
    }
    const char *script_path = NULL, *report_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--script") == 0 && i+1 < argc) {
            if (script_path) { fprintf(stderr, "duplicate option\n"); return EXIT_CLI; }
            script_path = argv[++i];
        } else if (strcmp(argv[i], "--report") == 0 && i+1 < argc) {
            if (report_path) { fprintf(stderr, "duplicate option\n"); return EXIT_CLI; }
            report_path = argv[++i];
        } else {
            fprintf(stderr, "unknown or incomplete option\n");
            return EXIT_CLI;
        }
    }
    if (!script_path || !report_path) {
        fprintf(stderr, "missing --script or --report\n");
        return EXIT_CLI;
    }
    if (strcmp(script_path, report_path) == 0) {
        fprintf(stderr, "script and report must differ\n");
        return EXIT_CLI;
    }

    unsigned char *sdata;
    size_t slen;
    if (read_file_binary(script_path, &sdata, &slen) != 0) {
        fprintf(stderr, "cannot read script\n");
        return EXIT_IO;
    }
    if (!utf8_validate(sdata, slen)) {
        fprintf(stderr, "invalid UTF-8 in script\n");
        tt_free(sdata);
        return EXIT_SYNTAX;
    }

    Script sc;
    ErrorInfo err;
    memset(&err, 0, sizeof(err));
    if (script_parse(sdata, slen, &sc, &err) != 0) {
        fprintf(stderr, "script syntax error at line %d: %s\n", err.script_line, err.message);
        tt_free(sdata);
        script_free(&sc);
        return EXIT_SYNTAX;
    }
    tt_free(sdata);

    int rc = script_execute(&sc, report_path, &err);
    script_free(&sc);
    if (rc != 0) return err.exit_code ? err.exit_code : EXIT_DOMAIN;
    return EXIT_OK;
}
