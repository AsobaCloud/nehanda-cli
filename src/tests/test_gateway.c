/* test_gateway.c: pure gateway routing and session-key tests. */
#include "gateway/gateway_ctx.h"
#include "gateway/gateway_platform.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#define PASS(name) printf("  PASS: %s\n", name)

typedef struct test_adapter_state
{
   int text_calls;
   int attachment_calls;
   char last_platform[32];
   char last_chat[128];
   char last_text[256];
   char last_path[512];
} test_adapter_state_t;

static int test_send_text(platform_adapter_t *self, const delivery_target_t *target,
                          const char *text)
{
   test_adapter_state_t *state = (test_adapter_state_t *)self->user;
   state->text_calls++;
   snprintf(state->last_platform, sizeof(state->last_platform), "%s", target->platform);
   snprintf(state->last_chat, sizeof(state->last_chat), "%s", target->chat_id);
   snprintf(state->last_text, sizeof(state->last_text), "%s", text);
   return 0;
}

static int test_send_attachment(platform_adapter_t *self, const delivery_target_t *target,
                                const char *path, const char *mime)
{
   (void)target;
   (void)mime;
   test_adapter_state_t *state = (test_adapter_state_t *)self->user;
   state->attachment_calls++;
   snprintf(state->last_path, sizeof(state->last_path), "%s", path);
   return 0;
}

static void test_send_text_via_registry(void)
{
   test_adapter_state_t state;
   memset(&state, 0, sizeof(state));
   platform_adapter_t adapter;
   memset(&adapter, 0, sizeof(adapter));
   adapter.name = "ntfy";
   adapter.display_name = "ntfy";
   adapter.send_text = test_send_text;
   adapter.send_attachment = test_send_attachment;
   adapter.user = &state;

   gateway_platform_register(&adapter);

   delivery_target_t target;
   assert(delivery_target_parse("ntfy:ops", &target) == 0);

   gateway_ctx_t *ctx = gateway_ctx_new();
   assert(ctx != NULL);
   assert(gateway_send_text(ctx, &target, "build finished") == 0);
   assert(state.text_calls == 1);
   assert(strcmp(state.last_platform, "ntfy") == 0);
   assert(strcmp(state.last_chat, "ops") == 0);
   assert(strcmp(state.last_text, "build finished") == 0);

   gateway_ctx_free(ctx);
   gateway_platform_unregister("ntfy");
   PASS("send_text_via_registry");
}

static void test_send_text_rejects_unknown_platform(void)
{
   delivery_target_t target;
   assert(delivery_target_parse("unknown:1234", &target) == 0);

   gateway_ctx_t *ctx = gateway_ctx_new();
   assert(ctx != NULL);
   assert(gateway_send_text(ctx, &target, "hello") == -1);
   gateway_ctx_free(ctx);
   PASS("send_text_rejects_unknown_platform");
}

static void test_send_text_rejects_null_inputs(void)
{
   gateway_ctx_t *ctx = gateway_ctx_new();
   assert(ctx != NULL);

   delivery_target_t target;
   memset(&target, 0, sizeof(target));
   snprintf(target.platform, sizeof(target.platform), "%s", "ntfy");

   assert(gateway_send_text(NULL, &target, "hello") == -1);
   assert(gateway_send_text(ctx, NULL, "hello") == -1);
   assert(gateway_send_text(ctx, &target, NULL) == -1);

   gateway_ctx_free(ctx);
   PASS("send_text_rejects_null_inputs");
}

