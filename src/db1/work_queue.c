/* db1/work_queue.c: inter-session work queue primitives — sqlite via DB1.
 *
 * Ported from the former db2/work_queue.c (Postgres/libpq). The work
 * queue is server coordination state and now lives in DB1, owned by
 * aimee-server and reached in-process. SQL uses numbered ?N placeholders
 * (bound by index) and RETURNING, both supported by the bundled sqlite. */

#include "work_queue.h"
#include "db1_internal.h"
#include "../headers/aimee.h" /* now_utc */

#include <sqlite3.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void wq_bind_text(sqlite3_stmt *st, int idx, const char *v)
{
   sqlite3_bind_text(st, idx, v ? v : "", -1, SQLITE_TRANSIENT);
}

static void wq_copy(char *dst, size_t cap, sqlite3_stmt *st, int col)
{
   const unsigned char *v = sqlite3_column_text(st, col);
   snprintf(dst, cap, "%s", v ? (const char *)v : "");
}

int db1_work_queue_release_claimed_by(const char *session_id)
{
   if (!session_id || !session_id[0])
      return 0;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql =
       "UPDATE work_queue SET status = 'pending', claimed_by = NULL, claimed_at = NULL,"
       " lane = '' WHERE claimed_by = ?1 AND status = 'claimed'";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, session_id);
   int rc = sqlite3_step(st);
   int changes = sqlite3_changes(db);
   sqlite3_finalize(st);
   return (rc == SQLITE_DONE) ? changes : -1;
}

int db1_work_queue_insert(const char *id, const char *title, const char *description,
                          const char *source, int priority, const char *created_by)
{
   if (!id || !*id || !title || !*title)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql =
       "INSERT INTO work_queue (id, title, description, source, priority, status, created_by,"
       " created_at) VALUES (?1, ?2, ?3, ?4, ?5, 'pending', ?6, ?7)";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, id);
   wq_bind_text(st, 2, title);
   wq_bind_text(st, 3, description);
   wq_bind_text(st, 4, source);
   sqlite3_bind_int(st, 5, priority);
   wq_bind_text(st, 6, created_by);
   wq_bind_text(st, 7, ts);
   int rc = (sqlite3_step(st) == SQLITE_DONE) ? 0 : -1;
   sqlite3_finalize(st);
   return rc;
}

void db1_work_queue_log_transition(const char *item_id, const char *old_status,
                                   const char *new_status, const char *session_id,
                                   const char *detail)
{
   sqlite3 *db = db1_conn();
   if (!db)
      return;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql = "INSERT INTO work_queue_log"
                            " (item_id, old_status, new_status, session_id, detail, created_at)"
                            " VALUES (?1, ?2, ?3, ?4, ?5, ?6)";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return;
   wq_bind_text(st, 1, item_id);
   wq_bind_text(st, 2, old_status);
   wq_bind_text(st, 3, new_status);
   wq_bind_text(st, 4, session_id);
   wq_bind_text(st, 5, detail);
   wq_bind_text(st, 6, ts);
   (void)sqlite3_step(st);
   sqlite3_finalize(st);
}

int db1_work_queue_alloc_log(const char *item_id, db1_work_queue_log_row_t **out, size_t *count)
{
   if (out)
      *out = NULL;
   if (count)
      *count = 0;
   if (!out || !count || !item_id || !*item_id)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql = "SELECT old_status, new_status, session_id, detail, created_at"
                            " FROM work_queue_log WHERE item_id = ?1 ORDER BY id ASC";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, item_id);

   db1_work_queue_log_row_t *rows = NULL;
   size_t row_count = 0;
   size_t row_cap = 0;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      if (row_count == row_cap)
      {
         size_t ncap = row_cap ? row_cap * 2 : 8;
         db1_work_queue_log_row_t *grown =
             (db1_work_queue_log_row_t *)realloc(rows, ncap * sizeof(*grown));
         if (!grown)
         {
            free(rows);
            sqlite3_finalize(st);
            return -1;
         }
         rows = grown;
         row_cap = ncap;
      }
      db1_work_queue_log_row_t *r = &rows[row_count];
      memset(r, 0, sizeof(*r));
      wq_copy(r->old_status, sizeof(r->old_status), st, 0);
      wq_copy(r->new_status, sizeof(r->new_status), st, 1);
      wq_copy(r->session_id, sizeof(r->session_id), st, 2);
      wq_copy(r->detail, sizeof(r->detail), st, 3);
      wq_copy(r->created_at, sizeof(r->created_at), st, 4);
      row_count++;
   }
   sqlite3_finalize(st);
   *out = rows;
   *count = row_count;
   return 0;
}

