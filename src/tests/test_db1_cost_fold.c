/* test_db1_cost_fold.c: tests for the parent<->child cost fold log. */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cost_fold.h"

int db1_init(const char *path);
void db1_shutdown(void);

static char tmp_db_path[256];

static void setup_db(void)
{
   snprintf(tmp_db_path, sizeof(tmp_db_path), "/tmp/test_db1_cf_%d.sqlite", (int)getpid());
   unlink(tmp_db_path);
   char p2[300];
   snprintf(p2, sizeof(p2), "%s-wal", tmp_db_path);
   unlink(p2);
   snprintf(p2, sizeof(p2), "%s-shm", tmp_db_path);
   unlink(p2);
   assert(db1_init(tmp_db_path) == 0);
}

static void teardown_db(void)
{
   db1_shutdown();
   unlink(tmp_db_path);
   char p2[300];
   snprintf(p2, sizeof(p2), "%s-wal", tmp_db_path);
   unlink(p2);
   snprintf(p2, sizeof(p2), "%s-shm", tmp_db_path);
   unlink(p2);
}

static int approx_equal(double a, double b)
{
   return fabs(a - b) < 1e-9;
}

static void test_record_then_total(void)
{
   setup_db();
   assert(db1_cost_fold_record("parent-A", "child-1", 0.42, "model") == 1);
   assert(approx_equal(db1_cost_fold_total("parent-A"), 0.42));
   teardown_db();
   printf("  PASS: test_record_then_total\n");
}

static void test_duplicate_is_noop(void)
{
   setup_db();
   assert(db1_cost_fold_record("parent-A", "child-1", 0.42, "model") == 1);
   /* Second call with same (parent, child) pair returns 0 (no rows
    * changed) and the total stays at 0.42 — re-fold is idempotent. */
   assert(db1_cost_fold_record("parent-A", "child-1", 0.99, "model") == 0);
   assert(approx_equal(db1_cost_fold_total("parent-A"), 0.42));
   teardown_db();
   printf("  PASS: test_duplicate_is_noop\n");
}

static void test_multi_child_sum(void)
{
   setup_db();
   assert(db1_cost_fold_record("parent-A", "child-1", 0.10, "model") == 1);
   assert(db1_cost_fold_record("parent-A", "child-2", 0.20, "model") == 1);
   assert(db1_cost_fold_record("parent-A", "child-3", 0.30, "subagent") == 1);
   assert(approx_equal(db1_cost_fold_total("parent-A"), 0.60));
   teardown_db();
   printf("  PASS: test_multi_child_sum\n");
}

static void test_unknown_parent_zero(void)
{
   setup_db();
   assert(approx_equal(db1_cost_fold_total("does-not-exist"), 0.0));
   teardown_db();
   printf("  PASS: test_unknown_parent_zero\n");
}

static void test_isolation_between_parents(void)
{
   setup_db();
   assert(db1_cost_fold_record("parent-A", "child-shared", 0.40, "model") == 1);
   assert(db1_cost_fold_record("parent-B", "child-shared", 0.70, "model") == 1);
   /* Same child id, different parents — both rows land independently
    * because the UNIQUE constraint is on the pair, not the child alone. */
   assert(approx_equal(db1_cost_fold_total("parent-A"), 0.40));
   assert(approx_equal(db1_cost_fold_total("parent-B"), 0.70));
   teardown_db();
   printf("  PASS: test_isolation_between_parents\n");
}

int main(void)
{
   printf("db1_cost_fold:\n");
   test_record_then_total();
   test_duplicate_is_noop();
   test_multi_child_sum();
   test_unknown_parent_zero();
   test_isolation_between_parents();
   printf("ok\n");
   return 0;
}
