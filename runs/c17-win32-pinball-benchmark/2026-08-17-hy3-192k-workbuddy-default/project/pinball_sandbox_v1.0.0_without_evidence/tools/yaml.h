/* yaml.h - minimal YAML subset parser producing a JValue tree (see json.h).
 * Supports: mappings (key: value), block sequences (- item), scalars
 * (string/int/bool/null), quoted strings, and # comments outside quotes.
 * Anchors, flow collections, multi-doc, and tags are NOT supported and
 * produce a clear error. */
#ifndef PB_YAML_H
#define PB_YAML_H
#include "json.h"

JValue *yaml_parse(const char *text, char *err, int errcap);

#endif /* PB_YAML_H */
