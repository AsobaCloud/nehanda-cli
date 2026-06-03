#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "tool_args_coerce.h"
#include "cJSON.h"

/* Returns 1 if prop_schema declares `type` == want (string or array form). */
static int schema_declares_type(const cJSON *prop_schema, const char *want)
{
   if (!prop_schema)
      return 0;

   const cJSON *type = cJSON_GetObjectItemCaseSensitive(prop_schema, "type");
   if (!type)
      return 0;

   if (cJSON_IsString(type) && strcmp(type->valuestring, want) == 0)
      return 1;

   if (cJSON_IsArray(type))
   {
      cJSON *item = NULL;
      cJSON_ArrayForEach(item, type)
      {
         if (cJSON_IsString(item) && strcmp(item->valuestring, want) == 0)
            return 1;
      }
   }

   return 0;
}

/* Try to coerce a string scalar to the given primitive type.
 * Returns a new cJSON on success, NULL on failure. */
static cJSON *coerce_string_to(const cJSON *src, const char *target_type)
{
   if (!cJSON_IsString(src))
      return NULL;

   const char *s = src->valuestring;

   if (strcmp(target_type, "integer") == 0)
   {
      char *end = NULL;
      long val = strtol(s, &end, 10);
      while (*end && isspace((unsigned char)*end))
         end++;
      if (*end != '\0')
         return NULL;
      return cJSON_CreateNumber(val);
   }

   if (strcmp(target_type, "number") == 0)
   {
      char *end = NULL;
      double val = strtod(s, &end);
      while (*end && isspace((unsigned char)*end))
         end++;
      if (*end != '\0')
         return NULL;
      return cJSON_CreateNumber(val);
   }

   if (strcmp(target_type, "boolean") == 0)
   {
      if (strcmp(s, "true") == 0 || strcmp(s, "1") == 0)
         return cJSON_CreateTrue();
      if (strcmp(s, "false") == 0 || strcmp(s, "0") == 0)
         return cJSON_CreateFalse();
      return NULL;
   }

   return NULL;
}

/* Coerce one property value given its per-property schema.
 * Returns a new cJSON (caller owns) or NULL to indicate "use original". */
static cJSON *coerce_one(const cJSON *val, const cJSON *prop_schema)
{
   if (!prop_schema)
      return NULL;

   /* integer: string "42" → 42 */
   if (schema_declares_type(prop_schema, "integer") && cJSON_IsString(val))
   {
      cJSON *res = coerce_string_to(val, "integer");
      if (res)
         return res;
   }

   /* number: string "3.14" → 3.14 */
   if (schema_declares_type(prop_schema, "number") && cJSON_IsString(val))
   {
      cJSON *res = coerce_string_to(val, "number");
      if (res)
         return res;
   }

   /* boolean: string "true"/"1" → true, "false"/"0" → false */
   if (schema_declares_type(prop_schema, "boolean") && cJSON_IsString(val))
   {
      cJSON *res = coerce_string_to(val, "boolean");
      if (res)
         return res;
   }

   /* array: scalar → [scalar], with optional items.type coercion */
   if (schema_declares_type(prop_schema, "array") && !cJSON_IsArray(val))
   {
      cJSON *arr = cJSON_CreateArray();
      cJSON *elem = NULL;

      const cJSON *items = cJSON_GetObjectItemCaseSensitive(prop_schema, "items");
      if (items)
      {
         const cJSON *items_type = cJSON_GetObjectItemCaseSensitive(items, "type");
         if (items_type && cJSON_IsString(items_type))
         {
            elem = coerce_string_to(val, items_type->valuestring);
         }
      }

      if (elem)
         cJSON_AddItemToArray(arr, elem);
      else
         cJSON_AddItemToArray(arr, cJSON_Duplicate(val, 1));

      return arr;
   }

   /* object: JSON string → parsed object */
   if (schema_declares_type(prop_schema, "object") && cJSON_IsString(val))
   {
      const char *p = val->valuestring;
      while (*p && isspace((unsigned char)*p))
         p++;
      if (*p == '{')
      {
         cJSON *parsed = cJSON_Parse(p);
         if (parsed)
            return parsed;
      }
   }

   return NULL;
}

cJSON *tool_args_coerce(const cJSON *declared_schema, const cJSON *raw_args)
{
   if (!declared_schema)
      return cJSON_Duplicate(raw_args, 1);

   if (!cJSON_IsObject(raw_args))
      return cJSON_Duplicate(raw_args, 1);

   if (!schema_declares_type(declared_schema, "object"))
      return cJSON_Duplicate(raw_args, 1);

   const cJSON *properties = cJSON_GetObjectItemCaseSensitive(declared_schema, "properties");
   if (!properties || !cJSON_IsObject(properties))
      return cJSON_Duplicate(raw_args, 1);

   cJSON *result = cJSON_CreateObject();
   if (!result)
      return NULL;

   cJSON *child = NULL;
   cJSON_ArrayForEach(child, raw_args)
   {
      const cJSON *prop_schema = cJSON_GetObjectItemCaseSensitive(properties, child->string);
      cJSON *coerced = coerce_one(child, prop_schema);

      /* Always add an owning copy. Reference items would leave the result
       * holding dangling pointers as soon as the caller cJSON_Deletes the
       * input — the dispatch path does exactly that to swap args. */
      if (coerced)
         cJSON_AddItemToObject(result, child->string, coerced);
      else
         cJSON_AddItemToObject(result, child->string, cJSON_Duplicate(child, 1));
   }

   return result;
}
