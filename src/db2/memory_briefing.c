/* db2/memory_briefing.c: SELECT primitives feeding memory_briefing —
 * Postgres via libpq. */

#include "memory_briefing.h"
#include "db2_internal.h"
#include "db_postgres.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MB_ERRBUF 256

static void br_copy(char *dst, size_t cap, const char *src)
{
   snprintf(dst, cap, "%s", src ? src : "");
}

int db2_memory_briefing_list_key_facts(db2_memory_briefing_fact_t *out, int max)
{
   if (!out || max <= 0)
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;

   /* Restrict to user-facing tiers (L2+) and kinds that survive curation.
    * L0/L1 are ephemeral scratch — noise in a briefing. */
   static const char *sql =
       "SELECT id, tier, kind, key, content, confidence, evidence_strength,"
       "       observation_count, use_count, COALESCE(last_used_at, updated_at)"
       "  FROM memories"
       " WHERE tier IN ('L2','L3','L4','L5')"
       "   AND kind != 'scratch'"
       "   AND (sensitivity IS NULL OR sensitivity != 'secret')"
       /* Final tiebreak by id keeps ordering stable across runs on the same
        * fixture even if two rows end up with identical scores. */
       " ORDER BY (confidence + evidence_strength) DESC,"
       "          observation_count DESC, use_count DESC, id DESC"
       " LIMIT ?1";
   char err[MB_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int(st, "?1", max);

   int n = 0;
   while (n < max && aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      memset(&out[n], 0, sizeof(out[n]));
      out[n].memory_id = aimee_pg_column_int64(st, 0);
      br_copy(out[n].tier, sizeof(out[n].tier), aimee_pg_column_text(st, 1));
      br_copy(out[n].kind, sizeof(out[n].kind), aimee_pg_column_text(st, 2));
      br_copy(out[n].key, sizeof(out[n].key), aimee_pg_column_text(st, 3));
      br_copy(out[n].text, sizeof(out[n].text), aimee_pg_column_text(st, 4));
      out[n].confidence = aimee_pg_column_double(st, 5);
      out[n].evidence_strength = aimee_pg_column_double(st, 6);
      out[n].observation_count = aimee_pg_column_int(st, 7);
      out[n].use_count = aimee_pg_column_int(st, 8);
      br_copy(out[n].last_seen_at, sizeof(out[n].last_seen_at), aimee_pg_column_text(st, 9));
      n++;
   }
   aimee_pg_finalize(st);
   return n;
}

int db2_memory_briefing_list_recent_activity(db2_memory_briefing_activity_t *out, int max)
{
   if (!out || max <= 0)
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;

   /* For each recent distinct source_session, take the most recent episode
    * card. The outer WHERE pins to the (source_session, created_at) pair
    * that the inner GROUP BY declared as the latest for that session. */
   static const char *sql = "SELECT e.source_session, e.episode_text, e.reference_time,"
                            "       e.created_at"
                            "  FROM memory_episodes e"
                            "  JOIN (SELECT source_session, MAX(created_at) AS latest"
                            "          FROM memory_episodes"
                            "         WHERE source_session <> ''"
                            "         GROUP BY source_session) s"
                            "    ON e.source_session = s.source_session"
                            "   AND e.created_at = s.latest"
                            " ORDER BY e.created_at DESC, e.source_session DESC"
                            " LIMIT ?1";
   char err[MB_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int(st, "?1", max);

   int n = 0;
   while (n < max && aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      memset(&out[n], 0, sizeof(out[n]));
      br_copy(out[n].session_id, sizeof(out[n].session_id), aimee_pg_column_text(st, 0));
      br_copy(out[n].summary, sizeof(out[n].summary), aimee_pg_column_text(st, 1));
      br_copy(out[n].reference_time, sizeof(out[n].reference_time), aimee_pg_column_text(st, 2));
      br_copy(out[n].created_at, sizeof(out[n].created_at), aimee_pg_column_text(st, 3));
      n++;
   }
   aimee_pg_finalize(st);
   return n;
}

int db2_memory_briefing_list_active_entities(db2_memory_briefing_entity_t *out, int max)
{
   if (!out || max <= 0)
      return 0;
   void *conn = db2_conn();
   if (!conn)
      return 0;

   /* Rank entities by mention count over memories that have been touched in
    * roughly the last month (or that have no last_used_at stamp yet, so fresh
    * writes still count). Deterministic tiebreak by entity name. */
   static const char *sql = "SELECT me.entity, COUNT(*) AS mentions,"
                            "       MAX(COALESCE(m.last_used_at, m.updated_at)) AS last_seen"
                            "  FROM memory_entities me"
                            "  JOIN memories m ON m.id = me.memory_id"
                            " WHERE me.entity <> ''"
                            "   AND (m.last_used_at IS NULL"
                            "        OR m.last_used_at >= pg_now_text('-30 days'))"
                            " GROUP BY me.entity"
                            " ORDER BY mentions DESC, last_seen DESC, me.entity ASC"
                            " LIMIT ?1";
   char err[MB_ERRBUF] = "";
   aimee_pg_stmt_t *st = aimee_pg_prepare(conn, sql, err, sizeof(err));
   if (!st)
      return 0;
   aimee_pg_bind_int(st, "?1", max);

   int n = 0;
   while (n < max && aimee_pg_step(st, err, sizeof(err)) == AIMEE_PG_ROW)
   {
      memset(&out[n], 0, sizeof(out[n]));
      br_copy(out[n].name, sizeof(out[n].name), aimee_pg_column_text(st, 0));
      out[n].mentions = aimee_pg_column_int(st, 1);
      br_copy(out[n].last_seen, sizeof(out[n].last_seen), aimee_pg_column_text(st, 2));
      n++;
   }
   aimee_pg_finalize(st);
   return n;
}
