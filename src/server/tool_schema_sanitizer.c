/* tool_schema_sanitizer.c: see tool_schema_sanitizer.h */

#include "tool_schema_sanitizer.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Per-provider rewrite flags. Combined into a bitmask passed to walk(). */
enum
{
   REWRITE_STRIP_BARE_OBJECT = 1 << 0,
   REWRITE_UNWRAP_SINGLE_ONEOF = 1 << 1,
   REWRITE_STRIP_FORMAT = 1 << 2,
   REWRITE_STRIP_X_VENDOR = 1 << 3,
   REWRITE_STRIP_ADDITIONAL_PROPS = 1 << 4,
};

static int rewrites_for_provider(const char *provider_name)
{
   if (!provider_name)
      return 0;
   int llama_set = REWRITE_STRIP_BARE_OBJECT | REWRITE_UNWRAP_SINGLE_ONEOF | REWRITE_STRIP_FORMAT |
                   REWRITE_STRIP_X_VENDOR;
   if (strcmp(provider_name, "llama_native") == 0 || strcmp(provider_name, "llama-eval") == 0)
      return llama_set;
   if (strcmp(provider_name, "ollama") == 0)
      return llama_set | REWRITE_STRIP_ADDITIONAL_PROPS;
   /* openai, openrouter, gemini, codex, anthropic: no walk-time rewrites. */
   return 0;
}

static int is_bare_object(const cJSON *node)
{
   if (!node || !cJSON_IsObject(node))
      return 0;
   const cJSON *type = cJSON_GetObjectItemCaseSensitive(node, "type");
   if (!type || !cJSON_IsString(type) || strcmp(type->valuestring, "object") != 0)
      return 0;
   const cJSON *properties = cJSON_GetObjectItemCaseSensitive(node, "properties");
   if (properties)
      return 0;
   return 1;
}

/* Replace contents of node (an object) with an empty object by deleting all
 * children. Does NOT free node itself. */
static void replace_with_empty_object(cJSON *node)
{
   while (node->child)
      cJSON_DeleteItemFromObjectCaseSensitive(node, node->child->string);
}

static void walk(cJSON *node, int rewrites);

static void apply_root_strips(cJSON *node, int rewrites)
{
   if (!cJSON_IsObject(node))
      return;
   if (rewrites & REWRITE_STRIP_ADDITIONAL_PROPS)
      cJSON_DeleteItemFromObjectCaseSensitive(node, "additionalProperties");
   if (rewrites & REWRITE_STRIP_FORMAT)
      cJSON_DeleteItemFromObjectCaseSensitive(node, "format");
   if (rewrites & REWRITE_STRIP_X_VENDOR)
   {
      const char *to_delete[32];
      int n_delete = 0;
      const cJSON *c = node->child;
      while (c && n_delete < 32)
      {
         if (c->string && c->string[0] == 'x' && c->string[1] == '-')
            to_delete[n_delete++] = c->string;
         c = c->next;
      }
      for (int i = 0; i < n_delete; i++)
         cJSON_DeleteItemFromObjectCaseSensitive(node, to_delete[i]);
   }
}

static void unwrap_single_oneof(cJSON *node)
{
   const char *keys[2] = {"oneOf", "anyOf"};
   for (int k = 0; k < 2; k++)
   {
      cJSON *list = cJSON_GetObjectItemCaseSensitive(node, keys[k]);
      if (!list || !cJSON_IsArray(list) || cJSON_GetArraySize(list) != 1)
         continue;
      cJSON *only = cJSON_DetachItemFromArray(list, 0);
      cJSON_DeleteItemFromObjectCaseSensitive(node, keys[k]);
      if (only && cJSON_IsObject(only))
      {
         while (only->child)
         {
            cJSON *moved = cJSON_DetachItemViaPointer(only, only->child);
            cJSON_AddItemToObject(node, moved->string, moved);
         }
      }
      cJSON_Delete(only);
   }
}

static void walk(cJSON *node, int rewrites)
{
   if (!node)
      return;

   if (cJSON_IsObject(node))
   {
      if ((rewrites & REWRITE_STRIP_BARE_OBJECT) && is_bare_object(node))
      {
         replace_with_empty_object(node);
         return;
      }
      apply_root_strips(node, rewrites);
      if (rewrites & REWRITE_UNWRAP_SINGLE_ONEOF)
         unwrap_single_oneof(node);

      cJSON *child = node->child;
      while (child)
      {
         cJSON *next = child->next;
         walk(child, rewrites);
         child = next;
      }
   }
   else if (cJSON_IsArray(node))
   {
      cJSON *child = node->child;
      while (child)
      {
         cJSON *next = child->next;
         walk(child, rewrites);
         child = next;
      }
   }
}

static void rename_parameters_to_input_schema(cJSON *node)
{
   if (!node || !cJSON_IsObject(node))
      return;
   cJSON *params = cJSON_DetachItemFromObjectCaseSensitive(node, "parameters");
   if (!params)
      return;
   cJSON_AddItemToObject(node, "input_schema", params);
}

cJSON *tool_schema_sanitize(const char *provider_name, const cJSON *original_schema)
{
   if (!original_schema)
      return NULL;
   cJSON *out = cJSON_Duplicate(original_schema, 1);
   if (!out)
      return NULL;

   if (provider_name && strcmp(provider_name, "anthropic") == 0)
   {
      rename_parameters_to_input_schema(out);
      return out;
   }

   int rewrites = rewrites_for_provider(provider_name);
   if (rewrites == 0)
      return out;

   /* Skip the root bare-object check so a top-level {type:"object"} schema
    * stays intact; only nested bare objects get stripped. Recurse into root's
    * children, then apply non-bare-object strips at root level too. */
   if (cJSON_IsObject(out))
   {
      cJSON *child = out->child;
      while (child)
      {
         cJSON *next = child->next;
         walk(child, rewrites);
         child = next;
      }
      apply_root_strips(out, rewrites);
      if (rewrites & REWRITE_UNWRAP_SINGLE_ONEOF)
         unwrap_single_oneof(out);
   }
   else
   {
      walk(out, rewrites);
   }

   return out;
}
