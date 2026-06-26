/* test_kb_graph.c: unit tests for the code-graph P1 build orchestrator and the
 * derived edge-provenance tag. DB2 is stubbed so the idempotency control flow
 * (skip-when-unchanged vs publish-a-new-generation) is driven deterministically
 * without a live Postgres. */
#include "kb_service_graph.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---- controllable DB2 stubs ---- */
static char g_fp[64] = "";      /* fingerprint the project "currently" hashes to */
static char g_visible[64] = ""; /* fingerprint stored on the visible generation  */
static int g_synced = 0;        /* db2_code_projection_sync_project call count    */
static int g_published = 0;     /* db2_code_projection_generation_publish count   */
static int g_aborted = 0;

int db2_is_initialized(void)
{
   return 1;
}
int db2_code_projection_project_fingerprint(const char *project, char *out, size_t out_len)
{
   (void)project;
   snprintf(out, out_len, "%s", g_fp);
   return g_fp[0] ? 0 : -1; /* empty g_fp simulates a fingerprint error */
}
int db2_code_projection_visible_source_hash(const char *project, char *out, size_t out_len)
{
   (void)project;
   snprintf(out, out_len, "%s", g_visible);
   return 0;
}
int64_t db2_code_projection_generation_create(const char *project)
{
   (void)project;
   return 42;
}
int db2_code_projection_generation_set_source_hash(int64_t gen, const char *h)
{
   (void)gen;
   (void)h;
   return 0;
}
int64_t db2_code_projection_sync_project(const char *project, int64_t gen)
{
   (void)project;
   (void)gen;
   g_synced++;
   return 7; /* pretend 7 edges */
}
int db2_code_projection_generation_publish(int64_t gen, const char *project)
{
   (void)gen;
   (void)project;
   g_published++;
   return 0;
}
int db2_code_projection_generation_abort(int64_t gen, const char *err)
{
   (void)gen;
   (void)err;
   g_aborted++;
   return 0;
}

/* ---- link-only stubs for the rest of kb_service_graph.o (unused here) ---- */
struct cJSON;
struct cJSON *jo_ok(void)
{
   return 0;
}
int db2_entity_edge_explain_by_entity(const char *e, void *out, int n)
{
   (void)e;
   (void)out;
   (void)n;
   return 0;
}
int db2_entity_node_get(const char *e, void *out)
{
   (void)e;
   (void)out;
   return -1;
}
struct cJSON *db2_kb_service_code_audit_json(const char *p, int n)
{
   (void)p;
   (void)n;
   return 0;
}
int kb_send_error(int fd, const char *m)
{
   (void)fd;
   (void)m;
   return 0;
}
int kb_send_response(int fd, struct cJSON *r)
{
   (void)fd;
   (void)r;
   return 0;
}

static void test_provenance(void)
{
   /* deterministic AST/index edge */
   assert(strcmp(kb_graph_edge_provenance("code_projection", 0), "structural") == 0);
   /* any positive structural-trust weight is structural regardless of origin */
   assert(strcmp(kb_graph_edge_provenance("session", 3), "structural") == 0);
   /* raw session co-occurrence, no structural grounding -> ambiguous */
   assert(strcmp(kb_graph_edge_provenance("session", 0), "ambiguous") == 0);
   /* curator/semantic-derived -> inferred */
   assert(strcmp(kb_graph_edge_provenance("curator", 0), "inferred") == 0);
   assert(strcmp(kb_graph_edge_provenance(NULL, 0), "inferred") == 0);
   printf("  test_provenance: ok\n");
}

static void test_idempotent_build(void)
{
   int rebuilt = -1;
   int64_t r;

   /* (1) Unchanged: fingerprint == the visible generation's hash -> SKIP, no sync. */
   snprintf(g_fp, sizeof(g_fp), "abc");
   snprintf(g_visible, sizeof(g_visible), "abc");
   g_synced = g_published = 0;
   r = kb_graph_build_project_if_changed("p", &rebuilt);
   assert(r == 0 && rebuilt == 0 && g_synced == 0 && g_published == 0);

   /* (2) Changed: fingerprint differs -> rebuild (sync + publish), edges returned. */
   snprintf(g_visible, sizeof(g_visible), "def");
   g_synced = g_published = 0;
   r = kb_graph_build_project_if_changed("p", &rebuilt);
   assert(r == 7 && rebuilt == 1 && g_synced == 1 && g_published == 1);

   /* (3) First build: no visible generation yet -> rebuild. */
   g_visible[0] = '\0';
   g_synced = g_published = 0;
   r = kb_graph_build_project_if_changed("p", &rebuilt);
   assert(r == 7 && rebuilt == 1 && g_synced == 1 && g_published == 1);

   /* (4) Fingerprint error -> -1, no work. */
   g_fp[0] = '\0';
   g_synced = g_published = 0;
   r = kb_graph_build_project_if_changed("p", &rebuilt);
   assert(r == -1 && rebuilt == 0 && g_synced == 0 && g_published == 0);

   printf("  test_idempotent_build: ok\n");
}

int main(void)
{
   printf("test_kb_graph:\n");
   test_provenance();
   test_idempotent_build();
   printf("ALL PASS\n");
   return 0;
}
