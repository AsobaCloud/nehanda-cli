static int start_async_verify(server_ctx_t *ctx, const char *session_id)
{
   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 1);
   cJSON *resp = handle_git_verify(ctx, args, session_id);
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   int job_id = 0;
   assert(sscanf(text, "Started background verification job #%d.", &job_id) == 1);
   cJSON_Delete(resp);
   cJSON_Delete(args);
   return job_id;
}

static void wait_async_verify_finished(int job_id)
{
   for (int i = 0; i < 200; i++)
   {
      cJSON *status_resp = NULL;
      char *status = git_verify_status_text(job_id, &status_resp);
      assert(status != NULL);
      int finished = strstr(status, "finished") != NULL;
      cJSON_Delete(status_resp);
      if (finished)
         return;
      usleep(10000);
   }
   assert(0 && "async verify did not finish");
}

static void test_git_verify_async_rejects_same_session_overlap(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-one-per-session");
   char fake_home[256];
   verify_test_write_yaml(
       tmpdir, fake_home, sizeof(fake_home),
       "verify:\n  enforce: true\n  steps:\n    - name: hangs\n      run: sleep 10\n");
   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   assert(ctx != NULL);
   int job_id = start_async_verify(ctx, "sid-one-verify");
   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 1);
   cJSON *resp = handle_git_verify(ctx, args, "sid-one-verify");
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   assert(strstr(text, "session already has a running verification") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(args);
   assert(verify_cancel_session("sid-one-verify") > 0);
   wait_async_verify_finished(job_id);
   free(ctx);
   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}

static void test_git_verify_async_reaps_finished_jobs(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-reap-jobs");
   char fake_home[256];
   verify_test_write_yaml(
       tmpdir, fake_home, sizeof(fake_home),
       "verify:\n  enforce: true\n  steps:\n    - name: quick\n      run: echo ok\n");
   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   assert(ctx != NULL);
   for (int i = 0; i < 40; i++)
      wait_async_verify_finished(start_async_verify(ctx, "sid-reap-verify"));
   free(ctx);
   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}

static void test_git_verify_sync_rejects_same_session_overlap(void)
{
   char tmpdir[256];
   verify_test_setup_repo(tmpdir, sizeof(tmpdir), "aimee-test-verify-sync-overlap");
   char started_path[512];
   snprintf(started_path, sizeof(started_path), "%s-started", tmpdir);
   char yaml[1024];
   snprintf(yaml, sizeof(yaml),
            "verify:\n  enforce: true\n  steps:\n    - name: hangs\n      run: sh -c 'touch %s; "
            "sleep 10'\n",
            started_path);
   char fake_home[256];
   verify_test_write_yaml(tmpdir, fake_home, sizeof(fake_home), yaml);
   char saved_cwd[4096];
   assert(getcwd(saved_cwd, sizeof(saved_cwd)) != NULL);
   assert(chdir(tmpdir) == 0);
   sync_verify_thread_t state = {.session_id = "sid-sync-overlap", .resp = NULL};
   pthread_t tid;
   assert(pthread_create(&tid, NULL, run_sync_verify_thread, &state) == 0);
   struct stat st;
   for (int i = 0; i < 3000 && stat(started_path, &st) != 0; i++)
      usleep(10000);
   assert(stat(started_path, &st) == 0);
   cJSON *args = cJSON_CreateObject();
   cJSON_AddBoolToObject(args, "async", 0);
   cJSON *resp = handle_git_verify(NULL, args, state.session_id);
   char *text = get_mcp_text(resp);
   assert(text != NULL);
   assert(strstr(text, "session already has a running verification") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(args);
   assert(verify_cancel_session(state.session_id) > 0);
   assert(pthread_join(tid, NULL) == 0);
   cJSON_Delete(state.resp);
   assert(chdir(saved_cwd) == 0);
   verify_test_teardown(tmpdir, fake_home);
}
