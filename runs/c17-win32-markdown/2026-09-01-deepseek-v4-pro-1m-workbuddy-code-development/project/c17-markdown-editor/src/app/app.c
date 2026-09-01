/* app.c - Win32 Markdown editor (Workstream B). */
#include "app.h"
#include "ce_common.h"
#include "buf.h"
#include "utf8.h"
#include "winutil.h"
#include "sha256.h"
#include "stats.h"
#include "search.h"
#include "diff.h"
#include "imgcodec.h"
#include "json.h"
#include "base64.h"
#include <wchar.h>
#include <shellapi.h>
#include <shlobj.h>
#include <time.h>

#define APP_TITLE "C17 Markdown Editor"

extern Theme g_light, g_dark;

/* forward */
void app_render_preview(App *a, DocTab *t, int x, int y, int w, int h);
char *relpath_for(App *a, DocTab *t, const char *subdir, const char *name);
void tree_build_rec(App *a, TreeNode *node, const char *dirpath);

/* ---------------- utf-8 string helpers ---------------- */

static char *path_basename(const char *p){
    const char *s = strrchr(p, '\\');
    if(!s) s = strrchr(p, '/');
    return ce_strdup(s ? s + 1 : p);
}

/* ---------------- document / tab ---------------- */

DocTab *app_active(App *a){
    if(a->ntabs == 0) return NULL;
    if(a->active < 0) a->active = 0;
    if(a->active >= (int)a->ntabs) a->active = (int)a->ntabs - 1;
    return a->tabs[a->active];
}

static DocTab *tab_new(void){
    DocTab *t = ce_calloc(1, sizeof(DocTab));
    md_document_init(&t->doc);
    t->mode = MODE_SOURCE;
    t->zoom = 1.0;
    t->split_ratio = 0.5;
    t->caret = 0; t->sel_start = 0; t->sel_end = 0; t->has_sel = false;
    t->history = md_history_create();
    t->parsed = NULL; t->parsed_dirty = true;
    t->file_exists = false; t->external_conflict = false; t->external_missing = false;
    return t;
}

static void tab_free(DocTab *t){
    if(!t) return;
    md_document_free(&t->doc);
    if(t->history){ md_history_free(t->history); }
    if(t->history_path) ce_free(t->history_path);
    if(t->file_hash) ce_free(t->file_hash);
    if(t->display_name) ce_free(t->display_name);
    if(t->parsed) md_free(t->parsed);
    ce_free(t);
}

static int tab_add(App *a){
    if(a->ntabs == a->cap){ a->cap = a->cap ? a->cap * 2 : 8; a->tabs = ce_realloc(a->tabs, a->cap * sizeof(DocTab*)); }
    DocTab *t = tab_new();
    a->tabs[a->ntabs++] = t;
    a->active = (int)a->ntabs - 1;
    return a->active;
}

void app_reparse(App *a, DocTab *t){
    (void)a;
    if(t->parsed){ md_free(t->parsed); t->parsed = NULL; }
    t->parsed = md_parse(md_document_text(&t->doc), md_document_len(&t->doc));
    t->parsed_dirty = false;
}

/* ---------------- preferences ---------------- */

static void prefs_defaults(Prefs *p){
    memset(p, 0, sizeof(*p));
    p->font_size = 15;
    p->line_spacing = 1;
    p->default_image_mode = 0;
    p->autosave_enabled = true;
    p->autosave_interval = 30;
    p->default_mode = MODE_SOURCE;
    p->sync_scroll = true;
    p->restore_session = true;
}

static void app_load_prefs(App *a){
    prefs_defaults(&a->prefs);
    if(!a->prefs_path) return;
    size_t len = 0;
    char *d = wu_read_file(a->prefs_path, &len);
    if(!d) return;
    ce_arena ar; ce_arena_init(&ar);
    ce_json *j = ce_json_parse(&ar, d, NULL);
    if(j){
        ce_json *v;
        if((v = ce_json_obj_get(j, "dark"))) a->prefs.dark = ce_json_bool(v, false);
        if((v = ce_json_obj_get(j, "font_size"))) a->prefs.font_size = (int)ce_json_int(v, 15);
        if((v = ce_json_obj_get(j, "line_spacing"))) a->prefs.line_spacing = (int)ce_json_int(v, 1);
        if((v = ce_json_obj_get(j, "image_mode"))) a->prefs.default_image_mode = (int)ce_json_int(v, 0);
        if((v = ce_json_obj_get(j, "autosave"))) a->prefs.autosave_enabled = ce_json_bool(v, true);
        if((v = ce_json_obj_get(j, "autosave_interval"))) a->prefs.autosave_interval = (int)ce_json_int(v, 30);
        if((v = ce_json_obj_get(j, "mode"))) a->prefs.default_mode = (int)ce_json_int(v, MODE_SOURCE);
        if((v = ce_json_obj_get(j, "sync_scroll"))) a->prefs.sync_scroll = ce_json_bool(v, true);
        if((v = ce_json_obj_get(j, "restore_session"))) a->prefs.restore_session = ce_json_bool(v, true);
    }
    ce_arena_free(&ar);
    ce_free(d);
    if(a->prefs.font_size < 10) a->prefs.font_size = 10;
    if(a->prefs.font_size > 32) a->prefs.font_size = 32;
    if(a->prefs.autosave_interval < 10) a->prefs.autosave_interval = 10;
    if(a->prefs.autosave_interval > 300) a->prefs.autosave_interval = 300;
}

void app_save_prefs(App *a){
    if(!a->prefs_path) return;
    ce_arena ar; ce_arena_init(&ar);
    ce_json *j = ce_json_new_obj(&ar);
    ce_json_obj_set(&ar, j, "dark", ce_json_new_bool(&ar, a->prefs.dark));
    ce_json_obj_set(&ar, j, "font_size", ce_json_new_int(&ar, a->prefs.font_size));
    ce_json_obj_set(&ar, j, "line_spacing", ce_json_new_int(&ar, a->prefs.line_spacing));
    ce_json_obj_set(&ar, j, "image_mode", ce_json_new_int(&ar, a->prefs.default_image_mode));
    ce_json_obj_set(&ar, j, "autosave", ce_json_new_bool(&ar, a->prefs.autosave_enabled));
    ce_json_obj_set(&ar, j, "autosave_interval", ce_json_new_int(&ar, a->prefs.autosave_interval));
    ce_json_obj_set(&ar, j, "mode", ce_json_new_int(&ar, a->prefs.default_mode));
    ce_json_obj_set(&ar, j, "sync_scroll", ce_json_new_bool(&ar, a->prefs.sync_scroll));
    ce_json_obj_set(&ar, j, "restore_session", ce_json_new_bool(&ar, a->prefs.restore_session));
    char *s = ce_json_to_string(j);
    wu_write_file(a->prefs_path, s, strlen(s));
    ce_free(s);
    ce_arena_free(&ar);
}

