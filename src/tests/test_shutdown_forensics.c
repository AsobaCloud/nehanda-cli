/* test_shutdown_forensics.c: shutdown/crash forensics record tests */
#include "shutdown_forensics.h"
#include "platform_test_util.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *saved_env(const char *name)
{
   const char *v = getenv(name);
   return v ? strdup(v) : NULL;
}

static void restore_env(const char *name, char *old)
{
   if (old)
      assert(platform_setenv(name, old) == 0);
   else
      assert(platform_unsetenv(name) == 0);
   free(old);
}

static void test_snapshot_defaults(void)
{
   shutdown_ctx_t ctx;
   time_t started = time(NULL) - 7;
   shutdown_forensics_snapshot(&ctx, "server", SIGTERM, NULL, started, 2, 3, 4);
   assert(strcmp(ctx.daemon, "server") == 0);
   assert(strcmp(ctx.signal_name, "SIGTERM") == 0);
   assert(ctx.signal_number == SIGTERM);
   assert(ctx.daemon_pid > 0);
   assert(ctx.uptime_s >= 0);
   assert(ctx.inflight_turns == 2);
   assert(ctx.inflight_jobs == 3);
   assert(ctx.inflight_workers == 4);
}

static void test_record_round_trip_and_rotation(void)
{
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-test-forensics-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);

   char *old_dir = saved_env("AIMEE_FORENSICS_DIR");
   assert(platform_setenv("AIMEE_FORENSICS_DIR", tmpdir) == 0);

   shutdown_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   snprintf(ctx.daemon, sizeof(ctx.daemon), "server");
   snprintf(ctx.signal_name, sizeof(ctx.signal_name), "SIGTERM");
   snprintf(ctx.sender_comm, sizeof(ctx.sender_comm), "tester");
   ctx.signal_number = SIGTERM;
   ctx.daemon_pid = 100;
   ctx.sender_pid = getpid();
   ctx.at = 1700000000;
   ctx.uptime_s = 12;
   ctx.inflight_turns = 1;
   ctx.inflight_jobs = 2;
   ctx.inflight_workers = 3;
   ctx.rss_kb = 456;
   assert(shutdown_forensics_write_record(&ctx) == 0);

   shutdown_ctx_t rows[4];
   int count = shutdown_forensics_list_recent(rows, 4);
   assert(count == 1);
   assert(strcmp(rows[0].daemon, "server") == 0);
   assert(strcmp(rows[0].signal_name, "SIGTERM") == 0);
   assert(strcmp(rows[0].sender_comm, "tester") == 0);
   assert(rows[0].inflight_jobs == 2);

   ctx.at = 1700000100;
   ctx.daemon_pid = 101;
   assert(shutdown_forensics_write_record(&ctx) == 0);
   ctx.at = 1700000200;
   ctx.daemon_pid = 102;
   assert(shutdown_forensics_write_record(&ctx) == 0);
   assert(shutdown_forensics_rotate(2) == 0);
   count = shutdown_forensics_list_recent(rows, 4);
   assert(count == 2);
   assert(rows[0].daemon_pid == 102);
   assert(rows[1].daemon_pid == 101);

   char line[256];
   shutdown_forensics_format_summary(&rows[0], line, sizeof(line));
   assert(strstr(line, "server") != NULL);
   assert(strstr(line, "SIGTERM") != NULL);

   restore_env("AIMEE_FORENSICS_DIR", old_dir);
   platform_test_rmrf(tmpdir);
}

static void test_async_diagnostic_and_unclean_marker(void)
{
   char tmpdir[512];
   snprintf(tmpdir, sizeof(tmpdir), "%s/aimee-test-forensics-XXXXXX", platform_tmpdir());
   assert(platform_mkdtemp(tmpdir) != NULL);

   char *old_dir = saved_env("AIMEE_FORENSICS_DIR");
   assert(platform_setenv("AIMEE_FORENSICS_DIR", tmpdir) == 0);

   shutdown_ctx_t ctx;
   memset(&ctx, 0, sizeof(ctx));
   snprintf(ctx.daemon, sizeof(ctx.daemon), "gateway");
   snprintf(ctx.signal_name, sizeof(ctx.signal_name), "SIGTERM");
   ctx.signal_number = SIGTERM;
   ctx.daemon_pid = 300;
   ctx.sender_pid = getpid();
   ctx.at = 1700000300;
   assert(shutdown_forensics_write_record(&ctx) == 0);
   assert(shutdown_forensics_spawn_async_diagnostic(&ctx) == 0);

   shutdown_ctx_t rows[4];
   int saw_tree = 0;
   for (int i = 0; i < 50; i++)
   {
      int count = shutdown_forensics_list_recent(rows, 4);
      assert(count >= 1);
      if (rows[0].process_tree[0])
      {
         saw_tree = 1;
         break;
      }
      usleep(20000);
   }
   assert(saw_tree);

   char marker[512];
   snprintf(marker, sizeof(marker), "%s/running-server-99999999.json", tmpdir);
   FILE *f = fopen(marker, "w");
   assert(f != NULL);
   fprintf(f, "{\n"
              "  \"at\": 1700000400,\n"
              "  \"daemon\": \"server\",\n"
              "  \"daemon_pid\": 99999999\n"
              "}\n");
   fclose(f);

   assert(shutdown_forensics_record_unclean_exits() == 1);
   int count = shutdown_forensics_list_recent(rows, 4);
   assert(count >= 1);
   assert(rows[0].unclean_exit == 1);
   assert(strcmp(rows[0].signal_name, "UNCLEAN") == 0);
   shutdown_forensics_format_summary(&rows[0], marker, sizeof(marker));
   assert(strstr(marker, "UNCLEAN") != NULL);

   restore_env("AIMEE_FORENSICS_DIR", old_dir);
   platform_test_rmrf(tmpdir);
}

int main(void)
{
   test_snapshot_defaults();
   test_record_round_trip_and_rotation();
   test_async_diagnostic_and_unclean_marker();
   printf("shutdown_forensics: all tests passed\n");
   return 0;
}
