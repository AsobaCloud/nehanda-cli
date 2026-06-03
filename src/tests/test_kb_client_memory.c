/* test_kb_client_memory.c: the kb_client memory read wrappers must
 * distinguish "kb unreachable" from "genuinely empty". Regression guard for
 * the bug where an unreachable knowledge service was reported as an empty
 * store (e.g. `aimee memory list` printing "No memories" during an outage).
 *
 * Drives the wrappers through the mocked agent_http transport: one handler
 * fails at the transport layer (unreachable), another returns a well-formed
 * ok envelope with empty arrays (healthy but empty). The contract is:
 *   unreachable -> < 0,   healthy+empty -> 0. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "kb_client.h"
#include "support/mock_agent_http.h"
#include "cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Transport failure: no response body, sub-100 status. kb_v1_action_request
 * yields no successful envelope, so readers must report unavailability. */
static int unreachable_post_handler(const char *url, const char *auth_header, const char *body,
                                    char **response_buf, int timeout_ms, const char *extra_headers)
{
   (void)url;
   (void)auth_header;
   (void)body;
   (void)timeout_ms;
   (void)extra_headers;
   if (response_buf)
      *response_buf = NULL;
   return -1;
}

/* Healthy kb that simply has no rows: well-formed "ok" envelope with every
 * result array empty. Readers must report 0, never < 0. */
static int empty_ok_post_handler(const char *url, const char *auth_header, const char *body,
                                 char **response_buf, int timeout_ms, const char *extra_headers)
{
   (void)url;
   (void)auth_header;
   (void)body;
   (void)timeout_ms;
   (void)extra_headers;
   if (response_buf)
      *response_buf = strdup("{\"status\":\"ok\",\"memories\":[],\"facts\":[],\"results\":[],"
                             "\"conflicts\":[],\"edges\":[],\"relations\":[],\"links\":[]}");
   return 200;
}

static void test_readers_distinguish_unreachable_from_empty(void)
{
   memory_t mems[8];
   search_result_t windows[8];
   conflict_t conflicts[8];
   char *clusters[] = {"hello"};

   /* --- kb unreachable: every count-returning reader reports < 0 --- */
   mock_agent_http_set_post_handler(unreachable_post_handler);
   assert(kb_client_memory_list(NULL, NULL, 8, mems, 8) < 0);
   assert(kb_client_memory_find_facts("q", 8, mems, 8) < 0);
   assert(kb_client_memory_find_facts_scoped("q", NULL, NULL, 8, mems, 8) < 0);
   assert(kb_client_memory_find_facts_visible("q", NULL, NULL, 8, mems, 8) < 0);
   assert(kb_client_memory_search(clusters, 1, 8, windows, 8) < 0);
   assert(kb_client_memory_list_conflicts(conflicts, 8) < 0);

   /* --- healthy but empty: same readers report exactly 0 (not < 0) --- */
   mock_agent_http_set_post_handler(empty_ok_post_handler);
   assert(kb_client_memory_list(NULL, NULL, 8, mems, 8) == 0);
   assert(kb_client_memory_find_facts("q", 8, mems, 8) == 0);
   assert(kb_client_memory_find_facts_scoped("q", NULL, NULL, 8, mems, 8) == 0);
   assert(kb_client_memory_find_facts_visible("q", NULL, NULL, 8, mems, 8) == 0);
   assert(kb_client_memory_search(clusters, 1, 8, windows, 8) == 0);
   assert(kb_client_memory_list_conflicts(conflicts, 8) == 0);

   mock_agent_http_reset();
   printf("  PASS: test_readers_distinguish_unreachable_from_empty\n");
}

int main(void)
{
   /* A configured kb URL routes kb_client_v1_post_json through agent_http_post
    * (mocked) rather than the unix-socket / spawn path. */
   assert(setenv("AIMEE_KB_API_URL", "http://127.0.0.1:4010/", 1) == 0);
   assert(setenv("AIMEE_KB_API_BEARER_TOKEN", "test-token", 1) == 0);

   test_readers_distinguish_unreachable_from_empty();

   unsetenv("AIMEE_KB_API_URL");
   unsetenv("AIMEE_KB_API_BEARER_TOKEN");
   printf("test_kb_client_memory: ok\n");
   return 0;
}
