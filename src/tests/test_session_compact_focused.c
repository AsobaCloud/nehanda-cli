/* test_session_compact_focused.c: verify session_compact_focused() produces
 * a summary with all 11 named sections (AC6 acceptance test).
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "session_compact.h"

/* All 11 section headers the output must contain. */
static const char *REQUIRED_SECTIONS[] = {
    "## Active Task",       "## Goal",           "## Constraints",
    "## Completed Actions", "## Active State",   "## In Progress",
    "## Blocked",           "## Key Decisions",  "## Resolved Questions",
    "## Pending User Asks", "## Relevant Files", NULL,
};

int main(void)
{
   printf("session_compact_focused: ");

   /* ---------------------------------------------------------------
    * 1. NULL inputs rejected
    * ------------------------------------------------------------- */
   {
      char buf[64];
      assert(session_compact_focused(NULL, buf, sizeof(buf)) == -1);
      assert(session_compact_focused("topic", NULL, 64) == -1);
      assert(session_compact_focused("topic", buf, 0) == -1);
      printf("1");
   }

   /* ---------------------------------------------------------------
    * 2. All 11 sections present in the output
    * ------------------------------------------------------------- */
   {
      char summary[SESSION_COMPACT_SUMMARY_MAX];
      int rc = session_compact_focused("Implement the plugin loader", summary, sizeof(summary));
      assert(rc == 0);
      assert(summary[0] != '\0');

      for (int i = 0; REQUIRED_SECTIONS[i] != NULL; i++)
      {
         if (strstr(summary, REQUIRED_SECTIONS[i]) == NULL)
         {
            fprintf(stderr, "\nMISSING section: %s\n", REQUIRED_SECTIONS[i]);
            assert(0);
         }
      }
      printf("2");
   }

   /* ---------------------------------------------------------------
    * 3. [CONTEXT COMPACTION — REFERENCE ONLY] prefix present
    * ------------------------------------------------------------- */
   {
      char summary[SESSION_COMPACT_SUMMARY_MAX];
      assert(session_compact_focused("Test focus topic", summary, sizeof(summary)) == 0);
      assert(strstr(summary, "[CONTEXT COMPACTION") != NULL);
      printf("3");
   }

   /* ---------------------------------------------------------------
    * 4. Focus topic appears in ## Active Task section
    * ------------------------------------------------------------- */
   {
      char summary[SESSION_COMPACT_SUMMARY_MAX];
      assert(session_compact_focused("my-unique-focus-topic-xyz", summary, sizeof(summary)) == 0);
      assert(strstr(summary, "my-unique-focus-topic-xyz") != NULL);

      /* The topic must appear AFTER ## Active Task, not only somewhere else */
      const char *task_section = strstr(summary, "## Active Task");
      assert(task_section != NULL);
      assert(strstr(task_section, "my-unique-focus-topic-xyz") != NULL);
      printf("4");
   }

   /* ---------------------------------------------------------------
    * 5. All 11 sections appear in the correct order
    * ------------------------------------------------------------- */
   {
      char summary[SESSION_COMPACT_SUMMARY_MAX];
      assert(session_compact_focused("ordering test", summary, sizeof(summary)) == 0);

      const char *prev_pos = summary;
      for (int i = 0; REQUIRED_SECTIONS[i] != NULL; i++)
      {
         const char *pos = strstr(prev_pos, REQUIRED_SECTIONS[i]);
         if (pos == NULL)
         {
            fprintf(stderr, "\nSection out of order or missing: %s\n", REQUIRED_SECTIONS[i]);
            assert(0);
         }
         prev_pos = pos + 1;
      }
      printf("5");
   }

   /* ---------------------------------------------------------------
    * 6. Different topics produce different Active Task content
    * ------------------------------------------------------------- */
   {
      char s1[SESSION_COMPACT_SUMMARY_MAX], s2[SESSION_COMPACT_SUMMARY_MAX];
      assert(session_compact_focused("topic-alpha", s1, sizeof(s1)) == 0);
      assert(session_compact_focused("topic-beta", s2, sizeof(s2)) == 0);
      assert(strstr(s1, "topic-alpha") != NULL);
      assert(strstr(s2, "topic-beta") != NULL);
      assert(strstr(s1, "topic-beta") == NULL);
      assert(strstr(s2, "topic-alpha") == NULL);
      printf("6");
   }

   printf(" OK\n");
   return 0;
}
