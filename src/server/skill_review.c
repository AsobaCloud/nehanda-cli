#include "aimee.h"
#include "config.h"
#include "db1_internal.h"
#include "db1/interaction_events.h"
#include "platform_process.h"
#include "log.h"
#include "skill_review.h"

static const char DELEGATE_PROMPT[] =
    "Skill review pass. Inspect this just-finished session for two signals: "
    "(a) patches that should be applied to a skill the session activated; "
    "(b) class-level guidance worth lifting into a new skill or umbrella. "
    "Use skill_manage to propose any mutation as a skill_change artifact. "
    "Rules: names must be class-level never session artifacts (no PR numbers "
    "no codenames). Prefer patching an existing skill to creating a new one. "
    "Nothing to save is a real outcome. When patching cite the session "
    "messages that motivated the change.";

int skill_review_nudge(const config_t *cfg)
{
   if (!cfg || !cfg->skills_manage_enabled)
   {
      return 0;
   }

   const char *sid = session_id();
   if (!sid || !sid[0])
   {
      return 0;
   }

   sqlite3 *db = db1_conn();
   if (!db)
   {
      return -1;
   }

   int turn_count = 0;
   sqlite3_stmt *stmt = NULL;
   int rc = sqlite3_prepare_v2(db,
                               "SELECT COUNT(*) FROM interaction_events "
                               "WHERE session_id = ? AND event_type = 'user_turn'",
                               -1, &stmt, NULL);
   if (rc != SQLITE_OK)
   {
      return -1;
   }
   sqlite3_bind_text(stmt, 1, sid, -1, SQLITE_TRANSIENT);
   if (sqlite3_step(stmt) == SQLITE_ROW)
   {
      turn_count = sqlite3_column_int(stmt, 0);
   }
   sqlite3_finalize(stmt);

   if (turn_count <= 0)
   {
      return 0;
   }

   int nudge_interval =
       cfg->skills_review_nudge_interval > 0 ? cfg->skills_review_nudge_interval : 10;
   if (turn_count % nudge_interval != 0)
   {
      return 0;
   }

   char exe[512];
   if (platform_get_exe_path(exe, sizeof(exe)) != 0)
   {
      aimee_log(LOG_ERROR, "skill.review", "failed to resolve exe path");
      return -1;
   }

   const char *argv[] = {exe, "delegate", "review", "--background", DELEGATE_PROMPT, NULL};
   pid_t pid = platform_spawn_daemon(argv);

   if (pid < 0)
   {
      aimee_log(LOG_ERROR, "skill.review", "failed to spawn review delegate");
      return -1;
   }

   aimee_log(LOG_INFO, "skill.review", "launched review delegate pid=%d", pid);
   return 1;
}