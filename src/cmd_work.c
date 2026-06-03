#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
/* cmd_work.c: inter-session work queue for distributing tasks across sessions */
#include "aimee.h"
#include "commands.h"
#include "config.h"
#include "db1.h"
#include "workspace.h"
#include "platform_process.h"
#include "lifecycle.h"
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/* Log a work queue state transition to the audit trail. Thin wrapper
 * over db1_work_queue_log_transition that defaults the session_id to
 * the active session(). */
static void wq_log(const char *item_id, const char *old_status, const char *new_status,
                   const char *detail)
{
   db1_work_queue_log_transition(item_id, old_status, new_status, session_id(), detail);
}

/* Generate a short random ID */
static void generate_work_id(char *buf, size_t len)
{
   unsigned char raw[8];
   if (platform_random_bytes(raw, sizeof(raw)) != 0)
      memset(raw, 0, sizeof(raw));
   snprintf(buf, len, "%02x%02x%02x%02x%02x%02x%02x%02x", raw[0], raw[1], raw[2], raw[3], raw[4],
            raw[5], raw[6], raw[7]);
}

/* --- work add --- */

static void work_add(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *title = opt_pos(&opts, 0);
   if (!title)
      fatal("usage: aimee work add \"title\" [--desc \"...\"] [--source \"...\"] [--priority N]");

   const char *desc = opt_get(&opts, "desc");
   const char *source = opt_get(&opts, "source");
   int priority = opt_get_int(&opts, "priority", 0);

   char id[32];
   generate_work_id(id, sizeof(id));

   if (db1_work_queue_insert(id, title, desc ? desc : "", source ? source : "", priority,
                             session_id()) != 0)
      fatal("failed to insert work item");

   if (ctx->json_output)
   {
      cJSON *obj = cJSON_CreateObject();
      cJSON_AddStringToObject(obj, "id", id);
      cJSON_AddStringToObject(obj, "title", title);
      cJSON_AddStringToObject(obj, "status", "pending");
      char *json = cJSON_PrintUnformatted(obj);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(obj);
   }
   else
   {
      printf("Added work item %s: %s\n", id, title);
   }
}

/* --- work add-batch --- */

