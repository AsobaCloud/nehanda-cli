#ifndef DEC_DB2_DB_SCHEMA_H
#define DEC_DB2_DB_SCHEMA_H 1

/* DB2 (Postgres) idempotent schema bootstrap. The DB1 (SQLite) half
 * of the split lives in db1/db_schema.h. See
 * docs/proposals/accepted/three-db-split-user-shared-vectors.md. */

#include <stddef.h>

struct sqlite3;

#ifdef __cplusplus
extern "C"
{
#endif

   /* Apply the consolidated Postgres schema to an already-open libpq
    * connection (PGconn *, passed as void * so this header stays
    * libpq-free). Returns 0 on success, -1 on failure (writes to
    * errbuf/errlen). */
   int db_apply_schema_postgres(void *pg_conn, char *errbuf, size_t errlen);

   /* Apply the consolidated SQLite schema for DB2's libpq shim/test
    * compatibility path. Production DB2 remains Postgres-only. */
   int db2_apply_schema_sqlite_shim(struct sqlite3 *db, char *errbuf, size_t errlen);

#ifdef __cplusplus
}
#endif

#endif /* DEC_DB2_DB_SCHEMA_H */
