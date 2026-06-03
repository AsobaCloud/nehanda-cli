static void test_delegate_launch_repairs_paths_from_request_cwd(void)
{
   reset_last_response();
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);

   char cwd[MAX_PATH_LEN];
   assert(getcwd(cwd, sizeof(cwd)) != NULL);

   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "cwd", cwd);
   cJSON *plan = cJSON_AddObjectToObject(req, "plan");
   cJSON_AddStringToObject(plan, "schema", "delegate_plan_v1");
   cJSON_AddStringToObject(plan, "title", "Repair Launch Plan");
   cJSON *packets = cJSON_AddArrayToObject(plan, "packets");
   add_launch_packet(packets, "packet-repair", "Repair path", "test_server_compute.c");

   assert(handle_delegate_launch(ctx, conn, req) == 0);
   assert(g_last_response != NULL);
   int job_id = cJSON_GetObjectItem(g_last_response, "job_id")->valueint;
   assert(job_id > 0);

   db1_coord_task_t tasks[2];
   int count = db1_coord_job_list_tasks(job_id, tasks, 2);
   assert(count == 1);
   assert(strstr(tasks[0].files, "src/tests/test_server_compute.c") != NULL);

   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_delegate_launch_repairs_paths_from_request_cwd\n");
}

static void test_noop_write_delegate_fires(void)
{
   reset_last_response();
   unlink("/tmp/aimee_noop_test.c");
   int fds[2];
   assert(pipe(fds) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "I completed the task and created the function";
   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   /* Prompt includes a path with create intent so pre-flight drift passes,
    * but the fake agent never creates the file — triggers no-op detection. */
   cJSON_AddStringToObject(req, "prompt",
                           "Please create /tmp/aimee_noop_test.c to implement the add_two "
                           "function that returns the sum of two integers.");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   char buf[4096];
   ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   cJSON *resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0);
   cJSON *msg = cJSON_GetObjectItemCaseSensitive(resp, "message");
   assert(cJSON_IsString(msg));
   assert(strstr(msg->valuestring, "no owned files changed") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_noop_write_delegate_fires\n");
}
/* Diff grounding: inspection delegates must receive the evidence bundle in
 * their task prompt while reading from the parent workspace by default. */
static void assert_role_gets_evidence_bundle_with_cwd(const char *role, const char *cwd)
{
   reset_last_response();
   int fds[2];
   assert(pipe(fds) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "Validation complete — no findings";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0;
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");
   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", role);
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt",
                           "validate the changes in the current diff for correctness and coverage");
   if (cwd)
      cJSON_AddStringToObject(req, "cwd", cwd);
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   char buf[4096];
   ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   assert(strstr(g_last_agent_prompt, "Parent Worktree Diff Evidence") != NULL);
   assert(strstr(g_last_agent_prompt, "Validation Evidence Bundle") != NULL);
   assert(g_worktree_create_calls == 0);
   assert(g_worktree_apply_calls == 0);
   cJSON *resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "ok") == 0);
   cJSON *launch_path = cJSON_GetObjectItemCaseSensitive(resp, "launch_worktree_path");
   assert(cJSON_IsString(launch_path));
   if (cwd)
      assert(strcmp(launch_path->valuestring, cwd) == 0);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
}

static void assert_role_gets_evidence_bundle(const char *role)
{
   assert_role_gets_evidence_bundle_with_cwd(role, "/tmp");
}

static void test_read_only_delegate_uses_parent_workspace(void)
{
   reset_last_response();
   int fds[2];
   assert(pipe(fds) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "Read-only validation complete";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0;
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");

   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "validate");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt",
                           "Read-only. Validate the changes in the current diff for correctness.");
   cJSON_AddStringToObject(req, "cwd", "/tmp/aimee-parent-repo");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);

   char buf[4096];
   ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);

   assert(g_worktree_create_calls == 0);
   assert(g_worktree_apply_calls == 0);
   assert(strstr(g_last_agent_prompt, "Validation Evidence Bundle") != NULL);
   cJSON *resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "ok") == 0);
   cJSON *launch_path = cJSON_GetObjectItemCaseSensitive(resp, "launch_worktree_path");
   assert(cJSON_IsString(launch_path));
   assert(strcmp(launch_path->valuestring, "/tmp/aimee-parent-repo") == 0);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_read_only_delegate_uses_parent_workspace\n");
}

static void test_read_only_branch_delegate_rejected(void)
{
   reset_last_response();
   int fds[2];
   assert(pipe(fds) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "should not run";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0;
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");

   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "validate");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt",
                           "Read-only. Validate the changes in the current diff for correctness.");
   cJSON_AddStringToObject(req, "cwd", "/tmp/aimee-parent-repo");
   cJSON_AddStringToObject(req, "branch", "feature/read-only-review");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);

   char buf[4096];
   ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);

   assert(g_worktree_create_calls == 0);
   assert(g_agent_run_calls == 0);
   assert(g_agent_tool_run_calls == 0);
   cJSON *resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0);
   cJSON *msg = cJSON_GetObjectItemCaseSensitive(resp, "message");
   assert(cJSON_IsString(msg));
   assert(strstr(msg->valuestring, "read-only delegates must use the parent worktree") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_read_only_branch_delegate_rejected\n");
}