static const char *wq_lane_value(const char *lane)
{
   return (lane && lane[0]) ? lane : "";
}

static void wq_claim_fill(sqlite3_stmt *st, db1_work_queue_claim_t *out)
{
   memset(out, 0, sizeof(*out));
   wq_copy(out->id, sizeof(out->id), st, 0);
   wq_copy(out->title, sizeof(out->title), st, 1);
   wq_copy(out->description, sizeof(out->description), st, 2);
   wq_copy(out->source, sizeof(out->source), st, 3);
   out->priority = sqlite3_column_int(st, 4);
   wq_copy(out->lane, sizeof(out->lane), st, 5);
}

int db1_work_queue_claim_by_id(const char *id, const char *session_id, const char *lane,
                               db1_work_queue_claim_t *out)
{
   if (!id || !*id || !session_id || !*session_id || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql =
       "UPDATE work_queue SET status = 'claimed', claimed_by = ?1, claimed_at = ?2, lane = ?4"
       " WHERE id = ?3 AND status = 'pending'"
       " AND (?4 = '' OR NOT EXISTS ("
       "  SELECT 1 FROM work_queue WHERE status = 'claimed' AND lane = ?4"
       " ))"
       " RETURNING id, title, description, source, priority, lane";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, session_id);
   wq_bind_text(st, 2, ts);
   wq_bind_text(st, 3, id);
   wq_bind_text(st, 4, wq_lane_value(lane));

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_claim_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

int db1_work_queue_claim_next(const char *session_id, const char *effort_filter,
                              const char *tag_filter, const char *exclude_tag, int skip,
                              const char *lane, db1_work_queue_claim_t *out)
{
   if (!session_id || !*session_id || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   /* Build the inner candidate-id query with optional filters. The
    * outer UPDATE retries the status='pending' check so we can't
    * accidentally claim an item another session just took. */
   char filter_sql[1024];
   int foff = 0;
   foff += snprintf(filter_sql + foff, sizeof(filter_sql) - (size_t)foff,
                    "SELECT id FROM work_queue WHERE status = 'pending'");
   if (effort_filter && *effort_filter)
      foff += snprintf(filter_sql + foff, sizeof(filter_sql) - (size_t)foff, " AND effort = ?3");
   if (tag_filter && *tag_filter)
      foff += snprintf(filter_sql + foff, sizeof(filter_sql) - (size_t)foff,
                       " AND (',' || tags || ',') LIKE '%%,' || ?4 || ',%%'");
   if (exclude_tag && *exclude_tag)
      foff += snprintf(filter_sql + foff, sizeof(filter_sql) - (size_t)foff,
                       " AND (',' || tags || ',') NOT LIKE '%%,' || ?5 || ',%%'");
   snprintf(filter_sql + foff, sizeof(filter_sql) - (size_t)foff,
            " ORDER BY priority DESC, created_at ASC LIMIT 1 OFFSET ?6");

   char full_sql[2048];
   snprintf(full_sql, sizeof(full_sql),
            "UPDATE work_queue SET status = 'claimed', claimed_by = ?1, claimed_at = ?2,"
            " lane = ?7"
            " WHERE id = (%s) AND status = 'pending'"
            " AND (?7 = '' OR NOT EXISTS ("
            "  SELECT 1 FROM work_queue WHERE status = 'claimed' AND lane = ?7"
            " ))"
            " RETURNING id, title, description, source, priority, lane",
            filter_sql);

   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, full_sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, session_id);
   wq_bind_text(st, 2, ts);
   if (effort_filter && *effort_filter)
      wq_bind_text(st, 3, effort_filter);
   if (tag_filter && *tag_filter)
      wq_bind_text(st, 4, tag_filter);
   if (exclude_tag && *exclude_tag)
      wq_bind_text(st, 5, exclude_tag);
   sqlite3_bind_int(st, 6, skip);
   wq_bind_text(st, 7, wq_lane_value(lane));

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_claim_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

static void wq_finish_fill(sqlite3_stmt *st, db1_work_queue_finish_t *out)
{
   memset(out, 0, sizeof(*out));
   wq_copy(out->id, sizeof(out->id), st, 0);
   wq_copy(out->title, sizeof(out->title), st, 1);
}

int db1_work_queue_finish_by_id(const char *id, const char *new_status, const char *result,
                                db1_work_queue_finish_t *out)
{
   if (!id || !*id || !new_status || !*new_status || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql =
       "UPDATE work_queue SET status = ?1, completed_at = ?2, result = ?3, lane = ''"
       " WHERE id = ?4 AND status = 'claimed'"
       " RETURNING id, title";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, new_status);
   wq_bind_text(st, 2, ts);
   wq_bind_text(st, 3, result);
   wq_bind_text(st, 4, id);

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_finish_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

int db1_work_queue_finish_session_recent(const char *session_id, const char *new_status,
                                         const char *result, db1_work_queue_finish_t *out)
{
   if (!session_id || !*session_id || !new_status || !*new_status || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql = "UPDATE work_queue SET status = ?1, completed_at = ?2, result = ?3,"
                            " lane = '' WHERE id = ("
                            "  SELECT id FROM work_queue WHERE claimed_by = ?4"
                            "    AND status = 'claimed' ORDER BY claimed_at DESC LIMIT 1"
                            " ) AND status = 'claimed'"
                            " RETURNING id, title";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, new_status);
   wq_bind_text(st, 2, ts);
   wq_bind_text(st, 3, result);
   wq_bind_text(st, 4, session_id);

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_finish_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

int db1_work_queue_cancel_by_id(const char *id, db1_work_queue_finish_t *out)
{
   if (!id || !*id || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql = "UPDATE work_queue SET status = 'cancelled'"
                            " WHERE id = ?1 AND status = 'pending'"
                            " RETURNING id, title";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, id);

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_finish_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

int db1_work_queue_release_by_id(const char *id, db1_work_queue_finish_t *out)
{
   if (!id || !*id || !out)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql =
       "UPDATE work_queue SET status = 'pending', claimed_by = NULL, claimed_at = NULL,"
       " lane = '' WHERE id = ?1 AND status = 'claimed'"
       " RETURNING id, title";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, id);

   int hit = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      wq_finish_fill(st, out);
      hit = 1;
   }
   sqlite3_finalize(st);
   return hit;
}

int db1_work_queue_alloc_ids_by_status(const char *status, char (**out_ids)[32], size_t *count)
{
   if (out_ids)
      *out_ids = NULL;
   if (count)
      *count = 0;
   if (!out_ids || !count || !status || !*status)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql = "SELECT id FROM work_queue WHERE status = ?1";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, status);

   char(*ids)[32] = NULL;
   size_t row_count = 0;
   size_t row_cap = 0;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      if (row_count == row_cap)
      {
         size_t ncap = row_cap ? row_cap * 2 : 64;
         char(*grown)[32] = realloc(ids, ncap * sizeof(*grown));
         if (!grown)
         {
            free(ids);
            sqlite3_finalize(st);
            return -1;
         }
         ids = grown;
         row_cap = ncap;
      }
      wq_copy(ids[row_count], sizeof(ids[row_count]), st, 0);
      row_count++;
   }
   sqlite3_finalize(st);

   *out_ids = ids;
   *count = row_count;
   return 0;
}

int db1_work_queue_delete_by_status(const char *status)
{
   if (!status || !*status)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql = "DELETE FROM work_queue WHERE status = ?1";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, status);
   int rc = sqlite3_step(st);
   int changes = sqlite3_changes(db);
   sqlite3_finalize(st);
   return (rc == SQLITE_DONE) ? changes : -1;
}

