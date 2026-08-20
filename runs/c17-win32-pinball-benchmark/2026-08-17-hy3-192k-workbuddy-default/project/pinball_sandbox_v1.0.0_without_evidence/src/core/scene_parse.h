#ifndef PB_SCENE_PARSE_H
#define PB_SCENE_PARSE_H

#include "types.h"
#include "scene.h"

/* Hard parser limits (doc 06.29 / doc 31). */
#define PB_LINE_MAX   1048576   /* 1 MiB physical line */
#define PB_TOKEN_MAX  4096      /* identifier / string hard limit */
#define PB_ID_HARD    63        /* doc 17.12 */
#define PB_FILE_MAX   (16u * 1024u * 1024u)  /* 16 MiB file size cap */

/* Parse text into scene. Returns primary (first) diagnostic code; PBT_OK on success.
   diag receives all collected diagnostics (first code is the primary). */
PbtCode parse_scene(const char *text, size_t len, Scene *out, DiagList *diag);

/* Load + parse a UTF-8 file from disk. Returns parse code; fills *out on success. */
PbtCode parse_scene_file(const char *path, Scene *out, DiagList *diag);

#endif
