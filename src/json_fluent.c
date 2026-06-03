/* json_fluent.c: implementation of concise cJSON helpers. See json_fluent.h. */
#include "json_fluent.h"
#include <stddef.h>

const char *jo_str(cJSON *obj, const char *key, const char *defval)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   return cJSON_IsString(v) ? v->valuestring : defval;
}

int jo_int(cJSON *obj, const char *key, int defval)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   return cJSON_IsNumber(v) ? (int)v->valuedouble : defval;
}

int64_t jo_i64(cJSON *obj, const char *key, int64_t defval)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   return cJSON_IsNumber(v) ? (int64_t)v->valuedouble : defval;
}

double jo_num(cJSON *obj, const char *key, double defval)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   return cJSON_IsNumber(v) ? v->valuedouble : defval;
}

int jo_bool(cJSON *obj, const char *key, int defval)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   if (cJSON_IsBool(v))
      return cJSON_IsTrue(v) ? 1 : 0;
   if (cJSON_IsNumber(v))
      return v->valuedouble != 0;
   return defval;
}

int jo_need_str(cJSON *obj, const char *key, const char **out)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   if (!cJSON_IsString(v))
      return -1;
   if (out)
      *out = v->valuestring;
   return 0;
}

int jo_need_num(cJSON *obj, const char *key, double *out)
{
   cJSON *v = cJSON_GetObjectItemCaseSensitive(obj, key);
   if (!cJSON_IsNumber(v))
      return -1;
   if (out)
      *out = v->valuedouble;
   return 0;
}

void jo_add_str(cJSON *obj, const char *key, const char *val)
{
   cJSON_AddStringToObject(obj, key, val ? val : "");
}

void jo_add_i64(cJSON *obj, const char *key, int64_t val)
{
   cJSON_AddNumberToObject(obj, key, (double)val);
}

void jo_add_num(cJSON *obj, const char *key, double val)
{
   cJSON_AddNumberToObject(obj, key, val);
}

void jo_add_bool(cJSON *obj, const char *key, int val)
{
   cJSON_AddBoolToObject(obj, key, val ? 1 : 0);
}

cJSON *jo_ok(void)
{
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "status", "ok");
   return o;
}

cJSON *jo_ok_kv(const char *key, const char *val)
{
   cJSON *o = jo_ok();
   if (key)
      jo_add_str(o, key, val);
   return o;
}

cJSON *jo_err(const char *message)
{
   cJSON *o = cJSON_CreateObject();
   cJSON_AddStringToObject(o, "status", "error");
   cJSON_AddStringToObject(o, "message", message ? message : "");
   return o;
}
