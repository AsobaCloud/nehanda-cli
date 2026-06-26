/* test_kb_http_routes_code.inc: code-index / scan / ingest / maintenance / job
 * route tests, split out of test_kb_http_routes.c to keep that translation
 * unit under the 2000-line hard limit. Included (with
 * test_kb_http_routes_endpoints.inc) just before main(); all helpers and
 * fixtures live in test_kb_http_routes.c above the include point. */
/* Full memory_t layout (mirrors headers/memory.h) so the stub writes each field
 * at the offset the handler — compiled against the real struct — reads. The main
 * file keeps a truncated local memory_t for other stubs, so we cast a void* here
 * rather than redeclare the type, exactly like canonical_index_code_search. Lives
 * here (with the hybrid test it feeds) to keep test_kb_http_routes.c under the
 * 2000-line build-integrity limit. */
typedef struct
{
   int64_t id;
   char tier[4];
   char kind[16];
   char key[512];
   char headline[512];
   char content[2048];
   char use_cases[1024];
   double confidence;
   int use_count;
   char last_used_at[32];
   char created_at[32];
   char updated_at[32];
   char source_session[128];
   double salience;
   char provenance_category[32];
   double retrieval_score;
   int hybrid_rank;
} test_full_memory_t;

int db2_memory_find_facts_like(const char *query, int limit, void *out, int max)
{
   assert(out);
   if (!query || strcmp(query, "needle") != 0 || limit < 1 || max < 1)
      return 0;
   test_full_memory_t *m = (test_full_memory_t *)out;
   memset(&m[0], 0, sizeof(m[0]));
   m[0].id = 7;
   snprintf(m[0].kind, sizeof(m[0].kind), "decision");
   snprintf(m[0].headline, sizeof(m[0].headline), "why needle exists");
   snprintf(m[0].content, sizeof(m[0].content), "chose needle over haystack for O(1) lookup");
   return 1;
}

/* canonical_index_find_callers stub (used by the callers + hybrid route tests
 * below) — moved here from test_kb_http_routes.c to keep that file under the
 * 2000-line build-integrity limit; cast a void* like the other canonical stubs. */
typedef struct
{
   char project[128];
   char file_path[MAX_PATH_LEN];
   char caller[128];
   int line;
} test_caller_hit_t;

int canonical_index_find_callers(const char *project, const char *symbol, void *out, int max)
{
   assert(symbol);
   assert(out);
   if (strcmp(symbol, "target_fn") != 0 || !project || strcmp(project, "proj-alpha") != 0)
      return 0;
   if (max < 1)
      return 0;
   test_caller_hit_t *hits = (test_caller_hit_t *)out;
   snprintf(hits[0].project, sizeof(hits[0].project), "proj-alpha");
   snprintf(hits[0].file_path, sizeof(hits[0].file_path), "src/caller.c");
   snprintf(hits[0].caller, sizeof(hits[0].caller), "caller_fn");
   hits[0].line = 44;
   return 1;
}

/* Vector-leg stubs (§5): the query embedder + pgvec code search. OFF by default
 * (g_vec_enabled=0 -> memory_embed_text returns 0 -> the leg is skipped), so the
 * existing hybrid tests are unaffected; test_code_hybrid_vector_ok flips it on. */
static int g_vec_enabled = 0;
int memory_embed_text(const char *text, const char *command, float *out, int max_dim)
{
   (void)text;
   (void)command;
   if (!g_vec_enabled || !out || max_dim <= 0)
      return 0;
   int d = 2560; /* the stub embedder's FIXED output dim (independent of the corpus
                  * dim) so the route's qdim==db2_embedding_dim() gate can mismatch. */
   if (d > max_dim)
      return 0;
   for (int i = 0; i < d; i++)
      out[i] = 0.01f * (float)(i % 7);
   return d;
}
int pgvec_code_search_paths(const char *project, const float *vec, int dim, int limit, char *paths,
                            int path_cap, double *scores, int max)
{
   (void)project;
   (void)vec;
   (void)dim;
   (void)limit;
   if (!g_vec_enabled || !paths || path_cap <= 0 || !scores || max < 2)
      return 0;
   /* Two hits: src/search.c OVERLAPS the lexical leg (-> 2-signal consensus) and
    * src/semantic.c is vector-only (a file the lexical+graph legs would miss). */
   snprintf(paths + 0 * path_cap, (size_t)path_cap, "src/search.c");
   scores[0] = 0.91;
   snprintf(paths + 1 * path_cap, (size_t)path_cap, "src/semantic.c");
   scores[1] = 0.88;
   return 2;
}

/* §4 surprising-links candidate gather. Returns two high-cosine pairs for
 * proj-alpha: (file:x,file:y) are ABSENT from the projection edges (hub/a/b/c) so
 * they're disconnected -> surprising; (hub,a) is 1 hop -> filtered by d_min. */
