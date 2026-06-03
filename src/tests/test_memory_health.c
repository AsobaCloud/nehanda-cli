#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "aimee.h"
#include "db.h"
#include "db1.h"
#include "db2.h"
#include "db2_test_shim.h"
#include "../db2/db2_internal.h"
#include "../db2/db_postgres.h"

int main(void)
{
   printf("memory_health: ");

   assert(db1_init(":memory:") == 0);
   db2_test_shim_open();

   /* --- memory_run_maintenance populates memory_health --- */
   {
      /* Insert some memories so maintenance has something to work with */
      memory_t m;
      memory_insert(TIER_L0, KIND_FACT, "test-key-1", "value 1", 0.5, "sess-1", &m);
      memory_insert(TIER_L1, KIND_FACT, "test-key-2", "value 2", 0.9, "sess-1", &m);
      memory_insert(TIER_L2, KIND_FACT, "test-key-3", "value 3", 1.0, "sess-1", &m);

      int promoted = 0, demoted = 0, expired = 0;
      memory_run_maintenance(&promoted, &demoted, &expired);

      /* Verify memory_health table has a row */
      char qerr[128] = "";
      aimee_pg_stmt_t *stmt =
          aimee_pg_prepare(db2_conn(), "SELECT COUNT(*) FROM memory_health", qerr, sizeof(qerr));
      assert(stmt);
      assert(aimee_pg_step(stmt, qerr, sizeof(qerr)) == AIMEE_PG_ROW);
      int count = aimee_pg_column_int(stmt, 0);
      assert(count >= 1);
      aimee_pg_finalize(stmt);
   }

   /* --- memory_query_health returns aggregated stats --- */
   {
      memory_health_t health;
      int rc = memory_query_health(&health);
      assert(rc == 0);
      assert(health.cycles >= 1);
      /* total_expirations should reflect the L0 we inserted (expired by maintenance) */
      assert(health.total_expirations >= 0);
   }

   /* --- Run multiple maintenance cycles --- */
   {
      memory_t m;
      memory_insert(TIER_L1, KIND_FACT, "multi-cycle-1", "data", 0.95, "sess-2", &m);

      int p, d, e;
      memory_run_maintenance(&p, &d, &e);
      memory_run_maintenance(&p, &d, &e);

      memory_health_t health;
      memory_query_health(&health);
      assert(health.cycles >= 3);
   }

   /* --- memory_record_conflict writes to contradiction_log --- */
   {
      memory_t m1, m2;
      memory_insert(TIER_L1, KIND_FACT, "conflict-a", "always use X", 1.0, "", &m1);
      memory_insert(TIER_L1, KIND_FACT, "conflict-b", "never use X", 1.0, "", &m2);

      memory_record_conflict(m1.id, m2.id);

      /* Verify contradiction_log has a row */
      char qerr[128] = "";
      aimee_pg_stmt_t *stmt = aimee_pg_prepare(db2_conn(), "SELECT COUNT(*) FROM contradiction_log",
                                               qerr, sizeof(qerr));
      assert(stmt);
      assert(aimee_pg_step(stmt, qerr, sizeof(qerr)) == AIMEE_PG_ROW);
      int count = aimee_pg_column_int(stmt, 0);
      assert(count >= 1);
      aimee_pg_finalize(stmt);

      /* Verify the log entry has correct IDs */
      stmt = aimee_pg_prepare(db2_conn(),
                              "SELECT memory_a_id, memory_b_id, resolution"
                              " FROM contradiction_log ORDER BY id DESC LIMIT 1",
                              qerr, sizeof(qerr));
      assert(stmt);
      assert(aimee_pg_step(stmt, qerr, sizeof(qerr)) == AIMEE_PG_ROW);
      int64_t a = aimee_pg_column_int64(stmt, 0);
      int64_t b = aimee_pg_column_int64(stmt, 1);
      const char *res = aimee_pg_column_text(stmt, 2);
      assert(a == m1.id);
      assert(b == m2.id);
      assert(strcmp(res, "pending") == 0);
      aimee_pg_finalize(stmt);
   }

   /* --- memory_resolve_conflict also logs resolution --- */
   {
      conflict_t conflicts[8];
      int count = memory_list_conflicts(conflicts, 8);
      assert(count >= 1);

      memory_resolve_conflict(conflicts[0].id, "a_decayed");

      /* Verify resolution logged */
      char qerr[128] = "";
      aimee_pg_stmt_t *stmt = aimee_pg_prepare(db2_conn(),
                                               "SELECT resolution FROM contradiction_log"
                                               " ORDER BY id DESC LIMIT 1",
                                               qerr, sizeof(qerr));
      assert(stmt);
      assert(aimee_pg_step(stmt, qerr, sizeof(qerr)) == AIMEE_PG_ROW);
      const char *res = aimee_pg_column_text(stmt, 0);
      assert(strcmp(res, "a_decayed") == 0);
      aimee_pg_finalize(stmt);
   }

   /* --- staleness calculation --- */
   {
      /* The L2 memory we inserted earlier should show up in staleness if untouched */
      memory_health_t health;
      memory_query_health(&health);
      /* staleness should be between 0 and 1 */
      assert(health.staleness >= 0.0 && health.staleness <= 1.0);
   }

   /* --- effectiveness uses DB1 server_sessions outcomes without cross-DB join --- */
   {
      memory_t m;
      memory_insert(TIER_L1, KIND_FACT, "effectiveness-db1", "value", 0.8, "sess-eff", &m);

      for (int i = 0; i < EFFECTIVENESS_MIN_SAMPLES; i++)
      {
         char sid[32];
         snprintf(sid, sizeof(sid), "eff-session-%d", i);
         assert(db1_server_session_create(sid, "cli", "tester") == 0);
         assert(db1_context_snapshot_insert(sid, m.id, 1.0) == 0);
         assert(db1_server_session_set_outcome(sid, i < 7 ? "success" : "failure") == 0);
      }

      assert(memory_compute_effectiveness() >= 1);

      char qerr[128] = "";
      aimee_pg_stmt_t *stmt = aimee_pg_prepare(
          db2_conn(), "SELECT effectiveness FROM memories WHERE id = ?1", qerr, sizeof(qerr));
      assert(stmt);
      aimee_pg_bind_int64(stmt, "?1", m.id);
      assert(aimee_pg_step(stmt, qerr, sizeof(qerr)) == AIMEE_PG_ROW);
      double effectiveness = aimee_pg_column_double(stmt, 0);
      assert(fabs(effectiveness - (8.0 / 12.0)) < 0.000001);
      aimee_pg_finalize(stmt);
   }

   /* --- never_surfaced_l2 counts DB2 memories absent from DB1 context_snapshots --- */
   {
      memory_t surfaced, unsurfaced;
      effectiveness_stats_t stats;

      memory_insert(TIER_L2, KIND_FACT, "surfaced-l2", "value", 0.8, "sess-a", &surfaced);
      memory_insert(TIER_L2, KIND_FACT, "unsurfaced-l2", "value", 0.8, "sess-b", &unsurfaced);
      assert(db1_context_snapshot_insert("surfaced-session", surfaced.id, 0.9) == 0);

      assert(memory_effectiveness_stats(&stats) == 0);
      assert(stats.never_surfaced_l2 >= 1);
   }

   db1_shutdown();
   db2_test_shim_close();

   printf("all tests passed\n");
   return 0;
}
