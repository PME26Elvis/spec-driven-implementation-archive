#ifndef CVC_OBJECTS_H
#define CVC_OBJECTS_H

#include <stddef.h>
#include <stdint.h>
#include "util.h"

#define CVC_MAX_OBJECT_SIZE (64*1024*1024)

/* Tree entry types */
enum {
    OBJ_TREE_BLOB = 0x01,
    OBJ_TREE_FILE_SYMLINK = 0x02,
    OBJ_TREE_SUBTREE = 0x03,
    OBJ_TREE_DIR_SYMLINK = 0x04
};

/* In-memory tree entry */
typedef struct {
    uint8_t type;              /* one of OBJ_TREE_* */
    char *name;                /* one child filename component (UTF-8) */
    uint8_t id[32];            /* raw object id */
} TreeEntry;

/* In-memory tree */
typedef struct {
    TreeEntry *entries;
    size_t count;
    size_t cap;
} Tree;

/* In-memory commit */
typedef struct {
    uint8_t root_tree[32];
    uint8_t parent_count;
    uint8_t parents[2][32];
    int64_t timestamp;
    char *message;
    size_t message_len;
} Commit;

/* Object on disk read */
typedef struct {
    char type;                 /* 'b' blob, 's' symlink, 't' tree, 'c' commit */
    Bytes payload;             /* raw payload bytes (after envelope) */
} ObjectData;

void tree_init(Tree *t);
void tree_free(Tree *t);
int tree_add(Tree *t, uint8_t type, const char *name, const uint8_t id[32]);
/* Sort entries by unsigned name bytes ascending. Also detects exact-duplicate
 * and Windows ordinal case-insensitive sibling collisions; returns -1 if
 * collision, else 0. */
int tree_sort_validate(Tree *t);
int tree_has_case_collision(const Tree *t);

/* Encode a tree payload (raw, without envelope). Returns heap bytes, sets len. */
int tree_encode(const Tree *t, Bytes *out);
/* Decode tree payload. Returns 0 success / -1 malformed. Sets t (caller frees). */
int tree_decode(const uint8_t *payload, size_t len, Tree *t);

/* Build canonical envelope: "<type> <len>\0<payload>" and hash to id. */
void object_envelope(const char *type, const uint8_t *payload, size_t plen, Bytes *out, uint8_t id[32]);
/* Encode a commit payload. */
int commit_encode(const Commit *c, Bytes *out);
int commit_decode(const uint8_t *payload, size_t len, Commit *c);
void commit_free(Commit *c);

/* --- Repository object store access (needs repo root) ---------------- */

typedef struct Repo Repo; /* fwd */

/* Read object by id from repo object store. Returns CVC_OK on success, with
 * obj->type and payload. On missing/corrupt returns error and sets type=0. */
CvcStatus obj_read(const Repo *repo, const uint8_t id[32], ObjectData *obj);
void object_free(ObjectData *obj);

/* Ensure a loose object exists with the given canonical envelope bytes.
 * Writes temp + flush + rename, reuses identical existing object, fails on
 * conflicting corrupt existing object. Returns CVC_OK or error. */
CvcStatus obj_write_envelope(const Repo *repo, const char *type,
                             const uint8_t *payload, size_t plen, uint8_t id[32]);

/* Compute object path (extended wide) for an id. Heap. */
uint16_t *obj_path(const Repo *repo, const uint8_t id[32]);

/* Get id hex of a raw id. */
void id_hex(const uint8_t id[32], char out[65]);

#endif