int pgvec_code_similar_pairs(const char *project, int k, double min_cosine, int anchor_cap,
                             char *a_keys, char *b_keys, int key_cap, double *cosines, int max)
{
   (void)k;
   (void)min_cosine;
   (void)anchor_cap;
   if (!a_keys || !b_keys || key_cap <= 0 || !cosines || max < 2 || !project)
      return 0;
   if (strcmp(project, "proj-vecdown") == 0)
      return -1; /* simulate a vector-store outage (no connection / query error) */
   if (strcmp(project, "proj-alpha") == 0)
   {
      snprintf(a_keys + 0 * key_cap, (size_t)key_cap, "file:x");
      snprintf(b_keys + 0 * key_cap, (size_t)key_cap, "file:y");
      cosines[0] = 0.95;
      snprintf(a_keys + 1 * key_cap, (size_t)key_cap, "hub");
      snprintf(b_keys + 1 * key_cap, (size_t)key_cap, "a");
      cosines[1] = 0.90;
      return 2;
   }
   if (strcmp(project, "proj-hub") == 0)
   {
      /* The two files are similar; their only projection edges are to the project
       * hub, so the route must EXCLUDE those and report them disconnected. */
      snprintf(a_keys + 0 * key_cap, (size_t)key_cap, "file:f1");
      snprintf(b_keys + 0 * key_cap, (size_t)key_cap, "file:f2");
      cosines[0] = 0.97;
      return 1;
   }
   return 0;
}

/* Mirror of code_projection_edge_t (db2/code_projection.h) so the stub writes
 * fields at the offsets the handler reads; cast a void* rather than include the
 * db2 header (same pattern as the canonical_index stubs). */
typedef struct
{
   char source[512];
   char relation[64];
   char target[512];
   int structural_weight;
} test_projection_edge_t;

typedef struct
{
   const char *s, *r, *t;
   int w;
} test_seed_edge_t;

int db2_code_projection_list_edges(const char *project, void *out, int max)
{
   assert(out);
   if (max < 4)
      return 0;
   test_projection_edge_t *e = (test_projection_edge_t *)out;
   /* hub: out=2 (->a,->b), in=1 (c->); a,b,c: degree 1 each. */
   static const test_seed_edge_t alpha[] = {
       {"hub", "calls", "a", 1}, {"hub", "calls", "b", 1}, {"c", "calls", "hub", 1}};
   /* A single recursive edge: exercises the source==target==node self-loop path. */
   static const test_seed_edge_t selfloop[] = {{"rec", "calls", "rec", 1}};
   /* Two files linked ONLY through the project containment hub: the surprising
    * route must drop these `project:` edges before computing hop distance. */
   static const test_seed_edge_t hubonly[] = {{"project:proj-hub", "contains", "file:f1", 1},
                                              {"project:proj-hub", "contains", "file:f2", 1}};
   const test_seed_edge_t *rows = NULL;
   int n = 0;
   if (project && strcmp(project, "proj-alpha") == 0)
   {
      rows = alpha;
      n = (int)(sizeof(alpha) / sizeof(alpha[0]));
   }
   else if (project && strcmp(project, "proj-selfloop") == 0)
   {
      rows = selfloop;
      n = (int)(sizeof(selfloop) / sizeof(selfloop[0]));
   }
   else if (project && strcmp(project, "proj-hub") == 0)
   {
      rows = hubonly;
      n = (int)(sizeof(hubonly) / sizeof(hubonly[0]));
   }
   else
      return 0;
   for (int i = 0; i < n && i < max; i++)
   {
      memset(&e[i], 0, sizeof(e[i]));
      snprintf(e[i].source, sizeof(e[i].source), "%s", rows[i].s);
      snprintf(e[i].relation, sizeof(e[i].relation), "%s", rows[i].r);
      snprintf(e[i].target, sizeof(e[i].target), "%s", rows[i].t);
      e[i].structural_weight = rows[i].w;
   }
   return n;
}

/* Hermetic mirror of the §3 provenance helper (kb_service_graph.c). The real
 * definition lives in a db2-heavy unit; this fake keeps the route test pure
 * while preserving the only branch the projection route exercises: a
 * code_projection edge is always "structural". */
const char *kb_graph_edge_provenance(const char *edge_origin, int structural_weight)
{
   if (structural_weight > 0 || (edge_origin && strcmp(edge_origin, "code_projection") == 0))
      return "structural";
   if (edge_origin && strcmp(edge_origin, "session") == 0)
      return "ambiguous";
   return "inferred";
}

static void test_code_structure_ok(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/structure",
                            "project=proj-alpha&file_path=src/main.c&max_results=4", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"definitions\"") != NULL);
   assert(strstr(buf, "\"name\":\"main\"") != NULL);
   assert(strstr(buf, "\"kind\":\"function\"") != NULL);
   assert(strstr(buf, "\"line\":12") != NULL);
}

static void test_code_search_missing_query(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/search", "", NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing query") != NULL);
}

static void test_code_search_ok(void)
{
   char buf[512];
   int s =
       kb_http_route_ex("GET", "/v1/code/search", "query=needle&project=proj-alpha&max_results=4",
                        NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"hits\"") != NULL);
   assert(strstr(buf, "\"file_path\":\"src/search.c\"") != NULL);
   assert(strstr(buf, "\"snippet\":\"int needle") != NULL);
   assert(strstr(buf, "\"rank\":0.75") != NULL);
   /* P2 Layer-1: the file content hash is surfaced for citation/drift. */
   assert(strstr(buf, "\"content_hash\":\"deadbeefcafe\"") != NULL);
   assert(strstr(buf, "\"next_cursor\":null") != NULL);
}

