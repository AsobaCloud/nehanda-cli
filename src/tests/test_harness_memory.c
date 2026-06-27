/* Unit tests for the DB1 harness_memory store + content-hash primitive (P1). */

#include "db1/db1.h"
#include "db1/harness_memory.h"
#include "harness_memory_common.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_sha(void)
{
   /* FIPS 180-4 known-answer vectors, incl. length-mod-64 boundaries. */
   char h[65];
   hmem_sha256_hex("", 0, h);
   assert(strcmp(h, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);
   hmem_sha256_hex("abc", 3, h);
   assert(strcmp(h, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
   char a[120];
   memset(a, 'a', sizeof(a));
   hmem_sha256_hex(a, 55, h); /* rem==55 (one pad block) */
   assert(strcmp(h, "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318") == 0);
   hmem_sha256_hex(a, 56, h); /* rem==56 (forces a second pad block) */
   assert(strcmp(h, "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a") == 0);
   hmem_sha256_hex(a, 64, h); /* exact block boundary */
   assert(strcmp(h, "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb") == 0);
   hmem_sha256_hex(a, 119, h); /* multi-block */
   assert(strcmp(h, "31eba51c313a5c08226adf18d4a359cfdfd8d2e816b13f4af952f7ea6584dcfb") == 0);
}

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

static void test_project_key_ok(void)
{
   /* path-shaped keys (the common case) and opaque ids are accepted */
   assert(hmem_project_key_ok("/home/u/dev/aimee") == 1);
   assert(hmem_project_key_ok("proj-xyz") == 1);
   /* empty / NULL rejected */
   assert(hmem_project_key_ok(NULL) == 0);
   assert(hmem_project_key_ok("") == 0);
   /* control chars / newlines rejected (would corrupt audit/JSON lines) */
   assert(hmem_project_key_ok("a\nb") == 0);
   assert(hmem_project_key_ok("a\tb") == 0);
   assert(hmem_project_key_ok("a\x7f"
                              "b") == 0);
   /* length: 255 ok, 256 rejected (store buffer is char[256]) */
   char big[300];
   memset(big, 'x', sizeof(big));
   big[255] = '\0'; /* 255 chars */
   assert(hmem_project_key_ok(big) == 1);
   big[255] = 'x';
   big[256] = '\0'; /* 256 chars */
   assert(hmem_project_key_ok(big) == 0);
}

int main(void)
{
   test_sha();
   test_hash();
   test_resolve();
   test_project_key_ok();
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

   /* prefix match is wildcard-free: '_' in dir must not over-match a sibling */
   hmem_row_t w1 = mkrow("proj", "a_b/foo", "fact", "1");
   hmem_row_t w2 = mkrow("proj", "axb/foo", "fact", "2");
   assert(hmem_upsert(&w1, NULL) == 0);
   assert(hmem_upsert(&w2, NULL) == 0);
   assert(hmem_tombstone_prefix("proj", "a_b") == 1);
   assert(hmem_get("proj", "axb/foo", &got) == 0); /* sibling untouched */
   hmem_row_free_fields(&got);
   assert(hmem_get("proj", "a_b/foo", &got) == -1); /* tombstoned */

   db1_shutdown();
   printf("test_harness_memory: OK\n");
   return 0;
}
