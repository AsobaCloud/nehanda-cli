/* events.c: POSIX-specific notification delivery (fork/exec) */
#include "aimee.h"
#include "log.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

void platform_events_deliver_detached(const char *cmd)
{
   /* Double-fork so the notification command is reparented under init.
    * The intermediate child exits immediately; the caller's waitpid()
    * returns without delay and no zombie is left behind.  This avoids
    * the racy process-wide signal(SIGCHLD, SIG_IGN) that the single-fork
    * variant required. */
   pid_t pid = fork();
   if (pid < 0)
   {
      aimee_log(LOG_ERROR, "notify", "fork failed: %s", strerror(errno));
      return;
   }

   if (pid > 0)
   {
      /* Parent: reap intermediate child immediately (it exits at once). */
      waitpid(pid, NULL, 0);
      return;
   }

   /* Intermediate child: fork actual worker, then exit. */
   pid_t worker = fork();
   if (worker < 0)
      _exit(1);
   if (worker > 0)
      _exit(0); /* exits → actual worker reparented to init */

   /* Actual worker */
   setsid();

   {
      int devnull = open("/dev/null", O_RDWR);
      if (devnull >= 0)
      {
         dup2(devnull, STDIN_FILENO);
         dup2(devnull, STDOUT_FILENO);
         dup2(devnull, STDERR_FILENO);
         if (devnull > STDERR_FILENO)
            close(devnull);
      }
   }

   execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
   _exit(127);
}