static void test_code_callers_missing_symbol(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/callers", "", NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing symbol") != NULL);
}

static void test_code_callers_ok(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/callers",
                            "symbol=target_fn&project=proj-alpha&max_results=4", NULL, NULL, NULL,
                            0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"hits\"") != NULL);
   assert(strstr(buf, "\"file_path\":\"src/caller.c\"") != NULL);
   assert(strstr(buf, "\"caller\":\"caller_fn\"") != NULL);
   assert(strstr(buf, "\"line\":44") != NULL);
   assert(strstr(buf, "\"next_cursor\":null") != NULL);
}

/* §5 hybrid retrieval: fuse lexical-code + graph-callers (RRF) + memory "why". */
static void test_code_hybrid_ok(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/hybrid",
                            "query=needle&symbol=target_fn&project=proj-alpha&max_results=10", NULL,
                            NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"results\"") != NULL);
   /* Both signals contribute a file, each labeled + enriched from its source. */
   assert(strstr(buf, "\"file_path\":\"src/search.c\"") != NULL); /* lexical-code leg */
   assert(strstr(buf, "\"signals\":[\"code\"]") != NULL);
   assert(strstr(buf, "\"snippet\":\"int needle") != NULL);
   assert(strstr(buf, "\"file_path\":\"src/caller.c\"") != NULL); /* graph-callers leg */
   assert(strstr(buf, "\"signals\":[\"graph\"]") != NULL);
   assert(strstr(buf, "\"caller\":\"caller_fn\"") != NULL);
   /* Memory recall surfaces as typed "why" context, not a fused row. */
   assert(strstr(buf, "\"why\"") != NULL);
   assert(strstr(buf, "why needle exists") != NULL);
}

static void test_code_hybrid_missing_query(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/hybrid", "symbol=target_fn", NULL, NULL, NULL, 0, buf,
                            sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing query") != NULL);
}

/* No symbol => graph leg empty; lexical-code + memory still fuse/return. */
static void test_code_hybrid_no_symbol(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/hybrid", "query=needle&project=proj-alpha", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"file_path\":\"src/search.c\"") != NULL);
   assert(strstr(buf, "\"file_path\":\"src/caller.c\"") == NULL); /* no graph leg */
   assert(strstr(buf, "why needle exists") != NULL);
}

/* §5 vector leg: with a dim-matched embedder it fuses as a 3rd signal — a
 * vector-only file surfaces (labeled "vector" + score) and a file that BOTH the
 * lexical and vector legs return fuses to a 2-signal consensus row. */
static void test_code_hybrid_vector_ok(void)
{
   g_test_embedding_dim = 2560; /* matches the embed stub's returned dim */
   g_vec_enabled = 1;
   char buf[4096];
   int s = kb_http_route_ex("GET", "/v1/code/hybrid",
                            "query=needle&symbol=target_fn&project=proj-alpha&max_results=10", NULL,
                            NULL, NULL, 0, buf, sizeof(buf));
   g_vec_enabled = 0;
   g_test_embedding_dim = 1024;
   assert(s == 200);
   /* The vector-only file appears, labeled and carrying its vector score. */
   assert(strstr(buf, "\"file_path\":\"src/semantic.c\"") != NULL);
   assert(strstr(buf, "\"vector\"") != NULL);
   assert(strstr(buf, "\"vector_score\"") != NULL);
   /* src/search.c was returned by BOTH the lexical and vector legs -> consensus. */
   assert(strstr(buf, "\"signals\":[\"code\",\"vector\"]") != NULL);
}

/* The dim gate: a wrong-dim embedder (output != corpus dim) disables the leg, so
 * the vector-only file never appears and the route degrades to code+graph. */
