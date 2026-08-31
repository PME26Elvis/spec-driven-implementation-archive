#ifndef DARC_INTERNAL_H
#define DARC_INTERNAL_H
#include "darc.h"

typedef struct {
 char *path;
 uint8_t type; /* TREE encoding type 1..4 */
 uint32_t mode;
 int64_t mtime_ns;
 DarcCid cid; /* FILE/TREE for type 1/2; resolved FILE for hardlink after flatten */
 uint64_t hardlink_group;
 uint8_t *target; size_t target_len;
 uint64_t logical_size;
 uint8_t file_digest[32];
} DarcFlatEnt;
typedef struct {DarcFlatEnt *v; size_t n,cap;} DarcFlatTree;
int darc_flatten_snapshot(const char *repo,const DarcSnapshotObj *snap,DarcFlatTree *out);
void darc_flat_free(DarcFlatTree *t);
DarcFlatEnt *darc_flat_find(DarcFlatTree *t,const char *path);
int darc_path_is_safe_archive(const char *path);
char *darc_xml_escape(const uint8_t *s,size_t n);

/* Object directory enumeration. */
typedef struct {DarcCid cid; DarcRhEnt h; char *path;} DarcDiskObj;
typedef struct {DarcDiskObj *v;size_t n,cap;} DarcDiskList;
int darc_list_objects(const char *repo,DarcDiskList *out,bool tolerate_bad_headers);
void darc_disklist_free(DarcDiskList *l);

#endif
