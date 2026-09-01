/* yaml.h - minimal YAML subset parser producing a ce_json DOM, used by locscan
 * configuration. Supports top-level "key: value" mappings where value is a
 * scalar, an inline [a, b] list, or a block sequence of "- item" lines. */
#ifndef CE_YAML_H
#define CE_YAML_H

#include "json.h"

/* Parse a YAML document into a ce_json object (in arena a).
 * Returns NULL on error and sets *errline (1-based) if non-NULL. */
ce_json *ce_yaml_parse(ce_arena *a, const char *s, int *errline);

#endif /* CE_YAML_H */
