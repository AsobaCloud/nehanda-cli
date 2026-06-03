/* server_work.c: typed server RPCs for the inter-session work queue */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "aimee.h"
#include "server.h"
#include "json_fluent.h"
#include "platform_random.h"
#include "db1.h"
#include "cJSON.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int work_send(server_conn_t *conn, cJSON *resp)
{
   return server_send_ok(conn, resp);
}

static void work_log(const char *item_id, const char *old_status, const char *new_status,
                     const char *session_id, const char *detail)
{
   db1_work_queue_log_transition(item_id, old_status, new_status, session_id, detail);
}

static const char *work_session(cJSON *req)
{
   const char *sid = jo_str(req, "session_id", NULL);
   return (sid && sid[0]) ? sid : "default";
}

static void generate_work_id(char *buf, size_t len)
{
   unsigned char raw[8];
   if (platform_random_bytes(raw, sizeof(raw)) != 0)
      memset(raw, 0, sizeof(raw));
   snprintf(buf, len, "%02x%02x%02x%02x%02x%02x%02x%02x", raw[0], raw[1], raw[2], raw[3], raw[4],
            raw[5], raw[6], raw[7]);
}

static void resolve_client_path(cJSON *req, const char *path, const char *fallback, char *out,
                                size_t cap)
{
   const char *cwd = jo_str(req, "client_cwd", NULL);
   char cwd_buf[MAX_PATH_LEN];
   if (!cwd || !cwd[0])
   {
      if (getcwd(cwd_buf, sizeof(cwd_buf)))
         cwd = cwd_buf;
      else
         cwd = ".";
   }

   const char *p = (path && path[0]) ? path : fallback;
   if (p && p[0] == '/')
      snprintf(out, cap, "%s", p);
   else
      snprintf(out, cap, "%s/%s", cwd, p ? p : "");
}

static void add_work_row(cJSON *arr, const db1_work_queue_list_row_t *row)
{
   cJSON *obj = cJSON_CreateObject();
   jo_add_str(obj, "id", row->id);
   jo_add_str(obj, "title", row->title);
   jo_add_str(obj, "source", row->source);
   jo_add_str(obj, "status", row->status);
   if (row->created_at[0])
      jo_add_str(obj, "created_at", row->created_at);
   if (row->claimed_by[0])
      jo_add_str(obj, "claimed_by", row->claimed_by);
   if (row->claimed_at[0])
      jo_add_str(obj, "claimed_at", row->claimed_at);
   if (row->result[0])
      jo_add_str(obj, "result", row->result);
   cJSON_AddItemToArray(arr, obj);
}

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

static int work_age_minutes(const db1_work_queue_list_row_t *row)
{
   const char *ts = row->created_at;
   if (row->claimed_at[0] && strcmp(row->status, "claimed") == 0)
      ts = row->claimed_at;

   time_t when;
   if (!parse_work_ts(ts, &when))
      return -1;

   time_t now = time(NULL);
   if (when > now)
      return 0;
   return (int)((now - when) / 60);
}

static void add_work_board_card(cJSON *arr, const db1_work_queue_list_row_t *row)
{
   cJSON *obj = cJSON_CreateObject();
   jo_add_str(obj, "id", row->id);
   jo_add_str(obj, "title", row->title);
   jo_add_str(obj, "status", row->status);
   if (row->source[0])
      jo_add_str(obj, "source", row->source);
   if (row->claimed_by[0])
      jo_add_str(obj, "claimed_by", row->claimed_by);
   if (row->result[0])
      jo_add_str(obj, "result", row->result);

   int age_min = work_age_minutes(row);
   if (age_min >= 0)
      cJSON_AddNumberToObject(obj, "age_minutes", age_min);

   cJSON_AddItemToArray(arr, obj);
}

int handle_work_add(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   const char *title = jo_str(req, "title", NULL);
   if (!title || !title[0])
      return server_send_error(conn, "usage: aimee work add \"title\" [--desc ...] [--source ...]",
                               NULL);

   char id[32];
   generate_work_id(id, sizeof(id));

   const char *desc = jo_str(req, "description", "");
   const char *source = jo_str(req, "source", "");
   int priority = jo_int(req, "priority", 0);
   const char *sid = work_session(req);

   if (db1_work_queue_insert(id, title, desc, source, priority, sid) != 0)
      return server_send_error(conn, "failed to insert work item", NULL);

   cJSON *resp = jo_ok();
   jo_add_str(resp, "id", id);
   jo_add_str(resp, "title", title);
   jo_add_str(resp, "work_status", "pending");
   return work_send(conn, resp);
}

