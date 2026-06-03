int git_repo_root(const char *dir, char *out_root, size_t out_len)
{
   (void)dir;
   if (g_git_repo_root_rc == 0 && out_root && out_len > 0)
   {
      snprintf(out_root, out_len, "%s", g_git_repo_root_value);
      return 0;
   }
   if (out_root && out_len > 0)
      out_root[0] = '\0';
   return -1;
}
int worktree_sibling_path(const char *git_root, const char *sid, const char *work_name, char *out,
                          size_t out_len)
{
   (void)git_root;
   (void)sid;
   (void)work_name;
   if (g_worktree_sibling_path_rc == 0 && out && out_len > 0)
   {
      snprintf(out, out_len, "%s", g_worktree_sibling_path_value);
      return 0;
   }
   if (out && out_len > 0)
      out[0] = '\0';
   return -1;
}
int worktree_delegate_work_name(const char *sid, char *out, size_t cap)
{
   if (!sid || !out || cap < 9)
      return -1;
   snprintf(out, cap, "deadbeef"); /* stub: deterministic fixed name */
   return 0;
}
int is_aimee_worktree_path(const char *path)
{
   (void)path;
   return 0;
}
int worktree_managed_git_root(const char *path, char *out, size_t out_len)
{
   (void)path;
   if (out && out_len > 0)
      out[0] = '\0';
   return -1;
}
int kb_client_index_code_search(const char *query, const char *project, code_search_hit_t *out,
                                int max)
{
   (void)query;
   (void)project;
   (void)out;
   (void)max;
   return 0;
}

int kb_client_index_find(const char *identifier, term_hit_t *out, int max)
{
   (void)identifier;
   (void)out;
   (void)max;
   return 0;
}
int worktree_create_sibling(const char *git_root, const char *sid, const char *work_name)
{
   (void)git_root;
   (void)sid;
   (void)work_name;
   g_worktree_create_calls++;
   return g_worktree_create_rc;
}

int worktree_create_sibling_from_anchor(const char *git_root, const char *sid,
                                        const char *work_name, const char *anchor_dir)
{
   (void)git_root;
   (void)sid;
   (void)work_name;
   (void)anchor_dir;
   g_worktree_create_calls++;
   return g_worktree_create_rc;
}

int worktree_create_sibling_on_branch(const char *git_root, const char *sid, const char *work_name,
                                      const char *branch, const char *anchor_dir)
{
   (void)git_root;
   (void)sid;
   (void)work_name;
   (void)branch;
   (void)anchor_dir;
   g_worktree_create_calls++;
   return g_worktree_create_rc;
}

void worktree_cleanup(const char *git_root, const char *sid, const char *work_name)
{
   (void)git_root;
   (void)sid;
   (void)work_name;
}

int worktree_apply_changes_to_parent(const char *src_wt, const char *dst_wt)
{
   g_worktree_apply_calls++;
   snprintf(g_last_apply_src, sizeof(g_last_apply_src), "%s", src_wt ? src_wt : "");
   snprintf(g_last_apply_dst, sizeof(g_last_apply_dst), "%s", dst_wt ? dst_wt : "");
   return 1;
}

int worktree_apply_delegate_changes_to_parent(const char *delegate_wt, const char *parent_wt,
                                              char *err, size_t err_len)
{
   g_delegate_apply_calls++;
   snprintf(g_last_apply_src, sizeof(g_last_apply_src), "%s", delegate_wt ? delegate_wt : "");
   snprintf(g_last_apply_dst, sizeof(g_last_apply_dst), "%s", parent_wt ? parent_wt : "");
   if (g_delegate_apply_rc < 0 && err && err_len > 0)
      snprintf(err, err_len, "stub delegate apply failed");
   return g_delegate_apply_rc;
}
int worktree_apply_delegate_changes_checked(const char *delegate_wt, const char *parent_hint,
                                            const char *launch_head, int *applied,
                                            char *parent_root, size_t parent_root_len, char *err,
                                            size_t err_len)
{
   (void)launch_head;
   char parent[MAX_PATH_LEN] = "";
   if (parent_hint && git_repo_root(parent_hint, parent, sizeof(parent)) != 0)
      snprintf(parent, sizeof(parent), "%s", parent_hint);
   int n = worktree_apply_delegate_changes_to_parent(delegate_wt, parent, err, err_len);
   if (applied)
      *applied = n;
   if (parent_root && parent_root_len > 0)
      snprintf(parent_root, parent_root_len, "%s", parent);
   return n < 0 ? -1 : 0;
}