static void work_add_batch(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {"from-proposals", NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   if (!opt_has(&opts, "from-proposals"))
      fatal("usage: aimee work add-batch --from-proposals [--dir path]");

   const char *dir = opt_get(&opts, "dir");

   /* Default: docs/proposals/pending/ relative to cwd */
   char default_dir[MAX_PATH_LEN];
   if (!dir)
   {
      char cwd[MAX_PATH_LEN];
      if (!getcwd(cwd, sizeof(cwd)))
         fatal("cannot get cwd");
      snprintf(default_dir, sizeof(default_dir), "%s/docs/proposals/pending", cwd);
      dir = default_dir;
   }

   DIR *d = opendir(dir);
   if (!d)
      fatal("cannot open proposals directory: %s", dir);

   const char *sid = session_id();

   int added = 0;
   int skipped = 0;
   struct dirent *ent;

   while ((ent = readdir(d)) != NULL)
   {
      size_t namelen = strlen(ent->d_name);
      if (namelen < 4 || strcmp(ent->d_name + namelen - 3, ".md") != 0)
         continue;

      /* Build source identifier with relative path */
      char source[512];
      {
         char cwd_buf[MAX_PATH_LEN];
         char abs_dir_buf[MAX_PATH_LEN];
         const char *rel_path = dir;
         if (getcwd(cwd_buf, sizeof(cwd_buf)) && realpath(dir, abs_dir_buf))
         {
            size_t cwdlen = strlen(cwd_buf);
            if (strncmp(abs_dir_buf, cwd_buf, cwdlen) == 0 && abs_dir_buf[cwdlen] == '/')
            {
               rel_path = abs_dir_buf + cwdlen + 1;
            }
         }
         snprintf(source, sizeof(source), "proposal:%s/%s", rel_path, ent->d_name);
      }

      /* Check if already queued (pending, claimed, or done) */
      if (db1_work_queue_count_active_by_source(source) > 0)
      {
         skipped++;
         continue;
      }

      /* Check if proposal exists in done/ or rejected/ directories */
      {
         char proposals_root[MAX_PATH_LEN];
         snprintf(proposals_root, sizeof(proposals_root), "%s", dir);
         char *last_slash = strrchr(proposals_root, '/');
         if (last_slash)
         {
            *last_slash = '\0';
            char check_path[MAX_PATH_LEN];
            struct stat st;

            snprintf(check_path, sizeof(check_path), "%s/done/%s", proposals_root, ent->d_name);
            if (stat(check_path, &st) == 0)
            {
               skipped++;
               continue;
            }

            snprintf(check_path, sizeof(check_path), "%s/rejected/%s", proposals_root, ent->d_name);
            if (stat(check_path, &st) == 0)
            {
               skipped++;
               continue;
            }
         }
      }

      /* Extract title, effort, and priority from proposal */
      char filepath[MAX_PATH_LEN];
      snprintf(filepath, sizeof(filepath), "%s/%s", dir, ent->d_name);

      char title[256];
      char effort[8] = "";
      int priority = 0;
      title[0] = '\0';
      FILE *fp = fopen(filepath, "r");
      if (fp)
      {
         char line[512];
         while (fgets(line, sizeof(line), fp))
         {
            if (!title[0] && strncmp(line, "# ", 2) == 0)
            {
               /* Trim "# Proposal: " prefix if present */
               const char *t = line + 2;
               if (strncmp(t, "Proposal: ", 10) == 0)
                  t += 10;
               size_t tlen = strlen(t);
               while (tlen > 0 && (t[tlen - 1] == '\n' || t[tlen - 1] == '\r'))
                  tlen--;
               if (tlen >= sizeof(title))
                  tlen = sizeof(title) - 1;
               memcpy(title, t, tlen);
               title[tlen] = '\0';
            }

            /* Extract effort: **Effort:** S or **Effort:** M-L */
            if (!effort[0])
            {
               const char *ep = strstr(line, "**Effort:**");
               if (!ep)
                  ep = strstr(line, "**Effort:**");
               if (ep)
               {
                  ep += 11;
                  while (*ep == ' ')
                     ep++;
                  if (*ep == 'S' || *ep == 's')
                     snprintf(effort, sizeof(effort), "S");
                  else if (*ep == 'M' || *ep == 'm')
                     snprintf(effort, sizeof(effort), "M");
                  else if (*ep == 'L' || *ep == 'l')
                     snprintf(effort, sizeof(effort), "L");
                  else if (*ep == 'X' || *ep == 'x')
                     snprintf(effort, sizeof(effort), "XL");
               }
            }

            /* Extract priority: P0=30, P1=20, P2=10, P3=0 */
            if (priority == 0)
            {
               const char *pp = strstr(line, "| P0");
               if (pp)
                  priority = 30;
               else if ((pp = strstr(line, "| P1")) != NULL)
                  priority = 20;
               else if ((pp = strstr(line, "| P2")) != NULL)
                  priority = 10;
            }
         }
         fclose(fp);
      }
      if (!title[0])
      {
         /* Fallback: use filename without .md */
         snprintf(title, sizeof(title), "%.*s", (int)(namelen - 3), ent->d_name);
      }

      /* Insert */
      char id[32];
      generate_work_id(id, sizeof(id));

      if (db1_work_queue_insert_with_effort(id, title, "", source, priority, sid, effort) == 0)
         added++;
   }
   closedir(d);

   if (ctx->json_output)
   {
      cJSON *obj = cJSON_CreateObject();
      cJSON_AddNumberToObject(obj, "added", added);
      cJSON_AddNumberToObject(obj, "skipped", skipped);
      char *json = cJSON_PrintUnformatted(obj);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(obj);
   }
   else
   {
      printf("Added %d work items from proposals (%d already queued).\n", added, skipped);
   }
}

/* --- work claim --- */

static void work_claim(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *specific_id = opt_get(&opts, "id");
   if (!specific_id)
      specific_id = opt_pos(&opts, 0); /* allow positional: aimee work claim <id> */
   const char *effort_filter = opt_get(&opts, "effort");
   const char *tag_filter = opt_get(&opts, "tag");
   const char *exclude_tag = opt_get(&opts, "exclude-tag");
   const char *lane = opt_get(&opts, "lane");
   int skip = opt_get_int(&opts, "skip", 0);
   const char *sid = session_id();

   db1_work_queue_claim_t claim;
   int rc;
   if (specific_id)
      rc = db1_work_queue_claim_by_id(specific_id, sid, lane, &claim);
   else
      rc = db1_work_queue_claim_next(sid, effort_filter, tag_filter, exclude_tag, skip, lane,
                                     &claim);
   if (rc < 0)
      fatal("failed to claim work item");

   if (rc == 1)
   {
      wq_log(claim.id, "pending", "claimed", NULL);

      if (ctx->json_output)
      {
         cJSON *obj = cJSON_CreateObject();
         cJSON_AddStringToObject(obj, "id", claim.id);
         cJSON_AddStringToObject(obj, "title", claim.title);
         cJSON_AddStringToObject(obj, "description", claim.description);
         cJSON_AddStringToObject(obj, "source", claim.source);
         cJSON_AddStringToObject(obj, "lane", claim.lane);
         cJSON_AddNumberToObject(obj, "priority", claim.priority);
         cJSON_AddStringToObject(obj, "status", "claimed");
         cJSON_AddStringToObject(obj, "claimed_by", sid);
         char *json = cJSON_PrintUnformatted(obj);
         printf("%s\n", json);
         free(json);
         cJSON_Delete(obj);
      }
      else
      {
         printf("Claimed: [%s] %s\n", claim.id, claim.title);
         if (claim.source[0])
            printf("  Source: %s\n", claim.source);
         if (claim.lane[0])
            printf("  Lane: %s\n", claim.lane);
         if (claim.description[0])
            printf("  Description: %s\n", claim.description);
      }
   }
   else
   {
      if (ctx->json_output)
         printf("{\"claimed\":false,\"message\":\"No pending work items\"}\n");
      else if (specific_id)
         printf("No pending item with id %s found.\n", specific_id);
      else
         printf("No pending work items to claim.\n");
   }
}

/* --- work complete / fail --- */

static void work_finish(app_ctx_t *ctx, int argc, char **argv, const char *new_status)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *specific_id = opt_get(&opts, "id");
   if (!specific_id)
      specific_id = opt_pos(&opts, 0); /* allow positional: aimee work complete <id> */
   const char *result = opt_get(&opts, "result");
   const char *sid = session_id();

   db1_work_queue_finish_t finish;
   int rc;
   if (specific_id)
      rc = db1_work_queue_finish_by_id(specific_id, new_status, result, &finish);
   else
      rc = db1_work_queue_finish_session_recent(sid, new_status, result, &finish);
   if (rc < 0)
      fatal("failed to update work item");

   if (rc == 1)
   {
      wq_log(finish.id, "claimed", new_status, result);

      if (ctx->json_output)
      {
         cJSON *obj = cJSON_CreateObject();
         cJSON_AddStringToObject(obj, "id", finish.id);
         cJSON_AddStringToObject(obj, "title", finish.title);
         cJSON_AddStringToObject(obj, "status", new_status);
         char *json = cJSON_PrintUnformatted(obj);
         printf("%s\n", json);
         free(json);
         cJSON_Delete(obj);
      }
      else
      {
         printf("Marked %s: [%s] %s\n", new_status, finish.id, finish.title);
      }
   }
   else
   {
      if (ctx->json_output)
         printf("{\"updated\":false,\"message\":\"No matching claimed item found\"}\n");
      else
         printf("No matching claimed work item found for this session.\n");
   }
}