int db1_work_queue_release_stale(const char *cutoff_iso, db1_work_queue_finish_t **out,
                                 size_t *count)
{
   if (out)
      *out = NULL;
   if (count)
      *count = 0;
   if (!out || !count || !cutoff_iso || !*cutoff_iso)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql =
       "UPDATE work_queue SET status = 'pending', claimed_by = NULL, claimed_at = NULL,"
       " lane = '' WHERE status = 'claimed' AND claimed_at < ?1"
       " RETURNING id, title";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, cutoff_iso);

   db1_work_queue_finish_t *rows = NULL;
   size_t row_count = 0;
   size_t row_cap = 0;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      if (row_count == row_cap)
      {
         size_t ncap = row_cap ? row_cap * 2 : 32;
         db1_work_queue_finish_t *grown =
             (db1_work_queue_finish_t *)realloc(rows, ncap * sizeof(*grown));
         if (!grown)
         {
            free(rows);
            sqlite3_finalize(st);
            return -1;
         }
         rows = grown;
         row_cap = ncap;
      }
      wq_finish_fill(st, &rows[row_count]);
      row_count++;
   }
   sqlite3_finalize(st);

   *out = rows;
   *count = row_count;
   return 0;
}

int db1_work_queue_alloc_list(const char *status_filter, db1_work_queue_list_row_t **out,
                              size_t *count)
{
   if (out)
      *out = NULL;
   if (count)
      *count = 0;
   if (!out || !count)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   const char *sql;
   if (status_filter && strcmp(status_filter, "all") == 0)
   {
      sql = "SELECT id, title, source, status, claimed_by, result, created_at, claimed_at"
            " FROM work_queue ORDER BY"
            " CASE status WHEN 'claimed' THEN 0 WHEN 'pending' THEN 1"
            "             WHEN 'done' THEN 2 ELSE 3 END,"
            " priority DESC, created_at ASC";
   }
   else if (status_filter && *status_filter)
   {
      sql = "SELECT id, title, source, status, claimed_by, result, created_at, claimed_at"
            " FROM work_queue WHERE status = ?1"
            " ORDER BY priority DESC, created_at ASC";
   }
   else
   {
      sql = "SELECT id, title, source, status, claimed_by, result, created_at, claimed_at"
            " FROM work_queue WHERE status IN ('pending', 'claimed')"
            " ORDER BY CASE status WHEN 'claimed' THEN 0 ELSE 1 END,"
            " priority DESC, created_at ASC";
   }

   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   if (status_filter && *status_filter && strcmp(status_filter, "all") != 0)
      wq_bind_text(st, 1, status_filter);

   db1_work_queue_list_row_t *rows = NULL;
   size_t row_count = 0;
   size_t row_cap = 0;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      if (row_count == row_cap)
      {
         size_t ncap = row_cap ? row_cap * 2 : 64;
         db1_work_queue_list_row_t *grown =
             (db1_work_queue_list_row_t *)realloc(rows, ncap * sizeof(*grown));
         if (!grown)
         {
            free(rows);
            sqlite3_finalize(st);
            return -1;
         }
         rows = grown;
         row_cap = ncap;
      }
      db1_work_queue_list_row_t *r = &rows[row_count];
      memset(r, 0, sizeof(*r));
      wq_copy(r->id, sizeof(r->id), st, 0);
      wq_copy(r->title, sizeof(r->title), st, 1);
      wq_copy(r->source, sizeof(r->source), st, 2);
      wq_copy(r->status, sizeof(r->status), st, 3);
      wq_copy(r->claimed_by, sizeof(r->claimed_by), st, 4);
      wq_copy(r->result, sizeof(r->result), st, 5);
      wq_copy(r->created_at, sizeof(r->created_at), st, 6);
      wq_copy(r->claimed_at, sizeof(r->claimed_at), st, 7);
      row_count++;
   }
   sqlite3_finalize(st);

   *out = rows;
   *count = row_count;
   return 0;
}