/* ---------------- history association path ---------------- */

static char *history_path_for(App *a, DocTab *t){
    /* workspace docs: .mdeditor/history/<sha256-of-relpath>.h */
    const char *docpath = t->doc.path;
    if(!docpath) return NULL;
    char *dir = NULL;
    if(a->workspace_root){
        size_t rl = strlen(a->workspace_root);
        if(strncmp(docpath, a->workspace_root, rl) == 0 && (docpath[rl] == '\\' || docpath[rl] == '/')){
            /* relative path for key */
            const char *rel = docpath + rl + 1;
            uint8_t sha[32]; ce_sha256_hash(rel, strlen(rel), sha);
            char hex[65];
            for(int i=0;i<32;i++){ hex[i*2]="0123456789abcdef"[sha[i]>>4]; hex[i*2+1]="0123456789abcdef"[sha[i]&15]; }
            hex[64]=0;
            ce_buf p; ce_buf_init(&p);
            ce_buf_append_str(&p, a->workspace_root);
            ce_buf_append_str(&p, "\\.mdeditor\\history\\");
            ce_buf_append_str(&p, hex);
            ce_buf_append_str(&p, ".h");
            dir = ce_buf_detach(&p);
        }
    }
    if(!dir){
        /* standalone: LocalAppData recovery/standalone history */
        char appdata[MAX_PATH];
        if(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) == S_OK){
            uint8_t sha[32]; ce_sha256_hash(docpath, strlen(docpath), sha);
            char hex[65];
            for(int i=0;i<32;i++){ hex[i*2]="0123456789abcdef"[sha[i]>>4]; hex[i*2+1]="0123456789abcdef"[sha[i]&15]; }
            hex[64]=0;
            ce_buf p; ce_buf_init(&p);
            ce_buf_append_str(&p, appdata);
            ce_buf_append_str(&p, "\\C17MarkdownEditor\\History\\");
            ce_buf_append_str(&p, hex);
            ce_buf_append_str(&p, ".h");
            dir = ce_buf_detach(&p);
        }
    }
    return dir;
}

void app_load_history(App *a, DocTab *t){
    if(t->history_path) ce_free(t->history_path);
    t->history_path = history_path_for(a, t);
    if(!t->history_path) return;
    size_t len = 0;
    char *d = wu_read_file(t->history_path, &len);
    if(d){
        md_history *h = md_history_load((unsigned char*)d, len);
        if(h){ md_history_free(t->history); t->history = h; }
        ce_free(d);
    }
}

void app_save_history(App *a, DocTab *t){
    if(!t || !t->history || !t->history_path) return;
    /* ensure dir */
    char *dir = ce_strdup(t->history_path);
    char *slash = strrchr(dir, '\\');
    if(slash){ *slash = 0; wchar_t *w = wu_u8_to_w(dir); if(w){ SHCreateDirectoryExW(NULL, w, NULL); ce_free(w); } }
    ce_free(dir);
    size_t len = 0;
    unsigned char *data = md_history_serialize(t->history, &len);
    if(data){ wu_write_file(t->history_path, data, len); ce_free(data); }
    (void)a;
}

void app_commit_history(App *a, DocTab *t){
    if(!t || !t->history) return;
    /* add version if differs from latest */
    size_t n = md_history_count(t->history);
    if(n > 0){
        size_t llen = 0;
        char *last = md_history_get(t->history, n - 1, &llen);
        if(last){
            const char *cur = md_document_text(&t->doc);
            if(llen == md_document_len(&t->doc) && memcmp(last, cur, llen) == 0){
                ce_free(last);
                return; /* unchanged */
            }
            ce_free(last);
        }
    }
    md_history_add(t->history, md_document_text(&t->doc), md_document_len(&t->doc), (uint64_t)time(NULL));
    app_save_history(a, t);
}

/* ---------------- recovery ---------------- */

static char *recovery_dir(App *a){
    (void)a;
    char appdata[MAX_PATH];
    if(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) != S_OK) return NULL;
    ce_buf p; ce_buf_init(&p);
    ce_buf_append_str(&p, appdata);
    ce_buf_append_str(&p, "\\C17MarkdownEditor\\Recovery");
    return ce_buf_detach(&p);
}

void app_autosave(App *a, DocTab *t){
    if(!t || !t->doc.dirty) return;
    char *dir = recovery_dir(a);
    if(!dir) return;
    wchar_t *wd = wu_u8_to_w(dir);
    if(wd){ SHCreateDirectoryExW(NULL, wd, NULL); ce_free(wd); }
    /* recovery record filename derived from path or untitled id */
    char key[64];
    if(t->doc.path){
        uint8_t sha[32]; ce_sha256_hash(t->doc.path, strlen(t->doc.path), sha);
        snprintf(key, sizeof(key), "%02x%02x%02x%02x", sha[0], sha[1], sha[2], sha[3]);
    } else {
        snprintf(key, sizeof(key), "untitled_%d", a->active);
    }
    ce_buf p; ce_buf_init(&p);
    ce_buf_append_str(&p, dir);
    ce_buf_append_fmt(&p, "\\%s.rc", key);
    /* record format: magic + version + path + content */
    ce_buf rec; ce_buf_init(&rec);
    ce_buf_append_str(&rec, "RCRD\x01");
    ce_buf_append_fmt(&rec, "%s\x00", t->doc.path ? t->doc.path : "");
    ce_buf_append(&rec, md_document_text(&t->doc), md_document_len(&t->doc));
    uint8_t sha[32]; ce_sha256_hash(rec.data, rec.len, sha);
    ce_buf_append(&rec, sha, 32);
    wu_write_file(p.data, rec.data, rec.len);
    ce_buf_free(&rec);
    ce_buf_free(&p);
    ce_free(dir);
}

/* ---------------- file operations ---------------- */