static int proposal_already_finalized(const char *proposals_root, const char *filename)
{
   static const char *dirs[] = {"done", "accepted", "deferred", "rejected", NULL};
   char path[MAX_PATH_LEN];
   struct stat st;
   for (int i = 0; dirs[i]; i++)
   {
      snprintf(path, sizeof(path), "%s/%s/%s", proposals_root, dirs[i], filename);
      if (stat(path, &st) == 0)
         return 1;
   }
   return 0;
}

static void extract_proposal_metadata(const char *path, const char *fallback_name, char *title,
                                      size_t title_cap, char *effort, size_t effort_cap,
                                      int *priority)
{
   title[0] = '\0';
   effort[0] = '\0';
   *priority = 0;

   FILE *fp = fopen(path, "r");
   if (fp)
   {
      char line[512];
      while (fgets(line, sizeof(line), fp))
      {
         if (!title[0] && strncmp(line, "# ", 2) == 0)
         {
            const char *t = line + 2;
            if (strncmp(t, "Proposal: ", 10) == 0)
               t += 10;
            size_t tlen = strlen(t);
            while (tlen > 0 && (t[tlen - 1] == '\n' || t[tlen - 1] == '\r'))
               tlen--;
            if (tlen >= title_cap)
               tlen = title_cap - 1;
            memcpy(title, t, tlen);
            title[tlen] = '\0';
         }

         if (!effort[0])
         {
            const char *ep = strstr(line, "**Effort:**");
            if (ep)
            {
               ep += 11;
               while (*ep == ' ')
                  ep++;
               if (*ep == 'S' || *ep == 's')
                  snprintf(effort, effort_cap, "S");
               else if (*ep == 'M' || *ep == 'm')
                  snprintf(effort, effort_cap, "M");
               else if (*ep == 'L' || *ep == 'l')
                  snprintf(effort, effort_cap, "L");
               else if (*ep == 'X' || *ep == 'x')
                  snprintf(effort, effort_cap, "XL");
            }
         }

         if (*priority == 0)
         {
            if (strstr(line, "| P0"))
               *priority = 30;
            else if (strstr(line, "| P1"))
               *priority = 20;
            else if (strstr(line, "| P2"))
               *priority = 10;
         }
      }
      fclose(fp);
   }

   if (!title[0])
   {
      size_t len = strlen(fallback_name);
      if (len > 3 && strcmp(fallback_name + len - 3, ".md") == 0)
         len -= 3;
      if (len >= title_cap)
         len = title_cap - 1;
      memcpy(title, fallback_name, len);
      title[len] = '\0';
   }
}

int handle_work_add_batch(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(req, "from_proposals")))
      return server_send_error(conn, "usage: aimee work add-batch --from-proposals [--dir path]",
                               NULL);

   char dir[MAX_PATH_LEN];
   resolve_client_path(req, jo_str(req, "dir", NULL), "docs/proposals/pending", dir, sizeof(dir));

   DIR *d = opendir(dir);
   if (!d)
      return server_send_error(conn, "cannot open proposals directory", NULL);

   char proposals_root[MAX_PATH_LEN];
   snprintf(proposals_root, sizeof(proposals_root), "%s", dir);
   char *slash = strrchr(proposals_root, '/');
   if (slash)
      *slash = '\0';

   const char *sid = work_session(req);
   const char *cwd = jo_str(req, "client_cwd", "");
   int added = 0;
   int skipped = 0;
   struct dirent *ent;
   while ((ent = readdir(d)) != NULL)
   {
      size_t namelen = strlen(ent->d_name);
      if (namelen < 4 || strcmp(ent->d_name + namelen - 3, ".md") != 0)
         continue;

      if (proposal_already_finalized(proposals_root, ent->d_name))
      {
         skipped++;
         continue;
      }

      char rel_dir[MAX_PATH_LEN];
      snprintf(rel_dir, sizeof(rel_dir), "%s", dir);
      if (cwd[0])
      {
         size_t cwdlen = strlen(cwd);
         if (strncmp(dir, cwd, cwdlen) == 0 && dir[cwdlen] == '/')
            snprintf(rel_dir, sizeof(rel_dir), "%s", dir + cwdlen + 1);
      }

      char source[512];
      snprintf(source, sizeof(source), "proposal:%s/%s", rel_dir, ent->d_name);
      if (db1_work_queue_count_active_by_source(source) > 0)
      {
         skipped++;
         continue;
      }

      char filepath[MAX_PATH_LEN];
      snprintf(filepath, sizeof(filepath), "%s/%s", dir, ent->d_name);

      char title[256];
      char effort[8];
      int priority = 0;
      extract_proposal_metadata(filepath, ent->d_name, title, sizeof(title), effort, sizeof(effort),
                                &priority);

      char id[32];
      generate_work_id(id, sizeof(id));
      if (db1_work_queue_insert_with_effort(id, title, "", source, priority, sid, effort) == 0)
         added++;
   }
   closedir(d);

   cJSON *resp = jo_ok();
   cJSON_AddNumberToObject(resp, "added", added);
   cJSON_AddNumberToObject(resp, "skipped", skipped);
   return work_send(conn, resp);
}

