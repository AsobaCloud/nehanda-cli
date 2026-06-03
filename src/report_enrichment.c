/* report_enrichment.c: typed subject identity for report enrichment. */
#include "report_enrichment.h"

#include "cJSON.h"
#include "util.h"
#include "util_url.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void subject_clear(report_subject_t *out)
{
   if (out)
      memset(out, 0, sizeof(*out));
}

static void subject_display_from_url(const char *url, char *out, size_t out_len)
{
   if (!out || out_len == 0)
      return;
   out[0] = '\0';
   if (!url || !url[0])
      return;

   const char *display = url;
   if (strncmp(display, "https://", 8) == 0)
      display += 8;
   snprintf(out, out_len, "%s", display);
}

static int subject_from_normalized_url(const char *type, char *normalized, report_subject_t *out)
{
   if (!type || !normalized || !out)
   {
      free(normalized);
      return -1;
   }

   subject_clear(out);
   snprintf(out->type, sizeof(out->type), "%s", type);
   snprintf(out->id, sizeof(out->id), "%s", normalized);
   subject_display_from_url(normalized, out->display, sizeof(out->display));
   free(normalized);
   return 0;
}

static char *trim_in_place(char *s)
{
   if (!s)
      return NULL;
   while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
      s++;
   char *end = s + strlen(s);
   while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\n' || end[-1] == '\r'))
      *--end = '\0';
   return s;
}

static int git_capture_args(const char *project_root, const char *const git_args[], char *buf,
                            size_t bufsz)
{
   if (!buf || bufsz == 0 || !git_args || !git_args[0])
      return -1;
   buf[0] = '\0';

   const char *root = (project_root && project_root[0]) ? project_root : ".";
   const char *argv[16];
   int argc = 0;
   argv[argc++] = "git";
   argv[argc++] = "-C";
   argv[argc++] = root;
   for (int i = 0; git_args[i] && argc < (int)(sizeof(argv) / sizeof(argv[0])) - 1; i++)
      argv[argc++] = git_args[i];
   argv[argc] = NULL;

   if (argc == (int)(sizeof(argv) / sizeof(argv[0])) - 1)
      return -1;

   char *out = NULL;
   int rc = safe_exec_capture(argv, &out, bufsz - 1);
   if (rc != 0 || !out)
   {
      free(out);
      return -1;
   }

   char *trimmed = trim_in_place(out);
   if (!trimmed || !trimmed[0])
   {
      free(out);
      return -1;
   }
   snprintf(buf, bufsz, "%s", trimmed);
   free(out);
   return 0;
}

int report_subject_from_git_remote(const char *remote_url, report_subject_t *out)
{
   subject_clear(out);
   char *normalized = util_url_normalize(remote_url);
   if (!normalized)
      return -1;
   return subject_from_normalized_url(REPORT_SUBJECT_TYPE_GIT_REPO, normalized, out);
}

int report_subject_from_project_root(const char *project_root, report_subject_t *out)
{
   subject_clear(out);

   char root[512];
   const char *rev_parse_args[] = {"rev-parse", "--show-toplevel", NULL};
   if (git_capture_args(project_root, rev_parse_args, root, sizeof(root)) != 0)
      return -1;

   char remote[1024];
   const char *remote_args[] = {"remote", "get-url", "origin", NULL};
   if (git_capture_args(root, remote_args, remote, sizeof(remote)) == 0 &&
       report_subject_from_git_remote(remote, out) == 0)
      return 0;

   return report_subject_from_local_repo_id(root, out);
}

int report_subject_from_git_org_url(const char *org_url, report_subject_t *out)
{
   subject_clear(out);
   char *normalized = util_url_normalize(org_url);
   if (!normalized)
      return -1;
   return subject_from_normalized_url(REPORT_SUBJECT_TYPE_GIT_ORG, normalized, out);
}

int report_subject_from_local_repo_id(const char *stable_id, report_subject_t *out)
{
   subject_clear(out);
   if (!stable_id || !stable_id[0] || !out)
      return -1;

   const char *id = stable_id;
   char prefixed[512];
   if (strncmp(stable_id, "local:", 6) != 0)
   {
      snprintf(prefixed, sizeof(prefixed), "local:%s", stable_id);
      id = prefixed;
   }

   snprintf(out->type, sizeof(out->type), "%s", REPORT_SUBJECT_TYPE_GIT_REPO);
   snprintf(out->id, sizeof(out->id), "%s", id);
   snprintf(out->display, sizeof(out->display), "%s", id);
   return 0;
}

int report_subject_is_local(const report_subject_t *subject)
{
   return subject && strncmp(subject->id, "local:", 6) == 0;
}

static cJSON *subject_json(const report_subject_t *subject)
{
   if (!subject || !subject->type[0] || !subject->id[0])
      return NULL;
   cJSON *sub = cJSON_CreateObject();
   if (!sub)
      return NULL;
   cJSON_AddStringToObject(sub, "type", subject->type);
   cJSON_AddStringToObject(sub, "id", subject->id);
   if (subject->display[0])
      cJSON_AddStringToObject(sub, "display", subject->display);
   else
      cJSON_AddStringToObject(sub, "display", subject->id);
   return sub;
}

int report_subject_add_json(cJSON *obj, const report_subject_t *subject)
{
   if (!obj || !subject)
      return -1;
   cJSON *sub = subject_json(subject);
   if (!sub)
      return -1;
   cJSON_DeleteItemFromObjectCaseSensitive(obj, "subject");
   cJSON_AddItemToObject(obj, "subject", sub);
   return 0;
}

int report_subject_add_aggregate_json(cJSON *obj, const char *aggregate_id,
                                      const report_subject_t *children, int child_count)
{
   if (!obj || !aggregate_id || !aggregate_id[0] || child_count < 0 ||
       (child_count > 0 && !children))
      return -1;

   cJSON *sub = cJSON_CreateObject();
   if (!sub)
      return -1;
   cJSON_AddStringToObject(sub, "type", REPORT_SUBJECT_TYPE_AGGREGATE);
   cJSON_AddStringToObject(sub, "id", aggregate_id);
   cJSON_AddStringToObject(sub, "display", aggregate_id);

   cJSON *arr = cJSON_AddArrayToObject(sub, "children");
   if (!arr)
   {
      cJSON_Delete(sub);
      return -1;
   }
   for (int i = 0; i < child_count; i++)
   {
      cJSON *child = subject_json(&children[i]);
      if (!child)
      {
         cJSON_Delete(sub);
         return -1;
      }
      cJSON_AddItemToArray(arr, child);
   }

   cJSON_DeleteItemFromObjectCaseSensitive(obj, "subject");
   cJSON_AddItemToObject(obj, "subject", sub);
   return 0;
}