static void work_complete(app_ctx_t *ctx, int argc, char **argv)
{
   work_finish(ctx, argc, argv, "done");
}

static void work_fail(app_ctx_t *ctx, int argc, char **argv)
{
   work_finish(ctx, argc, argv, "failed");
}

/* --- work list --- */

static int parse_work_ts(const char *ts, time_t *out)
{
   if (!ts || !ts[0] || !out)
      return 0;

   struct tm tm;
   memset(&tm, 0, sizeof(tm));
   if (!strptime(ts, "%Y-%m-%dT%H:%M:%SZ", &tm) && !strptime(ts, "%Y-%m-%dT%H:%M:%S", &tm))
      return 0;

   *out = timegm(&tm);
   return 1;
}

static int work_age_minutes(const char *ts)
{
   time_t when;
   if (!parse_work_ts(ts, &when))
      return -1;

   time_t now = time(NULL);
   if (when > now)
      return 0;
   return (int)((now - when) / 60);
}

static void format_work_age(const char *ts, char *buf, size_t cap)
{
   if (!buf || cap == 0)
      return;
   buf[0] = '\0';

   int age_min = work_age_minutes(ts);
   if (age_min < 0)
      return;

   if (age_min < 60)
      snprintf(buf, cap, "%dm", age_min);
   else if (age_min < 24 * 60)
      snprintf(buf, cap, "%dh%dm", age_min / 60, age_min % 60);
   else
      snprintf(buf, cap, "%dd%dh", age_min / (24 * 60), (age_min / 60) % 24);
}

