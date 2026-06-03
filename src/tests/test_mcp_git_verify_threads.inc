/* test_mcp_git_verify_threads.inc: git_verify multithreading / timeout /
 * cancellation tests split out of test_mcp_git.c to keep that .c under the
 * 2000-line hard limit. Included mid-file (same TU) so the white-box statics
 * and verify_test_setup/teardown helpers above stay in scope. */
/* --- Test git_verify runs multiple configured steps successfully --- */

static void test_git_verify_multithreaded_steps(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-mt");

   /* Write a project.yaml with multiple steps so sync verify exercises
    * aggregate step handling without using elapsed time as a test gate. */
   char fake_home[256];
   verify_test_write_yaml(tmpdir, fake_home, sizeof(fake_home),
                          "verify:\n"
                          "  enforce: true\n"
                          "  steps:\n"
                          "    - name: step-a\n"
                          "      run: echo step-a-done\n"
                          "    - name: step-b\n"
                          "      run: echo step-b-done\n");

   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);

   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 0); /* force sync for test assertions */
   cJSON *resp = handle_git_verify(NULL, args, NULL);
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   assert(strstr(text, "step-a") != NULL);
   assert(strstr(text, "step-b") != NULL);
   assert(strstr(text, "PASS") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(args);

   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}

static void test_git_verify_step_timeout_finishes(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-timeout");

   char fake_home[256];
   verify_test_write_yaml(tmpdir, fake_home, sizeof(fake_home),
                          "verify:\n"
                          "  enforce: true\n"
                          "  steps:\n"
                          "    - name: hangs\n"
                          "      run: sleep 2\n");

   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);
   assert(setenv("AIMEE_VERIFY_STEP_TIMEOUT_MS", "200", 1) == 0);

   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 0);
   cJSON *resp = handle_git_verify(NULL, args, NULL);
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   assert(strstr(text, "hangs") != NULL);
   assert(strstr(text, "FAIL") != NULL);
   assert(strstr(text, "timed out") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(args);

   unsetenv("AIMEE_VERIFY_STEP_TIMEOUT_MS");
   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}

static void marker_job(void *arg)
{
   volatile int *done = (volatile int *)arg;
   *done = 1;
}

static char *git_verify_status_text(int job_id, cJSON **resp_out)
{
   cJSON *args = cJSON_CreateObject();
   cJSON_AddStringToObject(args, "action", "status");
   cJSON_AddNumberToObject(args, "job_id", job_id);
   cJSON *resp = handle_git_verify(NULL, args, NULL);
   cJSON_Delete(args);
   *resp_out = resp;
   return get_mcp_text(resp);
}

static void test_git_verify_async_does_not_starve_server_pool(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-async-pool");

   char fake_home[256];
   verify_test_write_yaml(tmpdir, fake_home, sizeof(fake_home),
                          "verify:\n"
                          "  enforce: true\n"
                          "  steps:\n"
                          "    - name: hangs\n"
                          "      run: sleep 10\n");

   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);

   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   assert(ctx != NULL);
   assert(compute_pool_init(&ctx->pool, 1) == 0);

   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 1);
   cJSON *resp = handle_git_verify(ctx, args, "sid-async-pool");
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   int job_id = 0;
   assert(sscanf(text, "Started background verification job #%d.", &job_id) == 1);
   cJSON_Delete(resp);
   cJSON_Delete(args);

   volatile int done = 0;
   assert(compute_pool_submit(&ctx->pool, marker_job, (void *)&done) == 0);
   for (int i = 0; i < 50 && !done; i++)
      usleep(10000);
   assert(done == 1);

   int cancelled = 0;
   for (int i = 0; i < 50 && !cancelled; i++)
   {
      usleep(20000);
      cancelled = verify_cancel_session("sid-async-pool");
   }
   assert(cancelled > 0);

   for (int i = 0; i < 100; i++)
   {
      cJSON *status_resp = NULL;
      char *status = git_verify_status_text(job_id, &status_resp);
      assert(status != NULL);
      int finished = strstr(status, "finished") != NULL;
      cJSON_Delete(status_resp);
      if (finished)
         break;
      usleep(20000);
      assert(i < 99);
   }

   compute_pool_shutdown(&ctx->pool);
   free(ctx);
   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}

typedef struct
{
   const char *session_id;
   cJSON *resp;
} sync_verify_thread_t;

static void *run_sync_verify_thread(void *arg)
{
   sync_verify_thread_t *state = (sync_verify_thread_t *)arg;
   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 0);
   state->resp = handle_git_verify(NULL, args, state->session_id);
   cJSON_Delete(args);
   return NULL;
}

static void test_git_verify_sync_cancelled_by_session_close(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-cancel");

   char fake_home[256];
   verify_test_write_yaml(tmpdir, fake_home, sizeof(fake_home),
                          "verify:\n"
                          "  enforce: true\n"
                          "  steps:\n"
                          "    - name: hangs\n"
                          "      run: sleep 10\n");

   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);

   sync_verify_thread_t state = {.session_id = "sid-sync-cancel", .resp = NULL};
   pthread_t tid;
   assert(pthread_create(&tid, NULL, run_sync_verify_thread, &state) == 0);

   int cancelled = 0;
   for (int i = 0; i < 500 && !cancelled; i++)
   {
      usleep(20000);
      cancelled = verify_cancel_session(state.session_id);
   }
   assert(cancelled > 0);
   assert(pthread_join(tid, NULL) == 0);

   char *text = get_mcp_text(state.resp);
   assert(text != NULL);
   assert(strstr(text, "cancelled: owning session closed") != NULL);
   assert(strstr(text, "verification cancelled; state not recorded") != NULL);
   cJSON_Delete(state.resp);

   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}
