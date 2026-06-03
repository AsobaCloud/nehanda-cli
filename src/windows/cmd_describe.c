/* cmd_describe.c: Windows serial fallback for platform_describe_run_parallel. */
#include "aimee.h"
#include "cmd_describe_platform.h"

void platform_describe_run_parallel(describe_job_t *jobs, int job_count, int max_parallel,
                                    int do_style, int attempt, int attempts, int *described,
                                    int *failed)
{
   (void)max_parallel;
   for (int j = 0; j < job_count; j++)
   {
      int rc;
      if (do_style)
         rc = style_one(jobs[j].name, jobs[j].path);
      else
         rc = describe_one(jobs[j].name, jobs[j].path);
      if (rc == 0)
         (*described)++;
      else if (attempt == attempts - 1)
         (*failed)++;
   }
}
