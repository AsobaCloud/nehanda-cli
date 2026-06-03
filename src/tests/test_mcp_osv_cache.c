/* test_mcp_osv_cache.c: DB1 cache/audit coverage for MCP OSV gate. */
#include "db1.h"
#include "mcp_osv_cache.h"

#include <assert.h>
#include <sqlite3.h>
#include <stdio.h>
#include <string.h>

extern sqlite3 *db1_conn(void);

static int scalar_count(const char *sql)
{
   sqlite3_stmt *st = NULL;
   assert(sqlite3_prepare_v2(db1_conn(), sql, -1, &st, NULL) == SQLITE_OK);
   int count = 0;
   if (sqlite3_step(st) == SQLITE_ROW)
      count = sqlite3_column_int(st, 0);
   sqlite3_finalize(st);
   return count;
}

static void test_cache_ttl(void)
{
   assert(db1_mcp_osv_cache_upsert("npm", "pkg", "", "clean", "") == 0);
   db1_mcp_osv_cache_row_t row;
   assert(db1_mcp_osv_cache_get("npm", "pkg", "", 24, &row) == 1);
   assert(strcmp(row.verdict, "clean") == 0);

   assert(sqlite3_exec(db1_conn(),
                       "UPDATE mcp_osv_cache SET checked_at = strftime('%s','now') - 90000"
                       " WHERE ecosystem = 'npm' AND name = 'pkg'",
                       NULL, NULL, NULL) == SQLITE_OK);
   assert(db1_mcp_osv_cache_get("npm", "pkg", "", 24, &row) == 0);
   assert(db1_mcp_osv_cache_get("npm", "pkg", "", 0, &row) == 1);
}

static void test_list_and_audit(void)
{
   assert(db1_mcp_osv_cache_upsert("PyPI", "server", "1.0.0", "malware", "MAL-2026-1") == 0);
   db1_mcp_osv_cache_row_t rows[4];
   int n = db1_mcp_osv_cache_list(rows, 4);
   assert(n >= 2);

   assert(db1_mcp_osv_audit("client", "PyPI", "server", "1.0.0", "malware", "block",
                            "MAL-2026-1") == 0);
   assert(scalar_count("SELECT COUNT(*) FROM interaction_events"
                       " WHERE event_type = 'mcp_package_check' AND outcome = 'blocked'") == 1);
}

int main(void)
{
   printf("mcp_osv_cache: ");
   assert(db1_init(":memory:") == 0);
   test_cache_ttl();
   test_list_and_audit();
   db1_shutdown();
   printf("all tests passed\n");
   return 0;
}