static void test_code_hybrid_vector_dim_mismatch_skips(void)
{
   g_test_embedding_dim = 1024; /* corpus dim != the stub's 2560 output */
   g_vec_enabled = 1;
   char buf[4096];
   int s = kb_http_route_ex("GET", "/v1/code/hybrid", "query=needle&project=proj-alpha", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   g_vec_enabled = 0;
   assert(s == 200);
   assert(strstr(buf, "\"file_path\":\"src/search.c\"") != NULL);    /* lexical still works */
   assert(strstr(buf, "\"file_path\":\"src/semantic.c\"") == NULL); /* vector leg skipped */
}

static void test_code_project_stats_missing_project(void)
{
   char buf[256];
   int s =
       kb_http_route_ex("GET", "/v1/code/project-stats", "", NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing project") != NULL);
}

/* §4 graph analytics: hub/degree-centrality ranking over the projection graph. */
static void test_code_graph_hubs_ok(void)
{
   char buf[1024];
   int s = kb_http_route_ex("GET", "/v1/code/graph/hubs", "project=proj-alpha&max_results=10", NULL,
                            NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"edge_count\":3") != NULL);
   assert(strstr(buf, "\"truncated\":false") != NULL);
   /* hub is the top node: degree 3 (out 2, in 1). */
   assert(strstr(buf, "\"node\":\"hub\"") != NULL);
   assert(strstr(buf, "\"degree\":3") != NULL);
   assert(strstr(buf, "\"out_degree\":2") != NULL);
   assert(strstr(buf, "\"in_degree\":1") != NULL);
   /* hub must appear before the leaf nodes in the ranked array. */
   const char *hub = strstr(buf, "\"node\":\"hub\"");
   const char *a = strstr(buf, "\"node\":\"a\"");
   assert(hub && a && hub < a);
}

static void test_code_graph_hubs_missing_project(void)
{
   char buf[256];
   int s =
       kb_http_route_ex("GET", "/v1/code/graph/hubs", "", NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing project") != NULL);
}

/* §4 judge stub: confirm the first link (the disconnected file:x/file:y pair) with a
 * canned verdict so the route test stays hermetic (no curator-LLM link). Mirrors the
 * real kb_surprising_judge writing verdicts parallel to the links. */
int kb_surprising_judge(const config_t *cfg, const char *judge_cmd, const char *project,
                        const kb_graph_surprising_t *links, int n, kb_surprising_verdict_t *out,
                        char *errbuf, size_t errlen)
{
   (void)cfg;
   (void)judge_cmd;
   (void)project;
   (void)links;
   (void)errbuf;
   (void)errlen;
   if (!out || n <= 0)
      return -1;
   for (int i = 0; i < n; i++)
      memset(&out[i], 0, sizeof(out[i]));
   out[0].judged = 1;
   out[0].confirmed = 1;
   out[0].shared_symbols = 2;
   snprintf(out[0].reason, sizeof(out[0].reason), "parallel auth implementations");
   return 1;
}

/* §8 read-only node-neighborhood projection. Seed edges: hub->a, hub->b, c->hub. */
static void test_code_graph_node_ok(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/graph", "project=proj-alpha&node=hub&max_results=10",
                            NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"node\":\"hub\"") != NULL);
   /* hub --calls--> a, b (out); c --calls--> hub (in). */
   assert(strstr(buf, "\"neighbor\":\"a\"") != NULL);
   assert(strstr(buf, "\"neighbor\":\"b\"") != NULL);
   assert(strstr(buf, "\"neighbor\":\"c\"") != NULL);
   assert(strstr(buf, "\"direction\":\"out\"") != NULL);
   assert(strstr(buf, "\"direction\":\"in\"") != NULL);
   assert(strstr(buf, "\"relation\":\"calls\"") != NULL);
   /* projection edges are AST-derived -> §3 provenance "structural". */
   assert(strstr(buf, "\"provenance\":\"structural\"") != NULL);
   assert(strstr(buf, "\"neighbor_count\":3") != NULL);
   /* all 3 incident edges fit under max_results=10 -> complete neighborhood. */
   assert(strstr(buf, "\"match_count\":3") != NULL);
   assert(strstr(buf, "\"truncated\":false") != NULL);
}

/* The page cap (max_results) must drive `truncated` independently of the DB scan
 * window: 3 incident edges, max_results=1 -> 1 emitted, truncated=true. */
static void test_code_graph_node_capped_truncates(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/graph", "project=proj-alpha&node=hub&max_results=1",
                            NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"neighbor_count\":1") != NULL);
   assert(strstr(buf, "\"match_count\":3") != NULL);
   assert(strstr(buf, "\"truncated\":true") != NULL);
}

/* A recursive edge (rec->rec) is emitted once with direction "self". */
static void test_code_graph_node_self_loop(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/graph", "project=proj-selfloop&node=rec", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"neighbor\":\"rec\"") != NULL);
   assert(strstr(buf, "\"direction\":\"self\"") != NULL);
   assert(strstr(buf, "\"neighbor_count\":1") != NULL);
   /* exactly one entry: the else-if ladder must not double-emit a self-loop. */
   assert(strstr(buf, "\"direction\":\"out\"") == NULL);
   assert(strstr(buf, "\"direction\":\"in\"") == NULL);
}

/* §4 surprising links: high-cosine + graph-distant pairs. With d_min=2 the (hub,a)
 * 1-hop pair is filtered and only the disconnected (file:x,file:y) pair surfaces. */
static void test_code_graph_surprising_ok(void)
{
   char buf[4096];
   int s = kb_http_route_ex("GET", "/v1/code/graph/surprising",
                            "project=proj-alpha&percentile=0&d_min=2&max_results=10", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"candidate_count\":2") != NULL); /* both pairs gathered */
   /* the disconnected pair is surprising; the 1-hop (hub,a) pair is not. */
   assert(strstr(buf, "\"a\":\"file:x\"") != NULL);
   assert(strstr(buf, "\"b\":\"file:y\"") != NULL);
   assert(strstr(buf, "\"disconnected\":true") != NULL);
   assert(strstr(buf, "\"link_count\":1") != NULL);
   /* hub/a are graph-adjacent (1 hop < d_min=2) -> excluded from the links. */
   assert(strstr(buf, "\"a\":\"hub\"") == NULL);
}

/* Project-hub exclusion: two files whose ONLY edges are to the project containment
 * node. With those dropped the coupling graph is empty, so the pair is disconnected
 * and surfaces even at d_min=3 (without exclusion they'd be 2 hops via the hub and
 * be filtered). edge_count==0 proves the hub edges were excluded. */
