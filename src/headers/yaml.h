/* yaml.h -- minimal YAML <-> cJSON shim
 *
 * Supports the subset aimee uses for config files:
 *   - block-style mappings (key: value)
 *   - block-style sequences (- value, - key: value)
 *   - nested mappings and sequences
 *   - scalar types: string, int, bool, null
 *   - 2-space block indentation (or any consistent indent depth)
 *   - # comments (whole-line and end-of-line preceded by space)
 *
 * Does NOT support: flow style ({} / []), anchors, tags, multi-document
 * streams, multi-line scalars (|, >), or quoted keys.
 *
 * cJSON is the canonical in-memory format used everywhere else in aimee,
 * so callers don't have to learn a new API — yaml_parse returns a cJSON
 * tree and yaml_emit accepts one.
 */
#ifndef DEC_YAML_H
#define DEC_YAML_H 1

#include "../vendor/headers/cJSON.h"

/* Parse YAML text into a cJSON tree. Returns NULL on error.
 * Caller owns the returned node and must call cJSON_Delete. */
cJSON *yaml_parse(const char *yaml_text);

/* Emit a cJSON tree as YAML text. Returns a malloc'd, NUL-terminated
 * string the caller must free. Returns NULL on error. */
char *yaml_emit(const cJSON *root);

#endif /* DEC_YAML_H */
