/* Unit tests for the DB1 harness_memory store + content-hash primitive (P1). */

#include "db1/db1.h"
#include "db1/harness_memory.h"
#include "harness_memory_common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_hash(void)
{
   char a[65], b[65], c[65];
   assert(hmem_content_hash("fact", "n", "desc", "body", "{}", a) == 0);
   assert(strlen(a) == 64);

   /* NULL description == "" description */
   assert(hmem_content_hash("fact", "n", NULL, "body", "{}", b) == 0);
   assert(hmem_content_hash("fact", "n", "", "body", "{}", c) == 0);
   assert(strcmp(b, c) == 0);

   /* meta canonicalization: key order irrelevant; null/empty values omitted */
   char m1[65], m2[65];
   assert(hmem_content_hash("fact", "n", "d", "b", "{\"a\":1,\"b\":2}", m1) == 0);
   assert(hmem_content_hash("fact", "n", "d", "b", "{\"b\":2,\"a\":1,\"z\":null,\"e\":\"\"}", m2) ==
          0);
   assert(strcmp(m1, m2) == 0);

   /* different body => different hash */
   char x[65];
   assert(hmem_content_hash("fact", "n", "d", "other", "{}", x) == 0);
   assert(strcmp(m1, x) != 0);
}

static hmem_row_t mkrow(const char *proj, const char *name, const char *type, const char *body)
{
   hmem_row_t r;
   memset(&r, 0, sizeof(r));
   snprintf(r.project, sizeof(r.project), "%s", proj);
   snprintf(r.name, sizeof(r.name), "%s", name);
   snprintf(r.type, sizeof(r.type), "%s", type);
   r.body = (char *)body; /* borrowed; upsert only reads */
   return r;
}

static void test_resolve(void)
{
   char id[256], root[1024];
   /* $AIMEE_PROJECT_ID (opaque, non-path) becomes the id; root stays a real path */
   setenv("AIMEE_PROJECT_ID", "proj-xyz", 1);
   assert(hmem_resolve_project(".", id, sizeof(id), root, sizeof(root)) == 0);
   assert(strcmp(id, "proj-xyz") == 0);
   assert(root[0] == '/');
   unsetenv("AIMEE_PROJECT_ID");
   /* no env: id == resolved root */
   char id2[256], root2[1024];
   assert(hmem_resolve_project(".", id2, sizeof(id2), root2, sizeof(root2)) == 0);
   assert(strcmp(id2, root2) == 0);
}

int main(void)
{
   test_hash();
   test_resolve();
   assert(db1_init(":memory:") == 0);

   /* upsert + get (nested name round-trips) */
   hmem_row_t in = mkrow("proj", "topics/auth", "fact", "alpha");
   int64_t id = 0;
   assert(hmem_upsert(&in, &id) == 0);
   assert(id > 0);
   hmem_row_t got;
   assert(hmem_get("proj", "topics/auth", &got) == 0);
   assert(strcmp(got.body, "alpha") == 0);
   assert(strcmp(got.type, "fact") == 0);
   assert(got.deleted_at[0] == '\0');
   char ts1[32];
   snprintf(ts1, sizeof(ts1), "%s", got.updated_at);
   hmem_row_free_fields(&got);

   /* same content => no-op (updated_at unchanged) */
   assert(hmem_upsert(&in, NULL) == 0);
   assert(hmem_get("proj", "topics/auth", &got) == 0);
   assert(strcmp(got.updated_at, ts1) == 0);
   hmem_row_free_fields(&got);

   /* changed content => updates */
   hmem_row_t in2 = mkrow("proj", "topics/auth", "fact", "beta");
   assert(hmem_upsert(&in2, NULL) == 0);
   assert(hmem_get("proj", "topics/auth", &got) == 0);
   assert(strcmp(got.body, "beta") == 0);
   hmem_row_free_fields(&got);

   /* invalid type rejected */
   hmem_row_t bad = mkrow("proj", "x", "bogus", "b");
   assert(hmem_upsert(&bad, NULL) == -1);

   /* second row + list (live only) */
   hmem_row_t in3 = mkrow("proj", "notes/x", "note", "gamma");
   assert(hmem_upsert(&in3, NULL) == 0);
   hmem_row_t *rows = NULL;
   int n = 0;
   assert(hmem_list("proj", &rows, &n, 0) == 0);
   assert(n == 2);
   hmem_rows_free(rows, n);

   /* tombstone hides from get/list; include_deleted reveals */
   assert(hmem_tombstone("proj", "notes/x") == 0);
   assert(hmem_get("proj", "notes/x", &got) == -1);
   assert(hmem_list("proj", &rows, &n, 0) == 0);
   assert(n == 1);
   hmem_rows_free(rows, n);
   assert(hmem_list("proj", &rows, &n, 1) == 0);
   assert(n == 2);
   hmem_rows_free(rows, n);

   /* resurrection: upsert onto a tombstoned row clears deleted_at */
   hmem_row_t in4 = mkrow("proj", "notes/x", "note", "gamma2");
   assert(hmem_upsert(&in4, NULL) == 0);
   assert(hmem_get("proj", "notes/x", &got) == 0);
   assert(got.deleted_at[0] == '\0');
   assert(strcmp(got.body, "gamma2") == 0);
   hmem_row_free_fields(&got);

   /* search */
   assert(hmem_search("proj", "beta", &rows, &n) == 0);
   assert(n == 1);
   assert(strcmp(rows[0].name, "topics/auth") == 0);
   hmem_rows_free(rows, n);

   /* bulk prefix tombstone: both topics rows gone, notes/x stays */
   hmem_row_t in5 = mkrow("proj", "topics/sub", "fact", "s");
   assert(hmem_upsert(&in5, NULL) == 0);
   assert(hmem_tombstone_prefix("proj", "topics") == 2);
   assert(hmem_get("proj", "topics/auth", &got) == -1);
   assert(hmem_get("proj", "notes/x", &got) == 0);
   hmem_row_free_fields(&got);

   db1_shutdown();
   printf("test_harness_memory: OK\n");
   return 0;
}
