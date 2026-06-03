/* cmd_describe.c: POSIX parallel fork-based implementation of platform_describe_run_parallel. */
#include "aimee.h"
#include "cmd_describe_platform.h"
#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

/* Wait for one child to finish. Sets *finished_idx to the index in pids[].
 * Returns the child's exit status, or -1 on error. */
static int reap_one(pid_t *pids, int active, int *finished_idx)
{
   int status = 0;
   pid_t done = waitpid(-1, &status, 0);
   if (done <= 0)
      return -1;

   for (int i = 0; i < active; i++)
   {
      if (pids[i] == done)
      {
         *finished_idx = i;
         return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
      }
   }
   *finished_idx = -1;
   return -1;
}

void platform_describe_run_parallel(describe_job_t *jobs, int job_count, int max_parallel,
                                    int do_style, int attempt, int attempts, int *described,
                                    int *failed)
{
   /* Fork up to max_parallel children at a time */
   pid_t active_pids[DESCRIBE_MAX_PARALLEL_CAP];
   int active_jobs[DESCRIBE_MAX_PARALLEL_CAP]; /* index into jobs[] */
   int active = 0;
   int next_job = 0;

   while (next_job < job_count || active > 0)
   {
      /* Launch children up to the concurrency cap */
      while (active < max_parallel && next_job < job_count)
      {
         pid_t pid = fork();
         if (pid < 0)
         {
            fprintf(stderr, "describe: fork failed: %s\n", strerror(errno));
            (*failed)++;
            next_job++;
            continue;
         }

         if (pid == 0)
         {
            /* Child: describe or style-analyze one project and exit */
            int rc;
            if (do_style)
               rc = style_one(jobs[next_job].name, jobs[next_job].path);
            else
               rc = describe_one(jobs[next_job].name, jobs[next_job].path);
            _exit(rc == 0 ? 0 : 1);
         }

         /* Parent: track the child */
         active_pids[active] = pid;
         active_jobs[active] = next_job;
         active++;
         next_job++;
      }

      /* Wait for one child to finish */
      if (active > 0)
      {
         int finished_idx = -1;
         int exit_status = reap_one(active_pids, active, &finished_idx);

         if (finished_idx >= 0)
         {
            int job_idx = active_jobs[finished_idx];
            if (exit_status == 0)
            {
               (*described)++;
            }
            else
            {
               /* Only count as failed on last attempt */
               if (attempt == attempts - 1)
                  (*failed)++;
            }

            (void)job_idx;

            /* Remove from active list by shifting */
            for (int i = finished_idx; i < active - 1; i++)
            {
               active_pids[i] = active_pids[i + 1];
               active_jobs[i] = active_jobs[i + 1];
            }
            active--;
         }
      }
   }
}
