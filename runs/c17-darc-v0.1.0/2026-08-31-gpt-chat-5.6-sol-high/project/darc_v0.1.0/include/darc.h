#ifndef DARC_H
#define DARC_H
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DARC_VERSION "0.1.0"
#define DARC_FORMAT_VERSION 1
#define DARC_CID_LEN 32
#define DARC_HEX_LEN 64
#define DARC_MAX_CONFIG_DEPTH 64
#define DARC_MAX_CHUNK (16u*1024u*1024u)

enum { DARC_OK=0, DARC_E_USAGE=2, DARC_E_REPO=3, DARC_E_NOTFOUND=4, DARC_E_IO=5,
       DARC_E_CORRUPT=6, DARC_E_UNRECOVERABLE=7, DARC_E_RESTORE=8,
       DARC_E_LOCKED=9, DARC_E_UNSUPPORTED=10, DARC_E_INTERNAL=11 };

typedef struct { uint8_t b[32]; } DarcCid;
typedef enum { OBJ_CHUNK=1, OBJ_FILE=2, OBJ_TREE=3, OBJ_SNAPSHOT=4, OBJ_PARITY=5 } DarcObjType;
typedef enum { CODEC_RAW=0, CODEC_LZH1=1 } DarcCodec;

typedef struct { uint8_t *data; size_t len, cap; } DarcBuf;
typedef struct { char *s; size_t len; } DarcStr;

typedef struct {
  uint32_t h[8]; uint64_t bits; uint8_t block[64]; size_t used;
} DarcSha256;

void darc_sha256_init(DarcSha256 *c);
void darc_sha256_update(DarcSha256 *c, const void *data, size_t len);
void darc_sha256_final(DarcSha256 *c, uint8_t out[32]);
void darc_sha256(const void *data, size_t len, uint8_t out[32]);
uint32_t darc_crc32c(const void *data, size_t len);
void darc_buzhash_table(uint64_t t[256]);
uint64_t darc_rol64(uint64_t x, unsigned n);

typedef struct { uint8_t kind, lit; uint16_t dist, len; } DarcLzTok;
typedef struct { DarcLzTok *v; size_t n, cap; } DarcTokVec;
int darc_lz77_tokenize(const uint8_t *in,size_t n,DarcTokVec *out);
int darc_lz77_serialize(const DarcTokVec *t,DarcBuf *out);
void darc_tokvec_free(DarcTokVec *v);
int darc_huff_lengths(const uint8_t *in,size_t n,uint8_t lens[256]);
int darc_huff_encode(const uint8_t *in,size_t n,const uint8_t lens[256],DarcBuf *bits);
int darc_huff_decode(const uint8_t *bits,size_t bits_n,const uint8_t lens[256],size_t out_n,DarcBuf *out);
int darc_lzh1_compress(const uint8_t *in,size_t n,DarcBuf *out);
int darc_lzh1_decompress(const uint8_t *in,size_t n,size_t expected,DarcBuf *out);

typedef struct { size_t *ends; size_t n, cap; } DarcCuts;
int darc_cdc_buffer(const uint8_t *data,size_t n,size_t min,size_t avg,size_t max,DarcCuts *cuts);
void darc_cuts_free(DarcCuts *c);

