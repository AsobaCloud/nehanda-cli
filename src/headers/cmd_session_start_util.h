/* cmd_session_start_util.h: pure, dependency-light helpers extracted from
 * session_start_emit() so the parts that fail silently (hook-payload parsing,
 * worktree dedup, changelog buffer assembly) can be unit-tested in isolation.
 *
 * These are `static inline` so they add no link-time dependency to the many
 * translation units that pull in session state — matching the project's
 * jo_type_name / str_parse_iso8601 / config_issue pattern. The companion test
 * (test_session_start_util.c) links only cJSON.o. */
#ifndef DEC_CMD_SESSION_START_UTIL_H
#define DEC_CMD_SESSION_START_UTIL_H 1

#include "aimee.h"      /* MAX_PATH_LEN, severity_t, MAX_TDD_WRITES */
#include "cJSON.h"      /* cJSON_Parse + accessors */
#include "guardrails.h" /* session_state_t, worktree_mapping_t */
#include <stdio.h>
#include <string.h>

/* Parse the session-start hook payload. `*is_startup` defaults to 1 (the
 * source is "startup") and is set to 0 only when an explicit non-"startup"
 * source string is present. `client_cwd` is set to the payload's client_cwd
 * string when present, else cleared to "". Tolerates NULL/malformed input:
 * the defaults survive. Mirrors the inline block in session_start_emit. */
static inline void session_start_parse_hook(const char *hook_input, int *is_startup,
                                            char *client_cwd, size_t client_cwd_sz)
{
   if (is_startup)
      *is_startup = 1;
   if (client_cwd && client_cwd_sz)
      client_cwd[0] = '\0';
   if (!hook_input)
      return;

   cJSON *jin = cJSON_Parse(hook_input);
   if (!jin)
      return;
   cJSON *src = cJSON_GetObjectItemCaseSensitive(jin, "source");
   if (is_startup && cJSON_IsString(src) && src->valuestring[0])
      *is_startup = (strcmp(src->valuestring, "startup") == 0);
   cJSON *jcwd = cJSON_GetObjectItemCaseSensitive(jin, "client_cwd");
   if (client_cwd && client_cwd_sz && cJSON_IsString(jcwd) && jcwd->valuestring[0])
      snprintf(client_cwd, client_cwd_sz, "%s", jcwd->valuestring);
   cJSON_Delete(jin);
}

/* Return 1 if `git_root` is already registered among the session's worktree
 * mappings, else 0. Replaces three identical inline dedup loops. */
static inline int session_worktree_is_registered(const session_state_t *state, const char *git_root)
{
   if (!state || !git_root)
      return 0;
   for (int j = 0; j < state->worktree_count; j++)
      if (strcmp(state->worktrees[j].git_root, git_root) == 0)
         return 1;
   return 0;
}

/* Project name = the final path segment of a git root (text after the last
 * '/'), or the whole string when there is no '/'. Returns a pointer into
 * `git_root` (not copied); "" for NULL. Mirrors the changelog builder's inline
 * strrchr derivation. */
static inline const char *session_project_name_from_root(const char *git_root)
{
   if (!git_root)
      return "";
   const char *slash = strrchr(git_root, '/');
   return slash ? slash + 1 : git_root;
}

/* Append one project's section to the changelog buffer, advancing `*off`.
 * Emits a "## <proj> (<n> new commit[s])" header, the oneline log, and — when
 * `stat_out` is non-empty — the diffstat. `*off` advances by snprintf's return
 * exactly as the original inline code did (callers cap the buffer afterward).
 * Pass stat_out=NULL to omit the diffstat (e.g. when its git exec failed). */
static inline void session_changelog_append(char *buf, size_t buf_sz, int *off,
                                            const char *proj_name, int commit_count,
                                            const char *log_out, const char *stat_out)
{
   int n = snprintf(buf + *off, buf_sz - *off, "## %s (%d new commit%s)\n", proj_name, commit_count,
                    commit_count == 1 ? "" : "s");
   if (n > 0)
      *off += n;
   n = snprintf(buf + *off, buf_sz - *off, "%s", log_out);
   if (n > 0)
      *off += n;
   if (stat_out && stat_out[0])
   {
      n = snprintf(buf + *off, buf_sz - *off, "\n%s", stat_out);
      if (n > 0)
         *off += n;
   }
}

#endif /* DEC_CMD_SESSION_START_UTIL_H */