static void test_write_delegate_without_named_paths_noops(void)
{
   reset_last_response();
   int fds[2];
   assert(pipe(fds) == 0);
   server_ctx_t *ctx = calloc(1, sizeof(*ctx));
   server_conn_t *conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "implemented the requested change";
   cJSON *req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt", "Implement the requested change.");
   cJSON_AddTrueToObject(req, "tools");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   char buf[4096];
   ssize_t n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   cJSON *resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   cJSON *status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0);
   cJSON *msg = cJSON_GetObjectItemCaseSensitive(resp, "message");
   assert(cJSON_IsString(msg));
   assert(strstr(msg->valuestring, "no file changes detected") != NULL ||
          strstr(msg->valuestring, "no owned files changed") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_write_delegate_without_named_paths_noops\n");
}
static void test_inspection_roles_get_evidence_bundle(void)
{
   assert_role_gets_evidence_bundle("validate");
   assert_role_gets_evidence_bundle("review");
   assert_role_gets_evidence_bundle("diagnose");
   assert_role_gets_evidence_bundle_with_cwd("review", NULL);
   printf("  PASS: test_inspection_roles_get_evidence_bundle\n");
}
/* Write-delegate worktree modes:
 *   A. foreground (sole-writer) write delegate SHARES the parent worktree —
 *      no sibling created, no apply-back; it edits the parent's live tree;
 *   B-D. an isolated worktree is used only when forced (here via an explicit
 *      `branch`; the same isolation path serves background/parallel delegates).
 *      Create-fail refuses; create-ok applies changes back; apply-fail errors. */
static void test_write_delegate_worktree_modes(void)
{
   int fds[2];
   char buf[4096];
   ssize_t n;
   server_ctx_t *ctx;
   server_conn_t *conn;
   cJSON *req, *resp, *status, *msg;

   /* A. foreground write delegate shares the parent worktree. */
   reset_last_response();
   assert(pipe(fds) == 0);
   ctx = calloc(1, sizeof(*ctx));
   conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "implemented the requested change";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0; /* available, but a foreground delegate must not use it */
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");
   req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt", "Implement the requested change in src/example.c");
   cJSON_AddStringToObject(req, "cwd", "/tmp/aimee-parent-repo");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   assert(g_worktree_create_calls == 0); /* shared the parent — no sibling worktree */
   assert(g_delegate_apply_calls == 0);  /* no apply-back: edits land in the parent directly */
   assert(g_agent_run_calls > 0);        /* the delegate actually ran (in the parent worktree) */
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);

   /* B. branch-forced isolation, worktree creation fails -> refuse, do not run. */
   assert(pipe(fds) == 0);
   ctx = calloc(1, sizeof(*ctx));
   conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "should not run";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = -1;
   req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt", "Implement the requested change in src/example.c");
   cJSON_AddStringToObject(req, "cwd", "/tmp/aimee-parent-repo");
   cJSON_AddStringToObject(req, "branch", "feature/x");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   assert(g_agent_run_calls == 0);
   assert(g_agent_tool_run_calls == 0);
   resp = cJSON_Parse(buf);
   assert(cJSON_IsObject(resp));
   status = cJSON_GetObjectItemCaseSensitive(resp, "status");
   assert(cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0);
   msg = cJSON_GetObjectItemCaseSensitive(resp, "message");
   assert(cJSON_IsString(msg));
   assert(strstr(msg->valuestring, "refusing to run write-capable delegate") != NULL);
   cJSON_Delete(resp);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);

   /* C. branch-forced isolation, worktree OK -> isolated run + apply back. */
   assert(pipe(fds) == 0);
   ctx = calloc(1, sizeof(*ctx));
   conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "implemented the requested change";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0;
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");
   req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt", "Implement the requested delegated change.");
   cJSON_AddStringToObject(req, "cwd", "/tmp");
   cJSON_AddStringToObject(req, "branch", "feature/x");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   assert(strstr(buf, "\"status\":\"ok\"") != NULL);
   assert(g_worktree_apply_calls == 0);
   assert(g_delegate_apply_calls == 1);
   assert(strcmp(g_last_apply_src, "/tmp/aimee-delegate-wt") == 0);
   assert(strcmp(g_last_apply_dst, "/tmp/aimee-parent-repo") == 0);
   assert(strstr(buf, "\"applied_changes\":1") != NULL);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);

   /* D. branch-forced isolation, apply-back fails -> error. */
   assert(pipe(fds) == 0);
   ctx = calloc(1, sizeof(*ctx));
   conn = calloc(1, sizeof(*conn));
   assert(ctx != NULL && conn != NULL);
   conn->fd = fds[1];
   g_agent_response = "implemented the requested change";
   g_git_repo_root_rc = 0;
   snprintf(g_git_repo_root_value, sizeof(g_git_repo_root_value), "%s", "/tmp/aimee-parent-repo");
   g_worktree_create_rc = 0;
   g_worktree_sibling_path_rc = 0;
   snprintf(g_worktree_sibling_path_value, sizeof(g_worktree_sibling_path_value), "%s",
            "/tmp/aimee-delegate-wt");
   g_delegate_apply_rc = -1;
   req = cJSON_CreateObject();
   cJSON_AddStringToObject(req, "role", "code");
   cJSON_AddStringToObject(req, "persona", "engineer");
   cJSON_AddStringToObject(req, "prompt", "Implement the requested delegated change.");
   cJSON_AddStringToObject(req, "cwd", "/tmp");
   cJSON_AddStringToObject(req, "branch", "feature/x");
   assert(handle_delegate(ctx, conn, req) == 0);
   assert(g_submitted_fn == delegate_worker);
   g_submitted_fn(g_submitted_arg);
   g_submitted_arg = NULL;
   close(fds[1]);
   n = read(fds[0], buf, sizeof(buf) - 1);
   assert(n > 0);
   buf[n] = '\0';
   close(fds[0]);
   assert(strstr(buf, "\"status\":\"error\"") != NULL);
   assert(strstr(buf, "stub delegate apply failed") != NULL);
   assert(g_delegate_apply_calls == 1);
   cJSON_Delete(req);
   reset_last_response();
   free(conn);
   free(ctx);
   printf("  PASS: test_write_delegate_worktree_modes\n");
}