int handle_work_claim(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   const char *sid = work_session(req);
   const char *id = jo_str(req, "id", NULL);
   const char *lane = jo_str(req, "lane", NULL);
   db1_work_queue_claim_t claim;
   int rc = id && id[0] ? db1_work_queue_claim_by_id(id, sid, lane, &claim)
                        : db1_work_queue_claim_next(sid, jo_str(req, "effort", NULL),
                                                    jo_str(req, "tag", NULL),
                                                    jo_str(req, "exclude_tag", NULL),
                                                    jo_int(req, "skip", 0), lane, &claim);
   if (rc < 0)
      return server_send_error(conn, "failed to claim work item", NULL);

   cJSON *resp = jo_ok();
   if (rc == 1)
   {
      work_log(claim.id, "pending", "claimed", sid, NULL);
      cJSON_AddTrueToObject(resp, "claimed");
      jo_add_str(resp, "id", claim.id);
      jo_add_str(resp, "title", claim.title);
      jo_add_str(resp, "description", claim.description);
      jo_add_str(resp, "source", claim.source);
      jo_add_str(resp, "lane", claim.lane);
      cJSON_AddNumberToObject(resp, "priority", claim.priority);
      jo_add_str(resp, "work_status", "claimed");
      jo_add_str(resp, "claimed_by", sid);
   }
   else
   {
      cJSON_AddFalseToObject(resp, "claimed");
      if (id && id[0])
      {
         char msg[128];
         snprintf(msg, sizeof(msg), "No pending item with id %s found.", id);
         jo_add_str(resp, "message", msg);
      }
      else
         jo_add_str(resp, "message", "No pending work items to claim.");
   }
   return work_send(conn, resp);
}

static int work_finish(server_ctx_t *ctx, server_conn_t *conn, cJSON *req, const char *new_status)
{
   (void)ctx;

   const char *sid = work_session(req);
   const char *id = jo_str(req, "id", NULL);
   const char *result = jo_str(req, "result", NULL);
   db1_work_queue_finish_t finish;
   int rc = id && id[0] ? db1_work_queue_finish_by_id(id, new_status, result, &finish)
                        : db1_work_queue_finish_session_recent(sid, new_status, result, &finish);
   if (rc < 0)
      return server_send_error(conn, "failed to update work item", NULL);

   cJSON *resp = jo_ok();
   if (rc == 1)
   {
      work_log(finish.id, "claimed", new_status, sid, result);
      cJSON_AddTrueToObject(resp, "updated");
      jo_add_str(resp, "id", finish.id);
      jo_add_str(resp, "title", finish.title);
      jo_add_str(resp, "work_status", new_status);
   }
   else
   {
      cJSON_AddFalseToObject(resp, "updated");
      jo_add_str(resp, "message", "No matching claimed work item found for this session.");
   }
   return work_send(conn, resp);
}

int handle_work_complete(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   return work_finish(ctx, conn, req, "done");
}

int handle_work_fail(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   return work_finish(ctx, conn, req, "failed");
}

int handle_work_list(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   db1_work_queue_list_row_t *rows = NULL;
   size_t n_rows = 0;
   if (db1_work_queue_alloc_list(jo_str(req, "status_filter", NULL), &rows, &n_rows) != 0)
      return server_send_error(conn, "failed to list work items", NULL);

   cJSON *resp = jo_ok();
   cJSON *arr = cJSON_AddArrayToObject(resp, "items");
   for (size_t i = 0; i < n_rows; i++)
      add_work_row(arr, &rows[i]);
   free(rows);
   return work_send(conn, resp);
}