static void test_session_keys(void)
{
   session_source_t src;

   memset(&src, 0, sizeof(src));
   snprintf(src.platform, sizeof(src.platform), "%s", "telegram");
   snprintf(src.chat_type, sizeof(src.chat_type), "%s", "dm");
   snprintf(src.chat_id, sizeof(src.chat_id), "%s", "1234");

   char *key = gateway_session_key(&src);
   assert(key != NULL);
   assert(strcmp(key, "telegram:dm:1234") == 0);
   free(key);

   /* thread suffix */
   snprintf(src.thread_id, sizeof(src.thread_id), "%s", "42");
   key = gateway_session_key(&src);
   assert(key != NULL);
   assert(strcmp(key, "telegram:thread:1234:42") == 0);
   free(key);

   /* private with empty chat_type */
   memset(&src, 0, sizeof(src));
   snprintf(src.platform, sizeof(src.platform), "%s", "ntfy");
   snprintf(src.chat_id, sizeof(src.chat_id), "%s", "ops");
   /* chat_type left empty — treated as DM */
   key = gateway_session_key(&src);
   assert(key != NULL);
   assert(strcmp(key, "ntfy:dm:ops") == 0);
   free(key);

   /* channel style: non-private chat_type */
   memset(&src, 0, sizeof(src));
   snprintf(src.platform, sizeof(src.platform), "%s", "telegram");
   snprintf(src.chat_type, sizeof(src.chat_type), "%s", "group");
   snprintf(src.chat_id, sizeof(src.chat_id), "%s", "5678");
   key = gateway_session_key(&src);
   assert(key != NULL);
   assert(strcmp(key, "telegram:group:5678") == 0);
   free(key);

   PASS("session_keys");
}

static void test_session_key_null_input(void)
{
   assert(gateway_session_key(NULL) == NULL);
   PASS("session_key_null_input");
}

static void test_authorize_user(void)
{
   session_source_t src;
   memset(&src, 0, sizeof(src));
   snprintf(src.platform, sizeof(src.platform), "%s", "nonexistent");

   gateway_ctx_t *ctx = gateway_ctx_new();
   assert(ctx != NULL);
   /* no adapter registered — should deny */
   assert(gateway_authorize_user(ctx, &src) == -1);

   /* empty platform — should deny */
   memset(&src, 0, sizeof(src));
   assert(gateway_authorize_user(ctx, &src) == -1);

   gateway_ctx_free(ctx);
   PASS("authorize_user");
}

static void test_pr_command_parse(void)
{
   char ws[256], title[512];

   /* workspace + multi-word title */
   assert(gateway_parse_pr_command("aimee Fix the parser bug", ws, sizeof(ws), title,
                                   sizeof(title)) == 0);
   assert(strcmp(ws, "aimee") == 0);
   assert(strcmp(title, "Fix the parser bug") == 0);

   /* workspace only — empty title */
   assert(gateway_parse_pr_command("aimee", ws, sizeof(ws), title, sizeof(title)) == 0);
   assert(strcmp(ws, "aimee") == 0);
   assert(title[0] == '\0');

   /* leading + interior whitespace trimmed; full path as workspace token */
   assert(gateway_parse_pr_command("   /srv/proj   My  title  ", ws, sizeof(ws), title,
                                   sizeof(title)) == 0);
   assert(strcmp(ws, "/srv/proj") == 0);
   assert(strcmp(title, "My  title  ") == 0); /* interior/trailing kept; only leading trimmed */

   /* empty / whitespace-only → no workspace token */
   assert(gateway_parse_pr_command("", ws, sizeof(ws), title, sizeof(title)) == -1);
   assert(gateway_parse_pr_command("    ", ws, sizeof(ws), title, sizeof(title)) == -1);
   assert(gateway_parse_pr_command(NULL, ws, sizeof(ws), title, sizeof(title)) == -1);

   PASS("pr_command_parse");
}

int main(void)
{
   printf("=== gateway routing and session-key tests ===\n");
   test_send_text_via_registry();
   test_send_text_rejects_unknown_platform();
   test_send_text_rejects_null_inputs();
   test_session_keys();
   test_session_key_null_input();
   test_authorize_user();
   test_pr_command_parse();
   printf("\nAll gateway tests passed.\n");
   return 0;
}
