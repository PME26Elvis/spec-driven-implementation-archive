#ifndef PB_SCENE_WRITE_H
#define PB_SCENE_WRITE_H

#include "scene.h"

/* Canonical serialization (doc 06.24/06.44). Produces byte-stable format-2 text.
   Writes the full scene to a NUL-terminated UTF-8 string (caller frees with free()).
   Returns NULL on allocation failure. */
char *scene_write(const Scene *s);

/* Write directly to a file using atomic save semantics (doc 06.27):
   1) write to <path>.tmp  2) flush  3) rename over <path>.
   Returns 0 on success, negative PbtCode-derived error on failure. */
int scene_write_file(const Scene *s, const char *path);

#endif
