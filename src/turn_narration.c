/* turn_narration.c: rule-based turn summary for CLI chat and webchat */
#include "turn_narration.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>

/* Extract the most meaningful key argument from a tool's JSON args string.
 * Writes into dest (at most dest_len bytes including NUL). */
static void extract_key_arg(const char *args_json, const char *tool_name, char *dest,
                            size_t dest_len)
{
   dest[0] = '\0';
   if (!args_json || !args_json[0])
      return;

   cJSON *args = cJSON_Parse(args_json);
   if (!args)
      return;

   const char *val = NULL;

   /* Prefer "path" for file operations */
   if (strcmp(tool_name, "write_file") == 0 || strcmp(tool_name, "read_file") == 0 ||
       strcmp(tool_name, "list_files") == 0)
   {
      cJSON *p = cJSON_GetObjectItem(args, "path");
      if (p && cJSON_IsString(p))
         val = p->valuestring;
   }
   else if (strcmp(tool_name, "bash") == 0 || strcmp(tool_name, "run_background_process") == 0)
   {
      cJSON *c = cJSON_GetObjectItem(args, "command");
      if (c && cJSON_IsString(c))
         val = c->valuestring;
   }
   else if (strcmp(tool_name, "grep") == 0 || strcmp(tool_name, "code_search") == 0 ||
            strcmp(tool_name, "web_search") == 0 || strcmp(tool_name, "search_notes") == 0)
   {
      cJSON *p = cJSON_GetObjectItem(args, "pattern");
      if (!p)
         p = cJSON_GetObjectItem(args, "query");
      if (p && cJSON_IsString(p))
         val = p->valuestring;
   }

   if (val)
   {
      /* Strip leading directory components for paths to keep the summary short */
      const char *base = val;
      if (strcmp(tool_name, "write_file") == 0 || strcmp(tool_name, "read_file") == 0)
      {
         const char *slash = strrchr(val, '/');
         if (slash && slash[1])
            base = slash + 1;
      }
      snprintf(dest, dest_len, "%s", base);
      /* Truncate long command strings with ellipsis */
      if (dest_len > 4 && strlen(dest) == dest_len - 1)
         memcpy(dest + dest_len - 4, "...", 4);
   }

   cJSON_Delete(args);
}

/* ---- public API ---- */

void turn_narration_reset(turn_narration_t *nar)
{
   memset(nar, 0, sizeof(*nar));
}

void turn_narration_add(turn_narration_t *nar, const char *name, const char *args_json)
{
   if (!nar || !name || nar->count >= TURN_NAR_MAX_CALLS)
      return;

   int idx = nar->count++;
   snprintf(nar->names[idx], TURN_NAR_NAME_LEN, "%s", name);
   extract_key_arg(args_json, name, nar->args[idx], TURN_NAR_ARG_LEN);
}

void turn_narration_inc_iter(turn_narration_t *nar)
{
   if (nar)
      nar->iterations++;
}

/*
 * Classify a tool name into one of four categories for grouping.
 */
typedef enum
{
   CAT_WRITE,
   CAT_READ,
   CAT_RUN,
   CAT_OTHER
} tool_cat_t;

static tool_cat_t classify(const char *name)
{
   if (strcmp(name, "write_file") == 0)
      return CAT_WRITE;
   if (strcmp(name, "read_file") == 0 || strcmp(name, "list_files") == 0)
      return CAT_READ;
   if (strcmp(name, "bash") == 0 || strcmp(name, "run_background_process") == 0)
      return CAT_RUN;
   return CAT_OTHER;
}

/* Append str to buf, ensuring we never exceed len. Returns new used length. */
static size_t buf_cat(char *buf, size_t used, size_t len, const char *str)
{
   size_t avail = len > used ? len - used : 0;
   if (!avail)
      return used;
   size_t n = strlen(str);
   if (n >= avail)
      n = avail - 1;
   memcpy(buf + used, str, n);
   buf[used + n] = '\0';
   return used + n;
}

