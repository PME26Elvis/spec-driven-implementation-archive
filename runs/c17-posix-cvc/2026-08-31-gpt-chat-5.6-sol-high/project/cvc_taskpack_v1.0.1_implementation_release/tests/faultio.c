#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int (*mkdir_fn)(const char *, mode_t);
typedef int (*rename_fn)(const char *, const char *);
typedef int (*link_fn)(const char *, const char *);
typedef int (*fsync_fn)(int);

static int fail_next_fsync;

static int mode_is(const char *s) {
    const char *m = getenv("FI_MODE");
    return m != NULL && strcmp(m, s) == 0;
}

/* POSIX specifies dlsym for function lookup, but ISO C does not define a
 * direct object-pointer -> function-pointer cast. Copy the representation
 * into the function-pointer object to keep the test shim warning-clean. */
static void load_symbol(void *dst, size_t dst_size, const char *name) {
    void *sym = dlsym(RTLD_NEXT, name);
    if (sym == NULL || dst_size != sizeof sym) _exit(126);
    memcpy(dst, &sym, dst_size);
}

static int suffix_match(const char *path) {
    const char *q = getenv("FI_MATCH");
    size_t np = strlen(path);
    size_t nq = q != NULL ? strlen(q) : 0;
    return q != NULL && nq != 0 && np >= nq && strcmp(path + np - nq, q) == 0;
}

int mkdir(const char *p, mode_t m) {
    static mkdir_fn realf;
    if (realf == NULL) load_symbol(&realf, sizeof realf, "mkdir");
    if (mode_is("init-state") && strstr(p, "/.cvc/state") != NULL) {
        errno = EACCES;
        return -1;
    }
    return realf(p, m);
}

int rename(const char *a, const char *b) {
    static rename_fn realf;
    static int work_failed;
    if (realf == NULL) load_symbol(&realf, sizeof realf, "rename");
    if (mode_is("ref-main") && strstr(b, "/.cvc/refs/heads/main") != NULL) {
        errno = EIO;
        return -1;
    }
    if (mode_is("head") && strstr(b, "/.cvc/HEAD") != NULL) {
        errno = EIO;
        return -1;
    }
    if (mode_is("work") && !work_failed && strstr(b, "/.cvc/") == NULL && suffix_match(b)) {
        work_failed = 1;
        errno = EIO;
        return -1;
    }
    if (mode_is("work-fsync") && !work_failed && strstr(b, "/.cvc/") == NULL && suffix_match(b)) {
        int rc = realf(a, b);
        if (rc == 0) {
            work_failed = 1;
            fail_next_fsync = 1;
        }
        return rc;
    }
    return realf(a, b);
}

int fsync(int fd) {
    static fsync_fn realf;
    if (realf == NULL) load_symbol(&realf, sizeof realf, "fsync");
    if (fail_next_fsync) {
        fail_next_fsync = 0;
        errno = EIO;
        return -1;
    }
    return realf(fd);
}

int link(const char *a, const char *b) {
    static link_fn realf;
    if (realf == NULL) load_symbol(&realf, sizeof realf, "link");
    if (mode_is("object-link") && strstr(b, "/.cvc/objects/") != NULL) {
        errno = EIO;
        return -1;
    }
    return realf(a, b);
}