static void app_open_from_disk(App *a, const char *path){
    /* dedupe by case-insensitive path */
    for(size_t i = 0; i < a->ntabs; i++){
        if(a->tabs[i]->doc.path && ce_strcasecmp(a->tabs[i]->doc.path, path) == 0){
            a->active = (int)i;
            return;
        }
    }
    size_t len = 0;
    char *data = wu_read_file(path, &len);
    if(!data){
        app_show_modal(a, 7, "Cannot open file:\n\nThis file could not be read.");
        return;
    }
    if(!ce_utf8_valid((uint8_t*)data, len)){
        ce_free(data);
        app_show_modal(a, 7, "Invalid UTF-8:\n\nThe file is not valid UTF-8 and was not opened.");
        return;
    }
    int idx = tab_add(a);
    DocTab *t = a->tabs[idx];
    md_document_set_source(&t->doc, data, len);
    md_document_set_clean(&t->doc);
    ce_free(data);
    t->doc.path = ce_strdup(path);
    t->display_name = path_basename(path);
    t->file_exists = true;
    t->mode = a->prefs.default_mode;
    app_load_history(a, t);
    app_reparse(a, t);
}

void app_open_file(App *a, const wchar_t *path){
    char *u8 = wu_w_to_u8(path);
    if(u8){ app_open_from_disk(a, u8); ce_free(u8); }
}

void app_new_document(App *a){
    int idx = tab_add(a);
    DocTab *t = a->tabs[idx];
    a->untitled_counter++;
    ce_buf nm; ce_buf_init(&nm);
    ce_buf_append_fmt(&nm, "Untitled %llu", (unsigned long long)a->untitled_counter);
    t->display_name = ce_buf_detach(&nm);
    t->mode = a->prefs.default_mode;
    app_reparse(a, t);
}

