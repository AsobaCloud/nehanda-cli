/* kb_export_json.c: JSON format renderer for aimee kb export. */
#include "kb_export_json.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int add_string_copy(cJSON *dst, cJSON *src, const char *name, int required)
{
   cJSON *item = cJSON_GetObjectItemCaseSensitive(src, name);
   if (!cJSON_IsString(item))
      return required ? -1 : 0;
   return cJSON_AddStringToObject(dst, name, item->valuestring) ? 0 : -1;
}

static int add_array_copy(cJSON *dst, cJSON *src, const char *name, int required)
{
   cJSON *item = cJSON_GetObjectItemCaseSensitive(src, name);
   if (!cJSON_IsArray(item))
      return required ? -1 : 0;

   cJSON *copy = cJSON_Duplicate(item, 1);
   if (!copy)
      return -1;
   if (!cJSON_AddItemToObject(dst, name, copy))
   {
      cJSON_Delete(copy);
      return -1;
   }
   return 0;
}

cJSON *kb_export_json_build_envelope(cJSON *export_obj)
{
   if (!cJSON_IsObject(export_obj))
      return NULL;

   cJSON *envelope = cJSON_CreateObject();
   if (!envelope)
      return NULL;

   if (add_string_copy(envelope, export_obj, "schema_version", 1) != 0 ||
       add_string_copy(envelope, export_obj, "exported_at", 1) != 0 ||
       add_string_copy(envelope, export_obj, "workspace", 0) != 0 ||
       add_array_copy(envelope, export_obj, "memories", 1) != 0)
   {
      cJSON_Delete(envelope);
      return NULL;
   }

   if (add_array_copy(envelope, export_obj, "entity_profiles", 0) != 0)
   {
      cJSON_Delete(envelope);
      return NULL;
   }
   if (!cJSON_GetObjectItemCaseSensitive(envelope, "entity_profiles") &&
       !cJSON_AddArrayToObject(envelope, "entity_profiles"))
   {
      cJSON_Delete(envelope);
      return NULL;
   }

   return envelope;
}

int kb_export_json_validate_import(cJSON *import_obj)
{
   if (!cJSON_IsObject(import_obj))
      return -1;

   cJSON *schema = cJSON_GetObjectItemCaseSensitive(import_obj, "schema_version");
   if (!cJSON_IsString(schema) || strcmp(schema->valuestring, "1") != 0)
      return -1;

   cJSON *memories = cJSON_GetObjectItemCaseSensitive(import_obj, "memories");
   if (!cJSON_IsArray(memories))
      return -1;

   cJSON *entities = cJSON_GetObjectItemCaseSensitive(import_obj, "entity_profiles");
   if (entities && !cJSON_IsArray(entities))
      return -1;

   cJSON *exported_at = cJSON_GetObjectItemCaseSensitive(import_obj, "exported_at");
   if (exported_at && !cJSON_IsString(exported_at))
      return -1;

   cJSON *workspace = cJSON_GetObjectItemCaseSensitive(import_obj, "workspace");
   if (workspace && !cJSON_IsString(workspace))
      return -1;

   return 0;
}

int kb_export_json_render(cJSON *export_obj, const char *out_file)
{
   if (!export_obj || !out_file || !out_file[0])
      return -1;

   cJSON *envelope = kb_export_json_build_envelope(export_obj);
   if (!envelope)
      return -1;

   char *json = cJSON_Print(envelope);
   cJSON_Delete(envelope);
   if (!json)
      return -1;

   FILE *f = fopen(out_file, "w");
   if (!f)
   {
      free(json);
      return -1;
   }

   int rc = 0;
   if (fputs(json, f) == EOF || fputc('\n', f) == EOF)
      rc = -1;
   if (fclose(f) != 0)
      rc = -1;

   free(json);
   return rc;
}
