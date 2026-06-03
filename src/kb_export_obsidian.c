/* kb_export_obsidian.c: Obsidian markdown renderer for aimee kb export. */
#include "kb_export_obsidian.h"
#include "cJSON.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define KB_EXPORT_MAX_NODES 8192

typedef struct
{
   char key[256];
   char file_base[256];
   int is_entity;
} kb_export_link_target_t;

static const char *json_string(cJSON *obj, const char *name)
{
   cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, name);
   return (cJSON_IsString(item) && item->valuestring) ? item->valuestring : "";
}

static void sanitize_name(const char *in, char *out, size_t out_len)
{
   size_t n = 0;
   int last_was_sep = 0;
   if (!out || out_len == 0)
      return;
   if (!in || !in[0])
      in = "memory";
   for (const unsigned char *p = (const unsigned char *)in; *p && n + 1 < out_len && n < 200; p++)
   {
      unsigned char c = *p;
      if (c < 32 || c == '/' || c == '[' || c == ']' || c == ':' || c == '*' || c == '?' ||
          c == '"' || c == '\\' || c == '|' || c == '<' || c == '>')
      {
         if (n == 0 || last_was_sep)
            continue;
         c = '_';
         last_was_sep = 1;
      }
      else
      {
         last_was_sep = 0;
      }
      out[n++] = (char)c;
   }
   while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '.' || out[n - 1] == '_'))
      n--;
   if (n == 0)
   {
      snprintf(out, out_len, "memory");
      return;
   }
   out[n] = '\0';
}

static int file_base_seen(kb_export_link_target_t *targets, int target_count, const char *file_base)
{
   int i;
   for (i = 0; i < target_count; i++)
      if (strcmp(targets[i].file_base, file_base) == 0)
         return 1;
   return 0;
}

static void unique_file_base(kb_export_link_target_t *targets, int target_count, cJSON *obj,
                             const char *preferred, char *out, size_t out_len)
{
   char base[256];
   sanitize_name(preferred, base, sizeof(base));
   snprintf(out, out_len, "%s", base);
   if (!file_base_seen(targets, target_count, out))
      return;

   cJSON *id = obj ? cJSON_GetObjectItemCaseSensitive(obj, "id") : NULL;
   if (cJSON_IsNumber(id))
      snprintf(out, out_len, "%s_%.0f", base, id->valuedouble);
   else
      snprintf(out, out_len, "%s_%d", base, target_count + 1);

   if (file_base_seen(targets, target_count, out))
   {
      int n = 2;
      do
      {
         snprintf(out, out_len, "%s_%d", base, n++);
      } while (file_base_seen(targets, target_count, out));
   }
}

static int add_target(kb_export_link_target_t *targets, int *target_count, cJSON *obj,
                      const char *key, int is_entity)
{
   if (*target_count >= KB_EXPORT_MAX_NODES)
      return 0;
   if (!key || !key[0])
      key = "memory";
   snprintf(targets[*target_count].key, sizeof(targets[*target_count].key), "%s", key);
   unique_file_base(targets, *target_count, obj, key, targets[*target_count].file_base,
                    sizeof(targets[*target_count].file_base));
   targets[*target_count].is_entity = is_entity;
   (*target_count)++;
   return 1;
}

static const char *find_target(kb_export_link_target_t *targets, int target_count, const char *key)
{
   if (!key || !key[0])
      return NULL;
   int i;
   for (i = 0; i < target_count; i++)
      if (strcmp(targets[i].key, key) == 0)
         return targets[i].file_base;
   return NULL;
}

static int has_entity_targets(kb_export_link_target_t *targets, int target_count)
{
   int i;
   for (i = 0; i < target_count; i++)
      if (targets[i].is_entity)
         return 1;
   return 0;
}

static int yaml_quote_char(unsigned char c)
{
   switch (c)
   {
   case '"':
   case '\\':
   case ':':
   case '[':
   case ']':
   case '{':
   case '}':
   case '#':
   case ',':
      return 1;
   default:
      return c < 32;
   }
}

static int yaml_needs_quotes(const char *s)
{
   if (!s || !s[0])
      return 1;
   const unsigned char *p;
   for (p = (const unsigned char *)s; *p; p++)
   {
      if (yaml_quote_char(*p))
         return 1;
   }
   return isspace((unsigned char)s[0]) || isspace((unsigned char)s[strlen(s) - 1]);
}

