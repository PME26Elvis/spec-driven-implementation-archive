/* locstat_core.h - reusable, testable core for the locstat line counter.
 *
 * This header is the single public surface of the engine. The command line
 * entry point (locstat.c) and the unit tests (test_locstat.c) both build on
 * top of these functions only; nothing here touches the CRT entry point.
 *
 * Normative contracts:
 *   docs/03_DEV_TOOL_LOCSTAT.md          (primary workstream spec)
 *   docs/19_CANONICAL_FORMATS_AND_LIMITS.md sections 2-5, 13 (canonical)
 * Where the two disagree, docs/19 wins.
 *
 * The JSON config parser is self-implemented; no third-party JSON library is
 * used anywhere in this tool.
 */
#ifndef LOCSTAT_CORE_H
#define LOCSTAT_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#include "common/sdk_common.h"
#include "common/sdk_win.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================== */
/* Line classification                                                */
/* ================================================================== */

typedef enum loc_line_kind {
    LOC_LINE_BLANK = 0,
    LOC_LINE_COMMENT_ONLY,
    LOC_LINE_CODE,
    LOC_LINE_MIXED
} loc_line_kind;

typedef struct loc_lexical {
    uint64_t physical_lines;
    uint64_t blank_lines;
    uint64_t comment_only_lines;
    uint64_t code_lines;
    uint64_t mixed_code_comment_lines;
} loc_lexical;

/* Physical line count of a buffer. Zero-byte input yields 0. CRLF, LF and CR
 * each count as a single terminator; CRLF is never counted twice. A leading
 * UTF-8 BOM is ignored for content purposes. (docs/19 section 4.4) */
uint64_t loc_physical_lines(const uint8_t *data, size_t len);

/* Lexical C classification of an entire buffer. Only a lexical scan is
 * performed (no full C parse): blank / comment_only / code / mixed. Block
 * comments, // comments, strings, char literals and escaped quotes are handled.
 * (docs/03 section 7.2) */
loc_lexical loc_analyze_c(const uint8_t *data, size_t len);

/* Classifies a single line in the context of a still-open block comment.
 * *in_block must be initialised to 0 before the first line and is updated to
 * reflect whether a /* ... * / comment is still open at the end of the line. */
loc_line_kind loc_classify_c_line(const uint8_t *line, size_t len,
                                  int *in_block);

/* ================================================================== */
/* Name / extension helpers                                           */
/* ================================================================== */

/* Splits a base file name into its lower-cased extension (including the dot,
 * or "" when none) and the original base name. Buffers must be caller
 * supplied and large enough (SDK_LIMIT_VCS_PATH_BYTES is always sufficient). */
void loc_split_name(const char *name, char *ext_buf, size_t ext_cap,
                    char *base_buf, size_t base_cap);

/* Case-insensitive extension comparison (both include the leading dot). */
int loc_ext_eq(const char *ext, const char *pattern);
/* Case-insensitive base-name comparison. */
int loc_base_eq(const char *name, const char *pattern);

/* Returns non-zero when any '/' separated component of rel_path equals comp
 * under Windows ordinal ignore-case comparison. */
int loc_path_has_component(const char *rel_path, const char *comp);

/* ================================================================== */
/* Self-implemented JSON parser                                       */
/* ================================================================== */

typedef enum loc_json_type {
    LOC_JSON_NULL = 0,
    LOC_JSON_BOOL,
    LOC_JSON_NUMBER,
    LOC_JSON_STRING,
    LOC_JSON_ARRAY,
    LOC_JSON_OBJECT
} loc_json_type;

typedef struct loc_json {
    loc_json_type type;
    int           is_int;   /* meaningful only for LOC_JSON_NUMBER */
    union {
        int            boolean;
        double         number;
        char          *string;                 /* unescaped UTF-8 */
        struct {
            struct loc_json **items;
            size_t            count;
            size_t            cap;
        } array;
        struct {
            char            **keys;
            struct loc_json **vals;
            size_t            count;
            size_t            cap;
        } object;
    } u;
} loc_json;

typedef struct loc_json_err {
    char    msg[256];
    size_t  offset;
    int     line;
    int     column;
} loc_json_err;

