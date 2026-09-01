#include "glob.h"
#include <string.h>
#include <stdbool.h>

int glob_validate(const char *pattern){
    size_t len = strlen(pattern);
    if(len == 0) return -1;
    /* Reject runs of 3+ consecutive '*' */
    size_t i, run=0;
    for(i=0;i<len;i++){
        if(pattern[i]=='*'){ run++; if(run>=3) return -1; }
        else run = 0;
    }
    return 0;
}

static int match_here(const char *pat, const char *path){
    /* pat can be empty -> path must be empty */
    if(*pat == '\0') return *path == '\0';
    if(pat[0]=='*' && pat[1]=='*'){
        /* '**' matches zero+ including '/'. Consume all following '*' too. */
        const char *run = pat;
        while(*run=='*') run++;
        /* now run points after the star-run. match_here(run, path) tests
           the remaining pattern against the current suffix, and we also
           try consuming one more char of path. */
        /* First: doublestar-slash semantics handled naturally because '/'
           is allowed in the doublestar match. */
        if(*run == '/'){
            /* doublestar-slash: zero directory components allowed. try rest
               after the slash at current path, or consume path up to a '/'. */
            if(match_here(run+1, path)) return 1;
            const char *q = path;
            while(*q){
                if(match_here(run+1, q+1)) return 1;
                q++;
            }
            return 0;
        }
        /* generic ** */
        if(match_here(run, path)) return 1;
        const char *q = path;
        while(*q){
            if(match_here(run, q+1)) return 1;
            q++;
        }
        return 0;
    }
    if(*pat == '*'){
        /* single '*' matches zero+ non-separator */
        return match_here(pat+1, path) ||
               (*path != '\0' && *path != '/' && match_here(pat, path+1));
    }
    if(*pat == '?'){
        return *path != '\0' && *path != '/' && match_here(pat+1, path+1);
    }
    /* literal */
    return *path != '\0' && *pat == *path && match_here(pat+1, path+1);
}

int glob_match(const char *pattern, const char *path){
    return match_here(pattern, path);
}

int glob_has_doublestar(const char *pattern){
    while(*pattern){
        if(*pattern=='*' && pattern[1]=='*') return 1;
        pattern++;
    }
    return 0;
}