static void test_code_graph_surprising_hub_excluded(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/code/graph/surprising",
                            "project=proj-hub&percentile=0&d_min=3&max_results=10", NULL, NULL, NULL,
                            0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"edge_count\":0") != NULL); /* both edges were project-incident */
   assert(strstr(buf, "\"a\":\"file:f1\"") != NULL);
   assert(strstr(buf, "\"disconnected\":true") != NULL);
   assert(strstr(buf, "\"link_count\":1") != NULL);
}

static void test_code_graph_surprising_missing_project(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/graph/surprising", "", NULL, NULL, NULL, 0, buf,
                            sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing project") != NULL);
}

/* judge=true runs the §4 confirmation: the stubbed judge confirms the first link, so
 * the route annotates it with confirmed/reason/shared_symbols + a top-level judged. */
static void test_code_graph_surprising_judge(void)
{
   char buf[4096];
   int s = kb_http_route_ex("GET", "/v1/code/graph/surprising",
                            "project=proj-alpha&percentile=0&d_min=2&judge=true&max_results=10",
                            NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"confirmed\":true") != NULL);
   assert(strstr(buf, "\"shared_symbols\":2") != NULL);
   assert(strstr(buf, "parallel auth implementations") != NULL);
   assert(strstr(buf, "\"judged\":1") != NULL);
}

/* A vector-store failure (pgvec gather returns <0) is surfaced as 503, not a silent
 * empty 200 — an outage must be distinguishable from "no surprising links". */
static void test_code_graph_surprising_vecstore_down(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/graph/surprising", "project=proj-vecdown", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 503);
   assert(strstr(buf, "vector store unavailable") != NULL);
}

static void test_code_graph_node_missing_params(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/graph", "project=proj-alpha", NULL, NULL, NULL, 0, buf,
                            sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing node") != NULL);
   s = kb_http_route_ex("GET", "/v1/code/graph", "node=hub", NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing project") != NULL);
}

static void test_code_project_stats_ok(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/project-stats", "project=proj-alpha", NULL, NULL, NULL,
                            0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"project\":\"proj-alpha\"") != NULL);
   assert(strstr(buf, "\"files\":9") != NULL);
   assert(strstr(buf, "\"definitions\":4") != NULL);
   assert(strstr(buf, "\"langs\"") != NULL);
   assert(strstr(buf, "\"lang\":\"c\"") != NULL);
   assert(strstr(buf, "\"count\":7") != NULL);
}

static void test_code_project_stats_error_is_json(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/code/project-stats", "project=proj-missing", NULL, NULL,
                            NULL, 0, buf, sizeof(buf));
   assert(s == 503);
   cJSON *json = cJSON_Parse(buf);
   assert(json);
   cJSON *error = cJSON_GetObjectItemCaseSensitive(json, "error");
   assert(cJSON_IsString(error));
   assert(strstr(error->valuestring, "canonical index unavailable") != NULL);
   cJSON_Delete(json);
}

static void test_blast_radius_missing_params(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/blast-radius", NULL, NULL, NULL, NULL, 0, buf,
                            sizeof(buf));
   assert(s == 400);
}

static void test_blast_radius_not_found(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/code/blast-radius", "project=myproj&file_path=src/foo.c",
                            NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 404);
}

static void test_blast_radius_ok(void)
{
   char buf[512];
   int s =
       kb_http_route_ex("GET", "/v1/code/blast-radius", "project=proj-alpha&file_path=src/main.c",
                        NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"file\":\"src/main.c\"") != NULL);
   assert(strstr(buf, "\"dependents\"") != NULL);
   assert(strstr(buf, "\"src/app.c\"") != NULL);
   assert(strstr(buf, "\"dependent_count\":1") != NULL);
   assert(strstr(buf, "\"dependencies\"") != NULL);
   assert(strstr(buf, "\"src/lib.c\"") != NULL);
   assert(strstr(buf, "\"dependency_count\":1") != NULL);
}