/* Parses a strict JSON document. Returns NULL on error and fills err. The
 * returned tree must be released with loc_json_free. */
loc_json *loc_json_parse(const char *text, size_t len, loc_json_err *err);
void      loc_json_free(loc_json *j);

/* ================================================================== */
/* Configuration                                                      */
/* ================================================================== */

typedef struct loc_category_rule {
    char    *name;
    char   **extensions;   /* each entry either ".ext" or a bare base name */
    size_t   count;
} loc_category_rule;

typedef struct loc_config {
    char            **include_extensions;
    size_t            include_count;
    char            **exclude_extensions;
    size_t            exclude_count;
    char            **exclude_paths;
    size_t            exclude_paths_count;
    loc_category_rule *categories;
    size_t            categories_count;
    uint64_t          max_file_bytes;
    int               has_max_file_bytes;
    int               follow_reparse_points;
    int               has_follow_reparse;
} loc_config;

/* The fixed canonical default-config JSON. Its raw bytes (not a re-serialised
 * form) are the config_digest_sha256 when built-in defaults are used. */
extern const char LOC_DEFAULT_CONFIG_JSON[];

/* Parses config text into a loc_config. Returns 0 on success or a negative
 * SDK-style status (SDK_ERR_DATA) on malformed input, filling err. Unknown
 * keys, out-of-range max_file_bytes and follow_reparse_points == true are
 * rejected. The result must be released with loc_config_free. */
int loc_config_parse(const char *text, size_t len, loc_config *out,
                     loc_json_err *err);
void loc_config_free(loc_config *cfg);

/* ================================================================== */
/* Exclusion decision (pure, testable)                                */
/* ================================================================== */

typedef enum loc_exclude_reason {
    LOC_EXCLUDE_NONE = 0,
    LOC_EXCLUDE_REPARSE,
    LOC_EXCLUDE_DEFAULT_DIR,
    LOC_EXCLUDE_DEFAULT_EXT,
    LOC_EXCLUDE_CONFIG_PATH,
    LOC_EXCLUDE_CONFIG_EXT,
    LOC_EXCLUDE_INCLUDE_FILTER,
    LOC_EXCLUDE_OVERSIZE
} loc_exclude_reason;

const char *loc_exclude_reason_str(loc_exclude_reason r);

/* Determines whether an entry is excluded. Pure function of its arguments so
 * it can be unit tested with synthetic inputs (no filesystem needed).
 *   rel_path  root-relative, '/' separated, UTF-8
 *   name      base name (UTF-8)
 *   ext       lower-cased extension including dot, or ""  (from loc_split_name)
 *   is_dir    non-zero for directories
 *   is_reparse_point  non-zero for reparse points / junctions
 *   file_bytes  size of the entry (ignored for directories)
 */
loc_exclude_reason loc_entry_excluded(const loc_config *cfg,
                                      int default_excludes,
                                      const char *rel_path,
                                      const char *name,
                                      const char *ext,
                                      int is_dir,
                                      int is_reparse_point,
                                      uint64_t file_bytes);

/* ================================================================== */
/* Category classification (pure, testable)                           */
/* ================================================================== */

/* Returns the category name for a file (a static string; do not free).
 * Implements the docs/19 section 4.5 priority:
 *   1. tests path rule (component tests/ or test/ with .c/.h)
 *   2. explicit config category, in config declaration order
 *   3. built-in source (.c/.h)
 *   4. built-in docs (.md/.txt)
 *   5. built-in config (.json/.yaml/.yml/build.cmd/.rc/.manifest)
 *   6. "unclassified"
 * (matched_rules / selected_category are intentionally omitted from the
 * canonical JSON file object per docs/19 section 4.7; see note in report.)
 */
const char *loc_classify_category(const loc_config *cfg,
                                  const char *rel_path,
                                  const char *name,
                                  const char *ext);

/* Fills sel_buf with the selected category name and rules_buf with a
 * comma-separated list of every rule that matched (in priority order). Both
 * buffers are caller supplied. Implements docs/19 section 4.5 reporting of
 * matched_rules + selected_category without altering the canonical JSON file
 * object layout. */