static void write_yaml_value(FILE *f, const char *s)
{
   if (!yaml_needs_quotes(s))
   {
      fprintf(f, "%s", s);
      return;
   }

   fputc('"', f);
   const char *p;
   for (p = s ? s : ""; *p; p++)
   {
      if (*p == '"' || *p == '\\')
         fputc('\\', f);
      if (*p == '\n')
         fputs("\\n", f);
      else if (*p != '\r')
         fputc(*p, f);
   }
   fputc('"', f);
}

static void write_yaml_field(FILE *f, const char *name, const char *value)
{
   fprintf(f, "%s: ", name);
   write_yaml_value(f, value ? value : "");
   fputc('\n', f);
}

static int write_memory_file(const char *out_dir, cJSON *mem, const char *file_base,
                             kb_export_link_target_t *targets, int target_count)
{
   char path[4096];
   int pn = snprintf(path, sizeof(path), "%s/%s.md", out_dir, file_base);
   if (pn < 0 || (size_t)pn >= sizeof(path))
      return -1;

   FILE *f = fopen(path, "w");
   if (!f)
      return -1;

   const char *key = json_string(mem, "key");
   const char *content = json_string(mem, "content");
   int wrote_heading = 0;
   int wrote_link = 0;

   fprintf(f, "---\n");
   write_yaml_field(f, "kind", json_string(mem, "kind"));
   write_yaml_field(f, "tier", json_string(mem, "tier"));
   write_yaml_field(f, "key", key);
   cJSON *conf = cJSON_GetObjectItemCaseSensitive(mem, "confidence");
   fprintf(f, "confidence: %.4f\n", cJSON_IsNumber(conf) ? conf->valuedouble : 0.0);
   write_yaml_field(f, "lifecycle_state", json_string(mem, "lifecycle_state"));
   write_yaml_field(f, "workspace", json_string(mem, "workspace"));
   write_yaml_field(f, "created_at", json_string(mem, "created_at"));
   fprintf(f, "---\n\n");

   fprintf(f, "## %s\n\n", key && key[0] ? key : file_base);
   if (content[0])
      fprintf(f, "%s\n", content);

   cJSON *links = cJSON_GetObjectItemCaseSensitive(mem, "links");
   if (cJSON_IsArray(links) && cJSON_GetArraySize(links) > 0)
   {
      cJSON *link;
      cJSON_ArrayForEach(link, links)
      {
         const char *target_key = NULL;
         const char *relation = "related";
         if (cJSON_IsString(link))
            target_key = link->valuestring;
         else if (cJSON_IsObject(link))
         {
            target_key = json_string(link, "target_key");
            if (!target_key[0])
               target_key = json_string(link, "target");
            if (!target_key[0])
               target_key = json_string(link, "canonical_name");
            relation = json_string(link, "relation");
            if (!relation[0])
               relation = "related";
         }
         const char *target_file = find_target(targets, target_count, target_key);
         if (!target_file)
            continue;
         if (!wrote_heading)
         {
            fprintf(f, "\n## Links\n");
            wrote_heading = 1;
         }
         fprintf(f, "- %s: [[%s]]\n", relation, target_file);
         wrote_link = 1;
      }
   }

   for (int i = 0; i < target_count; i++)
   {
      if (strcmp(targets[i].file_base, file_base) == 0)
         continue;
      if (!content[0] || !strstr(content, targets[i].key))
         continue;
      if (!wrote_heading)
      {
         fprintf(f, "\n## Links\n");
         wrote_heading = 1;
      }
      fprintf(f, "- mentions: [[%s]]\n", targets[i].file_base);
      wrote_link = 1;
   }

   if (!wrote_link)
   {
      for (int i = 0; i < target_count; i++)
      {
         if (!targets[i].is_entity)
            continue;
         if (strcmp(targets[i].file_base, file_base) == 0)
            continue;
         if (!wrote_heading)
         {
            fprintf(f, "\n## Links\n");
            wrote_heading = 1;
         }
         fprintf(f, "- entity: [[%s]]\n", targets[i].file_base);
         wrote_link = 1;
      }
   }

   if (ferror(f))
   {
      fclose(f);
      return -1;
   }
   if (fclose(f) != 0)
      return -1;
   return 0;
}