/* safe save: stage + flush + replace */
static bool safe_save(App *a, DocTab *t, const char *path, char **errmsg){
    (void)a;
    const char *data = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    /* stage to temp in same dir */
    char *dir = ce_strdup(path);
    char *slash = strrchr(dir, '\\');
    if(slash) *slash = 0; else { ce_free(dir); dir = ce_strdup("."); }
    ce_buf tmp; ce_buf_init(&tmp);
    ce_buf_append_str(&tmp, dir);
    ce_buf_append_fmt(&tmp, "\\.md_tmp_%u_%u", (unsigned)GetCurrentProcessId(), (unsigned)GetTickCount());
    wchar_t *wtmp = wu_u8_to_native(tmp.data);
    wchar_t *wpath = wu_u8_to_native(path);
    bool ok = false;
    char emsg[512]; emsg[0] = 0;
    if(!wtmp || !wpath){ strcpy(emsg, "Path conversion failed."); }
    else {
        HANDLE h = CreateFileW(wtmp, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if(h == INVALID_HANDLE_VALUE){
            DWORD e = GetLastError();
            snprintf(emsg, sizeof(emsg), "Cannot create temporary file (error %lu).", (unsigned long)e);
        } else {
            DWORD wr = 0; size_t off = 0; bool all = true;
            while(off < len){
                DWORD chunk = (DWORD)((len - off) > 0x40000000 ? 0x40000000 : (len - off));
                if(!WriteFile(h, data + off, chunk, &wr, NULL) || wr == 0){ all = false; snprintf(emsg, sizeof(emsg), "Partial write failure."); break; }
                off += wr;
            }
            if(all && !FlushFileBuffers(h)){ all = false; snprintf(emsg, sizeof(emsg), "Flush failure."); }
            CloseHandle(h);
            if(all){
                if(!MoveFileExW(wtmp, wpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)){
                    DWORD e = GetLastError();
                    snprintf(emsg, sizeof(emsg), "Replace failed (error %lu). The original file is unchanged.", (unsigned long)e);
                } else ok = true;
            }
            if(!ok) DeleteFileW(wtmp);
        }
    }
    if(wtmp) ce_free(wtmp);
    if(wpath) ce_free(wpath);
    ce_buf_free(&tmp);
    ce_free(dir);
    if(errmsg){ *errmsg = emsg[0] ? ce_strdup(emsg) : NULL; }
    return ok;
}

void app_save(App *a, bool save_as){
    DocTab *t = app_active(a);
    if(!t) return;
    if(save_as || !t->doc.path || t->external_missing){
        /* use native save dialog */
        wchar_t fname[MAX_PATH] = {0};
        OPENFILENAMEW ofn; memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = a->hwnd;
        ofn.lpstrFilter = L"Markdown\0*.md;*.markdown;*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = fname;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        if(!GetSaveFileNameW(&ofn)) return;
        char *u8 = wu_w_to_u8(fname);
        char *err = NULL;
        if(safe_save(a, t, u8, &err)){
            if(t->doc.path) ce_free(t->doc.path);
            t->doc.path = ce_strdup(u8);
            if(t->display_name) ce_free(t->display_name);
            t->display_name = path_basename(u8);
            t->file_exists = true;
            md_document_set_clean(&t->doc);
            app_commit_history(a, t);
        } else {
            app_show_modal(a, 7, err ? err : "Save failed.");
        }
        if(err) ce_free(err);
        ce_free(u8);
    } else {
        char *err = NULL;
        if(safe_save(a, t, t->doc.path, &err)){
            t->file_exists = true;
            t->external_conflict = false;
            md_document_set_clean(&t->doc);
            app_commit_history(a, t);
        } else {
            app_show_modal(a, 7, err ? err : "Save failed.");
        }
        if(err) ce_free(err);
    }
}

void app_close_tab(App *a, int idx){
    if(idx < 0 || idx >= (int)a->ntabs) return;
    DocTab *t = a->tabs[idx];
    if(t->doc.dirty){
        app_show_modal(a, 8, "");
        a->unsaved_idx = idx;
        return;
    }
    tab_free(t);
    memmove(&a->tabs[idx], &a->tabs[idx+1], (a->ntabs - idx - 1) * sizeof(DocTab*));
    a->ntabs--;
    if(a->active >= (int)a->ntabs) a->active = (int)a->ntabs - 1;
    if(a->active < 0) a->active = 0;
}

void app_switch_tab(App *a, int idx){
    if(idx < 0 || idx >= (int)a->ntabs) return;
    a->active = idx;
}

void app_set_mode(App *a, int mode){
    DocTab *t = app_active(a);
    if(!t) return;
    t->mode = mode;
    app_reparse(a, t);
}

/* ---------------- find / replace ---------------- */

void app_find_next(App *a, int dir){
    DocTab *t = app_active(a);
    if(!t || !a->find_query[0]) return;
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    size_t ql = strlen(a->find_query);
    size_t from = t->caret;
    long r;
    if(dir >= 0){
        r = md_find_next(src, len, a->find_query, ql, a->find_case, a->find_word, from);
        if(r < 0) r = md_find_next(src, len, a->find_query, ql, a->find_case, a->find_word, 0); /* wrap */
    } else {
        r = md_find_prev(src, len, a->find_query, ql, a->find_case, a->find_word, from > 0 ? from - 1 : 0);
        if(r < 0) r = md_find_prev(src, len, a->find_query, ql, a->find_case, a->find_word, len); /* wrap */
    }
    if(r >= 0){
        t->sel_start = (size_t)r; t->sel_end = (size_t)r + ql; t->has_sel = true;
        t->caret = (size_t)r + ql;
    }
}

void app_replace_one(App *a){
    DocTab *t = app_active(a);
    if(!t || !a->find_query[0]) return;
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    size_t ql = strlen(a->find_query);
    if(t->has_sel){
        size_t s = t->sel_start, e = t->sel_end;
        if(e - s == ql){
            /* replace the active match */
            md_document_replace(&t->doc, s, ql, a->find_repl, strlen(a->find_repl));
            t->caret = s + strlen(a->find_repl);
            t->has_sel = false;
            t->parsed_dirty = true;
        }
    }
    app_find_next(a, 1);
    (void)src; (void)len;
}

void app_replace_all(App *a){
    DocTab *t = app_active(a);
    if(!t || !a->find_query[0]) return;
    const char *src = md_document_text(&t->doc);
    size_t len = md_document_len(&t->doc);
    size_t ql = strlen(a->find_query);
    md_match *m = NULL;
    size_t n = md_find_all(src, len, a->find_query, ql, a->find_case, a->find_word, &m);
    if(n == 0){ if(m) ce_free(m); return; }
    md_document_edit_begin(&t->doc);
    for(size_t i = 0; i < n; i++){
        md_document_edit_op(&t->doc, m[i].pos, m[i].len, a->find_repl, strlen(a->find_repl));
    }
    md_document_edit_end(&t->doc);
    if(m) ce_free(m);
    t->has_sel = false;
    t->parsed_dirty = true;
}

/* ---------------- inline formatting ---------------- */

void app_apply_fmt(App *a, int fmt){
    DocTab *t = app_active(a);
    if(!t) return;
    size_t s = t->has_sel ? t->sel_start : t->caret;
    size_t e = t->has_sel ? t->sel_end : t->caret;
    if(s > e){ size_t x = s; s = e; e = x; }
    const char *open = NULL, *close = NULL;
    switch(fmt){
        case 1: open = "**"; close = "**"; break;    /* bold */
        case 2: open = "*"; close = "*"; break;      /* italic */
        case 3: open = "~~"; close = "~~"; break;    /* strikethrough */
        case 4: open = "`"; close = "`"; break;      /* inline code */
        default: return;
    }
    const char *src = md_document_text(&t->doc);
    size_t ol = strlen(open), cl = strlen(close);
    /* toggle: if already wrapped, remove */
    if(s >= ol && e + cl <= md_document_len(&t->doc) &&
       memcmp(src + s - ol, open, ol) == 0 && memcmp(src + e, close, cl) == 0){
        md_document_edit_begin(&t->doc);
        md_document_edit_op(&t->doc, e, cl, "", 0);
        md_document_edit_op(&t->doc, s - ol, ol, "", 0);
        md_document_edit_end(&t->doc);
        t->caret = s - ol;
        t->sel_start = s - ol; t->sel_end = e - ol; t->has_sel = true;
    } else {
        md_document_edit_begin(&t->doc);
        md_document_edit_op(&t->doc, e, 0, close, cl);
        md_document_edit_op(&t->doc, s, 0, open, ol);
        md_document_edit_end(&t->doc);
        t->caret = s + ol;
        t->sel_start = s + ol; t->sel_end = e + ol; t->has_sel = true;
    }
    t->parsed_dirty = true;
}

/* ---------------- text edit helpers ---------------- */

void app_insert_text(App *a, DocTab *t, size_t pos, const char *text, size_t len){
    md_document_insert(&t->doc, pos, text, len);
    t->caret = pos + len;
    t->has_sel = false;
    t->sel_start = t->sel_end = t->caret;
    t->parsed_dirty = true;
    (void)a;
}

void app_delete_range(App *a, DocTab *t, size_t start, size_t end){
    if(start == end) return;
    if(start > end){ size_t x = start; start = end; end = x; }
    md_document_delete(&t->doc, start, end - start);
    t->caret = start;
    t->has_sel = false;
    t->sel_start = t->sel_end = start;
    t->parsed_dirty = true;
    (void)a;
}

/* ---------------- modal / overlay ---------------- */

void app_show_modal(App *a, int modal, const char *text){
    a->modal = modal;
    if(text){ strncpy(a->modal_text, text, sizeof(a->modal_text) - 1); a->modal_text[sizeof(a->modal_text)-1] = 0; }
    else a->modal_text[0] = 0;
    a->modal_anim = 0.0;
}

/* ---------------- command palette ---------------- */

static const Command g_commands[] = {
    {"new", "New Document", NULL, NULL},
    {"open", "Open File", NULL, NULL},
    {"open-workspace", "Open Workspace", NULL, NULL},
    {"save", "Save", NULL, NULL},
    {"save-as", "Save As", NULL, NULL},
    {"save-all", "Save All", NULL, NULL},
    {"close-tab", "Close Tab", NULL, NULL},
    {"find", "Find", NULL, NULL},
    {"replace", "Replace", NULL, NULL},
    {"mode-source", "Toggle Source Mode", NULL, NULL},
    {"mode-split", "Toggle Split Mode", NULL, NULL},
    {"mode-preview", "Toggle Preview Mode", NULL, NULL},
    {"mode-rendered", "Toggle Rendered Editing Mode", NULL, NULL},
    {"insert-image", "Insert Image", NULL, NULL},
    {"stats", "Document Statistics", NULL, NULL},
    {"history", "Version History", NULL, NULL},
    {"sidebar-files", "Toggle Files Sidebar", NULL, NULL},
    {"sidebar-outline", "Toggle Outline Sidebar", NULL, NULL},
    {"sync-scroll", "Toggle Synchronized Scrolling", NULL, NULL},
    {"theme", "Toggle Light/Dark Theme", NULL, NULL},
    {"prefs", "Preferences", NULL, NULL},
    {"shortcuts", "Shortcut Reference", NULL, NULL},
};

const Command *app_commands(void){ return g_commands; }
int app_command_count(void){ return (int)CE_ARRAY_LEN(g_commands); }

void app_do_command(App *a, const char *id){
    if(strcmp(id, "new") == 0){ app_new_document(a); }
    else if(strcmp(id, "open") == 0){
        wchar_t fname[MAX_PATH] = {0};
        OPENFILENAMEW ofn; memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = a->hwnd;
        ofn.lpstrFilter = L"Markdown\0*.md;*.markdown;*.txt\0All Files\0*.*\0";
        ofn.lpstrFile = fname; ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&ofn)) app_open_file(a, fname);
    }
    else if(strcmp(id, "open-workspace") == 0){
        BROWSEINFOW bi; memset(&bi, 0, sizeof(bi));
        bi.hwndOwner = a->hwnd; bi.lpszTitle = L"Select Workspace Folder";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if(pidl){
            wchar_t path[MAX_PATH];
            if(SHGetPathFromIDListW(pidl, path)) app_open_workspace(a, path);
            CoTaskMemFree(pidl);
        }
    }
    else if(strcmp(id, "save") == 0) app_save(a, false);
    else if(strcmp(id, "save-as") == 0) app_save(a, true);
    else if(strcmp(id, "close-tab") == 0) app_close_tab(a, a->active);
    else if(strcmp(id, "find") == 0){ a->find_open = true; a->find_replace = false; }
    else if(strcmp(id, "replace") == 0){ a->find_open = true; a->find_replace = true; }
    else if(strcmp(id, "mode-source") == 0) app_set_mode(a, MODE_SOURCE);
    else if(strcmp(id, "mode-split") == 0) app_set_mode(a, MODE_SPLIT);
    else if(strcmp(id, "mode-preview") == 0) app_set_mode(a, MODE_PREVIEW);
    else if(strcmp(id, "mode-rendered") == 0) app_set_mode(a, MODE_RENDERED);
    else if(strcmp(id, "stats") == 0){
        DocTab *t = app_active(a);
        if(t){ app_reparse(a, t); md_stats_compute(md_document_text(&t->doc), md_document_len(&t->doc), t->parsed, &a->stats); app_show_modal(a, 2, ""); }
    }
    else if(strcmp(id, "history") == 0){ a->modal = 3; a->hist_sel = (int)md_history_count(app_active(a)->history) - 1; a->modal_anim = 0; }
    else if(strcmp(id, "sidebar-files") == 0){ a->sidebar_tab = 0; a->sidebar_visible = !a->sidebar_visible; }
    else if(strcmp(id, "sidebar-outline") == 0){ a->sidebar_tab = 1; a->sidebar_visible = true; }
    else if(strcmp(id, "sync-scroll") == 0) a->prefs.sync_scroll = !a->prefs.sync_scroll;
    else if(strcmp(id, "theme") == 0){ a->prefs.dark = !a->prefs.dark; a->theme = a->prefs.dark ? &g_dark : &g_light; app_save_prefs(a); }
    else if(strcmp(id, "prefs") == 0) app_show_modal(a, 5, "");
    else if(strcmp(id, "shortcuts") == 0) app_show_modal(a, 12, "");
    else if(strcmp(id, "insert-image") == 0){
        wchar_t fname[MAX_PATH] = {0};
        OPENFILENAMEW ofn; memset(&ofn, 0, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = a->hwnd;
        ofn.lpstrFilter = L"Images\0*.png;*.jpg;*.jpeg;*.bmp\0All Files\0*.*\0";
        ofn.lpstrFile = fname; ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST;
        if(GetOpenFileNameW(&ofn)) app_image_insert(a, fname);
    }
}

