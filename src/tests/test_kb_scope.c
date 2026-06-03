/* test_kb_scope.c — unit tests for the pure bearer-token scope logic
 * (src/headers/kb_scope.h, src/kb/kb_scope.c). */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "kb_scope.h"

/* ---- token parsing ---- */
static void test_parse_scoped_token(void)
{
   char kind[32], id[128], secret[256];
   assert(kb_scope_token_parse("scope:project:alpha:s3cr3t", kind, sizeof(kind), id, sizeof(id),
                               secret, sizeof(secret)) == 0);
   assert(strcmp(kind, "project") == 0);
   assert(strcmp(id, "alpha") == 0);
   assert(strcmp(secret, "s3cr3t") == 0);
   printf("  parse_scoped_token: ok\n");
}

static void test_parse_workspace_token(void)
{
   char kind[32], id[128], secret[256];
   kb_scope_token_parse("scope:workspace:beta:abc123", kind, sizeof(kind), id, sizeof(id), secret,
                        sizeof(secret));
   assert(strcmp(kind, "workspace") == 0);
   assert(strcmp(id, "beta") == 0);
   assert(strcmp(secret, "abc123") == 0);
   printf("  parse_workspace_token: ok\n");
}

static void test_parse_plain_token_is_admin(void)
{
   char kind[32], id[128], secret[256];
   kb_scope_token_parse("plain-secret", kind, sizeof(kind), id, sizeof(id), secret, sizeof(secret));
   assert(kind[0] == '\0'); /* unscoped */
   assert(id[0] == '\0');
   assert(strcmp(secret, "plain-secret") == 0);
   printf("  parse_plain_token_is_admin: ok\n");
}

static void test_parse_malformed_degrades_to_admin(void)
{
   char kind[32], id[128], secret[256];
   /* Missing secret part → not treated as scoped (degrade to whole-string secret). */
   kb_scope_token_parse("scope:project:alpha", kind, sizeof(kind), id, sizeof(id), secret,
                        sizeof(secret));
   assert(kind[0] == '\0');
   assert(strcmp(secret, "scope:project:alpha") == 0);
   /* NULL token is safe. */
   kb_scope_token_parse(NULL, kind, sizeof(kind), id, sizeof(id), secret, sizeof(secret));
   assert(kind[0] == '\0' && secret[0] == '\0');
   printf("  parse_malformed_degrades_to_admin: ok\n");
}

/* ---- authorization ---- */
static void test_authorized_exact_match(void)
{
   assert(kb_scope_authorized("project", "alpha", "project", "alpha") == 1);
   assert(kb_scope_authorized("workspace", "beta", "workspace", "beta") == 1);
   printf("  authorized_exact_match: ok\n");
}

static void test_authorized_cross_scope_denied(void)
{
   assert(kb_scope_authorized("project", "alpha", "workspace", "beta") == 0);
   assert(kb_scope_authorized("project", "alpha", "project", "gamma") == 0);
   assert(kb_scope_authorized("workspace", "beta", "project", "alpha") == 0);
   printf("  authorized_cross_scope_denied: ok\n");
}

static void test_authorized_admin_and_unscoped_request(void)
{
   /* Unscoped (admin) token → always allowed. */
   assert(kb_scope_authorized("", "", "workspace", "beta") == 1);
   assert(kb_scope_authorized(NULL, NULL, "project", "alpha") == 1);
   /* Scoped token, request names no scope → allowed (nothing to deny). */
   assert(kb_scope_authorized("project", "alpha", "", "") == 1);
   printf("  authorized_admin_and_unscoped_request: ok\n");
}

/* ---- request target extraction ---- */
static void test_request_target_query(void)
{
   char kind[32], id[128];
   assert(kb_scope_request_target("query=x&project=alpha", NULL, kind, sizeof(kind), id,
                                  sizeof(id)) == 1);
   assert(strcmp(kind, "project") == 0 && strcmp(id, "alpha") == 0);

   assert(kb_scope_request_target("workspace=beta", NULL, kind, sizeof(kind), id, sizeof(id)) == 1);
   assert(strcmp(kind, "workspace") == 0 && strcmp(id, "beta") == 0);

   assert(kb_scope_request_target("scope=project:gamma", NULL, kind, sizeof(kind), id,
                                  sizeof(id)) == 1);
   assert(strcmp(kind, "project") == 0 && strcmp(id, "gamma") == 0);
   printf("  request_target_query: ok\n");
}

static void test_request_target_body(void)
{
   char kind[32], id[128];
   assert(kb_scope_request_target(NULL, "{\"scope_kind\":\"workspace\",\"scope_id\":\"beta\"}",
                                  kind, sizeof(kind), id, sizeof(id)) == 1);
   assert(strcmp(kind, "workspace") == 0 && strcmp(id, "beta") == 0);

   assert(kb_scope_request_target(NULL, "{\"scope_user\":\"u-42\"}", kind, sizeof(kind), id,
                                  sizeof(id)) == 1);
   assert(strcmp(kind, "user") == 0 && strcmp(id, "u-42") == 0);
   printf("  request_target_body: ok\n");
}

static void test_request_target_none(void)
{
   char kind[32], id[128];
   assert(kb_scope_request_target("query=hello&max=5", "{\"content\":\"hi\"}", kind, sizeof(kind),
                                  id, sizeof(id)) == 0);
   assert(kind[0] == '\0' && id[0] == '\0');
   assert(kb_scope_request_target(NULL, NULL, kind, sizeof(kind), id, sizeof(id)) == 0);
   printf("  request_target_none: ok\n");
}

int main(void)
{
   printf("kb_scope:\n");
   test_parse_scoped_token();
   test_parse_workspace_token();
   test_parse_plain_token_is_admin();
   test_parse_malformed_degrades_to_admin();
   test_authorized_exact_match();
   test_authorized_cross_scope_denied();
   test_authorized_admin_and_unscoped_request();
   test_request_target_query();
   test_request_target_body();
   test_request_target_none();
   printf("All kb_scope tests passed.\n");
   return 0;
}