int handle_work_board(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   (void)req;

   static const char *statuses[] = {"pending", "claimed", "done", "failed", "cancelled"};
   db1_work_queue_list_row_t *rows = NULL;
   size_t n_rows = 0;
   if (db1_work_queue_alloc_list("all", &rows, &n_rows) != 0)
      return server_send_error(conn, "failed to list work items", NULL);

   cJSON *resp = jo_ok();
   cJSON *board = cJSON_AddObjectToObject(resp, "board");
   for (int s = 0; s < 5; s++)
   {
      cJSON *arr = cJSON_AddArrayToObject(board, statuses[s]);
      for (size_t i = 0; i < n_rows; i++)
      {
         if (strcmp(rows[i].status, statuses[s]) == 0)
            add_work_board_card(arr, &rows[i]);
      }
   }

   free(rows);
   return work_send(conn, resp);
}

static int work_single_item(server_conn_t *conn, cJSON *req, const char *flag, const char *missing,
                            int (*fn)(const char *, db1_work_queue_finish_t *),
                            const char *old_status, const char *new_status, const char *detail)
{
   const char *id = jo_str(req, "id", NULL);
   if (!id || !id[0])
      return server_send_error(conn, "work item id required", NULL);

   db1_work_queue_finish_t finish;
   int rc = fn(id, &finish);
   if (rc < 0)
      return server_send_error(conn, "failed to update work item", NULL);

   cJSON *resp = jo_ok();
   if (rc == 1)
   {
      work_log(id, old_status, new_status, work_session(req), detail);
      cJSON_AddTrueToObject(resp, flag);
      jo_add_str(resp, "id", id);
      jo_add_str(resp, "title", finish.title);
   }
   else
   {
      cJSON_AddFalseToObject(resp, flag);
      jo_add_str(resp, "message", missing);
   }
   return work_send(conn, resp);
}

int handle_work_cancel(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   return work_single_item(conn, req, "cancelled", "No pending item found.",
                           db1_work_queue_cancel_by_id, "pending", "cancelled", NULL);
}

int handle_work_release(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   return work_single_item(conn, req, "released", "No claimed item found.",
                           db1_work_queue_release_by_id, "claimed", "pending", "released");
}

int handle_work_clear(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   const char *status = jo_str(req, "status_filter", NULL);
   if (!status || !status[0])
      return server_send_error(conn, "usage: aimee work clear --status done|failed|cancelled",
                               NULL);

   char(*ids)[32] = NULL;
   size_t n_ids = 0;
   if (db1_work_queue_alloc_ids_by_status(status, &ids, &n_ids) != 0)
      return server_send_error(conn, "failed to list work items", NULL);
   for (size_t i = 0; i < n_ids; i++)
      work_log(ids[i], status, "cleared", work_session(req), NULL);
   free(ids);

   int changes = db1_work_queue_delete_by_status(status);
   if (changes < 0)
      return server_send_error(conn, "failed to delete work items", NULL);

   cJSON *resp = jo_ok();
   cJSON_AddNumberToObject(resp, "cleared", changes);
   jo_add_str(resp, "status_filter", status);
   return work_send(conn, resp);
}

