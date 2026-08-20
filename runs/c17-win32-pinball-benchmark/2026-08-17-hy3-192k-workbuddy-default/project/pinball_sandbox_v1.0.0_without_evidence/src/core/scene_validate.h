#ifndef PB_SCENE_VALIDATE_H
#define PB_SCENE_VALIDATE_H

#include "types.h"
#include "scene.h"

/* Semantic validation (doc 31.2). Returns first Error-level code, or PBT_OK (0)
   when only warnings (or nothing) are present. All diagnostics are appended. */
PbtCode scene_validate(const Scene *s, DiagList *diag);

/* Convenience: 1 if any Error-level diagnostic is present. */
int scene_validate_has_error(const Scene *s, DiagList *diag);

#endif /* PB_SCENE_VALIDATE_H */
