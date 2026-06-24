/* test_openai_chat_policed.c: unit tests for the P2c response-side
 * tool-policing OpenAI /v1/responses SSE replay helper
 * (openai_responses_emit_policed). Pure shape tests — no agent
 * execution, no real provider, no streaming transport; just the
 * post-police `parsed_response_t` -> SSE event sequence. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "openai_shape.h"
/* aimee.h before agent_protocol.h: agent_types.h (pulled by agent_protocol.h)
 * uses MAX_PATH_LEN from aimee.h. Same include convention as the production
 * caller, src/server/openai_chat.c. openai_shape.h itself stays decoupled and
 * forward-declares the types it needs. */
#include "aimee.h"
#include "agent_protocol.h"
#include "cJSON.h"

#define PASS(name) printf("  PASS: %s\n", (name))

/* Captured SSE events. */
#define REPLAY_CAP 64
typedef struct
{
   char events[REPLAY_CAP][64];
   char data[REPLAY_CAP][4096];
   int count;
} cap_t;

static void cap_emit(void *ctx, const char *event, const char *data)
{
   cap_t *c = (cap_t *)ctx;
   assert(c->count < REPLAY_CAP);
   snprintf(c->events[c->count], sizeof(c->events[c->count]), "%s", event);
   snprintf(c->data[c->count], sizeof(c->data[c->count]), "%s", data);
   c->count++;
}

/* Build a parsed_response_t with N tool calls (caller-supplied names +
 * ids + arguments). is_tool_call=1 so the dispatcher takes the
 * tool-call branch. */
static void seed_parsed(parsed_response_t *p, int n, const char *names[], const char *ids[],
                        const char *args[], const char *stop_reason)
{
   int i;
   memset(p, 0, sizeof(*p));
   p->call_count = n;
   p->is_tool_call = 1;
   if (stop_reason)
      snprintf(p->stop_reason, sizeof(p->stop_reason), "%s", stop_reason);
   p->prompt_tokens = 17;
   p->completion_tokens = 23;
   for (i = 0; i < n; i++)
   {
      snprintf(p->calls[i].id, sizeof(p->calls[i].id), "%s", ids[i]);
      snprintf(p->calls[i].name, sizeof(p->calls[i].name), "%s", names[i]);
      p->calls[i].arguments = strdup(args[i] ? args[i] : "{}");
   }
}

static void free_parsed(parsed_response_t *p)
{
   int i;
   for (i = 0; i < p->call_count; i++)
      free(p->calls[i].arguments);
}

/* --- tests --- */

/* response.created is always emitted first, regardless of call_count. */
static void test_emit_response_created_first(void)
{
   cap_t cap = {0};
   parsed_response_t p;
   seed_parsed(&p, 0, NULL, NULL, NULL, "end_turn");
   char frame[2048];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));
   assert(cap.count >= 1);
   assert(strcmp(cap.events[0], "response.created") == 0);
   /* Sanity: the frame contains our id + the model. */
   assert(strstr(cap.data[0], "\"id\":\"resp_1\"") != NULL);
   assert(strstr(cap.data[0], "\"model\":\"claude-test\"") != NULL);
   free_parsed(&p);
   PASS("emit_response_created_first");
}

/* Single surviving tool_call: per-call frames + completed. */
static void test_emit_surviving_tool_call(void)
{
   cap_t cap = {0};
   const char *names[] = {"web_search"};
   const char *ids[] = {"call_1"};
   const char *args[] = {"{\"query\":\"aimee\"}"};
   parsed_response_t p;
   seed_parsed(&p, 1, names, ids, args, "tool_calls");
   char frame[2048];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));

   /* response.created + (output_item.added, args.delta, args.done,
    * output_item.done) + response.completed = 6 events. */
   assert(cap.count == 6);
   assert(strcmp(cap.events[0], "response.created") == 0);
   assert(strcmp(cap.events[1], "response.output_item.added") == 0);
   assert(strstr(cap.data[1], "\"name\":\"web_search\"") != NULL);
   assert(strstr(cap.data[1], "\"call_id\":\"call_1\"") != NULL);
   assert(strcmp(cap.events[2], "response.function_call_arguments.delta") == 0);
   assert(strstr(cap.data[2], "aimee") != NULL);
   assert(strcmp(cap.events[3], "response.function_call_arguments.done") == 0);
   assert(strcmp(cap.events[4], "response.output_item.done") == 0);
   assert(strcmp(cap.events[5], "response.completed") == 0);
   assert(strstr(cap.data[5], "\"output\":[") != NULL);
   free_parsed(&p);
   PASS("emit_surviving_tool_call");
}

/* Two surviving tool_calls: indices in `output[]` are 0 and 1. */
static void test_emit_multiple_surviving_tool_calls(void)
{
   cap_t cap = {0};
   const char *names[] = {"web_search", "Read"};
   const char *ids[] = {"call_1", "call_2"};
   const char *args[] = {"{\"q\":\"x\"}", "{\"p\":\"/etc\"}"};
   parsed_response_t p;
   seed_parsed(&p, 2, names, ids, args, "tool_calls");
   char frame[2048];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));

   /* response.created + (4 per-call frames × 2) + response.completed = 10. */
   assert(cap.count == 10);
   /* The two `output_item.added` events at indices 1 and 5 carry
    * fc_id suffixes "-fc-0" and "-fc-1" (built from the loop index). */
   assert(strstr(cap.data[1], "-fc-0") != NULL);
   assert(strstr(cap.data[5], "-fc-1") != NULL);
   /* Names preserved in original order. */
   assert(strstr(cap.data[1], "web_search") != NULL);
   assert(strstr(cap.data[5], "Read") != NULL);
   free_parsed(&p);
   PASS("emit_multiple_surviving_tool_calls");
}

