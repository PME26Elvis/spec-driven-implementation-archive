/* locscan — count authored lines per category (LOC-001..). Minimal v1. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdbool.h>

typedef struct {
    long prod, headers, tests, docs, config, other, total_files;
} counts_t;

static bool ends_with(const char *s, const char *suf) {
    size_t ls = strlen(s), lf = strlen(suf);
    return ls >= lf && strcmp(s + ls - lf, suf) == 0;
}

static bool is_ignored(const char *name) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return true;
    if (strcmp(name, "build") == 0 || strcmp(name, ".git") == 0) return true;
    if (ends_with(name, ".o") || ends_with(name, ".a") || ends_with(name, ".so")) return true;
    if (ends_with(name, ".edb") || ends_with(name, ".wal")) return true;
    return false;
}

static long count_lines(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    long n = 0;
    int c, prev = '\n';
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') n++;
        prev = c;
    }
    if (prev != '\n' && prev != EOF) n++; /* no final newline */
    fclose(f);
    return n;
}

static void walk(const char *dir, counts_t *c) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (is_ignored(e->d_name)) continue;
        char path[4096];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            walk(path, c);
            continue;
        }
        if (!S_ISREG(st.st_mode)) continue;
        /* binary heuristic: NUL in first 512 */
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        unsigned char buf[512];
        size_t n = fread(buf, 1, sizeof buf, f);
        fclose(f);
        bool binary = false;
        for (size_t i = 0; i < n; i++) if (buf[i] == 0) { binary = true; break; }
        if (binary) continue;

        long lines = count_lines(path);
        c->total_files++;
        if (strstr(path, "/src/") && ends_with(path, ".c")) c->prod += lines;
        else if (strstr(path, "/include/") && ends_with(path, ".h")) c->headers += lines;
        else if (strstr(path, "/tests/")) c->tests += lines;
        else if (strstr(path, "/docs/") || ends_with(path, ".md") || ends_with(path, ".txt")) c->docs += lines;
        else if (ends_with(path, ".json") || ends_with(path, ".yml") || ends_with(path, ".yaml") ||
                 strcmp(e->d_name, "Makefile") == 0) c->config += lines;
        else c->other += lines;
    }
    closedir(d);
}

int main(int argc, char **argv) {
    if (argc >= 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Usage: locscan [directory]\n");
        return 0;
    }
    const char *root = argc > 1 ? argv[1] : ".";
    counts_t c = {0};
    walk(root, &c);
    printf("locscan report for %s\n", root);
    printf("  production source (.c under src/): %ld\n", c.prod);
    printf("  headers (.h under include/):       %ld\n", c.headers);
    printf("  tests:                             %ld\n", c.tests);
    printf("  docs:                              %ld\n", c.docs);
    printf("  config/Makefile:                   %ld\n", c.config);
    printf("  other text:                        %ld\n", c.other);
    printf("  files counted:                     %ld\n", c.total_files);
    printf("  TOTAL lines:                       %ld\n",
           c.prod + c.headers + c.tests + c.docs + c.config + c.other);
    return 0;
}