static void test_code_scan_ok(void)
{
   char buf[512];
   g_db_initialized = 1;
   g_code_scan_rc = 5;
   g_code_scan_inspected = 12;
   g_code_scan_force = 0;
   g_curator_code_queued = 0;
   const char *body = "{\"project\":\"proj-alpha\",\"root_path\":\"/tmp/repo\",\"force\":true}";
   int s = kb_http_route_ex("POST", "/v1/code/scan", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(strstr(buf, "\"files\":5") != NULL);
   assert(strstr(buf, "\"inspected\":12") != NULL);
   assert(strcmp(g_code_scan_project, "proj-alpha") == 0);
   assert(strcmp(g_code_scan_root_path, "/tmp/repo") == 0);
   assert(g_code_scan_force == 1);
   assert(g_curator_code_queued == 1);
}

static void test_code_scan_missing_root_path(void)
{
   char buf[256];
   const char *body = "{\"project\":\"proj-alpha\"}";
   int s = kb_http_route_ex("POST", "/v1/code/scan", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing root_path") != NULL);
}

static void test_code_scan_pushed_files_ok(void)
{
   char buf[512];
   g_db_initialized = 1;
   g_code_scan_files_rc = 2;
   g_code_scan_files_inspected = 2;
   g_code_scan_files_force = 0;
   g_code_scan_files_count = 0;
   g_code_scan_file_project[0] = '\0';
   g_code_scan_file_root[0] = '\0';
   g_code_scan_file_rel[0] = '\0';
   g_code_scan_file_content[0] = '\0';
   g_curator_code_queued = 0;
   const char *body =
       "{\"project\":\"proj-alpha\",\"root_path\":\"remote-root\",\"force\":true,"
       "\"files\":[{\"rel_path\":\"src/a.c\",\"content\":\"int pushed(void) { return 1; }\"},"
       "{\"rel_path\":\"src/b.c\",\"content\":\"int other(void) { return 2; }\"}]}";
   int s = kb_http_route_ex("POST", "/v1/code/scan", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"files\":2") != NULL);
   assert(strstr(buf, "\"inspected\":2") != NULL);
   assert(strcmp(g_code_scan_file_project, "proj-alpha") == 0);
   assert(strcmp(g_code_scan_file_root, "remote-root") == 0);
   assert(g_code_scan_files_count == 2);
   assert(strcmp(g_code_scan_file_rel, "src/a.c") == 0);
   assert(strstr(g_code_scan_file_content, "pushed") != NULL);
   assert(g_code_scan_files_force == 1);
   /* The push path now queues code units for the curator just like a local scan
    * (the queue reads from DB2 by project name and self-gates on the curator
    * flag), so with the stubbed config_load reporting the gate on, it enqueues. */
   assert(g_curator_code_queued == 1);
}

static void test_code_scan_pushed_files_rejects_invalid_item(void)
{
   char buf[256];
   g_db_initialized = 1;
   const char *body = "{\"project\":\"proj-alpha\",\"files\":[{\"rel_path\":\"src/a.c\"}]}";
   int s = kb_http_route_ex("POST", "/v1/code/scan", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "invalid files array") != NULL);
}

static void test_code_scan_db_unavailable(void)
{
   char buf[256];
   g_db_initialized = 0;
   const char *body = "{\"project\":\"proj-alpha\",\"root_path\":\"/tmp/repo\"}";
   int s = kb_http_route_ex("POST", "/v1/code/scan", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 503);
   assert(strstr(buf, "knowledge service store") != NULL);
   g_db_initialized = 1;
}

static void test_code_build_ok(void)
{
   char buf[1024];
   g_db_initialized = 1;
   g_pgvec_ensure_rc = 0;
   g_kb_build_rc = 0;
   g_code_scan_rc = 0;
   g_runtime_state_set_now = 0;
   const char *body =
       "{\"path\":\"/tmp/kb\",\"project\":\"proj-alpha\",\"embedding_command\":\"embed-a\","
       "\"force\":true}";
   int s = kb_http_route_ex("POST", "/v1/code/build", NULL, NULL, NULL, body, (int)strlen(body),
                            buf, sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_kb_build_path, "/tmp/kb") == 0);
   /* /v1/code/build now also runs the canonical scan (matches the async ingest path). */
   assert(strcmp(g_code_scan_project, "proj-alpha") == 0);
   assert(strcmp(g_kb_build_project, "proj-alpha") == 0);
   assert(strcmp(g_kb_build_embed_cmd, "embed-a") == 0);
   assert(g_kb_build_force == 1);
   assert(g_runtime_state_set_now == 1);
   assert(strstr(buf, "\"files_scanned\":9") != NULL);
   assert(strstr(buf, "\"embeddings_added\":5") != NULL);
}

static void test_code_update_ok(void)
{
   char buf[1024];
   g_db_initialized = 1;
   g_kb_update_rc = 0;
   g_code_scan_rc = 0;
   g_runtime_state_set_now = 0;
   g_curator_docs_queued = 0;
   const char *body =
       "{\"path\":\"/tmp/kb\",\"project\":\"proj-alpha\",\"embedding_command\":\"embed-a\"}";
   int s = kb_http_route_ex("POST", "/v1/code/update", NULL, NULL, NULL, body, (int)strlen(body),
                            buf, sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_kb_update_path, "/tmp/kb") == 0);
   assert(strcmp(g_kb_update_project, "proj-alpha") == 0);
   assert(strcmp(g_kb_update_embed_cmd, "embed-a") == 0);
   assert(g_runtime_state_set_now == 1);
   assert(g_curator_docs_queued == 1);
   assert(strstr(buf, "\"files_indexed\":6") != NULL);
   assert(strstr(buf, "\"embeddings_added\":4") != NULL);
}

static void test_ingest_enqueue_ok(void)
{
   char buf[1024];
   g_db_initialized = 1;
   g_discover_count = 1;
   g_worker_notify_count = 0;
   const char *body = "{\"workspace\":\"/workspace\",\"force\":true}";
   int s = kb_http_route_ex("POST", "/v1/ingest", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 202);
   assert(strcmp(g_ingest_project, "proj-alpha") == 0);
   assert(strcmp(g_ingest_root, "/workspace/proj-alpha") == 0);
   assert(strcmp(g_ingest_workspace, "/workspace") == 0);
   assert(g_ingest_force == 1);
   assert(g_worker_notify_count == 1);
   assert(strstr(buf, "\"projects_queued\":1") != NULL);
}

