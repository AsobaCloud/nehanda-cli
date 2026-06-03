/* db2/memory_scope_query.c: read-side scope-tag probes against memory_scopes
 * and the legacy memory_workspaces table. Postgres via libpq. */

#include "../headers/aimee.h" /* memory_t */
#include "memory_query.h"
#include "db2_internal.h"
#include "db_postgres.h"

#include <stddef.h>
#include <stdio.h>

#define MSQ_ERRBUF 256

int db2_memory_scope_matches(int64_t memory_id, const char *scope_type, const char *scope_value)
{
   if (!scope_type || !scope_type[0] || !scope_value || !scope_value[0])
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;
   static const char *sql =
       "SELECT 1 FROM memory_scopes WHERE memory_id = ?1 AND scope_type = ?2 AND scope_value = ?3"
       " LIMIT 1";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int64(st, "?1", memory_id);
   aimee_pg_bind_text(st, "?2", scope_type);
   aimee_pg_bind_text(st, "?3", scope_value);
   int hit = (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW) ? 1 : 0;
   aimee_pg_finalize(st);
   return hit;
}

int db2_memory_workspace_matches(int64_t memory_id, const char *workspace)
{
   if (!workspace || !workspace[0])
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;
   static const char *sql =
       "SELECT 1 FROM memory_workspaces WHERE memory_id = ?1 AND workspace = ?2 LIMIT 1";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int64(st, "?1", memory_id);
   aimee_pg_bind_text(st, "?2", workspace);
   int hit = (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW) ? 1 : 0;
   aimee_pg_finalize(st);
   return hit;
}

int db2_memory_has_scope_type(int64_t memory_id, const char *scope_type)
{
   if (memory_id <= 0 || !scope_type || !scope_type[0])
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;
   static const char *sql =
       "SELECT 1 FROM memory_scopes WHERE memory_id = ?1 AND scope_type = ?2 LIMIT 1";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int64(st, "?1", memory_id);
   aimee_pg_bind_text(st, "?2", scope_type);
   int hit = (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW) ? 1 : 0;
   aimee_pg_finalize(st);
   return hit;
}

int db2_memory_has_any_workspace_tag(int64_t memory_id)
{
   if (memory_id <= 0)
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;
   static const char *sql = "SELECT 1 FROM memory_workspaces WHERE memory_id = ?1 LIMIT 1";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int64(st, "?1", memory_id);
   int hit = (aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW) ? 1 : 0;
   aimee_pg_finalize(st);
   return hit;
}

int db2_memory_scopes_list(int64_t memory_id, db2_memory_scope_tag_row_t *out, int max)
{
   if (memory_id <= 0 || !out || max <= 0)
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;
   static const char *sql = "SELECT scope_type, scope_value FROM memory_scopes WHERE memory_id = ?1"
                            " ORDER BY CASE scope_type"
                            "            WHEN 'project' THEN 0"
                            "            WHEN 'workspace' THEN 1"
                            "            WHEN 'global' THEN 2"
                            "            ELSE 3 END, scope_value ASC";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int64(st, "?1", memory_id);
   int n = 0;
   while (n < max && aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      const char *t = aimee_pg_column_text(st, 0);
      const char *v = aimee_pg_column_text(st, 1);
      snprintf(out[n].type, sizeof(out[n].type), "%s", t ? t : "");
      snprintf(out[n].value, sizeof(out[n].value), "%s", v ? v : "");
      n++;
   }
   aimee_pg_finalize(st);
   return n;
}

void db2_memory_scope_tag_insert(int64_t memory_id, const char *scope_type, const char *scope_value)
{
   if (memory_id <= 0 || !scope_type || !scope_type[0] || !scope_value || !scope_value[0])
      return;
   void *conn = db2_conn();
   if (!conn)
      return;
   static const char *sql = "INSERT INTO memory_scopes (memory_id, scope_type, scope_value)"
                            " VALUES (?1, ?2, ?3) ON CONFLICT DO NOTHING";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return;
   aimee_pg_bind_int64(st, "?1", memory_id);
   aimee_pg_bind_text(st, "?2", scope_type);
   aimee_pg_bind_text(st, "?3", scope_value);
   (void)aimee_pg_step(st, err, sizeof(err));
   aimee_pg_finalize(st);
}

void db2_memory_workspace_tag_insert(int64_t memory_id, const char *workspace)
{
   if (memory_id <= 0 || !workspace || !workspace[0])
      return;
   void *conn = db2_conn();
   if (!conn)
      return;
   static const char *sql = "INSERT INTO memory_workspaces (memory_id, workspace)"
                            " VALUES (?1, ?2) ON CONFLICT DO NOTHING";
   char err[MSQ_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return;
   aimee_pg_bind_int64(st, "?1", memory_id);
   aimee_pg_bind_text(st, "?2", workspace);
   (void)aimee_pg_step(st, err, sizeof(err));
   aimee_pg_finalize(st);
}