static void work_list(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *status_filter = opt_get(&opts, "status");

   db1_work_queue_list_row_t *rows = NULL;
   size_t n_rows = 0;
   if (db1_work_queue_alloc_list(status_filter, &rows, &n_rows) != 0)
      fatal("failed to list work items");

   if (ctx->json_output)
   {
      cJSON *arr = cJSON_CreateArray();
      for (size_t i = 0; i < n_rows; i++)
      {
         cJSON *obj = cJSON_CreateObject();
         cJSON_AddStringToObject(obj, "id", rows[i].id);
         cJSON_AddStringToObject(obj, "title", rows[i].title);
         cJSON_AddStringToObject(obj, "source", rows[i].source);
         cJSON_AddStringToObject(obj, "status", rows[i].status);
         if (rows[i].claimed_by[0])
            cJSON_AddStringToObject(obj, "claimed_by", rows[i].claimed_by);
         if (rows[i].result[0])
            cJSON_AddStringToObject(obj, "result", rows[i].result);
         cJSON_AddItemToArray(arr, obj);
      }
      char *json = cJSON_Print(arr);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(arr);
   }
   else
   {
      for (size_t i = 0; i < n_rows; i++)
      {
         const char *id = rows[i].id;
         const char *title = rows[i].title;
         const char *source = rows[i].source;
         const char *status = rows[i].status;
         const char *claimed = rows[i].claimed_by;
         const char *result = rows[i].result;
         const char *claimed_at = rows[i].claimed_at;
         printf("[%s] %-10s %s", id, status, title);
         if (claimed[0])
            printf("  (session: %.8s)", claimed);
         if (claimed_at[0] && strcmp(status, "claimed") == 0)
         {
            char age[32];
            format_work_age(claimed_at, age, sizeof(age));
            if (age[0])
               printf("  (%s ago)", age);
         }
         if (source[0])
            printf("  [%s]", source);
         if (result[0])
            printf("  -> %s", result);
         printf("\n");
      }
      if (n_rows == 0)
         printf("No work items found.\n");
   }
   free(rows);
}

/* --- work board --- */

static const char *work_board_age_source(const db1_work_queue_list_row_t *row)
{
   if (row->claimed_at[0] && strcmp(row->status, "claimed") == 0)
      return row->claimed_at;
   return row->created_at;
}

static void work_board_add_json_card(cJSON *arr, const db1_work_queue_list_row_t *row)
{
   cJSON *obj = cJSON_CreateObject();
   cJSON_AddStringToObject(obj, "id", row->id);
   cJSON_AddStringToObject(obj, "title", row->title);
   cJSON_AddStringToObject(obj, "status", row->status);
   if (row->claimed_by[0])
      cJSON_AddStringToObject(obj, "claimed_by", row->claimed_by);
   if (row->source[0])
      cJSON_AddStringToObject(obj, "source", row->source);
   if (row->result[0])
      cJSON_AddStringToObject(obj, "result", row->result);

   int age_min = work_age_minutes(work_board_age_source(row));
   if (age_min >= 0)
      cJSON_AddNumberToObject(obj, "age_minutes", age_min);
   cJSON_AddItemToArray(arr, obj);
}