static void test_pipeline_status_ok(void)
{
   char buf[512];
   g_queue_status = (db2_kb_service_async_queue_stats_t){
       .pending = 4,
       .running = 2,
       .done = 7,
       .failed = 1,
       .total = 14,
   };
   int s =
       kb_http_route_ex("GET", "/v1/pipeline/status", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "queue_depth") != NULL);
   assert(strstr(buf, "\"state\":\"running\"") != NULL);
   assert(strstr(buf, "\"queue_depth\":6") != NULL);
   assert(strstr(buf, "\"running\":2") != NULL);
}

static void test_ingest_status_ok(void)
{
   char buf[512];
   int s =
       kb_http_route_ex("GET", "/v1/ingest/status", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"done_last_24h\":9") != NULL);
   assert(strstr(buf, "\"active\":1") != NULL);
}

static void test_ingest_status_wrong_method(void)
{
   char buf[256];
   int s =
       kb_http_route_ex("POST", "/v1/ingest/status", NULL, NULL, NULL, "{}", 2, buf, sizeof(buf));
   assert(s == 405);
}

static void test_workers_ok(void)
{
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/workers", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"configured\":2") != NULL);
   assert(strstr(buf, "\"method\":\"v1.http\"") != NULL);
   assert(strstr(buf, "\"maintenance-timer\"") != NULL);
}

static void test_workers_wrong_method(void)
{
   char buf[256];
   int s = kb_http_route_ex("POST", "/v1/workers", NULL, NULL, NULL, "{}", 2, buf, sizeof(buf));
   assert(s == 405);
}

static void test_pipeline_status_failed(void)
{
   char buf[512];
   g_queue_status = (db2_kb_service_async_queue_stats_t){
       .failed = 2,
       .total = 2,
   };
   int s =
       kb_http_route_ex("GET", "/v1/pipeline/status", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"state\":\"failed\"") != NULL);
   assert(strstr(buf, "\"queue_depth\":0") != NULL);
}

static void test_drain_ok(void)
{
   char buf[512];
   const char *body = "{\"embedding_command\":\"test-embed\",\"timeout\":9}";
   g_drain_rc = 0;
   int s = kb_http_route_ex("POST", "/v1/drain", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_drain_embed_cmd, "test-embed") == 0);
   assert(g_drain_timeout == 9);
   assert(strcmp(g_drain_collection, "kb-test") == 0);
   assert(strstr(buf, "\"processed\":3") != NULL);
   assert(strstr(buf, "\"pending\":1") != NULL);
}

static void test_drain_default_embedding_command(void)
{
   char buf[512];
   g_drain_embed_cmd[0] = '\0';
   g_drain_rc = 0;
   int s = kb_http_route_ex("POST", "/v1/drain", NULL, NULL, NULL, "{}", 2, buf, sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_drain_embed_cmd, "builtin") == 0);
}

static void test_drain_error(void)
{
   char buf[512];
   g_drain_rc = -1;
   int s = kb_http_route_ex("POST", "/v1/drain", NULL, NULL, NULL, "{}", 2, buf, sizeof(buf));
   assert(s == 500);
   assert(strstr(buf, "queue drain failed") != NULL);
   g_drain_rc = 0;
}

static void test_maintenance_repair_ok(void)
{
   char buf[1024];
   g_db_initialized = 1;
   g_pgvec_ensure_rc = 0;
   g_kb_build_rc = 0;
   const char *body =
       "{\"path\":\"/tmp/kb\",\"project\":\"proj-alpha\",\"embedding_command\":\"embed-a\"}";
   int s = kb_http_route_ex("POST", "/v1/maintenance/repair", NULL, NULL, NULL, body,
                            (int)strlen(body), buf, sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_kb_build_path, "/tmp/kb") == 0);
   assert(strcmp(g_kb_build_project, "proj-alpha") == 0);
   assert(strcmp(g_kb_build_embed_cmd, "embed-a") == 0);
   assert(g_kb_build_force == 1);
   assert(strstr(buf, "\"files_scanned\":9") != NULL);
   assert(strstr(buf, "\"embeddings_added\":5") != NULL);
}

static void test_maintenance_repair_missing_project(void)
{
   char buf[256];
   int s = kb_http_route_ex("POST", "/v1/maintenance/repair", NULL, NULL, NULL,
                            "{\"path\":\"/tmp/kb\"}", 18, buf, sizeof(buf));
   assert(s == 400);
   assert(strstr(buf, "missing project") != NULL);
}

static void test_maintenance_reconcile_ok(void)
{
   char buf[512];
   g_reconcile_rc = 0;
   int s = kb_http_route_ex("POST", "/v1/maintenance/reconcile", NULL, NULL, NULL,
                            "{\"dry_run\":true}", 16, buf, sizeof(buf));
   assert(s == 200);
   assert(g_reconcile_dry_run == 1);
   assert(strstr(buf, "\"dry_run\":true") != NULL);
   assert(strstr(buf, "\"pruned\":1") != NULL);
   assert(strstr(buf, "\"kept\":6") != NULL);
}

static void test_maintenance_clear_ok(void)
{
   char buf[512];
   g_clear_deleted = 12;
   int s = kb_http_route_ex("POST", "/v1/maintenance/clear", NULL, NULL, NULL,
                            "{\"project\":\"proj-alpha\"}", 24, buf, sizeof(buf));
   assert(s == 200);
   assert(strcmp(g_clear_project, "proj-alpha") == 0);
   assert(strstr(buf, "\"chunks_deleted\":12") != NULL);
}