static const char *sync_source_pending_prefix(const char *source, const char *base, const char *cwd,
                                              char *out_filename, size_t out_cap)
{
   const char *prefix = "proposal:";
   size_t plen = strlen(prefix);
   if (!source || strncmp(source, prefix, plen) != 0)
      return NULL;

   char needle[MAX_PATH_LEN];
   snprintf(needle, sizeof(needle), "%s/pending/", base);
   const char *path = source + plen;
   size_t nlen = strlen(needle);
   if (strncmp(path, needle, nlen) != 0)
   {
      if (!cwd || !cwd[0])
         return NULL;
      size_t cwd_len = strlen(cwd);
      if (strncmp(base, cwd, cwd_len) != 0 || base[cwd_len] != '/')
         return NULL;
      snprintf(needle, sizeof(needle), "%s/pending/", base + cwd_len + 1);
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

static const char *sync_classify(const char *base, const char *filename)
{
   char path[MAX_PATH_LEN];
   struct stat st;

   snprintf(path, sizeof(path), "%s/pending/%s", base, filename);
   if (stat(path, &st) == 0)
      return NULL;
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

static int work_sync_proposals_server(cJSON *req, int *out_closed, int *out_cancelled)
{
   char base[MAX_PATH_LEN];
   resolve_client_path(req, jo_str(req, "base", NULL), "docs/proposals", base, sizeof(base));
   const char *cwd = jo_str(req, "client_cwd", "");
   const char *sid = work_session(req);

   db1_work_queue_active_row_t *rows = NULL;
   size_t nrows = 0;
   if (db1_work_queue_alloc_active(&rows, &nrows) != 0)
      return -1;

   int closed = 0;
   int cancelled = 0;
   for (size_t i = 0; i < nrows; i++)
   {
      char filename[256];
      if (!sync_source_pending_prefix(rows[i].source, base, cwd, filename, sizeof(filename)))
         continue;

      const char *new_status = sync_classify(base, filename);
      if (!new_status)
         continue;

      const char *result = strcmp(new_status, "done") == 0
                               ? "sync-proposals: proposal moved out of pending/"
                               : "sync-proposals: proposal deferred or rejected";
      if (db1_work_queue_force_finish(rows[i].id, new_status, result) > 0)
      {
         work_log(rows[i].id, rows[i].status, new_status, sid, result);
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

int handle_work_gc(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   int max_age_hours = jo_int(req, "max_age_hours", 2);
   if (max_age_hours < 1)
      max_age_hours = 1;

   char cutoff[32];
   time_t threshold = time(NULL) - (max_age_hours * 3600);
   struct tm tm;
   gmtime_r(&threshold, &tm);
   strftime(cutoff, sizeof(cutoff), "%Y-%m-%dT%H:%M:%SZ", &tm);

   db1_work_queue_finish_t *released_rows = NULL;
   size_t released_count = 0;
   if (db1_work_queue_release_stale(cutoff, &released_rows, &released_count) != 0)
      return server_send_error(conn, "failed to release stale claims", NULL);

   int closed = 0;
   int cancelled = 0;
   if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(req, "no_sync_proposals")) &&
       work_sync_proposals_server(req, &closed, &cancelled) != 0)
   {
      free(released_rows);
      return server_send_error(conn, "work sync-proposals failed", NULL);
   }

   cJSON *resp = jo_ok();
   cJSON *arr = cJSON_AddArrayToObject(resp, "released_items");
   for (size_t i = 0; i < released_count; i++)
   {
      work_log(released_rows[i].id, "claimed", "pending", work_session(req),
               "gc: stale claim released");
      cJSON *item = cJSON_CreateObject();
      jo_add_str(item, "id", released_rows[i].id);
      jo_add_str(item, "title", released_rows[i].title);
      cJSON_AddItemToArray(arr, item);
   }
   cJSON_AddNumberToObject(resp, "released", (double)released_count);
   cJSON_AddNumberToObject(resp, "closed", closed);
   cJSON_AddNumberToObject(resp, "cancelled", cancelled);
   cJSON_AddNumberToObject(resp, "max_age_hours", max_age_hours);
   free(released_rows);
   return work_send(conn, resp);
}

int handle_work_sync_proposals(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;

   int closed = 0;
   int cancelled = 0;
   if (work_sync_proposals_server(req, &closed, &cancelled) != 0)
      return server_send_error(conn, "work sync-proposals failed", NULL);

   cJSON *resp = jo_ok();
   cJSON_AddNumberToObject(resp, "closed", closed);
   cJSON_AddNumberToObject(resp, "cancelled", cancelled);
   return work_send(conn, resp);
}

int handle_work_stats(server_ctx_t *ctx, server_conn_t *conn, cJSON *req)
{
   (void)ctx;
   (void)req;

   int pending = 0;
   int claimed = 0;
   int done = 0;
   int failed = 0;
   int cancelled = 0;
   int completed = 0;
   double avg_minutes = 0.0;
   if (db1_work_queue_stats(&pending, &claimed, &done, &failed, &cancelled, &completed,
                            &avg_minutes) != 0)
      return server_send_error(conn, "failed to fetch work queue stats", NULL);

   cJSON *resp = jo_ok();
   cJSON_AddNumberToObject(resp, "total", pending + claimed + done + failed + cancelled);
   cJSON_AddNumberToObject(resp, "pending", pending);
   cJSON_AddNumberToObject(resp, "claimed", claimed);
   cJSON_AddNumberToObject(resp, "done", done);
   cJSON_AddNumberToObject(resp, "failed", failed);
   cJSON_AddNumberToObject(resp, "cancelled", cancelled);
   cJSON_AddNumberToObject(resp, "completed_with_timing", completed);
   if (completed > 0)
      cJSON_AddNumberToObject(resp, "avg_completion_minutes", avg_minutes);
   return work_send(conn, resp);
}
