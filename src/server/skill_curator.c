#include "aimee.h"
#include "aimee_home.h"
#include "config.h"
#include "db1_internal.h"
#include "db1/interaction_events.h"
#include "platform_process.h"
#include "log.h"
#include "skill_curator.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char DELEGATE_PROMPT[] =
    "Weekly skill curator pass. List skills with `aimee skill list --json` "
    "and identify prefix clusters for consolidation. For each cluster propose "
    "a skill_consolidation artifact via skill_manage if merging or creating an "
    "umbrella would reduce complexity. Rules: pinned skills are immutable, "
    "never delete (archive only), created_by=user skills are immutable. "
    "Nothing to consolidate is a real outcome.";

static int read_state_time(const char *path, time_t *out)
{
   FILE *f = fopen(path, "r");
   if (!f)
   {
      return -1;
   }
   char buf[64];
   if (!fgets(buf, sizeof(buf), f))
   {
      fclose(f);
      return -1;
   }
   fclose(f);

   struct tm tm;
   memset(&tm, 0, sizeof(tm));
   int n = sscanf(buf, "%d-%d-%dT%d:%d:%dZ", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour,
                  &tm.tm_min, &tm.tm_sec);
   if (n != 6)
   {
      return -1;
   }
   tm.tm_year -= 1900;
   tm.tm_mon -= 1;
   tm.tm_isdst = -1;
   *out = timegm(&tm);
   return 0;
}

static int write_state_time(const char *path)
{
   char tmp[512];
   snprintf(tmp, sizeof(tmp), "%s.tmp", path);
   FILE *f = fopen(tmp, "w");
   if (!f)
   {
      return -1;
   }
   time_t now = time(NULL);
   struct tm *utc = gmtime(&now);
   char buf[64];
   strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", utc);
   fprintf(f, "%s\n", buf);
   fclose(f);
   return rename(tmp, path);
}

int skill_curator_nudge(const config_t *cfg)
{
   if (!cfg || !cfg->skills_manage_enabled)
   {
      return 0;
   }

   const char *home = aimee_home();
   if (!home || !home[0])
   {
      return 0;
   }

   char state_path[512];
   snprintf(state_path, sizeof(state_path), "%s/skills/.curator_state", home);

   int curator_interval_hours =
       cfg->skills_curator_interval_hours > 0 ? cfg->skills_curator_interval_hours : 168;

   time_t last_run = 0;
   if (read_state_time(state_path, &last_run) == 0)
   {
      if (difftime(time(NULL), last_run) < (double)curator_interval_hours * 3600.0)
      {
         return 0;
      }
   }

   sqlite3 *db = db1_conn();
   if (!db)
   {
      return -1;
   }

   int min_idle = cfg->skills_min_idle_minutes > 0 ? cfg->skills_min_idle_minutes : 30;
   char query[256];
   snprintf(query, sizeof(query),
            "SELECT COUNT(*) FROM interaction_events "
            "WHERE event_type = 'user_turn' "
            "AND created_at >= datetime('now', '-%d minutes')",
            min_idle);

   sqlite3_stmt *stmt = NULL;
   int rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
   if (rc != SQLITE_OK)
   {
      return -1;
   }
   int recent_turns = 0;
   if (sqlite3_step(stmt) == SQLITE_ROW)
   {
      recent_turns = sqlite3_column_int(stmt, 0);
   }
   sqlite3_finalize(stmt);

   if (recent_turns > 0)
   {
      return 0;
   }

   if (write_state_time(state_path) != 0)
   {
      aimee_log(LOG_WARN, "skill.curator", "failed to write curator state file");
   }

   char exe[512];
   if (platform_get_exe_path(exe, sizeof(exe)) != 0)
   {
      aimee_log(LOG_ERROR, "skill.curator", "failed to resolve exe path");
      return -1;
   }

   const char *argv[] = {exe, "delegate", "review", "--background", DELEGATE_PROMPT, NULL};
   pid_t pid = platform_spawn_daemon(argv);

   if (pid < 0)
   {
      aimee_log(LOG_ERROR, "skill.curator", "failed to spawn curator delegate");
      return -1;
   }

   aimee_log(LOG_INFO, "skill.curator", "launched curator delegate pid=%d", pid);
   return 1;
}
