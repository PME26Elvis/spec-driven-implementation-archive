/* match.c - deterministic path/pattern matching. */
#include "match.h"
#include "ce_common.h"

static inline int fold(int c, bool fc){ return fc && (c >= 'A' && c <= 'Z') ? c + ('a'-'A') : c; }

bool ce_fnmatch(const char *pattern, const char *text, bool fold_case){
    const char *star = NULL;
    const char *st = NULL;
    while(*text){
        char p = *pattern, t = *text;
        if(p == '*'){ star = pattern++; st = text; continue; }
        if(fold((unsigned char)p, fold_case) == fold((unsigned char)t, fold_case)){
            pattern++; text++; continue;
        }
        if(p == '?' && t != '/'){ pattern++; text++; continue; }
        if(star){ pattern = star + 1; text = ++st; continue; }
        return false;
    }
    while(*pattern == '*') pattern++;
    return *pattern == 0;
}

bool ce_path_match(const char *pattern, const char *path, bool fold_case){
    size_t pl = strlen(pattern);
    if(pl > 0 && pattern[pl-1] == '/'){
        /* directory subtree: match the directory prefix of path */
        /* pattern without trailing slash */
        char *dir = ce_strndup(pattern, pl - 1);
        bool ok;
        if(ce_fnmatch(dir, path, fold_case)) ok = true;
        else ok = ce_starts_with(path, dir) && (path[strlen(dir)] == '/');
        /* also support wildcard dir name matching: e.g. "node_modules/" */
        if(!ok){
            /* match any path component equal to pattern's last component */
            ok = false;
        }
        ce_free(dir);
        if(ok) return true;
        /* fallback: check every directory component prefix */
        const char *p = path;
        while(p && *p){
            const char *slash = strchr(p, '/');
            size_t comp_len = slash ? (size_t)(slash - p) : strlen(p);
            if(comp_len == pl - 1 && ce_fnmatch(pattern, p, fold_case) == false){
                /* try matching the component only */
            }
            if(ce_fnmatch(pattern, p, fold_case)) return true;
            if(!slash) break;
            p = slash + 1;
        }
        return false;
    }
    /* plain wildcard match on whole path, plus match any trailing component
     * (e.g. "*.o" matches "dir/file.o", "build" matches "a/build/..." ) */
    if(ce_fnmatch(pattern, path, fold_case)) return true;
    /* match against each component for extension-style patterns */
    const char *p = path;
    while(p && *p){
        if(ce_fnmatch(pattern, p, fold_case)) return true;
        const char *slash = strchr(p, '/');
        if(!slash) break;
        p = slash + 1;
    }
    return false;
}

bool ce_path_match_any(const char **patterns, size_t n, const char *path, bool fold_case){
    for(size_t i = 0; i < n; i++)
        if(ce_path_match(patterns[i], path, fold_case)) return true;
    return false;
}