static void test_maintenance_clear_error(void)
{
   char buf[256];
   g_clear_deleted = -1;
   int s = kb_http_route_ex("POST", "/v1/maintenance/clear", NULL, NULL, NULL,
                            "{\"project\":\"proj-alpha\"}", 24, buf, sizeof(buf));
   assert(s == 500);
   assert(strstr(buf, "kb clear failed") != NULL);
   g_clear_deleted = 12;
}

static void test_job_not_found(void)
{
   char buf[256];
   g_job_get_rc = 0;
   int s = kb_http_route_ex("GET", "/v1/jobs/job-1", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 404);
   assert(strstr(buf, "not found") != NULL);
   s = kb_http_route_ex("GET", "/v1/jobs/999", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 404);
   assert(g_job_get_id == 999);
   assert(strstr(buf, "not found") != NULL);
   g_job_get_rc = 1;
}

static void test_job_status_ok(void)
{
   char buf[1024];
   int s = kb_http_route_ex("GET", "/v1/jobs/42", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(g_job_get_id == 42);
   assert(strstr(buf, "\"id\":42") != NULL);
   assert(strstr(buf, "\"document_id\":77") != NULL);
   assert(strstr(buf, "\"project\":\"proj-alpha\"") != NULL);
   assert(strstr(buf, "\"status\":\"running\"") != NULL);
   assert(strstr(buf, "\"last_error\":\"retry \\\"later\\\"\"") != NULL);
}

static void test_job_status_error(void)
{
   char buf[256];
   g_job_get_rc = -1;
   int s = kb_http_route_ex("GET", "/v1/jobs/42", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 503);
   assert(strstr(buf, "job status unavailable") != NULL);
   g_job_get_rc = 1;
}
static void test_entity_profile_not_found(void)
{
   char buf[256];
   int s =
       kb_http_route_ex("GET", "/v1/entities/nobody", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 404);
}

static void test_entity_search_ok(void)
{
   char buf[512];
   int s = kb_http_route_ex("POST", "/v1/entities/search", NULL, NULL, NULL,
                            "{\"query\":\"alice\"}", 16, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"entities\"") != NULL);
}

static void test_phase5_auth_rejected(void)
{
   char buf[256];
   int s = kb_http_route_ex("POST", "/v1/search", NULL, NULL, "secret", "{\"query\":\"foo\"}", 15,
                            buf, sizeof(buf));
   assert(s == 401);
}

static void test_phase1_fallthrough(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/health", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"ok\"") != NULL);
}

/* ── Bearer-token scope isolation ────────────────────────────────────────── */

static void test_scope_token_cross_scope_denied(void)
{
   /* A token scoped project:alpha is configured. A request targeting
    * workspace:beta must be rejected with 403; a request in its own scope
    * (project:alpha) passes. */
   const char *tok = "scope:project:alpha:s3cr3t";
   const char *auth = "Bearer scope:project:alpha:s3cr3t";
   char buf[1024];

   /* cross-scope (workspace=beta) → 403 */
   int s = kb_http_route_ex("GET", "/v1/search", "query=x&workspace=beta", auth, tok, NULL, 0, buf,
                            sizeof(buf));
   assert(s == 403);
   assert(strstr(buf, "forbidden") != NULL);

   /* cross-scope (project=gamma) → 403 */
   s = kb_http_route_ex("GET", "/v1/search", "query=x&project=gamma", auth, tok, NULL, 0, buf,
                        sizeof(buf));
   assert(s == 403);

   /* in-scope (project=alpha) → not 403 (auth + scope both pass) */
   s = kb_http_route_ex("GET", "/v1/search", "query=x&project=alpha", auth, tok, NULL, 0, buf,
                        sizeof(buf));
   assert(s != 403 && s != 401);
}

static void test_scope_token_secret_auth(void)
{
   /* The presented credential may be the full configured token or its secret
    * part; a wrong secret is 401. */
   const char *tok = "scope:project:alpha:s3cr3t";
   char buf[512];

   /* full token matches */
   assert(kb_http_route_ex("GET", "/v1/health", NULL, "Bearer scope:project:alpha:s3cr3t", tok,
                           NULL, 0, buf, sizeof(buf)) != 401);
   /* bare secret matches */
   assert(kb_http_route_ex("GET", "/v1/health", NULL, "Bearer s3cr3t", tok, NULL, 0, buf,
                           sizeof(buf)) != 401);
   /* wrong secret → 401 */
   assert(kb_http_route_ex("GET", "/v1/health", NULL, "Bearer nope", tok, NULL, 0, buf,
                           sizeof(buf)) == 401);
}

static void test_scope_admin_token_full_access(void)
{
   /* An unscoped (admin) token reaches any scope. */
   const char *tok = "plain-admin-secret";
   const char *auth = "Bearer plain-admin-secret";
   char buf[512];
   int s = kb_http_route_ex("GET", "/v1/search", "query=x&workspace=beta", auth, tok, NULL, 0, buf,
                            sizeof(buf));
   assert(s != 403 && s != 401);
}
