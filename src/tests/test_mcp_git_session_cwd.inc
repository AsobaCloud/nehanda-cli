static void test_mcp_chdir_session_cwd_precedes_proxy_cwd(void)
{
   setup_git_repo();

   char proxy_repo[sizeof(g_tmpdir)];
   snprintf(proxy_repo, sizeof(proxy_repo), "%s", g_tmpdir);

   char tracked_repo[256];
   strcpy(tracked_repo, "/tmp/aimee-test-mcp-git-tracked-XXXXXX");
   assert(mkdtemp(tracked_repo) != NULL);
   char cmd[1024];
   snprintf(cmd, sizeof(cmd), "cd '%s' && git init -q", tracked_repo);
   assert(system(cmd) == 0);

   session_id_set_override("test-session-cwd");

   char cwd_path[MAX_PATH_LEN];
   snprintf(cwd_path, sizeof(cwd_path), "%s/git-cwd-%s", config_output_dir(), session_id());
   FILE *fp = fopen(cwd_path, "w");
   assert(fp != NULL);
   fputs(tracked_repo, fp);
   fclose(fp);

   assert(chdir("/tmp") == 0);

   cJSON *args = cJSON_CreateObject();
   cJSON_AddStringToObject(args, "cwd", proxy_repo);
   assert(mcp_chdir_git_root(NULL, 0, args, NULL) == 1);
   const char *resolved = run_cmd_get_cwd();
   assert(resolved != NULL);
   assert(strcmp(resolved, tracked_repo) == 0);

   run_cmd_set_cwd(NULL);
   cJSON_Delete(args);
   unlink(cwd_path);
   session_id_clear_override();

   snprintf(cmd, sizeof(cmd), "rm -rf '%s'", tracked_repo);
   system(cmd);
   teardown_git_repo();
}

static void init_nested_git_repo(const char *path, const char *label)
{
   char cmd[2048];
   snprintf(cmd, sizeof(cmd),
            "mkdir -p '%s' && git -C '%s' init -q && "
            "git -C '%s' config user.email test@test && "
            "git -C '%s' config user.name test && "
            "echo %s > '%s/file.txt' && "
            "git -C '%s' add file.txt && git -C '%s' commit -q -m %s",
            path, path, path, path, label, path, path, path, label);
   assert(system(cmd) == 0);
}

static void test_mcp_chdir_repairs_stale_delegate_tracked_cwd(void)
{
   setup_git_repo();

   char session_main[MAX_PATH_LEN];
   char stale_delegate[MAX_PATH_LEN];
   snprintf(session_main, sizeof(session_main), "%s/.aimee/worktrees/102ee97d/main", g_tmpdir);
   snprintf(stale_delegate, sizeof(stale_delegate), "%s/.aimee/worktrees/deleg-24/37368447",
            g_tmpdir);

   init_nested_git_repo(session_main, "main");
   init_nested_git_repo(stale_delegate, "stale");

   session_id_set_override("102ee97d-session");

   char cwd_path[MAX_PATH_LEN];
   snprintf(cwd_path, sizeof(cwd_path), "%s/git-cwd-%s", config_output_dir(), session_id());
   FILE *fp = fopen(cwd_path, "w");
   assert(fp != NULL);
   fputs(stale_delegate, fp);
   fclose(fp);

   assert(chdir("/tmp") == 0);

   cJSON *args = cJSON_CreateObject();
   assert(mcp_chdir_git_root(NULL, 0, args, NULL) == 1);
   const char *resolved = run_cmd_get_cwd();
   assert(resolved != NULL);
   assert(strcmp(resolved, session_main) == 0);

   run_cmd_set_cwd(NULL);
   cJSON_Delete(args);
   unlink(cwd_path);
   session_id_clear_override();
   teardown_git_repo();
}