void *darc_malloc(size_t n);
void *darc_calloc(size_t n,size_t s);
void *darc_realloc(void *p,size_t n);
char *darc_strdup(const char *s);
char *darc_strndup(const char *s,size_t n);
void darc_buf_init(DarcBuf *b);
void darc_buf_free(DarcBuf *b);
int darc_buf_reserve(DarcBuf *b,size_t add);
int darc_buf_put(DarcBuf *b,const void *p,size_t n);
int darc_buf_u8(DarcBuf *b,uint8_t x);
int darc_buf_u16(DarcBuf *b,uint16_t x);
int darc_buf_u32(DarcBuf *b,uint32_t x);
int darc_buf_u64(DarcBuf *b,uint64_t x);
int darc_buf_i64(DarcBuf *b,int64_t x);
int darc_buf_bytestr(DarcBuf *b,const void *p,size_t n);
uint16_t darc_rd_u16(const uint8_t *p);
uint32_t darc_rd_u32(const uint8_t *p);
uint64_t darc_rd_u64(const uint8_t *p);
int64_t darc_rd_i64(const uint8_t *p);
void darc_cid_hex(const DarcCid *c,char out[65]);
int darc_hex_cid(const char *s,DarcCid *c);
int darc_cid_cmp(const DarcCid *a,const DarcCid *b);
int darc_cid_eq(const DarcCid *a,const DarcCid *b);
void darc_object_cid(uint8_t type,const void *payload,size_t len,DarcCid *out);
int darc_valid_utf8(const uint8_t *s,size_t n);
char *darc_display_bytes(const uint8_t *s,size_t n);
char *darc_json_escape_bytes(const uint8_t *s,size_t n);
int darc_mkdir_p(const char *path,mode_t mode);
int darc_read_file(const char *path,DarcBuf *out,size_t limit);
int darc_write_all(int fd,const void *buf,size_t n);
int darc_fsync_fd(int fd);
int darc_rename_path(const char *a,const char *b);
int darc_atomic_write(const char *path,const void *data,size_t n,mode_t mode);
int darc_atomic_write_checkpoint(const char *path,const void *data,size_t n,mode_t mode,const char *checkpoint);
int darc_remove_tree(const char *path);
char *darc_path_join(const char *a,const char *b);
char *darc_dirname_dup(const char *p);
char *darc_basename_dup(const char *p);
int64_t darc_stat_mtime_ns(const struct stat *st);
int64_t darc_stat_ctime_ns(const struct stat *st);
void darc_error(int exit_code,const char *code,const char *fmt,...);
extern int darc_last_error;
extern char darc_last_code[64];
void darc_fault_reset(void);
void darc_checkpoint(const char *name);

/* Generic parsed configuration value. */
typedef enum { V_NULL,V_BOOL,V_INT,V_NUM,V_STR,V_ARR,V_OBJ } DarcValType;
typedef struct DarcValue DarcValue;
typedef struct { char *key; DarcValue *val; } DarcPair;
struct DarcValue { DarcValType t; union { bool b; int64_t i; double num; DarcStr str; struct { DarcValue **v; size_t n,cap; } arr; struct { DarcPair *v; size_t n,cap; } obj; } u; };
void darc_value_free(DarcValue *v);
DarcValue *darc_json_parse(const uint8_t *s,size_t n,char *err,size_t errn,size_t *line,size_t *col);
DarcValue *darc_yaml_parse(const uint8_t *s,size_t n,char *err,size_t errn,size_t *line,size_t *col);

typedef struct {
  int format_version; bool parity_enabled; int parity_data_members;
  bool follow_symlinks,cross_filesystems,exclude_hidden;
  char **include; size_t include_n; char **exclude; size_t exclude_n;
  int on_special_file,on_permission_error; /* 0 error 1 skip */
  size_t chunk_window,chunk_min,chunk_avg,chunk_max;
  bool compression_enabled; size_t min_savings;
  char *snapshot_name,*snapshot_parent; bool timestamp_set; int64_t timestamp_ns; bool trust_unchanged_identity;
  int overwrite; bool preserve_mode,preserve_mtime,create_hardlinks;
  char *diff_path; int top_n_svg; bool show_chunk_metrics;
  int verify_level; bool verify_repair;
  bool gc_dry_run,gc_repack_parity;
  int output_format; int color; bool quiet,verbose;
  DarcCid config_hash,profile_hash;
} DarcConfig;
void darc_config_defaults(DarcConfig *c);
void darc_config_free(DarcConfig *c);
int darc_config_load_file(const char *path,DarcConfig *c,DarcBuf *normalized,char *err,size_t errn);
int darc_config_apply_repo_defaults(const char *repo,DarcConfig *c);
int darc_config_write_repo_defaults(const char *repo,const DarcConfig *c);
int darc_config_normalize(DarcConfig *c,DarcBuf *out,bool profile_only);

/* Robin Hood index */
typedef struct { DarcCid cid; uint8_t type,codec,used; uint64_t stored_len,uncomp_len; uint32_t dib; } DarcRhEnt;
typedef struct { DarcRhEnt *tab; size_t cap,n; } DarcRhMap;
int darc_rh_init(DarcRhMap *m,size_t cap);
void darc_rh_free(DarcRhMap *m);
int darc_rh_put(DarcRhMap *m,const DarcRhEnt *e);
DarcRhEnt *darc_rh_get(DarcRhMap *m,const DarcCid *cid);
int darc_rh_del(DarcRhMap *m,const DarcCid *cid);