/* ---------------- image insertion ---------------- */

void app_image_insert(App *a, const wchar_t *path){
    DocTab *t = app_active(a);
    if(!t) return;
    char *u8 = wu_w_to_u8(path);
    if(!u8) return;
    size_t ilen = 0;
    char *idata = wu_read_file(u8, &ilen);
    if(!idata){ ce_free(u8); return; }
    int fmt = 0;
    if(ce_ends_with(u8, ".png")) fmt = IMG_FMT_PNG;
    else if(ce_ends_with(u8, ".jpg") || ce_ends_with(u8, ".jpeg")) fmt = IMG_FMT_JPEG;
    else if(ce_ends_with(u8, ".bmp")) fmt = IMG_FMT_BMP;
    if(!fmt){ ce_free(idata); ce_free(u8); return; }
    int w = 0, h = 0;
    uint8_t *px = img_decode((unsigned char*)idata, ilen, &w, &h);
    ce_free(idata);
    if(!px){ ce_free(u8); app_show_modal(a, 7, "Image decode failed."); return; }
    ce_free(px);

    char *alt = path_basename(u8);
    size_t caret = t->caret;
    ce_buf ins; ce_buf_init(&ins);
    if(a->prefs.default_image_mode == 1 || !a->workspace_root){
        /* embed as base64 data URI */
        size_t blen = 0;
        char *bdata = wu_read_file(u8, &blen);
        if(bdata){
            char *b64 = ce_base64_encode((unsigned char*)bdata, blen);
            ce_buf_append_fmt(&ins, "![%s](data:image/%s;base64,%s)\n", alt, img_mime(fmt), b64);
            ce_free(b64);
            ce_free(bdata);
        }
    } else {
        /* copy into workspace assets/ and reference relatively */
        const char *name = path_basename(u8);
        /* compute asset absolute path + collision-safe name */
        char *bn = ce_strdup(name);
        ce_buf dst; ce_buf_init(&dst);
        ce_buf_append_str(&dst, a->workspace_root);
        ce_buf_append_str(&dst, "\\assets\\");
        ce_buf_append_str(&dst, bn);
        if(wu_exists(dst.data)){
            ce_free(bn);
            ce_buf nb; ce_buf_init(&nb);
            ce_buf_append_fmt(&nb, "img_%u_%s", (unsigned)GetTickCount(), name);
            bn = ce_buf_detach(&nb);
            ce_buf_clear(&dst);
            ce_buf_append_str(&dst, a->workspace_root);
            ce_buf_append_str(&dst, "\\assets\\");
            ce_buf_append_str(&dst, bn);
        }
        /* create assets dir */
        ce_buf adir; ce_buf_init(&adir);
        ce_buf_append_str(&adir, a->workspace_root);
        ce_buf_append_str(&adir, "\\assets");
        wchar_t *wad = wu_u8_to_w(adir.data);
        if(wad){ SHCreateDirectoryExW(NULL, wad, NULL); ce_free(wad); }
        ce_buf_free(&adir);
        size_t slen = 0;
        char *src = wu_read_file(u8, &slen);
        if(src){ wu_write_file(dst.data, src, slen); ce_free(src); }
        /* relative path from doc dir to asset */
        char *rel = relpath_for(a, t, "assets", bn);
        if(rel) ce_buf_append_fmt(&ins, "![%s](%s)\n", alt, rel);
        else ce_buf_append_fmt(&ins, "![%s](assets/%s)\n", alt, bn);
        if(rel) ce_free(rel);
        ce_free(bn);
        ce_buf_free(&dst);
    }
    ce_free(alt);
    if(ins.len == 0){ ce_buf_append_fmt(&ins, "![%s](%s)\n", "image", u8); }
    md_document_insert(&t->doc, caret, ins.data, ins.len);
    t->caret = caret + ins.len;
    t->parsed_dirty = true;
    ce_buf_free(&ins);
    ce_free(u8);
}