/* All-dropped (call_count == 0): response.created + empty-output
 * response.completed, no per-call frames. */
static void test_emit_all_dropped_empty_output(void)
{
   cap_t cap = {0};
   parsed_response_t p;
   seed_parsed(&p, 0, NULL, NULL, NULL, "end_turn");
   char frame[2048];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));

   assert(cap.count == 2);
   assert(strcmp(cap.events[0], "response.created") == 0);
   assert(strcmp(cap.events[1], "response.completed") == 0);
   /* Empty output array — `[]`. */
   assert(strstr(cap.data[1], "\"output\":[]") != NULL);
   free_parsed(&p);
   PASS("emit_all_dropped_empty_output");
}

/* Null parsed is tolerated (no crash, no events emitted except created
 * with default-0 usage). Guards against a future caller passing a
 * police error result. */
static void test_emit_null_parsed(void)
{
   cap_t cap = {0};
   char frame[2048];
   openai_responses_emit_policed(NULL, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));
   /* response.created + empty-output completed. */
   assert(cap.count == 2);
   assert(strcmp(cap.events[0], "response.created") == 0);
   assert(strcmp(cap.events[1], "response.completed") == 0);
   assert(strstr(cap.data[1], "\"output\":[]") != NULL);
   PASS("emit_null_parsed");
}

/* Arguments are propagated verbatim (byte-for-byte fidelity for the
 * tool_use contract — Codex's executor stringifies and re-parses the
 * JSON; a re-serialization that re-ordered keys or reformatted
 * whitespace would still parse but could break string equality
 * assertions in client code). */
static void test_emit_arguments_byte_fidelity(void)
{
   cap_t cap = {0};
   const char *names[] = {"f"};
   const char *ids[] = {"c1"};
   const char *args[] = {"{\"b\":2,\"a\":1,\"nested\":{\"x\":\"y\"}}"};
   parsed_response_t p;
   seed_parsed(&p, 1, names, ids, args, "tool_calls");
   char frame[2048];
   const char *original = args[0];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));
   /* The args.delta event carries the arguments as the JSON-string `delta`
    * field (the OpenAI wire shape — inner quotes are escaped on the wire).
    * Byte fidelity means: after parsing the event JSON, the decoded `delta`
    * string equals the original arguments byte-for-byte (no key reorder, no
    * whitespace reflow). Parse it back and compare rather than scanning the
    * escaped wire bytes. The same fidelity holds for the `.done` event. */
   cJSON *delta_ev = cJSON_Parse(cap.data[2]);
   assert(delta_ev != NULL);
   const cJSON *delta = cJSON_GetObjectItemCaseSensitive(delta_ev, "delta");
   assert(cJSON_IsString(delta) && delta->valuestring != NULL);
   assert(strcmp(delta->valuestring, original) == 0);
   cJSON_Delete(delta_ev);

   cJSON *done_ev = cJSON_Parse(cap.data[3]);
   assert(done_ev != NULL);
   const cJSON *full = cJSON_GetObjectItemCaseSensitive(done_ev, "arguments");
   assert(cJSON_IsString(full) && full->valuestring != NULL);
   assert(strcmp(full->valuestring, original) == 0);
   cJSON_Delete(done_ev);

   free_parsed(&p);
   PASS("emit_arguments_byte_fidelity");
}

/* Byte fidelity holds for the hard cases too: arguments containing characters
 * that must be JSON-escaped on the wire (embedded quotes/backslashes from a
 * nested JSON string) and multi-byte UTF-8. The decoded delta/arguments must
 * still equal the original byte-for-byte. */
static void test_emit_arguments_escapes_and_utf8(void)
{
   cap_t cap = {0};
   const char *names[] = {"f"};
   const char *ids[] = {"c1"};
   /* A path with a quote + backslash, and a UTF-8 string value (é, 日本). */
   const char *args[] = {"{\"path\":\"C:\\\\a\\\"b\",\"note\":\"café 日本\"}"};
   parsed_response_t p;
   seed_parsed(&p, 1, names, ids, args, "tool_calls");
   char frame[2048];
   const char *original = args[0];
   openai_responses_emit_policed(&p, "resp_1", "claude-test", 12345L, cap_emit, &cap, frame,
                                 sizeof(frame));
   cJSON *delta_ev = cJSON_Parse(cap.data[2]);
   assert(delta_ev != NULL); /* the emitted frame is itself valid JSON */
   const cJSON *delta = cJSON_GetObjectItemCaseSensitive(delta_ev, "delta");
   assert(cJSON_IsString(delta) && delta->valuestring != NULL);
   assert(strcmp(delta->valuestring, original) == 0);
   cJSON_Delete(delta_ev);
   free_parsed(&p);
   PASS("emit_arguments_escapes_and_utf8");
}

int main(void)
{
   printf("test_openai_chat_policed:\n");
   test_emit_response_created_first();
   test_emit_surviving_tool_call();
   test_emit_multiple_surviving_tool_calls();
   test_emit_all_dropped_empty_output();
   test_emit_null_parsed();
   test_emit_arguments_byte_fidelity();
   test_emit_arguments_escapes_and_utf8();
   printf("all openai_chat_policed tests passed\n");
   return 0;
}