/* Repository object APIs */
typedef struct { uint8_t type,codec; uint64_t uncomp_len,stored_len; DarcBuf payload; } DarcObject;
void darc_object_free(DarcObject *o);
int darc_repo_init(const char *path,const DarcConfig *cfg);
int darc_repo_check(const char *repo);
char *darc_object_path(const char *repo,const DarcCid *cid);
int darc_object_store(const char *repo,uint8_t type,const uint8_t *payload,size_t len,const DarcConfig *cfg,DarcCid *cid,uint64_t *stored_payload_bytes,bool *is_new);
int darc_object_load(const char *repo,const DarcCid *cid,DarcObject *out,int verify_cid);
int darc_object_header(const char *path,DarcRhEnt *out);
int darc_index_rebuild(const char *repo);
int darc_index_load(const char *repo,DarcRhMap *m);

/* Canonical FILE/TREE/SNAPSHOT structures */
typedef struct { DarcCid cid; uint64_t len; } DarcChunkRef;
typedef struct { uint64_t logical_size; DarcChunkRef *chunks; size_t chunk_n; uint8_t digest[32]; } DarcFileObj;
int darc_file_serialize(const DarcFileObj *f,DarcBuf *b);
int darc_file_parse(const uint8_t *p,size_t n,DarcFileObj *f);
void darc_file_free(DarcFileObj *f);

typedef struct DarcTreeEnt {
  uint8_t *name; size_t name_len; uint8_t type; uint32_t mode; int64_t mtime_ns;
  DarcCid cid; uint64_t hardlink_group; uint8_t *target; size_t target_len;
} DarcTreeEnt;
typedef struct { DarcTreeEnt *e; size_t n; } DarcTreeObj;
int darc_tree_serialize(DarcTreeObj *t,DarcBuf *b);
int darc_tree_parse(const uint8_t *p,size_t n,DarcTreeObj *t);
void darc_tree_free(DarcTreeObj *t);

typedef struct {
 int64_t created_ns; char *name; bool has_parent; DarcCid parent,root,profile_hash;
 uint32_t root_mode; int64_t root_mtime_ns; char **roots; size_t root_n;
 uint64_t logical_bytes,files,dirs,symlinks,hardlinks,unique_chunks,new_chunks,new_stored_bytes;
} DarcSnapshotObj;
int darc_snapshot_serialize(const DarcSnapshotObj *s,DarcBuf *b);
int darc_snapshot_parse(const uint8_t *p,size_t n,DarcSnapshotObj *s);
void darc_snapshot_free(DarcSnapshotObj *s);

/* parity */
typedef struct { DarcCid cid; uint64_t len; } DarcParityMember;
typedef struct { DarcCid parity_cid; DarcParityMember m[8]; size_t n; } DarcStripe;
typedef struct { DarcStripe *v; size_t n,cap; } DarcCatalog;
void darc_catalog_free(DarcCatalog *c);
int darc_catalog_load(const char *repo,DarcCatalog *c);
int darc_catalog_write(const char *repo,const DarcCatalog *c);
int darc_parity_build_payload(const DarcParityMember *m,size_t n,uint8_t **chunks,DarcBuf *out);
int darc_parity_parse(const uint8_t *p,size_t n,DarcStripe *s,DarcBuf *parity_bytes);
int darc_parity_protect_new(const char *repo,const DarcCid *cids,const uint64_t *lens,size_t n,const DarcConfig *cfg);
int darc_chunk_read_or_recover(const char *repo,const DarcCid *cid,DarcBuf *out,bool allow_repair,bool *recovered);

/* high-level commands */
int darc_lock_acquire(const char *repo,int *fd_out);
void darc_lock_release(const char *repo,int fd);
int darc_recover_journals(const char *repo);
int darc_cmd_snapshot_create(const char *repo,const DarcConfig *cfg,char **sources,size_t nsources);
int darc_cmd_snapshot_list(const char *repo,const DarcConfig *cfg);
int darc_cmd_snapshot_show(const char *repo,const DarcConfig *cfg,const char *sel);
int darc_cmd_snapshot_delete(const char *repo,const DarcConfig *cfg,const char *sel,bool yes,bool dry);
int darc_resolve_snapshot(const char *repo,const char *sel,DarcCid *cid,DarcSnapshotObj *snap);
int darc_cmd_diff(const char *repo,const DarcConfig *cfg,const char *a,const char *b);
int darc_cmd_restore(const char *repo,const DarcConfig *cfg,const char *sel,const char *to,const char *path);
int darc_cmd_verify(const char *repo,const DarcConfig *cfg);
int darc_cmd_gc(const char *repo,const DarcConfig *cfg);
int darc_cmd_stats(const char *repo,const DarcConfig *cfg);
int darc_cmd_repo_inspect(const char *repo,const DarcConfig *cfg);

int darc_cli(int argc,char **argv);
#endif