static int write_entity_profile_file(const char *out_dir, cJSON *entity, const char *file_base)
{
   char path[4096];
   int pn = snprintf(path, sizeof(path), "%s/%s.md", out_dir, file_base);
   if (pn < 0 || (size_t)pn >= sizeof(path))
      return -1;

   FILE *f = fopen(path, "w");
   if (!f)
      return -1;

   const char *name = json_string(entity, "canonical_name");
   fprintf(f, "---\n");
   write_yaml_field(f, "kind", "entity_profile");
   write_yaml_field(f, "canonical_name", name);
   cJSON *obs = cJSON_GetObjectItemCaseSensitive(entity, "observation_count");
   fprintf(f, "observation_count: %.0f\n", cJSON_IsNumber(obs) ? obs->valuedouble : 0.0);
   fprintf(f, "---\n\n");
   fprintf(f, "## %s\n\n", name && name[0] ? name : file_base);
   const char *card = json_string(entity, "card_json");
   if (card[0])
      fprintf(f, "```json\n%s\n```\n", card);

   if (ferror(f))
   {
      fclose(f);
      return -1;
   }
   if (fclose(f) != 0)
      return -1;
   return 0;
}

static int write_entity_index_file(const char *out_dir, kb_export_link_target_t *targets,
                                   int target_count)
{
   char index_base[256];
   char path[4096];
   int pn;
   FILE *f;
   int i;

   unique_file_base(targets, target_count, NULL, "Entities", index_base, sizeof(index_base));
   pn = snprintf(path, sizeof(path), "%s/%s.md", out_dir, index_base);
   if (pn < 0 || (size_t)pn >= sizeof(path))
      return -1;

   f = fopen(path, "w");
   if (!f)
      return -1;

   fprintf(f, "---\n");
   write_yaml_field(f, "kind", "entity_index");
   write_yaml_field(f, "title", "Entities");
   fprintf(f, "---\n\n");
   fprintf(f, "## Entities\n");

   for (i = 0; i < target_count; i++)
      if (targets[i].is_entity)
         fprintf(f, "- [[%s]]\n", targets[i].file_base);

   if (ferror(f))
   {
      fclose(f);
      return -1;
   }
   if (fclose(f) != 0)
      return -1;
   return 0;
}

int kb_export_obsidian_render(cJSON *export_obj, const char *out_dir)
{
   if (!export_obj || !out_dir || !out_dir[0])
      return -1;

   if (mkdir(out_dir, 0755) != 0 && errno != EEXIST)
      return -1;

   kb_export_link_target_t targets[KB_EXPORT_MAX_NODES];
   int target_count = 0;

   cJSON *memories = cJSON_GetObjectItemCaseSensitive(export_obj, "memories");
   if (cJSON_IsArray(memories))
   {
      cJSON *mem;
      cJSON_ArrayForEach(mem, memories)
      {
         if (cJSON_IsObject(mem))
            add_target(targets, &target_count, mem, json_string(mem, "key"),
                       strcmp(json_string(mem, "kind"), "entity") == 0);
      }
   }

   cJSON *entities = cJSON_GetObjectItemCaseSensitive(export_obj, "entity_profiles");
   if (cJSON_IsArray(entities))
   {
      cJSON *entity;
      cJSON_ArrayForEach(entity, entities)
      {
         if (cJSON_IsObject(entity))
            add_target(targets, &target_count, entity, json_string(entity, "canonical_name"), 1);
      }
   }

   int target_cursor = 0;
   if (cJSON_IsArray(memories))
   {
      cJSON *mem;
      cJSON_ArrayForEach(mem, memories)
      {
         if (cJSON_IsObject(mem))
         {
            if (target_cursor >= target_count ||
                write_memory_file(out_dir, mem, targets[target_cursor].file_base, targets,
                                  target_count) != 0)
               return -1;
            target_cursor++;
         }
      }
   }

   if (cJSON_IsArray(entities))
   {
      int i = 0;
      cJSON *entity;
      cJSON_ArrayForEach(entity, entities)
      {
         if (cJSON_IsObject(entity))
         {
            if (target_cursor + i >= target_count)
               return -1;
            if (write_entity_profile_file(out_dir, entity, targets[target_cursor + i].file_base) !=
                0)
               return -1;
            i++;
         }
      }
   }

   if (has_entity_targets(targets, target_count))
   {
      if (write_entity_index_file(out_dir, targets, target_count) != 0)
         return -1;
   }

   return 0;
}