/* ---------------- workspace ---------------- */

static void tree_free(TreeNode *n){
    if(!n) return;
    for(size_t i = 0; i < n->nchildren; i++) tree_free(n->children[i]);
    if(n->children) ce_free(n->children);
    if(n->name) ce_free(n->name);
    if(n->relpath) ce_free(n->relpath);
    ce_free(n);
}

static int name_cmp(const void *x, const void *y){
    const TreeNode *a = *(const TreeNode**)x, *b = *(const TreeNode**)y;
    if(a->is_dir != b->is_dir) return a->is_dir ? -1 : 1; /* dirs first */
    return ce_strcasecmp(a->name, b->name);
}

static TreeNode *tree_insert(TreeNode *parent, const char *name, bool is_dir, const char *relpath){
    for(size_t i = 0; i < parent->nchildren; i++){
        TreeNode *c = parent->children[i];
        if(ce_strcasecmp(c->name, name) == 0 && c->is_dir == is_dir) return c;
    }
    if(parent->nchildren == parent->cap){ parent->cap = parent->cap ? parent->cap*2 : 16; parent->children = ce_realloc(parent->children, parent->cap * sizeof(TreeNode*)); }
    TreeNode *n = ce_calloc(1, sizeof(TreeNode));
    n->name = ce_strdup(name);
    n->is_dir = is_dir;
    n->relpath = ce_strdup(relpath);
    n->depth = parent->depth + 1;
    n->parent = parent;
    parent->children[parent->nchildren++] = n;
    qsort(parent->children, parent->nchildren, sizeof(TreeNode*), name_cmp);
    return n;
}

static void tree_build(App *a){
    if(a->tree_root){ tree_free(a->tree_root); a->tree_root = NULL; }
    a->tree_root = ce_calloc(1, sizeof(TreeNode));
    a->tree_root->name = ce_strdup("workspace");
    a->tree_root->is_dir = true;
    a->tree_root->expanded = true;
    a->tree_root->depth = -1;
    if(!a->workspace_root) return;
    /* walk */
    struct { App *a; } ctx = {a};
    /* use wu_walk_dir but need nested tree building */
    /* collect all files/dirs recursively */
    size_t rl = strlen(a->workspace_root);
    /* simple recursive builder */
    extern void tree_build_rec(App *a, TreeNode *node, const char *dirpath);
    tree_build_rec(a, a->tree_root, a->workspace_root);
    (void)rl; (void)ctx;
}

