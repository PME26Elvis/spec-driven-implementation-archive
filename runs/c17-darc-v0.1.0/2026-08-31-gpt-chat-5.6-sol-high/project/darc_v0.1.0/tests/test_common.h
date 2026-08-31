#ifndef DARC_TEST_COMMON_H
#define DARC_TEST_COMMON_H
#define _POSIX_C_SOURCE 200809L
#include "darc.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
 int code;
 char *out;
 char *err;
} TestProc;

extern bool test_run_stress;
extern const char *test_root;
bool test_wants(const char *id);
void test_check(const char *id, bool ok, const char *fmt, ...);
void test_skip(const char *id, const char *why);
char *test_path(const char *a, const char *b);
int test_write(const char *path, const void *data, size_t n, unsigned mode);
int test_read(const char *path, DarcBuf *b);
int test_mkdir(const char *path);
char *test_case_dir(const char *name);
TestProc test_exec(const char *const argv[], const char *const envv[]);
void test_proc_free(TestProc *p);
bool test_contains(const char *s, const char *needle);
bool test_file_eq(const char *a, const char *b);
bool test_tree_eq(const char *a, const char *b, bool metadata);
size_t test_count_objects(const char *repo, int type);
int test_head(const char *repo, char out[65]);
void test_algorithms(void);
void test_config_cli(void);
void test_repository(void);
void test_scan_only(void);
void test_incremental(void);
void test_diff(void);
void test_restore(void);
void test_verify_gc(void);
void test_format(void);
void test_security(void);
void test_cli_delete(void);
void test_crash_fault(void);
void test_stress(void);
#endif