int turn_narration_build(const turn_narration_t *nar, char *buf, size_t len)
{
   if (!nar || nar->count == 0 || !buf || len == 0)
      return 0;

   buf[0] = '\0';
   size_t used = 0;

   /* Collect unique paths written, unique paths read, run commands, other names */
   char writes[TURN_NAR_MAX_CALLS][TURN_NAR_ARG_LEN];
   char reads[TURN_NAR_MAX_CALLS][TURN_NAR_ARG_LEN];
   char runs[TURN_NAR_MAX_CALLS][TURN_NAR_ARG_LEN];
   char others[TURN_NAR_MAX_CALLS][TURN_NAR_NAME_LEN];
   int nw = 0, nr = 0, nrun = 0, noth = 0;

   for (int i = 0; i < nar->count; i++)
   {
      tool_cat_t cat = classify(nar->names[i]);
      switch (cat)
      {
      case CAT_WRITE:
      {
         /* Deduplicate by arg */
         int dup = 0;
         for (int j = 0; j < nw; j++)
            if (strcmp(writes[j], nar->args[i]) == 0)
            {
               dup = 1;
               break;
            }
         if (!dup && nw < TURN_NAR_MAX_CALLS)
            snprintf(writes[nw++], TURN_NAR_ARG_LEN, "%s", nar->args[i]);
         break;
      }
      case CAT_READ:
      {
         int dup = 0;
         for (int j = 0; j < nr; j++)
            if (strcmp(reads[j], nar->args[i]) == 0)
            {
               dup = 1;
               break;
            }
         if (!dup && nr < TURN_NAR_MAX_CALLS)
            snprintf(reads[nr++], TURN_NAR_ARG_LEN, "%s", nar->args[i]);
         break;
      }
      case CAT_RUN:
      {
         if (nrun < TURN_NAR_MAX_CALLS)
            snprintf(runs[nrun++], TURN_NAR_ARG_LEN, "%s", nar->args[i]);
         break;
      }
      case CAT_OTHER:
      {
         /* Deduplicate by name */
         int dup = 0;
         for (int j = 0; j < noth; j++)
            if (strcmp(others[j], nar->names[i]) == 0)
            {
               dup = 1;
               break;
            }
         if (!dup && noth < TURN_NAR_MAX_CALLS)
            snprintf(others[noth++], TURN_NAR_NAME_LEN, "%s", nar->names[i]);
         break;
      }
      }
   }

   int need_sep = 0; /* whether to prepend "; " before the next segment */

   /* --- Writes --- */
   if (nw > 0)
   {
      if (need_sep)
         used = buf_cat(buf, used, len, "; ");
      if (nw == 1 && writes[0][0])
      {
         char tmp[TURN_NAR_ARG_LEN + 16];
         snprintf(tmp, sizeof(tmp), "Modified %s", writes[0]);
         used = buf_cat(buf, used, len, tmp);
      }
      else if (nw <= 3)
      {
         used = buf_cat(buf, used, len, "Modified ");
         for (int i = 0; i < nw; i++)
         {
            if (i > 0)
               used = buf_cat(buf, used, len, ", ");
            used = buf_cat(buf, used, len, writes[i][0] ? writes[i] : "file");
         }
      }
      else
      {
         char tmp[32];
         snprintf(tmp, sizeof(tmp), "Modified %d files", nw);
         used = buf_cat(buf, used, len, tmp);
      }
      need_sep = 1;
   }

   /* --- Reads (only mention if no writes, or there are many) --- */
   if (nr > 0 && nw == 0)
   {
      if (need_sep)
         used = buf_cat(buf, used, len, "; ");
      if (nr == 1 && reads[0][0])
      {
         char tmp[TURN_NAR_ARG_LEN + 16];
         snprintf(tmp, sizeof(tmp), "Read %s", reads[0]);
         used = buf_cat(buf, used, len, tmp);
      }
      else
      {
         char tmp[32];
         snprintf(tmp, sizeof(tmp), "Read %d files", nr);
         used = buf_cat(buf, used, len, tmp);
      }
      need_sep = 1;
   }

   /* --- Runs --- */
   if (nrun > 0)
   {
      if (need_sep)
         used = buf_cat(buf, used, len, "; ");
      if (nrun == 1 && runs[0][0])
      {
         char tmp[TURN_NAR_ARG_LEN + 16];
         snprintf(tmp, sizeof(tmp), "Ran `%s`", runs[0]);
         used = buf_cat(buf, used, len, tmp);
      }
      else if (nrun <= 2)
      {
         used = buf_cat(buf, used, len, "Ran ");
         for (int i = 0; i < nrun; i++)
         {
            if (i > 0)
               used = buf_cat(buf, used, len, ", ");
            char tmp[TURN_NAR_ARG_LEN + 4];
            if (runs[i][0])
               snprintf(tmp, sizeof(tmp), "`%s`", runs[i]);
            else
               snprintf(tmp, sizeof(tmp), "command");
            used = buf_cat(buf, used, len, tmp);
         }
      }
      else
      {
         char tmp[32];
         snprintf(tmp, sizeof(tmp), "Ran %d commands", nrun);
         used = buf_cat(buf, used, len, tmp);
      }
      need_sep = 1;
   }

   /* --- Other tools (if nothing else was noted, list them) --- */
   if (noth > 0 && nw == 0 && nrun == 0 && nr == 0)
   {
      if (need_sep)
         used = buf_cat(buf, used, len, "; ");
      if (noth == 1)
      {
         char tmp[TURN_NAR_NAME_LEN + 16];
         snprintf(tmp, sizeof(tmp), "Called %s", others[0]);
         used = buf_cat(buf, used, len, tmp);
      }
      else
      {
         char tmp[32];
         snprintf(tmp, sizeof(tmp), "%d tool calls", nar->count);
         used = buf_cat(buf, used, len, tmp);
      }
   }

   return used > 0 ? 1 : 0;
}
