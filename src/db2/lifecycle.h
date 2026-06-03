/* db2/lifecycle.h: DB2 lifecycle/status API without the DB2 umbrella. */
#ifndef DEC_DB2_LIFECYCLE_H
#define DEC_DB2_LIFECYCLE_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

   int db2_is_initialized(void);
   int db2_init(const char *libpq_url);
   int db2_fork_conn_url(char *out, size_t cap);
   int db2_health_probe(int *schema_ok, int *have_pg_trgm);
   int db2_kb_health_probe(int *kb_tables_ok);

   /* Returns the underlying postgres connection handle (or sqlite shim
    * handle in tests) for callers that need to dispatch SQL through
    * aimee_pg_* primitives directly. Production callers should prefer
    * the typed db2_* domain functions; this is exposed for KB-owned
    * migration tooling that copies arbitrary tables row-by-row. */
   void *db2_conn(void);

   /* Postgres-native diagnostics for `aimee doctor`. Each out parameter
    * is set to -1 on probe failure; returns 0 when the connection is
    * alive. Skipped (returns 0 with all outputs left at -1) under the
    * test shim. */
   int db2_pg_stat_summary(int *active_conns, int *max_conns, int *is_replica,
                           int64_t *replica_lag_bytes);

   void db2_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_LIFECYCLE_H */
