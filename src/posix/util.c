/* util.c: POSIX-specific utilities (process exec, regex, pclose status) */
#include "aimee.h"
#include <regex.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

int safe_exec_capture_env(const char *const argv[], char *const envp[], char **out_buf,
                          size_t max_out)
{
   *out_buf = NULL;
   if (!argv || !argv[0])
      return -1;

   int pipefd[2];
   if (pipe(pipefd) != 0)
      return -1;

   pid_t pid = fork();
   if (pid < 0)
   {
      close(pipefd[0]);
      close(pipefd[1]);
      return -1;
   }

   if (pid == 0)
   {
      /* Child: redirect stdout to pipe, close stdin/stderr */
      close(pipefd[0]);
      dup2(pipefd[1], STDOUT_FILENO);
      dup2(pipefd[1], STDERR_FILENO);
      close(pipefd[1]);
      /* Apply the explicit env in the child only (portable across glibc/BSD —
       * no execvpe). NULL leaves the inherited environment in place. */
      if (envp)
      {
         extern char **environ;
         environ = (char **)envp;
      }
      execvp(argv[0], (char *const *)argv);
      _exit(127);
   }

   /* Parent */
   close(pipefd[1]);

   char *buf = malloc(max_out + 1);
   if (!buf)
   {
      close(pipefd[0]);
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      return -1;
   }

   size_t total = 0;
   while (total < max_out)
   {
      ssize_t n = read(pipefd[0], buf + total, max_out - total);
      if (n <= 0)
         break;
      total += (size_t)n;
   }
   close(pipefd[0]);
   buf[total] = '\0';

   int status = 0;
   waitpid(pid, &status, 0);

   *out_buf = buf;
   return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int safe_exec_capture(const char *const argv[], char **out_buf, size_t max_out)
{
   return safe_exec_capture_env(argv, NULL, out_buf, max_out);
}

int regex_match(const char *pattern, const char *text, int flags)
{
   if (!pattern || !text)
      return 0;
   regex_t re;
   if (regcomp(&re, pattern, flags | REG_NOSUB) != 0)
      return 0;
   int matched = (regexec(&re, text, 0, NULL, 0) == 0);
   regfree(&re);
   return matched;
}
