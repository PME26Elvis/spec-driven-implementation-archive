#ifndef MDEDIT_STORAGE_H
#define MDEDIT_STORAGE_H

#include "mdedit/document.h"
#include "mdedit/diff.h"

typedef enum {
    MD_FAULT_NONE,
    MD_FAULT_ENOSPC,
    MD_FAULT_EACCES,
    MD_FAULT_PARTIAL_WRITE,
    MD_FAULT_CLOSE,
    MD_FAULT_RENAME
} MdFaultKind;

typedef struct {
    MdFaultKind kind;
    size_t fail_after;
} MdFaultInjection;

void md_storage_set_fault(MdFaultInjection fault);
void md_storage_clear_fault(void);
bool md_safe_save_document(MdDocument *doc, const char *path,
                           bool explicit_overwrite,
                           char *error, size_t error_cap);

typedef struct {
    bool dark_theme;
    int font_size;
    double line_spacing;
    bool default_embed_images;
    bool autosave_enabled;
    int autosave_interval;
    MdEditorMode default_mode;
    bool sync_scroll;
    bool restore_session;
} MdPreferences;

void md_preferences_defaults(MdPreferences *prefs);
bool md_preferences_path(char out[MD_PATH_MAX]);
bool md_preferences_load(MdPreferences *prefs, char *warning, size_t warning_cap);
bool md_preferences_save(const MdPreferences *prefs, char *error, size_t error_cap);

typedef struct {
    char path[MD_PATH_MAX];
    bool is_directory;
    bool is_symlink;
    int depth;
    uint64_t size;
} MdTreeEntry;

typedef struct {
    char root[MD_PATH_MAX];
    MdTreeEntry *entries;
    size_t count;
    size_t cap;
    bool sidebar_collapsed;
    double sidebar_width;
    char **recent_files;
    size_t recent_file_count;
    char **recent_workspaces;
    size_t recent_workspace_count;
    char **collapsed_directories;
    size_t collapsed_directory_count;
    size_t collapsed_directory_cap;
} MdWorkspace;

void md_workspace_init(MdWorkspace *ws);
void md_workspace_free(MdWorkspace *ws);
bool md_workspace_open(MdWorkspace *ws, const char *root,
                       char *error, size_t error_cap);
bool md_workspace_scan(MdWorkspace *ws, char *error, size_t error_cap);
bool md_workspace_save_session(const MdWorkspace *ws,
                               const MdDocument *docs, size_t doc_count,
                               size_t active_doc,
                               char *error, size_t error_cap);
bool md_workspace_load_session(MdWorkspace *ws, MdBuf *json,
                               char *warning, size_t warning_cap);
bool md_workspace_create_file(MdWorkspace *ws, const char *relative,
                              bool directory, char *error, size_t error_cap);
bool md_workspace_rename(MdWorkspace *ws, const char *old_relative,
                         const char *new_relative,
                         char *error, size_t error_cap);
bool md_workspace_delete(MdWorkspace *ws, const char *relative, bool recursive,
                         char *error, size_t error_cap);
bool md_workspace_directory_collapsed(const MdWorkspace *ws,
                                      const char *relative);
bool md_workspace_set_directory_collapsed(MdWorkspace *ws,
                                          const char *relative,
                                          bool collapsed);
bool md_recent_load(MdWorkspace *ws, char *warning, size_t warning_cap);
bool md_recent_save(const MdWorkspace *ws, char *error, size_t error_cap);
bool md_recent_add_file(MdWorkspace *ws, const char *path);
bool md_recent_add_workspace(MdWorkspace *ws, const char *path);
bool md_recent_remove_file(MdWorkspace *ws, const char *path);
bool md_recent_remove_workspace(MdWorkspace *ws, const char *path);
void md_recent_clear_files(MdWorkspace *ws);
void md_recent_clear_workspaces(MdWorkspace *ws);

typedef struct {
    uint64_t sequence;
    uint64_t timestamp;
    bool pinned;
    bool full_snapshot;
    uint64_t encoded_size;
    char record_path[MD_PATH_MAX];
} MdVersionInfo;

typedef struct {
    MdVersionInfo *items;
    size_t count;
    size_t cap;
} MdVersionList;

void md_version_list_init(MdVersionList *list);
void md_version_list_free(MdVersionList *list);
bool md_history_create(const char *history_root, const MdDocument *doc,
                       bool explicit_create, MdVersionInfo *created,
                       char *error, size_t error_cap);
bool md_history_list(const char *history_root, const MdDocument *doc,
                     MdVersionList *list, char *error, size_t error_cap);
bool md_history_reconstruct(const MdVersionList *list, size_t index,
                            MdBuf *source, char *error, size_t error_cap);
bool md_history_pin(const MdVersionList *list, size_t index, bool pinned,
                    char *error, size_t error_cap);
bool md_history_delete(const MdVersionList *list, size_t index,
                       char *error, size_t error_cap);
void md_history_set_retention_limits(size_t max_versions, uint64_t max_bytes);
void md_history_reset_retention_limits(void);

typedef struct {
    char record_path[MD_PATH_MAX];
    char document_path[MD_PATH_MAX];
    char document_id[65];
    uint64_t timestamp;
    bool untitled;
    bool valid;
} MdRecoveryInfo;

typedef struct {
    MdRecoveryInfo *items;
    size_t count;
    size_t cap;
} MdRecoveryList;

void md_recovery_list_init(MdRecoveryList *list);
void md_recovery_list_free(MdRecoveryList *list);
bool md_recovery_write(const char *root, const MdDocument *doc,
                       char *error, size_t error_cap);
bool md_recovery_scan(const char *root, MdRecoveryList *list,
                      char *warning, size_t warning_cap);
bool md_recovery_open(const MdRecoveryInfo *info, MdBuf *source,
                      char *error, size_t error_cap);
bool md_recovery_remove(const MdRecoveryInfo *info,
                        char *error, size_t error_cap);

bool md_asset_import_relative(const char *workspace_root,
                              const char *document_path,
                              const char *source_image,
                              char relative_out[MD_PATH_MAX],
                              char *error, size_t error_cap);
bool md_asset_externalize(const char *workspace_root,
                          const char *document_path,
                          const char *data_uri, size_t data_uri_len,
                          char relative_out[MD_PATH_MAX],
                          char *error, size_t error_cap);
typedef enum {
    MD_RELOCATE_COPY_REBASE,
    MD_RELOCATE_KEEP_REFERENCES
} MdRelocationPolicy;

bool md_document_has_relative_images(const MdDocument *doc);
bool md_save_as_with_relocation(MdDocument *doc, const char *destination,
                                MdRelocationPolicy policy, bool overwrite,
                                char *error, size_t error_cap);
bool md_export_portable_single(const MdDocument *doc, const char *destination,
                               char *error, size_t error_cap);
bool md_export_portable_assets(const MdDocument *doc, const char *destination,
                               char *error, size_t error_cap);

#endif
