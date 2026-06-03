/* kb_export_json.h: JSON format renderer for aimee kb export. */
#ifndef DEC_KB_EXPORT_JSON_H
#define DEC_KB_EXPORT_JSON_H 1
#include "cJSON.h"
/* Build the stable JSON export envelope from a kb.export response object.
 * Caller owns the returned object. */
cJSON *kb_export_json_build_envelope(cJSON *export_obj);

/* Validate a JSON import envelope. Returns 0 on success, -1 on error. */
int kb_export_json_validate_import(cJSON *import_obj);

/* Write the stable JSON export envelope to out_file as pretty-printed JSON.
 * Returns 0 on success, -1 on error. */
int kb_export_json_render(cJSON *export_obj, const char *out_file);
#endif
