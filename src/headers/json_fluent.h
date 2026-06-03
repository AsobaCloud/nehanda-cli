/* json_fluent.h: concise helpers for cJSON request parsing and response
 * building. Used across render/server/cmd modules to strip the repeated
 * `cJSON_IsString(x) ? x->valuestring : defval` and
 * `cJSON_AddStringToObject(o, k, v ? v : "")` boilerplate. */
#ifndef DEC_JSON_FLUENT_H
#define DEC_JSON_FLUENT_H 1

#include "cJSON.h"
#include <stdint.h>

/* ---- Optional getters with defaults ---- */
/* All tolerate NULL obj. Return defval if the key is missing or has the wrong
 * JSON type. Strings are not copied — borrowed from the parent cJSON value, so
 * only valid while the parent is alive. */
const char *jo_str(cJSON *obj, const char *key, const char *defval);
int jo_int(cJSON *obj, const char *key, int defval);
int64_t jo_i64(cJSON *obj, const char *key, int64_t defval);
double jo_num(cJSON *obj, const char *key, double defval);
int jo_bool(cJSON *obj, const char *key, int defval);

/* Const-correct "string field or empty": the value at `key` if it is a non-empty
 * string, else "" (never NULL). For read-only payload/record JSON access. */
static inline const char *jo_cstr(const cJSON *obj, const char *key)
{
   const cJSON *j = cJSON_GetObjectItemCaseSensitive(obj, key);
   return (cJSON_IsString(j) && j->valuestring[0]) ? j->valuestring : "";
}

/* Human-readable JSON type name for `item` (string/number/boolean/array/object/
 * null, else "unknown"). For "expected X, got %s" config-validation diagnostics.
 * Inline so it adds no link-time dependency to the many TUs that include this. */
static inline const char *jo_type_name(const cJSON *item)
{
   if (cJSON_IsString(item))
      return "string";
   if (cJSON_IsNumber(item))
      return "number";
   if (cJSON_IsBool(item))
      return "boolean";
   if (cJSON_IsArray(item))
      return "array";
   if (cJSON_IsObject(item))
      return "object";
   if (cJSON_IsNull(item))
      return "null";
   return "unknown";
}

/* ---- Required getters. Return 0 on success, -1 on missing/wrong type. ---- */
int jo_need_str(cJSON *obj, const char *key, const char **out);
int jo_need_num(cJSON *obj, const char *key, double *out);

/* ---- NUL-safe add helpers: treat NULL strings as "". ---- */
void jo_add_str(cJSON *obj, const char *key, const char *val);
void jo_add_i64(cJSON *obj, const char *key, int64_t val);
void jo_add_num(cJSON *obj, const char *key, double val);
void jo_add_bool(cJSON *obj, const char *key, int val);

/* ---- Response-object builders. Ownership transfers to caller. ---- */
cJSON *jo_ok(void);                                /* {"status":"ok"} */
cJSON *jo_ok_kv(const char *key, const char *val); /* ok + one string field */
cJSON *jo_err(const char *message);                /* {"status":"error","message":msg} */

/* Add a string to a cJSON object with NUL-safety.
 * Wraps cJSON_AddStringToObject so NULL values become "" rather than crashing. */
#define JSON_ADD_STR(obj, key, val) cJSON_AddStringToObject((obj), (key), (val) ? (val) : "")

#endif /* DEC_JSON_FLUENT_H */