static void work_board_history(app_ctx_t *ctx, const char *id)
{
   db1_work_queue_log_row_t *rows = NULL;
   size_t n_rows = 0;
   if (db1_work_queue_alloc_log(id, &rows, &n_rows) != 0)
      fatal("failed to read work history");

   if (ctx->json_output)
   {
      cJSON *root = cJSON_CreateObject();
      cJSON_AddStringToObject(root, "id", id);
      cJSON *arr = cJSON_AddArrayToObject(root, "history");
      for (size_t i = 0; i < n_rows; i++)
      {
         cJSON *obj = cJSON_CreateObject();
         cJSON_AddStringToObject(obj, "old_status", rows[i].old_status);
         cJSON_AddStringToObject(obj, "new_status", rows[i].new_status);
         cJSON_AddStringToObject(obj, "session_id", rows[i].session_id);
         cJSON_AddStringToObject(obj, "detail", rows[i].detail);
         cJSON_AddStringToObject(obj, "created_at", rows[i].created_at);
         cJSON_AddItemToArray(arr, obj);
      }
      char *json = cJSON_Print(root);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(root);
   }
   else
   {
      printf("Work history for %s\n", id);
      for (size_t i = 0; i < n_rows; i++)
      {
         printf("  %s  %s -> %s", rows[i].created_at, rows[i].old_status, rows[i].new_status);
         if (rows[i].session_id[0])
            printf("  session:%.8s", rows[i].session_id);
         if (rows[i].detail[0])
            printf("  detail:%s", rows[i].detail);
         printf("\n");
      }
      if (n_rows == 0)
         printf("  (no history)\n");
   }
   free(rows);
}

static void work_board(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);
   const char *history_id = opt_get(&opts, "history");
   if (history_id)
   {
      if (!history_id[0])
         fatal("--history requires an item id");
      work_board_history(ctx, history_id);
      return;
   }

   static const char *statuses[] = {"pending", "claimed", "done", "failed", "cancelled"};
   db1_work_queue_list_row_t *rows = NULL;
   size_t n_rows = 0;
   if (db1_work_queue_alloc_list("all", &rows, &n_rows) != 0)
      fatal("failed to list work items");

   if (ctx->json_output)
   {
      cJSON *root = cJSON_CreateObject();
      for (int s = 0; s < 5; s++)
      {
         cJSON *arr = cJSON_AddArrayToObject(root, statuses[s]);
         for (size_t i = 0; i < n_rows; i++)
         {
            if (strcmp(rows[i].status, statuses[s]) == 0)
               work_board_add_json_card(arr, &rows[i]);
         }
      }
      char *json = cJSON_Print(root);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(root);
      free(rows);
      return;
   }

   printf("Work board\n");
   int total = 0;
   for (int s = 0; s < 5; s++)
   {
      int count = 0;
      for (size_t i = 0; i < n_rows; i++)
      {
         if (strcmp(rows[i].status, statuses[s]) == 0)
            count++;
      }
      total += count;

      printf("\n%s (%d)\n", statuses[s], count);
      if (count == 0)
      {
         printf("  (empty)\n");
         continue;
      }

      for (size_t i = 0; i < n_rows; i++)
      {
         db1_work_queue_list_row_t *row = &rows[i];
         if (strcmp(row->status, statuses[s]) != 0)
            continue;

         char age[32];
         format_work_age(work_board_age_source(row), age, sizeof(age));
         printf("  [%s] %s", row->id, row->title);
         if (age[0])
            printf("  age:%s", age);
         if (row->claimed_by[0])
            printf("  owner:%.8s", row->claimed_by);
         if (row->source[0])
            printf("  source:%s", row->source);
         if (row->result[0])
            printf("  result:%s", row->result);
         printf("\n");
      }
   }

   if (total == 0)
      printf("\nNo work items found.\n");

   free(rows);
}

/* --- work cancel --- */

static void work_cancel(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *id = opt_get(&opts, "id");
   if (!id)
      fatal("usage: aimee work cancel --id ITEM_ID");

   db1_work_queue_finish_t finish;
   int rc = db1_work_queue_cancel_by_id(id, &finish);
   if (rc < 0)
      fatal("failed to cancel work item");
   if (rc == 1)
   {
      wq_log(id, "pending", "cancelled", NULL);
      printf("Cancelled: [%s] %s\n", id, finish.title);
   }
   else
   {
      printf("No pending item with id %s found.\n", id);
   }
}

/* --- work release --- */