void loc_classify_category_detail(const loc_config *cfg,
                                  const char *rel_path,
                                  const char *name,
                                  const char *ext,
                                  char *sel_buf, size_t sel_cap,
                                  char *rules_buf, size_t rules_cap);

/* ================================================================== */
/* Report + scan                                                      */
/* ================================================================== */

typedef struct loc_file_record {
    char       *path;             /* root-relative, '/' separated */
    char       *category;         /* malloc'd; may be "unclassified" */
    char       *matched_rules;    /* malloc'd; comma list, or "" */
    uint64_t    bytes;
    uint64_t    physical_lines;
    loc_lexical lex;              /* 0 for non-C categories */
    int         encoding_warning; /* non-zero if not valid UTF-8 */
    int         binary_like;      /* non-zero if NUL seen in first 8 KiB */
} loc_file_record;

typedef struct loc_excluded_record {
    char            *path;
    loc_exclude_reason reason;
} loc_excluded_record;

typedef struct loc_cat_agg {
    char       *name;
    uint64_t    file_count;
    loc_lexical lex;
} loc_cat_agg;

typedef struct loc_report {
    int         scanned;            /* non-zero once a traversal completed */
    char       *root;              /* canonical UTF-8, '/' separated */
    char        config_digest_hex[65];
    int         used_default_config;
    int64_t     scan_started_epoch_ms;
    int64_t     scan_duration_ms;

    loc_file_record *files;
    size_t            files_count;
    size_t            files_cap;

    loc_excluded_record *excluded;
    size_t               excluded_count;
    size_t               excluded_cap;

    char   **warnings;
    size_t   warnings_count;
    size_t   warnings_cap;

    char   **errors;
    size_t   errors_count;
    size_t   errors_cap;

    /* Derived in loc_report_finalize, in first-encounter order. */
    loc_cat_agg *cats;
    size_t        cats_count;
    size_t        cats_cap;

    loc_lexical totals;
    uint64_t    total_files;
    uint64_t    total_excluded;
    uint64_t    total_warnings;
    uint64_t    total_errors;
    uint64_t    total_bytes;
} loc_report;

typedef struct loc_options {
    const wchar_t *root;             /* original ROOT argument */
    const wchar_t *config_path;      /* NULL => built-in defaults */
    const wchar_t *json_path;        /* NULL => no JSON; L"-" => stdout */
    const char    *category_filter;  /* NULL => all categories */
    int            fail_on_error;
    int            no_default_excludes;
} loc_options;

void loc_report_init(loc_report *rep);
void loc_report_free(loc_report *rep);

/* Scans ROOT and fills rep. Returns an SDK-style exit status:
 *   SDK_EXIT_OK      success (also when unreadable files exist but
 *                    --fail-on-error was not given)
 *   SDK_EXIT_USAGE   unknown --category
 *   SDK_EXIT_DATA    malformed config file
 *   SDK_EXIT_IO      root missing, or an unreadable file with --fail-on-error
 *   SDK_EXIT_INTERNAL  unexpected invariant failure
 * On SDK_EXIT_DATA / missing-root the report is not populated (rep->scanned
 * remains 0). On a completed scan rep->scanned is set and the caller may emit
 * the report even when the return value is SDK_EXIT_IO (unreadable files). */
int loc_scan(const loc_options *opts, loc_report *rep);

/* Writes the human-readable report to the given stream. */
void loc_write_text(const loc_report *rep, const loc_options *opts, FILE *out);

/* Serialises the canonical JSON report into buf. */
void loc_report_to_json(const loc_report *rep, const loc_options *opts,
                        sdk_buf *buf);

/* Emits outputs according to opts:
 *   json_path == NULL        -> text to stdout
 *   json_path == L"-"        -> JSON to stdout, text to stderr
 *   json_path == L"file"     -> JSON to file (parents created), text to stdout
 * Returns 0 on success or SDK_EXIT_IO when the JSON file cannot be written. */
int loc_emit(const loc_report *rep, const loc_options *opts);

#ifdef __cplusplus
}
#endif

#endif /* LOCSTAT_CORE_H */