static void test_mcp_chdir_keeps_stale_delegate_cwd_when_repair_missing(void)
{
   setup_git_repo();

   char stale_delegate[MAX_PATH_LEN];
   snprintf(stale_delegate, sizeof(stale_delegate), "%s/.aimee/worktrees/deleg-24/37368447",
            g_tmpdir);
   init_nested_git_repo(stale_delegate, "stale");

   session_id_set_override("102ee97d-session");

   char cwd_path[MAX_PATH_LEN];
   snprintf(cwd_path, sizeof(cwd_path), "%s/git-cwd-%s", config_output_dir(), session_id());
   FILE *fp = fopen(cwd_path, "w");
   assert(fp != NULL);
   fputs(stale_delegate, fp);
   fclose(fp);

   assert(chdir("/tmp") == 0);

   cJSON *args = cJSON_CreateObject();
   assert(mcp_chdir_git_root(NULL, 0, args, NULL) == 1);
   const char *resolved = run_cmd_get_cwd();
   assert(resolved != NULL);
   assert(strcmp(resolved, stale_delegate) == 0);

   run_cmd_set_cwd(NULL);
   cJSON_Delete(args);
   unlink(cwd_path);
   session_id_clear_override();
   teardown_git_repo();
}

static void test_mcp_chdir_does_not_repair_delegate_session_cwd(void)
{
   setup_git_repo();

   char session_main[MAX_PATH_LEN];
   char stale_delegate[MAX_PATH_LEN];
   snprintf(session_main, sizeof(session_main), "%s/.aimee/worktrees/deleg-abc/main", g_tmpdir);
   snprintf(stale_delegate, sizeof(stale_delegate), "%s/.aimee/worktrees/deleg-24/37368447",
            g_tmpdir);
   init_nested_git_repo(session_main, "main");
   init_nested_git_repo(stale_delegate, "stale");

   session_id_set_override("deleg-abcdef");

   char cwd_path[MAX_PATH_LEN];
   snprintf(cwd_path, sizeof(cwd_path), "%s/git-cwd-%s", config_output_dir(), session_id());
   FILE *fp = fopen(cwd_path, "w");
   assert(fp != NULL);
   fputs(stale_delegate, fp);
   fclose(fp);

   assert(chdir("/tmp") == 0);

   cJSON *args = cJSON_CreateObject();
   assert(mcp_chdir_git_root(NULL, 0, args, NULL) == 1);
   const char *resolved = run_cmd_get_cwd();
   assert(resolved != NULL);
   assert(strcmp(resolved, stale_delegate) == 0);

   run_cmd_set_cwd(NULL);
   cJSON_Delete(args);
   unlink(cwd_path);
   session_id_clear_override();
   teardown_git_repo();
}

static void test_mcp_chdir_keeps_explicit_managed_worktree_despite_stale_session_state(void)
{
   setup_git_repo();
   setup_ownership_db();

   char active_worktree[MAX_PATH_LEN];
   char stale_worktree[MAX_PATH_LEN];
   snprintf(active_worktree, sizeof(active_worktree), "%s/.aimee/worktrees/102ee97d/main",
            g_tmpdir);
   snprintf(stale_worktree, sizeof(stale_worktree), "%s/.aimee/worktrees/6ab82f0e/main", g_tmpdir);
   init_nested_git_repo(active_worktree, "active");
   init_nested_git_repo(stale_worktree, "stale");

   session_id_set_override("6ab82f0e-session");
   session_state_t state;
   memset(&state, 0, sizeof(state));
   snprintf(state.session_mode, sizeof(state.session_mode), "implement");
   snprintf(state.guardrail_mode, sizeof(state.guardrail_mode), "approve");
   state.worktree_count = 1;
   snprintf(state.worktrees[0].git_root, sizeof(state.worktrees[0].git_root), "%s", g_tmpdir);
   snprintf(state.worktrees[0].worktree_path, sizeof(state.worktrees[0].worktree_path), "%s",
            stale_worktree);
   session_state_force_save(&state, session_id());

   assert(chdir("/tmp") == 0);

   cJSON *args = cJSON_CreateObject();
   cJSON_AddStringToObject(args, "cwd", active_worktree);
   char *mismatch_err = NULL;
   assert(mcp_chdir_git_root(NULL, 0, args, &mismatch_err) == 1);
   const char *resolved = run_cmd_get_cwd();
   assert(resolved != NULL);
   assert(strcmp(resolved, active_worktree) == 0);
   assert(mismatch_err == NULL);

   run_cmd_set_cwd(NULL);
   cJSON_Delete(args);
   session_id_clear_override();
   teardown_ownership_db();
   teardown_git_repo();
}
