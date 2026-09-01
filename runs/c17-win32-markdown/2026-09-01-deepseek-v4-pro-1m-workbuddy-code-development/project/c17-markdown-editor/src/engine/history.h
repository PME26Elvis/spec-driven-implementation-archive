/* history.h - persistent per-document version history with snapshot/delta/LZSS
 * and integrity checks. */
#ifndef MD_HISTORY_H
#define MD_HISTORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint64_t id;
    uint64_t timestamp;
    uint64_t parent;
    bool pinned;
    bool is_snapshot;
    bool compressed;
    bool corrupt;           /* failed integrity / reconstruction */
    unsigned char *payload;
    size_t payload_len;
    /* cached full content (lazily reconstructed) */
    char *cached; size_t cached_len;
} md_version;

typedef struct {
    md_version *v;
    size_t n, cap;
    uint64_t next_id;
    size_t snapshot_interval;   /* 20 */
    size_t max_versions;        /* 200 */
    uint64_t max_payload;       /* 64 MiB */
    uint64_t total_payload;     /* running encoded payload bytes */
} md_history;

md_history *md_history_create(void);
void md_history_free(md_history *h);

/* Add a version with content. Returns index, or -1 on failure. Creates a
 * snapshot every `snapshot_interval` versions (and for the first version),
 * otherwise a delta from the previous version. */
int md_history_add(md_history *h, const char *content, size_t len, uint64_t timestamp);

/* Reconstruct a version's content. Returns malloc'd NUL-terminated string.
 * NULL if the version is corrupt/unreconstructable. */
char *md_history_get(md_history *h, size_t index, size_t *out_len);

void md_history_pin(md_history *h, size_t index, bool pinned);
bool md_history_delete(md_history *h, size_t index);

/* Prune to retention limits, preserving pinned versions. Returns true if
 * pruning stopped early because pinned versions blocked compliance. */
bool md_history_prune(md_history *h);

/* Serialize to a byte buffer (malloc'd). Returns length. */
unsigned char *md_history_serialize(md_history *h, size_t *out_len);

/* Load from a byte buffer. Returns NULL on fatal format error; individual
 * corrupt records are marked corrupt (accessible but flagged). */
md_history *md_history_load(const unsigned char *data, size_t len);

/* Number of versions. */
size_t md_history_count(const md_history *h);

#endif /* MD_HISTORY_H */