static void work_release(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *id = opt_get(&opts, "id");
   if (!id)
      fatal("usage: aimee work release --id ITEM_ID");

   db1_work_queue_finish_t finish;
   int rc = db1_work_queue_release_by_id(id, &finish);
   if (rc < 0)
      fatal("failed to release work item");
   if (rc == 1)
   {
      wq_log(id, "claimed", "pending", "released");
      printf("Released: [%s] %s\n", id, finish.title);
   }
   else
   {
      printf("No claimed item with id %s found.\n", id);
   }
}

/* --- work clear --- */

static void work_clear(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *status = opt_get(&opts, "status");
   if (!status)
      fatal("usage: aimee work clear --status done|failed|cancelled");

   /* Materialize-then-log-then-delete: the DB2 connection permits one active
    * result at a time, and wq_log issues its own INSERT. */
   char(*ids)[32] = NULL;
   size_t n_ids = 0;
   if (db1_work_queue_alloc_ids_by_status(status, &ids, &n_ids) != 0)
      fatal("failed to list %s items", status);
   for (size_t i = 0; i < n_ids; i++)
      wq_log(ids[i], status, "cleared", NULL);
   free(ids);

   int changes = db1_work_queue_delete_by_status(status);
   if (changes < 0)
      fatal("failed to delete %s items", status);

   printf("Cleared %d %s items.\n", changes, status);
}

/* --- work gc --- */

