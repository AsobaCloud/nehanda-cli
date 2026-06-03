/* test_kb_http_routes_endpoints.inc: OpenAPI / reflection / feedback / facet
 * route tests split out of test_kb_http_routes.c to keep that .c under the
 * 2000-line hard limit. Included mid-file (same translation unit) so the
 * shared mock harness and request helpers above remain in scope. */

static void test_openapi_json_ok(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/openapi.json", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   /* The response is YAML (openapi-v1.yaml content); check a known header field. */
   assert(strstr(buf, "openapi:") != NULL || strstr(buf, "aimee-kb") != NULL);
}

static void test_openapi_yaml_ok(void)
{
   char buf[2048];
   int s = kb_http_route_ex("GET", "/v1/openapi.yaml", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "openapi:") != NULL || strstr(buf, "aimee-kb") != NULL);
}

static void test_openapi_method_not_allowed(void)
{
   char buf[64];
   int s =
       kb_http_route_ex("POST", "/v1/openapi.json", NULL, NULL, NULL, "{}", 2, buf, sizeof(buf));
   assert(s == 405);
}

/* ── Scope isolation: /v1/feedback/in-session cross-user check ──────────── */

static void test_feedback_scope_isolation(void)
{
   /* Scope isolation contract: /v1/feedback/in-session accepts a scope_user
    * field; the handler stores artifacts at scope_kind="user" with that
    * scope_id. Here we verify the routing and response contract — each
    * submission must succeed (201) and produce an artifact id in the response.
    * Full DB-level isolation is exercised by tests/test_kb_http_routes_db.c
    * which links the real kb_http_reflections.c against a DB2 shim. */
   char buf_a[256], buf_b[256];
   const char *body_a = "{\"kind\":\"feedback_positive\",\"session_id\":\"s-user-a\","
                        "\"scope_user\":\"user-a\",\"content\":\"good\"}";
   const char *body_b = "{\"kind\":\"feedback_negative\",\"session_id\":\"s-user-b\","
                        "\"scope_user\":\"user-b\",\"content\":\"bad\"}";

   int sa = kb_http_route_ex("POST", "/v1/feedback/in-session", NULL, NULL, NULL, body_a,
                             (int)strlen(body_a), buf_a, sizeof(buf_a));
   int sb = kb_http_route_ex("POST", "/v1/feedback/in-session", NULL, NULL, NULL, body_b,
                             (int)strlen(body_b), buf_b, sizeof(buf_b));

   assert(sa == 201);
   assert(sb == 201);
   assert(strstr(buf_a, "\"id\"") != NULL);
   assert(strstr(buf_b, "\"id\"") != NULL);
   /* Wrong method must be rejected. */
   int sd = kb_http_route_ex("DELETE", "/v1/feedback/in-session", NULL, NULL, NULL, NULL, 0, buf_a,
                             sizeof(buf_a));
   assert(sd == 404 || sd == 405);
}

/* ── Phase 6 route tests ─────────────────────────────────────────────────── */

static void test_post_reflections_ok(void)
{
   char buf[256];
   const char *body =
       "{\"entries\":[{\"kind\":\"session_summary\",\"confidence\":0.8,\"payload\":{}}]}";
   int s = kb_http_route_ex("POST", "/v1/reflections", NULL, NULL, NULL, body, (int)strlen(body),
                            buf, sizeof(buf));
   assert(s == 201);
   assert(strstr(buf, "\"created\"") != NULL);
}

static void test_get_reflections_ok(void)
{
   char buf[256];
   int s = kb_http_route_ex("GET", "/v1/reflections", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"items\"") != NULL);
}

static void test_reflection_accept_ok(void)
{
   char buf[256];
   int s = kb_http_route_ex("POST", "/v1/reflections/some-uuid/accept", NULL, NULL, NULL, "{}", 2,
                            buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "committed") != NULL);
}

static void test_reflection_reject_ok(void)
{
   char buf[256];
   int s = kb_http_route_ex("POST", "/v1/reflections/some-uuid/reject", NULL, NULL, NULL,
                            "{\"verdict_tag\":\"wrong\"}", 23, buf, sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "rejected") != NULL);
}

static void test_reflection_wrong_method(void)
{
   char buf[256];
   int s =
       kb_http_route_ex("DELETE", "/v1/reflections", NULL, NULL, NULL, NULL, 0, buf, sizeof(buf));
   assert(s == 404 || s == 405);
}

static void test_feedback_in_session_ok(void)
{
   char buf[256];
   const char *body =
       "{\"kind\":\"feedback_negative\",\"session_id\":\"s1\",\"content\":\"wrong\"}";
   int s = kb_http_route_ex("POST", "/v1/feedback/in-session", NULL, NULL, NULL, body,
                            (int)strlen(body), buf, sizeof(buf));
   assert(s == 201);
   assert(strstr(buf, "\"id\"") != NULL);
}

/* POST /v1/search with typed facets routes through the artifact facet filter
 * and returns facet-shaped hits (deep-curator). */
static void test_search_facet_filter(void)
{
   char buf[2048];
   const char *body = "{\"query\":\"three db\",\"filters\":{\"status\":\"done\","
                      "\"component\":\"pgvector\",\"kind\":\"doc_summary\"}}";
   int s = kb_http_route_ex("POST", "/v1/search", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"fusion_mode_used\":\"facet\"") != NULL);
   assert(strstr(buf, "\"artifact_id\":\"art-facet-1\"") != NULL);
   assert(strstr(buf, "\"kind\":\"doc_summary\"") != NULL);
   assert(strstr(buf, "\"total_hits\":1") != NULL);
   /* Bound to the active release by default (stub returns 7) and cites it. */
   assert(strstr(buf, "\"release_id\":7") != NULL);
}

/* POST /v1/search with explicit release_id:0 searches across all releases. */
static void test_search_facet_all_releases(void)
{
   char buf[2048];
   const char *body = "{\"query\":\"q\",\"release_id\":0,\"filters\":{\"kind\":\"doc_summary\"}}";
   int s = kb_http_route_ex("POST", "/v1/search", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"fusion_mode_used\":\"facet\"") != NULL);
   assert(strstr(buf, "\"release_id\":null") != NULL);
}

/* POST /v1/search without filters keeps the existing non-facet search path. */
static void test_search_no_filters_not_facet(void)
{
   char buf[2048];
   const char *body = "{\"query\":\"three db\"}";
   int s = kb_http_route_ex("POST", "/v1/search", NULL, NULL, NULL, body, (int)strlen(body), buf,
                            sizeof(buf));
   assert(s == 200);
   assert(strstr(buf, "\"fusion_mode_used\":\"rrf\"") != NULL);
   assert(strstr(buf, "\"fusion_mode_used\":\"facet\"") == NULL);
}