void db1_work_queue_count_by_status(int *pending, int *claimed, int *done, int *failed,
                                    int *cancelled)
{
   if (pending)
      *pending = 0;
   if (claimed)
      *claimed = 0;
   if (done)
      *done = 0;
   if (failed)
      *failed = 0;
   if (cancelled)
      *cancelled = 0;

   sqlite3 *db = db1_conn();
   if (!db)
      return;

   static const char *sql = "SELECT status, COUNT(*) FROM work_queue GROUP BY status";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      const char *s = (const char *)sqlite3_column_text(st, 0);
      int c = sqlite3_column_int(st, 1);
      if (!s)
         continue;
      if (pending && strcmp(s, "pending") == 0)
         *pending = c;
      else if (claimed && strcmp(s, "claimed") == 0)
         *claimed = c;
      else if (done && strcmp(s, "done") == 0)
         *done = c;
      else if (failed && strcmp(s, "failed") == 0)
         *failed = c;
      else if (cancelled && strcmp(s, "cancelled") == 0)
         *cancelled = c;
   }
   sqlite3_finalize(st);
}

void db1_work_queue_completion_stats(int *completed, double *avg_minutes)
{
   if (completed)
      *completed = 0;
   if (avg_minutes)
      *avg_minutes = 0.0;

   sqlite3 *db = db1_conn();
   if (!db)
      return;

   /* now_utc() stamps an ISO-8601 string with a trailing 'Z' that
    * sqlite's julianday() will not parse; strip it before the diff. */
   static const char *sql = "SELECT COUNT(*), AVG("
                            "  (julianday(replace(done_log.created_at,'Z','')) -"
                            "   julianday(replace(claim_log.created_at,'Z',''))) * 24 * 60"
                            ") FROM work_queue_log done_log"
                            " JOIN work_queue_log claim_log"
                            "   ON claim_log.item_id = done_log.item_id"
                            "   AND claim_log.new_status = 'claimed'"
                            " WHERE done_log.new_status = 'done'";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return;
   if (sqlite3_step(st) == SQLITE_ROW)
   {
      if (completed)
         *completed = sqlite3_column_int(st, 0);
      if (avg_minutes && sqlite3_column_type(st, 1) != SQLITE_NULL)
         *avg_minutes = sqlite3_column_double(st, 1);
   }
   sqlite3_finalize(st);
}