static void work_gc(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   static const char *bool_flags[] = {"no-sync-proposals", NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   int max_age_hours = opt_get_int(&opts, "max-age", 2);

   char cutoff[32];
   {
      time_t now = time(NULL);
      time_t threshold = now - (max_age_hours * 3600);
      struct tm tm;
      gmtime_r(&threshold, &tm);
      strftime(cutoff, sizeof(cutoff), "%Y-%m-%dT%H:%M:%SZ", &tm);
   }

   db1_work_queue_finish_t *released_rows = NULL;
   size_t released_count = 0;
   if (db1_work_queue_release_stale(cutoff, &released_rows, &released_count) != 0)
      fatal("failed to release stale claims");

   for (size_t i = 0; i < released_count; i++)
   {
      wq_log(released_rows[i].id, "claimed", "pending", "gc: stale claim released");
      printf("Released stale claim: [%s] %s\n", released_rows[i].id, released_rows[i].title);
   }
   free(released_rows);
   int released = (int)released_count;

   int closed = 0, cancelled = 0;
   if (!opt_has(&opts, "no-sync-proposals"))
      work_sync_proposals(NULL, &closed, &cancelled);

   if (released == 0 && closed == 0 && cancelled == 0)
      printf("No stale claims found (threshold: %d hours).\n", max_age_hours);
   else
   {
      if (released > 0)
         printf("Released %d stale claims.\n", released);
      if (closed > 0 || cancelled > 0)
         printf("Synced proposals: %d closed as done, %d cancelled.\n", closed, cancelled);
   }
}

/* --- work stats --- */

static void work_stats(app_ctx_t *ctx, int argc, char **argv)
{
   (void)argc;
   (void)argv;

   int pending = 0, claimed = 0, done = 0, failed = 0, cancelled = 0;
   int log_completed = 0;
   double avg_minutes = 0;
   db1_work_queue_stats(&pending, &claimed, &done, &failed, &cancelled, &log_completed,
                        &avg_minutes);

   int total = pending + claimed + done + failed + cancelled;

   if (ctx->json_output)
   {
      cJSON *obj = cJSON_CreateObject();
      cJSON_AddNumberToObject(obj, "total", total);
      cJSON_AddNumberToObject(obj, "pending", pending);
      cJSON_AddNumberToObject(obj, "claimed", claimed);
      cJSON_AddNumberToObject(obj, "done", done);
      cJSON_AddNumberToObject(obj, "failed", failed);
      cJSON_AddNumberToObject(obj, "cancelled", cancelled);
      cJSON_AddNumberToObject(obj, "completed_with_timing", log_completed);
      if (log_completed > 0)
         cJSON_AddNumberToObject(obj, "avg_completion_minutes", avg_minutes);
      char *json = cJSON_PrintUnformatted(obj);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(obj);
   }
   else
   {
      printf("Queue stats:\n");
      printf("  Total items: %d\n", total);
      printf("  Pending:     %d\n", pending);
      printf("  Claimed:     %d\n", claimed);
      printf("  Completed:   %d", done);
      if (log_completed > 0)
         printf(" (avg %.0fm)", avg_minutes);
      printf("\n");
      printf("  Failed:      %d\n", failed);
      printf("  Cancelled:   %d\n", cancelled);
   }
}

/* --- work sync-proposals ---
 *
 * Reconciles the work queue against the state of proposal files on disk. When
 * a proposal is moved from pending/ to done/ (or accepted/deferred/rejected/),
 * the corresponding pending/claimed work item is closed with a matching status
 * so the queue doesn't keep offering work whose source has already been
 * finalized. */

static const char *sync_source_pending_prefix(const char *source, const char *base,
                                              char *out_filename, size_t out_cap)
{
   /* Accept sources of the form:
    *    proposal:<base>/pending/<file>.md
    * or proposal:<relative-base>/pending/<file>.md when the queue item was
    * imported using a path relative to cwd.
    * Returns the <file>.md portion if source matches, else NULL. */
   if (!source || !base || !out_filename)
      return NULL;

   const char *prefix = "proposal:";
   size_t plen = strlen(prefix);
   if (strncmp(source, prefix, plen) != 0)
      return NULL;

   char needle[MAX_PATH_LEN];
   int n = snprintf(needle, sizeof(needle), "%s/pending/", base);
   if (n <= 0 || (size_t)n >= sizeof(needle))
      return NULL;

   const char *path = source + plen;
   size_t nlen = strlen(needle);
   if (strncmp(path, needle, nlen) != 0)
   {
      char cwd[MAX_PATH_LEN];
      if (!getcwd(cwd, sizeof(cwd)))
         return NULL;

      size_t cwd_len = strlen(cwd);
      if (strncmp(base, cwd, cwd_len) != 0 || base[cwd_len] != '/')
         return NULL;

      n = snprintf(needle, sizeof(needle), "%s/pending/", base + cwd_len + 1);
      if (n <= 0 || (size_t)n >= sizeof(needle))
         return NULL;
      nlen = strlen(needle);
      if (strncmp(path, needle, nlen) != 0)
         return NULL;
   }

   const char *fname = path + nlen;
   if (!*fname || strchr(fname, '/') != NULL)
      return NULL;

   snprintf(out_filename, out_cap, "%s", fname);
   return out_filename;
}

/* Returns "done" (moved to done/accepted), "cancelled" (moved to
 * deferred/rejected), or NULL (still in pending/ or not found anywhere). */
static const char *sync_classify(const char *base, const char *filename)
{
   char path[MAX_PATH_LEN];
   struct stat st;

   snprintf(path, sizeof(path), "%s/pending/%s", base, filename);
   if (stat(path, &st) == 0)
      return NULL; /* still pending */

   snprintf(path, sizeof(path), "%s/done/%s", base, filename);
   if (stat(path, &st) == 0)
      return "done";
   snprintf(path, sizeof(path), "%s/accepted/%s", base, filename);
   if (stat(path, &st) == 0)
      return "done";
   snprintf(path, sizeof(path), "%s/deferred/%s", base, filename);
   if (stat(path, &st) == 0)
      return "cancelled";
   snprintf(path, sizeof(path), "%s/rejected/%s", base, filename);
   if (stat(path, &st) == 0)
      return "cancelled";

   return NULL;
}

int work_sync_proposals(const char *proposals_base, int *out_closed, int *out_cancelled)
{
   if (!db1_is_initialized())
      return -1;

   char base_buf[MAX_PATH_LEN];
   if (!proposals_base)
   {
      char cwd[MAX_PATH_LEN];
      if (!getcwd(cwd, sizeof(cwd)))
         return -1;
      snprintf(base_buf, sizeof(base_buf), "%s/docs/proposals", cwd);
      proposals_base = base_buf;
   }

   /* Snapshot pending + claimed items first; update rows in a separate step to
    * avoid iterating while mutating. */
   db1_work_queue_active_row_t *rows = NULL;
   size_t nrows = 0;
   if (db1_work_queue_alloc_active(&rows, &nrows) != 0)
      return -1;

   int closed = 0, cancelled = 0;
   for (size_t i = 0; i < nrows; i++)
   {
      char filename[256];
      if (!sync_source_pending_prefix(rows[i].source, proposals_base, filename, sizeof(filename)))
         continue;

      const char *new_status = sync_classify(proposals_base, filename);
      if (!new_status)
         continue;

      char result[256];
      if (strcmp(new_status, "done") == 0)
         snprintf(result, sizeof(result), "sync-proposals: proposal moved out of pending/");
      else
         snprintf(result, sizeof(result), "sync-proposals: proposal deferred or rejected");

      if (db1_work_queue_force_finish(rows[i].id, new_status, result) > 0)
      {
         wq_log(rows[i].id, rows[i].status, new_status, result);
         if (strcmp(new_status, "done") == 0)
            closed++;
         else
            cancelled++;
      }
   }

   free(rows);
   if (out_closed)
      *out_closed = closed;
   if (out_cancelled)
      *out_cancelled = cancelled;
   return 0;
}

static void work_sync_proposals_cmd(app_ctx_t *ctx, int argc, char **argv)
{
   static const char *bool_flags[] = {NULL};
   opt_parsed_t opts;
   opt_parse(argc, argv, bool_flags, &opts);

   const char *base = opt_get(&opts, "base");

   int closed = 0, cancelled = 0;
   if (work_sync_proposals(base, &closed, &cancelled) != 0)
      fatal("work sync-proposals failed");

   if (ctx->json_output)
   {
      cJSON *obj = cJSON_CreateObject();
      cJSON_AddNumberToObject(obj, "closed", closed);
      cJSON_AddNumberToObject(obj, "cancelled", cancelled);
      char *json = cJSON_PrintUnformatted(obj);
      printf("%s\n", json);
      free(json);
      cJSON_Delete(obj);
   }
   else if (closed == 0 && cancelled == 0)
   {
      printf("No work items to sync — all sources are still in pending/.\n");
   }
   else
   {
      printf("Synced work queue: %d closed as done, %d cancelled.\n", closed, cancelled);
   }
}

/* --- Subcmd table --- */

static const subcmd_t work_subcmds[] = {
    {"add", "Add a work item to the queue", work_add},
    {"add-batch", "Batch-add items (--from-proposals)", work_add_batch},
    {"claim", "Claim the next pending item", work_claim},
    {"complete", "Mark claimed item as done", work_complete},
    {"fail", "Mark claimed item as failed", work_fail},
    {"list", "List work items", work_list},
    {"board", "Show a status-board view or --history ITEM", work_board},
    {"cancel", "Cancel a pending item", work_cancel},
    {"release", "Release a claimed item back to pending", work_release},
    {"clear", "Remove items by status", work_clear},
    {"gc", "Release stale claims (--max-age N hours, default 2)", work_gc},
    {"sync-proposals", "Close items whose proposal has moved out of pending/",
     work_sync_proposals_cmd},
    {"stats", "Show queue statistics and throughput", work_stats},
    {NULL, NULL, NULL},
};

const subcmd_t *get_work_subcmds(void)
{
   return work_subcmds;
}

/* --- work queue summary (for session-start context) --- */

int work_queue_summary(char *buf, size_t cap)
{
   int pending = 0, claimed = 0;
   if (db1_work_queue_stats(&pending, &claimed, NULL, NULL, NULL, NULL, NULL) != 0)
      return -1;

   if (pending == 0 && claimed == 0)
      return 0;

   int len = snprintf(buf, cap,
                      "# Work Queue\n"
                      "There are %d pending items in the work queue. "
                      "Run `aimee work claim` to pick one up.\n",
                      pending);
   if (claimed > 0 && (size_t)len < cap)
      len += snprintf(buf + len, cap - (size_t)len,
                      "Currently claimed by other sessions: %d items.\n", claimed);

   return len;
}

/* --- Main entry point --- */

void cmd_work(app_ctx_t *ctx, int argc, char **argv)
{
   if (argc < 1)
   {
      subcmd_usage("work", work_subcmds);
      return;
   }

   if (!db1_is_initialized())
   {
      config_t cfg;
      config_load(&cfg);
      if (db1_init(cfg.db1_path) != 0)
         fatal("cannot open database");
   }

   subcmd_dispatch(work_subcmds, argv[0], ctx, argc - 1, argv + 1);
}
