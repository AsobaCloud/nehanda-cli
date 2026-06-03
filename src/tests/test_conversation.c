/* test_conversation.c: unit tests for conversation branching and threads */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "conversation.h"
#include "cJSON.h"

/* --- Helpers --- */

static cJSON *make_msg(const char *role, const char *content)
{
   cJSON *m = cJSON_CreateObject();
   cJSON_AddStringToObject(m, "role", role);
   cJSON_AddStringToObject(m, "content", content);
   return m;
}

static cJSON *make_messages(int count)
{
   cJSON *arr = cJSON_CreateArray();
   for (int i = 0; i < count; i++)
   {
      char buf[64];
      snprintf(buf, sizeof(buf), "message %d", i);
      cJSON_AddItemToArray(arr, make_msg(i % 2 == 0 ? "user" : "assistant", buf));
   }
   return arr;
}

/* --- Tests --- */

static void test_init_creates_root_thread(void)
{
   conv_threads_t ct;
   cJSON *msgs = make_messages(4);
   conv_threads_init(&ct, msgs);

   assert(ct.count == 1);
   assert(ct.active == 0);
   assert(ct.threads[0].id == 0);
   assert(ct.threads[0].parent_id == -1);
   assert(conv_active_id(&ct) == 0);
   assert(conv_active_messages(&ct) == msgs);

   conv_threads_free(&ct);
}

static void test_init_null_messages(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, NULL);

   assert(ct.count == 1);
   assert(conv_active_messages(&ct) != NULL);
   assert(cJSON_GetArraySize(conv_active_messages(&ct)) == 0);

   conv_threads_free(&ct);
}

static void test_branch_creates_thread(void)
{
   conv_threads_t ct;
   cJSON *msgs = make_messages(6);
   conv_threads_init(&ct, msgs);

   int new_id = conv_branch(&ct, "approach-B");
   assert(new_id == 1);
   assert(ct.count == 2);

   /* Should have auto-switched to the new thread */
   assert(conv_active_id(&ct) == 1);

   /* New thread should have a copy of all 6 messages */
   cJSON *new_msgs = conv_active_messages(&ct);
   assert(cJSON_GetArraySize(new_msgs) == 6);

   /* Thread metadata */
   const conv_thread_t *t = &ct.threads[1];
   assert(t->parent_id == 0);
   assert(t->branch_msg_index == 6);
   assert(strcmp(t->label, "approach-B") == 0);

   conv_threads_free(&ct);
}

static void test_branch_default_label(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(2));

   int id = conv_branch(&ct, NULL);
   const conv_thread_t *t = &ct.threads[id];
   assert(strncmp(t->label, "thread-", 7) == 0);

   conv_threads_free(&ct);
}

static void test_branch_isolation(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(4));

   conv_branch(&ct, "fork");
   /* Add a message to the new thread */
   cJSON_AddItemToArray(conv_active_messages(&ct), make_msg("user", "only in fork"));

   /* Switch back to root */
   conv_switch(&ct, 0);
   /* Root should still have only 4 messages */
   assert(cJSON_GetArraySize(conv_active_messages(&ct)) == 4);

   /* Switch to fork — should have 5 */
   conv_switch(&ct, 1);
   assert(cJSON_GetArraySize(conv_active_messages(&ct)) == 5);

   conv_threads_free(&ct);
}

static void test_switch_valid(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(2));
   conv_branch(&ct, "b1");
   assert(conv_active_id(&ct) == 1);

   assert(conv_switch(&ct, 0) == 0);
   assert(conv_active_id(&ct) == 0);

   assert(conv_switch(&ct, 1) == 0);
   assert(conv_active_id(&ct) == 1);

   conv_threads_free(&ct);
}

static void test_switch_invalid(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(2));

   assert(conv_switch(&ct, 99) == -1);
   assert(conv_active_id(&ct) == 0);

   conv_threads_free(&ct);
}

static void test_branch_max_threads(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(2));

   /* Create threads until we hit the limit */
   int last_id = -1;
   for (int i = 1; i < CONV_MAX_THREADS; i++)
   {
      last_id = conv_branch(&ct, NULL);
      assert(last_id >= 0);
      /* Switch back to root to branch from root each time */
      conv_switch(&ct, 0);
   }

   /* One more should fail */
   assert(conv_branch(&ct, "overflow") == -1);
   assert(ct.count == CONV_MAX_THREADS);

   conv_threads_free(&ct);
}

static void test_active_messages_null(void)
{
   assert(conv_active_messages(NULL) == NULL);
   assert(conv_active_id(NULL) == -1);
}

static void test_multiple_branches_from_same_point(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(4));

   /* Create two branches from the root at the same point */
   conv_branch(&ct, "option-A");
   conv_switch(&ct, 0);
   conv_branch(&ct, "option-B");

   assert(ct.count == 3);

   /* Both should have 4 messages initially */
   conv_switch(&ct, 1);
   assert(cJSON_GetArraySize(conv_active_messages(&ct)) == 4);
   conv_switch(&ct, 2);
   assert(cJSON_GetArraySize(conv_active_messages(&ct)) == 4);

   /* Add different messages to each */
   cJSON_AddItemToArray(conv_active_messages(&ct), make_msg("user", "B path"));
   conv_switch(&ct, 1);
   cJSON_AddItemToArray(conv_active_messages(&ct), make_msg("user", "A path"));

   /* Verify isolation */
   conv_switch(&ct, 1);
   cJSON *last_a = cJSON_GetArrayItem(conv_active_messages(&ct), 4);
   assert(strcmp(cJSON_GetObjectItem(last_a, "content")->valuestring, "A path") == 0);

   conv_switch(&ct, 2);
   cJSON *last_b = cJSON_GetArrayItem(conv_active_messages(&ct), 4);
   assert(strcmp(cJSON_GetObjectItem(last_b, "content")->valuestring, "B path") == 0);

   conv_threads_free(&ct);
}

static void test_nested_branches(void)
{
   conv_threads_t ct;
   conv_threads_init(&ct, make_messages(2));

   /* Branch from root */
   int t1 = conv_branch(&ct, "level-1");
   cJSON_AddItemToArray(conv_active_messages(&ct), make_msg("user", "t1 msg"));

   /* Branch from t1 */
   int t2 = conv_branch(&ct, "level-2");
   assert(t2 == 2);

   const conv_thread_t *thread2 = &ct.threads[t2];
   assert(thread2->parent_id == t1);
   assert(thread2->branch_msg_index == 3); /* 2 original + 1 added */

   conv_threads_free(&ct);
}

static void test_free_null(void)
{
   /* Should not crash */
   conv_threads_free(NULL);
}

int main(void)
{
   printf("conversation: ");

   test_init_creates_root_thread();
   test_init_null_messages();
   test_branch_creates_thread();
   test_branch_default_label();
   test_branch_isolation();
   test_switch_valid();
   test_switch_invalid();
   test_branch_max_threads();
   test_active_messages_null();
   test_multiple_branches_from_same_point();
   test_nested_branches();
   test_free_null();

   printf("all tests passed\n");
   return 0;
}