int db1_work_queue_stats(int *pending, int *claimed, int *done, int *failed, int *cancelled,
                         int *log_completed, double *avg_minutes)
{
   if (pending)
      *pending = 0;
   if (claimed)
      *claimed = 0;
   if (done)
      *done = 0;
   if (failed)
      *failed = 0;
   if (cancelled)
      *cancelled = 0;
   if (log_completed)
      *log_completed = 0;
   if (avg_minutes)
      *avg_minutes = 0.0;

   if (!db1_conn())
      return -1;
   db1_work_queue_count_by_status(pending, claimed, done, failed, cancelled);
   db1_work_queue_completion_stats(log_completed, avg_minutes);
   return 0;
}

int db1_work_queue_alloc_active(db1_work_queue_active_row_t **out, size_t *count)
{
   if (out)
      *out = NULL;
   if (count)
      *count = 0;
   if (!out || !count)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   static const char *sql =
       "SELECT id, source, status FROM work_queue WHERE status IN ('pending', 'claimed')";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;

   db1_work_queue_active_row_t *rows = NULL;
   size_t row_count = 0;
   size_t row_cap = 0;
   while (sqlite3_step(st) == SQLITE_ROW)
   {
      if (row_count == row_cap)
      {
         size_t ncap = row_cap ? row_cap * 2 : 16;
         db1_work_queue_active_row_t *grown =
             (db1_work_queue_active_row_t *)realloc(rows, ncap * sizeof(*grown));
         if (!grown)
         {
            free(rows);
            sqlite3_finalize(st);
            return -1;
         }
         rows = grown;
         row_cap = ncap;
      }
      db1_work_queue_active_row_t *r = &rows[row_count];
      memset(r, 0, sizeof(*r));
      wq_copy(r->id, sizeof(r->id), st, 0);
      wq_copy(r->source, sizeof(r->source), st, 1);
      wq_copy(r->status, sizeof(r->status), st, 2);
      row_count++;
   }
   sqlite3_finalize(st);

   *out = rows;
   *count = row_count;
   return 0;
}

int db1_work_queue_force_finish(const char *id, const char *new_status, const char *result)
{
   if (!id || !*id || !new_status || !*new_status)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql = "UPDATE work_queue SET status = ?1, completed_at = ?2, result = ?3,"
                            " claimed_by = NULL, claimed_at = NULL, lane = ''"
                            " WHERE id = ?4";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, new_status);
   wq_bind_text(st, 2, ts);
   wq_bind_text(st, 3, result);
   wq_bind_text(st, 4, id);
   int rc = sqlite3_step(st);
   int changes = sqlite3_changes(db);
   sqlite3_finalize(st);
   return (rc == SQLITE_DONE) ? changes : -1;
}

int db1_work_queue_count_active_by_source(const char *source)
{
   if (!source || !*source)
      return 0;
   sqlite3 *db = db1_conn();
   if (!db)
      return 0;

   static const char *sql = "SELECT COUNT(*) FROM work_queue"
                            " WHERE source = ?1 AND status IN ('pending', 'claimed', 'done')";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return 0;
   wq_bind_text(st, 1, source);
   int count = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
      count = sqlite3_column_int(st, 0);
   sqlite3_finalize(st);
   return count;
}

int db1_work_queue_insert_with_effort(const char *id, const char *title, const char *description,
                                      const char *source, int priority, const char *created_by,
                                      const char *effort)
{
   if (!id || !*id || !title || !*title)
      return -1;
   sqlite3 *db = db1_conn();
   if (!db)
      return -1;

   char ts[32];
   now_utc(ts, sizeof(ts));

   static const char *sql =
       "INSERT INTO work_queue (id, title, description, source, priority, status, created_by,"
       " created_at, effort) VALUES (?1, ?2, ?3, ?4, ?5, 'pending', ?6, ?7, ?8)";
   sqlite3_stmt *st = NULL;
   if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
      return -1;
   wq_bind_text(st, 1, id);
   wq_bind_text(st, 2, title);
   wq_bind_text(st, 3, description);
   wq_bind_text(st, 4, source);
   sqlite3_bind_int(st, 5, priority);
   wq_bind_text(st, 6, created_by);
   wq_bind_text(st, 7, ts);
   wq_bind_text(st, 8, effort);
   int rc = (sqlite3_step(st) == SQLITE_DONE) ? 0 : -1;
   sqlite3_finalize(st);
   return rc;
}
