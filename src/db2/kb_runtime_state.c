/* kb_runtime_state.c: DB2-backed runtime state for aimee-kb.
 *
 * Postgres-only: drives the kb_runtime_state table over libpq via the
 * shared db2 connection. Returns -1 / 0 for "no handle" so callers fail
 * soft when DB2 isn't initialised. */

#include "kb_runtime_state.h"

#include "db2_internal.h"
#include "db_postgres.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define KBRS_ERRBUF 256

int db2_kb_runtime_state_set(const char *key, const char *value)
{
   if (!key)
      return -1;
   void *conn = db2_conn();
   if (!conn)
      return -1;

   char err[KBRS_ERRBUF] = "";
   aimee_pg_stmt_t *st =
       aimee_pg_prepare(conn,
                        "INSERT INTO kb_runtime_state (state_key, state_value) VALUES (?1, ?2) "
                        "ON CONFLICT (state_key) DO UPDATE SET state_value = EXCLUDED.state_value",
                        err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_text(st, "?1", key);
   aimee_pg_bind_text(st, "?2", value ? value : "");
   aimee_pg_step_t rc = aimee_pg_step(st, err, sizeof(err));
   aimee_pg_finalize(st);
   return (rc == AIMEE_PG_DONE) ? 0 : -1;
}

int db2_kb_runtime_state_get(const char *key, char *out, size_t out_len)
{
   if (!key || !out || out_len == 0)
      return -1;
   out[0] = '\0';
   void *conn = db2_conn();
   if (!conn)
      return -1;

   char err[KBRS_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(
       conn, "SELECT state_value FROM kb_runtime_state WHERE state_key = ?1", err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_text(st, "?1", key);

   int rc = -1;
   if (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      const char *v = aimee_pg_column_text(st, 0);
      snprintf(out, out_len, "%s", v ? v : "");
      rc = 0;
   }
   aimee_pg_finalize(st);
   return rc;
}

int db2_kb_runtime_state_delete(const char *key)
{
   if (!key)
      return -1;
   void *conn = db2_conn();
   if (!conn)
      return -1;

   char err[KBRS_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, "DELETE FROM kb_runtime_state WHERE state_key = ?1",
                                          err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_text(st, "?1", key);
   aimee_pg_step_t rc = aimee_pg_step(st, err, sizeof(err));
   aimee_pg_finalize(st);
   return (rc == AIMEE_PG_DONE) ? 0 : -1;
}

int db2_kb_runtime_state_set_now(const char *key)
{
   if (!key)
      return -1;
   void *conn = db2_conn();
   if (!conn)
      return -1;

   char err[KBRS_ERRBUF] = "";
   /* pg_now_text() stores the DB2 canonical UTC text format. */
   aimee_pg_stmt_t *st = aimee_pg_prepare(
       conn,
       "INSERT INTO kb_runtime_state (state_key, state_value) VALUES (?1, pg_now_text()) "
       "ON CONFLICT (state_key) DO UPDATE SET state_value = EXCLUDED.state_value",
       err, sizeof(err));
   if (!st)
      return -1;
   aimee_pg_bind_text(st, "?1", key);
   aimee_pg_step_t rc = aimee_pg_step(st, err, sizeof(err));
   aimee_pg_finalize(st);
   return (rc == AIMEE_PG_DONE) ? 0 : -1;
}

int db2_kb_runtime_state_vector_rebuild_lock_held(void)
{
   void *conn = db2_conn();
   if (!conn)
      return 0;

   char err[KBRS_ERRBUF] = "";
   /* 1800s = 30 minutes. state_value is the UTC TEXT timestamp written
    * by pg_now_text(); CURRENT_TIMESTAMP is forced to UTC for the
    * comparison. */
   aimee_pg_stmt_t *st = aimee_pg_prepare(
       conn,
       "SELECT 1 FROM kb_runtime_state"
       " WHERE state_key = 'vector_rebuild_lock'"
       "   AND state_value::timestamp > (CURRENT_TIMESTAMP AT TIME ZONE 'UTC') - interval '1800 "
       "seconds'",
       err, sizeof(err));
   if (!st)
      return 0;
   int held = (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW) ? 1 : 0;
   aimee_pg_finalize(st);
   return held;
}

int db2_kb_runtime_state_vector_rebuild_lock_try_acquire(void)
{
   if (db2_kb_runtime_state_vector_rebuild_lock_held())
      return 0;
   return db2_kb_runtime_state_set_now("vector_rebuild_lock") == 0 ? 1 : 0;
}

void db2_kb_runtime_state_vector_rebuild_lock_release(void)
{
   (void)db2_kb_runtime_state_delete("vector_rebuild_lock");
}
