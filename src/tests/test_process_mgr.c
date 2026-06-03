/* test_process_mgr.c: unit tests for process_mgr.c */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "cJSON.h"
#include "process_mgr.h"
#include "server_mcp_process.h"

/* --- helpers --- */

static void drain_output(int id, int poll_ms)
{
   /* Give reader thread time to capture output */
   usleep((useconds_t)poll_ms * 1000);
   (void)id;
}

/* --- tests --- */

static void test_start_returns_id(void)
{
   char err[256] = "";
   int id = proc_start("true", NULL, err, sizeof(err));
   assert(id >= 1);
   /* let it finish */
   usleep(200000);
   proc_cleanup_all();
   printf("  start_returns_id: ok\n");
}

static void test_start_captures_stdout(void)
{
   char err[256] = "";
   int id = proc_start("echo hello_world", NULL, err, sizeof(err));
   assert(id >= 1);

   /* wait for output to be captured */
   drain_output(id, 300);

   char out[4096] = {0};
   int rc = proc_get_output(id, 50, out, sizeof(out));
   assert(rc == 0);
   assert(strstr(out, "hello_world") != NULL);

   proc_cleanup_all();
   printf("  start_captures_stdout: ok\n");
}

static void test_start_captures_stderr(void)
{
   char err[256] = "";
   int id = proc_start("echo err_output >&2", NULL, err, sizeof(err));
   assert(id >= 1);

   drain_output(id, 300);

   char out[4096] = {0};
   int rc = proc_get_output(id, 50, out, sizeof(out));
   assert(rc == 0);
   assert(strstr(out, "err_output") != NULL);

   proc_cleanup_all();
   printf("  start_captures_stderr: ok\n");
}

static void test_list_shows_process(void)
{
   char err[256] = "";
   int id = proc_start("sleep 60", NULL, err, sizeof(err));
   assert(id >= 1);

   char list[4096] = {0};
   proc_list(list, sizeof(list));
   assert(strstr(list, "sleep") != NULL);
   assert(strstr(list, "running") != NULL);

   proc_kill(id);
   proc_cleanup_all();
   printf("  list_shows_process: ok\n");
}

static void test_kill_terminates_process(void)
{
   char err[256] = "";
   int id = proc_start("sleep 60", NULL, err, sizeof(err));
   assert(id >= 1);

   int rc = proc_kill(id);
   assert(rc == 0);

   /* After kill, process should show as exited in list */
   char list[4096] = {0};
   proc_list(list, sizeof(list));
   assert(strstr(list, "exited") != NULL);

   proc_cleanup_all();
   printf("  kill_terminates_process: ok\n");
}

static void test_kill_nonexistent_returns_error(void)
{
   int rc = proc_kill(99999);
   assert(rc == -1);
   proc_cleanup_all();
   printf("  kill_nonexistent_returns_error: ok\n");
}

static void test_output_nonexistent_returns_error(void)
{
   char out[256] = {0};
   int rc = proc_get_output(99999, 10, out, sizeof(out));
   assert(rc == -1);
   assert(strstr(out, "error") != NULL || strstr(out, "not found") != NULL);
   proc_cleanup_all();
   printf("  output_nonexistent_returns_error: ok\n");
}

static void test_concurrent_limit_enforced(void)
{
   char err[256] = "";
   int ids[PROC_MAX_CONCURRENT + 1];
   int started = 0;

   /* start up to the limit */
   for (int i = 0; i < PROC_MAX_CONCURRENT; i++)
   {
      ids[i] = proc_start("sleep 60", NULL, err, sizeof(err));
      if (ids[i] < 0)
         break;
      started++;
   }
   assert(started == PROC_MAX_CONCURRENT);

   /* one more should fail */
   int extra = proc_start("sleep 60", NULL, err, sizeof(err));
   assert(extra < 0);
   assert(strstr(err, "limit") != NULL || strstr(err, "full") != NULL);

   /* cleanup */
   for (int i = 0; i < started; i++)
      proc_kill(ids[i]);
   proc_cleanup_all();
   printf("  concurrent_limit_enforced: ok\n");
}

static void test_empty_command_rejected(void)
{
   char err[256] = "";
   int id = proc_start("", NULL, err, sizeof(err));
   assert(id < 0);
   assert(err[0] != '\0');
   proc_cleanup_all();
   printf("  empty_command_rejected: ok\n");
}

static void test_list_empty_is_valid_json(void)
{
   char list[256] = {0};
   proc_list(list, sizeof(list));
   /* must start with '[' and end with ']' */
   assert(list[0] == '[');
   size_t len = strlen(list);
   assert(len > 0 && list[len - 1] == ']');
   proc_cleanup_all();
   printf("  list_empty_is_valid_json: ok\n");
}

static void test_cleanup_idempotent(void)
{
   char err[256] = "";
   (void)proc_start("sleep 60", NULL, err, sizeof(err));
   proc_cleanup_all();
   proc_cleanup_all(); /* second call should not crash */
   printf("  cleanup_idempotent: ok\n");
}

static void test_mcp_list_background_processes_dispatch(void)
{
   cJSON *args = cJSON_CreateObject();
   cJSON *content = server_mcp_process_tool("list_background_processes", args);
   assert(content != NULL);
   cJSON *item = cJSON_GetArrayItem(content, 0);
   cJSON *text = cJSON_GetObjectItemCaseSensitive(item, "text");
   assert(cJSON_IsString(text));
   assert(text->valuestring[0] == '[');
   cJSON_Delete(content);

   content = server_mcp_process_tool("not_a_process_tool", args);
   assert(content == NULL);
   cJSON_Delete(args);
   proc_cleanup_all();
   printf("  mcp_list_background_processes_dispatch: ok\n");
}

int main(void)
{
   printf("test_process_mgr:\n");

   test_start_returns_id();
   test_start_captures_stdout();
   test_start_captures_stderr();
   test_list_shows_process();
   test_kill_terminates_process();
   test_kill_nonexistent_returns_error();
   test_output_nonexistent_returns_error();
   test_concurrent_limit_enforced();
   test_empty_command_rejected();
   test_list_empty_is_valid_json();
   test_cleanup_idempotent();
   test_mcp_list_background_processes_dispatch();

   printf("all process_mgr tests passed.\n");
   return 0;
}