void tree_build_rec(App *a, TreeNode *node, const char *dirpath){
    /* enumerate dir contents */
    ce_buf pat; ce_buf_init(&pat);
    ce_buf_append_str(&pat, dirpath);
    ce_buf_append_str(&pat, "\\*");
    wchar_t *wpat = wu_u8_to_w(pat.data);
    ce_buf_free(&pat);
    if(!wpat) return;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wpat, &fd);
    ce_free(wpat);
    if(h == INVALID_HANDLE_VALUE) return;
    /* gather names first for deterministic order */
    char **names = NULL; size_t nn = 0, ncap = 0;
    bool *isdir = NULL;
    do {
        if(wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        char *nm = wu_w_to_u8(fd.cFileName);
        if(strcmp(nm, ".mdeditor") == 0){ ce_free(nm); continue; }
        bool dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        bool reparse = (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        if(dir && reparse) continue; /* don't follow reparse dirs */
        if(nn == ncap){ ncap = ncap ? ncap*2 : 32; names = ce_realloc(names, ncap*sizeof(char*)); isdir = ce_realloc(isdir, ncap*sizeof(bool)); }
        names[nn] = nm; isdir[nn] = dir; nn++;
    } while(FindNextFileW(h, &fd));
    FindClose(h);
    /* sort by dir-first then name */
    for(size_t i = 0; i < nn; i++){
        for(size_t j = i+1; j < nn; j++){
            if((!isdir[i] && isdir[j]) || ((isdir[i]==isdir[j]) && ce_strcasecmp(names[i], names[j]) > 0)){
                char *t = names[i]; names[i] = names[j]; names[j] = t;
                bool td = isdir[i]; isdir[i] = isdir[j]; isdir[j] = td;
            }
        }
    }
    size_t rl = strlen(a->workspace_root);
    for(size_t i = 0; i < nn; i++){
        /* relpath */
        ce_buf full; ce_buf_init(&full);
        ce_buf_append_str(&full, dirpath);
        ce_buf_append_c(&full, '\\');
        ce_buf_append_str(&full, names[i]);
        const char *rel = full.data + rl + 1;
        ce_buf reln; ce_buf_init(&reln);
        for(const char *p = rel; *p; p++) ce_buf_append_c(&reln, (*p=='\\')?'/':*p);
        TreeNode *c = tree_insert(node, names[i], isdir[i], reln.data);
        ce_buf_free(&reln);
        if(isdir[i]) tree_build_rec(a, c, full.data);
        ce_buf_free(&full);
    }
    for(size_t i = 0; i < nn; i++) ce_free(names[i]);
    if(names) ce_free(names);
    if(isdir) ce_free(isdir);
}

void app_open_workspace(App *a, const wchar_t *path){
    char *u8 = wu_w_to_u8(path);
    if(!u8) return;
    if(!wu_is_dir(u8)){ ce_free(u8); return; }
    if(a->workspace_root) ce_free(a->workspace_root);
    a->workspace_root = ce_strdup(u8);
    ce_free(u8);
    a->sidebar_visible = true;
    tree_build(a);
    /* add to recent workspaces */
}

/* ---------------- app lifecycle ---------------- */

void app_invalidate(App *a){ InvalidateRect(a->hwnd, NULL, FALSE); }

void app_init(App *a){
    memset(a, 0, sizeof(*a));
    a->prefs_path = NULL;
    char appdata[MAX_PATH];
    if(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK){
        ce_buf p; ce_buf_init(&p);
        ce_buf_append_str(&p, appdata);
        ce_buf_append_str(&p, "\\C17MarkdownEditor\\prefs.json");
        a->prefs_path = ce_buf_detach(&p);
    }
    app_load_prefs(a);
    a->theme = a->prefs.dark ? &g_dark : &g_light;
    a->width = 1280; a->height = 800;
    a->scale = 1.0; a->dpi = 96;
    a->sidebar_visible = true;
    a->sidebar_width = 240;
    a->sidebar_tab = 0;
    a->modal = 0;
    a->untitled_counter = 0;
    a->running = true;
}

/* ------------------------------------------------------------ screenshot */

void app_capture_shot(App *a){
    if(!a->shot_out) return;
    /* encode framebuffer to PNG */
    int w = a->fb_w, h = a->fb_h;
    uint8_t *rgba = ce_malloc((size_t)w * h * 4);
    uint32_t *fb = (uint32_t*)a->fb;
    for(int i = 0; i < w * h; i++){
        uint32_t p = fb[i];
        rgba[i*4+0] = (p >> 16) & 0xFF;
        rgba[i*4+1] = (p >> 8) & 0xFF;
        rgba[i*4+2] = p & 0xFF;
        rgba[i*4+3] = 0xFF;
    }
    size_t len = 0;
    unsigned char *png = img_encode(rgba, w, h, IMG_FMT_PNG, &len);
    ce_free(rgba);
    if(png){ wu_write_file(a->shot_out, png, len); ce_free(png); }
    a->running = false;
    PostMessageW(a->hwnd, WM_CLOSE, 0, 0);
}

/* The main render + input functions are implemented in view.c and input.c. */

/* ---------------- screenshot setup ---------------- */

void app_setup_screenshot(App *a, const char *id);

static void load_into(App *a, DocTab *t, const char *path){
    size_t len=0; char *d = wu_read_file(path, &len);
    if(!d) return;
    md_document_set_source(&t->doc, d, len);
    md_document_set_clean(&t->doc);
    t->doc.path = ce_strdup(path);
    if(t->display_name) ce_free(t->display_name);
    t->display_name = path_basename(path);
    ce_free(d);
    app_reparse(a, t);
}

void app_setup_screenshot(App *a, const char *id){
    /* theme */
    bool dark = (strstr(id, "DARK") != NULL);
    if(strstr(id, "DPI-SCALED") == NULL){
        a->prefs.dark = dark;
        a->theme = dark ? &g_dark : &g_light;
    }
    /* close all existing tabs */
    while(a->ntabs > 0) tab_free(a->tabs[--a->ntabs]);
    a->active = 0;

    if(strcmp(id, "UI-EMPTY-LIGHT") == 0 || strcmp(id, "UI-EMPTY-DARK") == 0){
        app_new_document(a);
    } else if(strcmp(id, "UI-WORKSPACE-MULTITAB") == 0){
        app_open_workspace(a, L"D:/fixtures_out/workspace");
        app_open_file(a, L"D:/fixtures_out/workspace/README.md");
        app_new_document(a);
        DocTab *t = a->tabs[a->active];
        load_into(a, t, "D:/fixtures_out/workspace/docs/guide.md");
    } else if(strcmp(id, "UI-SOURCE") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_SOURCE;
    } else if(strcmp(id, "UI-SPLIT") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_SPLIT;
    } else if(strcmp(id, "UI-PREVIEW") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_PREVIEW;
    } else if(strcmp(id, "UI-RENDERED-EDIT") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_RENDERED;
    } else if(strcmp(id, "UI-MARKDOWN-ALL") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_PREVIEW;
    } else if(strcmp(id, "UI-OUTLINE") == 0){
        app_open_workspace(a, L"D:/fixtures_out/workspace");
        app_open_file(a, L"D:/fixtures_out/workspace/docs/guide.md");
        a->tabs[a->active]->mode = MODE_SOURCE;
        a->sidebar_tab = 1;
    } else if(strcmp(id, "UI-STATISTICS") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        DocTab *t = a->tabs[a->active];
        app_reparse(a, t);
        md_stats_compute(md_document_text(&t->doc), md_document_len(&t->doc), t->parsed, &a->stats);
        a->modal = 2;
    } else if(strcmp(id, "UI-VERSION-HISTORY") == 0){
        app_new_document(a);
        DocTab *t = a->tabs[a->active];
        load_into(a, t, "D:/fixtures_out/markdown-all/markdown-all.md");
        app_commit_history(a, t);
        load_into(a, t, "D:/fixtures_out/markdown-all/markdown-all.md");
        app_commit_history(a, t);
        a->modal = 3; a->hist_sel = (int)md_history_count(t->history) - 1;
    } else if(strcmp(id, "UI-DIFF-SIDE-BY-SIDE") == 0 || strcmp(id, "UI-DIFF-INLINE") == 0){
        a->modal = 4; a->diff_mode = (strcmp(id, "UI-DIFF-INLINE") == 0) ? 1 : 0;
    } else if(strcmp(id, "UI-MODAL-BLUR") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        snprintf(a->modal_text, sizeof(a->modal_text),
            "This operation cannot be completed because the file is read-only.\n\nClick 'Continue' to override (not recommended) or 'Cancel' to abort.");
        a->modal = 7;
    } else if(strcmp(id, "UI-FROSTED-SCROLLED") == 0){
        app_open_workspace(a, L"D:/fixtures_out/workspace");
        app_open_file(a, L"D:/fixtures_out/workspace/docs/guide.md");
        a->tabs[a->active]->mode = MODE_SOURCE;
        a->tabs[a->active]->scroll_y = 600;
        a->nav_scroll = 1.0;
    } else if(strcmp(id, "UI-EXTERNAL-CONFLICT") == 0){
        app_new_document(a);
        DocTab *t = a->tabs[a->active];
        load_into(a, t, "D:/fixtures_out/markdown-all/markdown-all.md");
        t->external_conflict = true;
        snprintf(a->modal_text, sizeof(a->modal_text),
            "The file has been modified externally.\n\nA: Reload from disk\nB: Keep current buffer\nC: Compare differences");
        a->modal = 10;
    } else if(strcmp(id, "UI-RECOVERY-CENTER") == 0){
        snprintf(a->modal_text, sizeof(a->modal_text),
            "Recovery Center\n\nThe following documents were recovered from an unclean shutdown:\n\n  • untitled_1.md  (12:04:33)\n  • notes.md  (11:58:12)\n\n[Open All]  [Discard All]  [Continue]");
        a->modal = 9;
    } else if(strcmp(id, "UI-ERROR-SAVE") == 0){
        snprintf(a->modal_text, sizeof(a->modal_text),
            "Save failed (error 32).\n\nThe process cannot access the file because it is being used by another process.\n\nThe original file is unchanged.");
        a->modal = 7;
    } else if(strcmp(id, "UI-COMMAND-PALETTE") == 0){
        snprintf(a->modal_text, sizeof(a->modal_text),
            "Command Palette\n\n> sta\n  Toggle Source Mode\n  Document Statistics\n\n[Enter] execute  [Esc] close");
        a->modal = 1; snprintf(a->palette_query, sizeof(a->palette_query), "sta");
    } else if(strcmp(id, "UI-IMAGE-SELECTED") == 0 || strcmp(id, "UI-IMAGE-RESIZE") == 0){
        app_new_document(a);
        DocTab *t = a->tabs[a->active];
        md_document_set_source(&t->doc, "# Image\n\n<img src=\"assets/logo.png\" alt=\"logo\" width=\"240\" />\n\n", 0);
        size_t l = strlen("# Image\n\n<img src=\"assets/logo.png\" alt=\"logo\" width=\"240\" />\n\n");
        md_document_set_source(&t->doc, "# Image\n\n<img src=\"assets/logo.png\" alt=\"logo\" width=\"240\" />\n\n", l);
        app_reparse(a, t);
        t->mode = MODE_RENDERED;
    } else if(strcmp(id, "UI-TABLE-EDIT") == 0){
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_RENDERED;
    } else if(strcmp(id, "UI-DPI-SCALED") == 0){
        /* 150% simulated by larger font */
        a->prefs.font_size = 22;
        app_new_document(a);
        load_into(a, a->tabs[a->active], "D:/fixtures_out/markdown-all/markdown-all.md");
        a->tabs[a->active]->mode = MODE_SOURCE;
    } else {
        app_new_document(a);
    }
    a->modal_anim = 1.0; /* show fully open */
}

/* ---------------- path helpers ---------------- */

static int split_components(const char *p, char ***out){
    /* split absolute path into components (no drive letter handling needed
     * since both are under the same workspace root). */
    char *copy = ce_strdup(p);
    for(char *q = copy; *q; q++) if(*q == '\\' || *q == '/') *q = '\x01';
    size_t cap = 16, n = 0;
    char **arr = ce_malloc(cap * sizeof(char*));
    char *tok = strtok(copy, "\x01");
    while(tok){
        if(strlen(tok) && strcmp(tok, ".") != 0){
            if(n == cap){ cap *= 2; arr = ce_realloc(arr, cap * sizeof(char*)); }
            arr[n++] = ce_strdup(tok);
        }
        tok = strtok(NULL, "\x01");
    }
    /* ignore drive letter component (C:) if present */
    size_t start = 0;
    if(n > 0 && arr[0][0] && arr[0][1] == ':') start = 1;
    /* shift */
    if(start){
        for(size_t i = start; i < n; i++) arr[i - start] = arr[i];
        n -= start;
    }
    ce_free(copy);
    *out = arr;
    return (int)n;
}

static char *path_relative(const char *from_dir, const char *to_path){
    char **fa = NULL, **ta = NULL;
    int fn = split_components(from_dir, &fa);
    int tn = split_components(to_path, &ta);
    int common = 0;
    while(common < fn && common < tn && ce_strcasecmp(fa[common], ta[common]) == 0) common++;
    ce_buf out; ce_buf_init(&out);
    for(int i = common; i < fn; i++) ce_buf_append_str(&out, "../");
    for(int i = common; i < tn; i++){
        ce_buf_append_str(&out, ta[i]);
        if(i + 1 < tn) ce_buf_append_c(&out, '/');
    }
    if(out.len == 0) ce_buf_append_c(&out, '.');
    for(int i = 0; i < fn; i++) ce_free(fa[i]);
    for(int i = 0; i < tn; i++) ce_free(ta[i]);
    ce_free(fa); ce_free(ta);
    return ce_buf_detach(&out);
}

char *relpath_for(App *a, DocTab *t, const char *subdir, const char *name){
    if(!a->workspace_root) return NULL;
    const char *doc = t->doc.path;
    if(!doc || strncmp(doc, a->workspace_root, strlen(a->workspace_root)) != 0){
        ce_buf p; ce_buf_init(&p);
        ce_buf_append_fmt(&p, "%s/%s", subdir, name);
        return ce_buf_detach(&p);
    }
    char *dd = ce_strdup(doc);
    char *slash = strrchr(dd, '\\');
    if(slash) *slash = 0;
    ce_buf tgt; ce_buf_init(&tgt);
    ce_buf_append_str(&tgt, a->workspace_root);
    ce_buf_append_c(&tgt, '\\');
    ce_buf_append_str(&tgt, subdir);
    ce_buf_append_c(&tgt, '\\');
    ce_buf_append_str(&tgt, name);
    char *rel = path_relative(dd, tgt.data);
    ce_free(dd);
    ce_buf_free(&tgt);
    return rel;
}
