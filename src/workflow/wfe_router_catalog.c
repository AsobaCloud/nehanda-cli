/* wfe_router_catalog.c -- the router catalog I/O layer: built-in read-only
 * converse/research lanes + every $AIMEE_HOME/workflows/<name>.yaml's router metadata.
 * Kept out of wfe_router.c so the decision core stays I/O-free and unit-testable.
 */
#include "wfe_router.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "aimee_home.h"
#include "cJSON.h"
#include "yaml.h"

static int obj_true(const cJSON *root, const char *key)
{
   const cJSON *it = cJSON_GetObjectItemCaseSensitive(root, key);
   return it && (cJSON_IsTrue(it) ||
                 (cJSON_IsString(it) && it->valuestring && strcmp(it->valuestring, "true") == 0));
}

static const char *obj_str(const cJSON *root, const char *key)
{
   const cJSON *it = cJSON_GetObjectItemCaseSensitive(root, key);
   return (it && cJSON_IsString(it)) ? it->valuestring : NULL;
}

static void add_lane(wfe_router_catalog_t *c, const char *id, int is_default, int read_only)
{
   if (c->n >= WFE_ROUTER_MAX_WF)
      return;
   wfe_router_wf_t *w = &c->wf[c->n++];
   memset(w, 0, sizeof *w);
   snprintf(w->id, sizeof w->id, "%s", id);
   w->is_default = is_default;
   w->read_only = read_only;
}

static int ends_yaml(const char *n)
{
   size_t l = strlen(n);
   return l > 5 && strcmp(n + l - 5, ".yaml") == 0;
}

static char *read_file(const char *path)
{
   FILE *f = fopen(path, "rb");
   if (!f)
      return NULL;
   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   if (sz < 0 || sz > (4 << 20)) /* 4MB sanity cap */
   {
      fclose(f);
      return NULL;
   }
   fseek(f, 0, SEEK_SET);
   char *b = malloc((size_t)sz + 1);
   if (!b)
   {
      fclose(f);
      return NULL;
   }
   size_t rd = fread(b, 1, (size_t)sz, f);
   b[rd] = '\0';
   fclose(f);
   return b;
}

int wfe_router_catalog_load(wfe_router_catalog_t *out, char *err, size_t errlen)
{
   if (err && errlen)
      err[0] = '\0';
   memset(out, 0, sizeof *out);
   /* built-in read-only lanes; research is the safe default. */
   add_lane(out, "converse", 0, 1);
   add_lane(out, "research", 1, 1);

   char dir[1024];
   snprintf(dir, sizeof dir, "%s/workflows", aimee_home());
   DIR *d = opendir(dir);
   if (d)
   {
      struct dirent *e;
      while ((e = readdir(d)) != NULL)
      {
         if (!ends_yaml(e->d_name))
            continue;
         char path[2048];
         snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
         struct stat st;
         /* lstat (not stat): skip symlinks + non-regular files so a symlink in
          * the workflows dir cannot pull in a file from outside the trusted root. */
         if (lstat(path, &st) != 0 || S_ISLNK(st.st_mode) || !S_ISREG(st.st_mode))
            continue;
         char *buf = read_file(path);
         if (!buf)
            continue;
         cJSON *root = yaml_parse(buf);
         free(buf);
         if (!root)
            continue;
         const char *name = obj_str(root, "name");
         if (name && name[0] && !wfe_router_find(out, name) && out->n < WFE_ROUTER_MAX_WF)
         {
            if (obj_true(root, "default"))
            {
               snprintf(err, errlen,
                        "workflow '%s' must not set 'default' (research is the built-in default)",
                        name);
               cJSON_Delete(root);
               closedir(d);
               return -1;
            }
            wfe_router_wf_t *w = &out->wf[out->n++];
            memset(w, 0, sizeof *w);
            snprintf(w->id, sizeof w->id, "%s", name);
            w->read_only = obj_true(root, "read_only");
            const cJSON *tags = cJSON_GetObjectItemCaseSensitive(root, "intent_tags");
            if (tags && cJSON_IsArray(tags))
            {
               const cJSON *t = NULL;
               cJSON_ArrayForEach(t, tags)
               {
                  if (w->n_tags >= WFE_ROUTER_MAX_TAGS)
                     break;
                  if (cJSON_IsString(t) && t->valuestring && t->valuestring[0])
                     snprintf(w->tags[w->n_tags++], WFE_ROUTER_TAG_LEN, "%s", t->valuestring);
               }
            }
         }
         cJSON_Delete(root);
      }
      closedir(d);
   }
   return wfe_router_catalog_validate(out, err, errlen);
}
