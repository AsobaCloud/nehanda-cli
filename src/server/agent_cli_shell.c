/* agent_cli_shell.c: execute provider CLI shell drivers as one-shot text adapters. */
#include "aimee.h"
#include "util.h"
#include "agent.h"
#include "agent_shell.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SHELL_CLI_MAX_ARGS 32
#define SHELL_CLI_POLL_MS  250

typedef struct
{
   char *text;
   size_t len;
   size_t cap;
   int tool_calls;
   char error[256];
} shell_collect_t;

static int shell_collect_append(shell_collect_t *c, const char *s)
{
   if (!c || !s || !s[0])
      return 0;
   size_t n = strlen(s);
   if (c->len + n + 1 > c->cap)
   {
      size_t next = c->cap ? c->cap : 4096;
      while (next < c->len + n + 1)
         next *= 2;
      char *grown = realloc(c->text, next);
      if (!grown)
         return -1;
      c->text = grown;
      c->cap = next;
   }
   memcpy(c->text + c->len, s, n);
   c->len += n;
   c->text[c->len] = '\0';
   return 0;
}

static void shell_collect_cb(agent_shell_event_t event, const char *data, void *user)
{
   shell_collect_t *c = (shell_collect_t *)user;
   if (!c)
      return;
   if (event == SHELL_EVENT_TEXT_DELTA)
   {
      if (shell_collect_append(c, data ? data : "") != 0 && !c->error[0])
         snprintf(c->error, sizeof(c->error), "out of memory collecting CLI output");
   }
   else if (event == SHELL_EVENT_TOOL_START)
      c->tool_calls++;
   else if (event == SHELL_EVENT_ERROR && data && data[0] && !c->error[0])
      snprintf(c->error, sizeof(c->error), "%s", data);
}

static int shell_split_cli_cmd(const char *cli_cmd, char **argv, int max_args, char *errbuf,
                               size_t errbuf_len)
{
   int count = shlex_split(cli_cmd, argv, max_args);
   if (count <= 0)
   {
      snprintf(errbuf, errbuf_len, "empty provider CLI command");
      return -1;
   }
   if (count >= max_args)
   {
      util_free_tokens(argv, count);
      snprintf(errbuf, errbuf_len, "provider CLI command has too many arguments");
      return -1;
   }
   for (int i = 0; i < count; i++)
   {
      if (util_token_is_shell_operator(argv[i]))
      {
         util_free_tokens(argv, count);
         snprintf(errbuf, errbuf_len,
                  "provider CLI command may not contain shell operators; configure an argv-style "
                  "command instead");
         return -1;
      }
   }
   argv[count] = NULL;
   return count;
}

static void shell_terminate_child(pid_t pid)
{
   if (pid <= 0)
      return;
   kill(-pid, SIGTERM);
   kill(pid, SIGTERM);
   for (int i = 0; i < 20; i++)
   {
      int status = 0;
      pid_t r = waitpid(pid, &status, WNOHANG);
      if (r == pid || (r < 0 && errno == ECHILD))
         return;
      struct timespec ts = {0, 50 * 1000000L};
      nanosleep(&ts, NULL);
   }
   kill(-pid, SIGKILL);
   kill(pid, SIGKILL);
   (void)waitpid(pid, NULL, 0);
}

static void shell_close_fd(int *fd)
{
   if (fd && *fd >= 0)
   {
      close(*fd);
      *fd = -1;
   }
}

static int shell_set_nonblock(int fd)
{
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags < 0)
      return -1;
   if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
      return -1;
   return 0;
}

static int shell_collect_process(const char *cli_cmd, const char *display_name, const char *prompt,
                                 int timeout_ms, agent_result_t *out)
{
   char *argv[SHELL_CLI_MAX_ARGS + 1] = {0};
   int argc =
       shell_split_cli_cmd(cli_cmd, argv, SHELL_CLI_MAX_ARGS, out->error, sizeof(out->error));
   if (argc < 0)
      return -1;

   int in_pipe[2] = {-1, -1};
   int out_pipe[2] = {-1, -1};
   if (pipe(in_pipe) < 0)
   {
      util_free_tokens(argv, argc);
      snprintf(out->error, sizeof(out->error), "failed to start %s", display_name);
      return -1;
   }
   if (pipe(out_pipe) < 0)
   {
      util_free_tokens(argv, argc);
      close(in_pipe[0]);
      close(in_pipe[1]);
      snprintf(out->error, sizeof(out->error), "failed to start %s", display_name);
      return -1;
   }

   pid_t pid = fork();
   if (pid < 0)
   {
      util_free_tokens(argv, argc);
      close(in_pipe[0]);
      close(in_pipe[1]);
      close(out_pipe[0]);
      close(out_pipe[1]);
      snprintf(out->error, sizeof(out->error), "failed to start %s", display_name);
      return -1;
   }

   if (pid == 0)
   {
      setsid();
      close(in_pipe[1]);
      close(out_pipe[0]);
      dup2(in_pipe[0], STDIN_FILENO);
      dup2(out_pipe[1], STDOUT_FILENO);
      dup2(out_pipe[1], STDERR_FILENO);
      close(in_pipe[0]);
      close(out_pipe[1]);
      execvp(argv[0], argv);
      _exit(127);
   }
   util_free_tokens(argv, argc);

   close(in_pipe[0]);
   close(out_pipe[1]);

   struct sigaction old_pipe;
   struct sigaction ignore_pipe;
   memset(&ignore_pipe, 0, sizeof(ignore_pipe));
   ignore_pipe.sa_handler = SIG_IGN;
   sigemptyset(&ignore_pipe.sa_mask);
   int restore_pipe = sigaction(SIGPIPE, &ignore_pipe, &old_pipe) == 0;

   shell_collect_t collected = {0};
   if (shell_set_nonblock(in_pipe[1]) != 0 || shell_set_nonblock(out_pipe[0]) != 0)
   {
      if (restore_pipe)
         sigaction(SIGPIPE, &old_pipe, NULL);
      shell_close_fd(&in_pipe[1]);
      shell_close_fd(&out_pipe[0]);
      shell_terminate_child(pid);
      snprintf(out->error, sizeof(out->error), "failed to configure pipes for %s", display_name);
      return -1;
   }

   const size_t prompt_len = strlen(prompt);
   size_t prompt_off = 0;
   int input_closed = 0;
   int output_closed = 0;
   int child_reaped = 0;
   int status = 0;
   int timed_out = 0;
   int write_rc = 0;
   long long deadline = timeout_ms > 0 ? util_now_ms() + timeout_ms : 0;

   for (;;)
   {
      while (!output_closed)
      {
         char chunk[4096 + 1];
         ssize_t r = read(out_pipe[0], chunk, sizeof(chunk) - 1);
         if (r > 0)
         {
            chunk[r] = '\0';
            if (shell_collect_append(&collected, chunk) != 0 && !collected.error[0])
               snprintf(collected.error, sizeof(collected.error),
                        "out of memory collecting CLI output");
            continue;
         }
         if (r == 0)
         {
            output_closed = 1;
            shell_close_fd(&out_pipe[0]);
            break;
         }
         if (r < 0 && errno == EINTR)
            continue;
         if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

         snprintf(collected.error, sizeof(collected.error), "failed to read output from %s",
                  display_name);
         output_closed = 1;
         shell_close_fd(&out_pipe[0]);
         break;
      }

      while (!input_closed && write_rc == 0 && prompt_off < prompt_len)
      {
         ssize_t n = write(in_pipe[1], prompt + prompt_off, prompt_len - prompt_off);
         if (n > 0)
         {
            prompt_off += (size_t)n;
            continue;
         }
         if (n < 0 && errno == EINTR)
            continue;
         if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;

         write_rc = -1;
         break;
      }
      if (!input_closed && (prompt_off >= prompt_len || write_rc != 0))
      {
         shell_close_fd(&in_pipe[1]);
         input_closed = 1;
      }

      if (!child_reaped)
      {
         pid_t wr = waitpid(pid, &status, WNOHANG);
         if (wr == pid)
            child_reaped = 1;
         else if (wr < 0 && errno == ECHILD)
            child_reaped = 1;
      }
      if (output_closed && child_reaped)
         break;

      int wait_ms = SHELL_CLI_POLL_MS;
      if (deadline > 0)
      {
         long long now = util_now_ms();
         if (now >= deadline)
         {
            timed_out = 1;
            break;
         }
         long long remaining = deadline - now;
         if (remaining < SHELL_CLI_POLL_MS)
            wait_ms = (int)remaining;
      }

      struct pollfd pfds[2];
      nfds_t nfds = 0;
      if (!output_closed)
      {
         pfds[nfds].fd = out_pipe[0];
         pfds[nfds].events = POLLIN | POLLHUP | POLLERR;
         pfds[nfds].revents = 0;
         nfds++;
      }
      if (!input_closed && write_rc == 0)
      {
         pfds[nfds].fd = in_pipe[1];
         pfds[nfds].events = POLLOUT | POLLHUP | POLLERR;
         pfds[nfds].revents = 0;
         nfds++;
      }
      if (nfds == 0)
      {
         struct timespec ts = {0, (long)wait_ms * 1000000L};
         nanosleep(&ts, NULL);
         continue;
      }
      while (poll(pfds, nfds, wait_ms) < 0 && errno == EINTR)
         ;
   }
   if (restore_pipe)
      sigaction(SIGPIPE, &old_pipe, NULL);
   shell_close_fd(&in_pipe[1]);
   shell_close_fd(&out_pipe[0]);
   if (!timed_out && write_rc == 0 && prompt_off < prompt_len)
      write_rc = -1;

   if (timed_out)
   {
      shell_terminate_child(pid);
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "%s timed out after %d ms", display_name,
               timeout_ms);
      return -1;
   }
   if (!child_reaped)
   {
      while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
         ;
   }

   if (write_rc != 0)
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "failed to send prompt to %s", display_name);
      return -1;
   }
   if (collected.error[0])
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "%s", collected.error);
      return -1;
   }
   if (WIFSIGNALED(status))
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "%s exited after signal %d", display_name,
               WTERMSIG(status));
      return -1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
   {
      if (collected.text && collected.text[0])
         snprintf(out->error, sizeof(out->error), "%s exited with status %d: %.320s", display_name,
                  WEXITSTATUS(status), collected.text);
      else
         snprintf(out->error, sizeof(out->error), "%s exited with status %d", display_name,
                  WEXITSTATUS(status));
      free(collected.text);
      return -1;
   }
   if (!collected.text || !collected.text[0])
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "empty response from %s", display_name);
      return -1;
   }

   out->response = collected.text;
   out->success = 1;
   out->turns = 1;
   return 0;
}

int agent_execute_cli_shell_driver(const agent_t *agent, const char *driver_name,
                                   const char *display_name, const char *default_cli_cmd,
                                   const char *system_prompt, const char *user_prompt,
                                   agent_result_t *out)
{
   if (!user_prompt || !user_prompt[0])
   {
      snprintf(out->error, sizeof(out->error), "empty prompt");
      return -1;
   }

   size_t plen = (system_prompt ? strlen(system_prompt) : 0) + strlen(user_prompt) + 4;
   char *full_prompt = malloc(plen);
   if (!full_prompt)
   {
      snprintf(out->error, sizeof(out->error), "out of memory");
      return -1;
   }
   if (system_prompt && system_prompt[0])
      snprintf(full_prompt, plen, "%s\n\n%s", system_prompt, user_prompt);
   else
      snprintf(full_prompt, plen, "%s", user_prompt);

   const char *cli_cmd = (agent && agent->cli_cmd[0]) ? agent->cli_cmd : default_cli_cmd;
   if (cli_cmd && cli_cmd[0])
   {
      int timeout_ms = (agent && agent->cli_idle_timeout_ms > 0)
                           ? agent->cli_idle_timeout_ms
                           : ((agent && agent->timeout_ms > 0) ? agent->timeout_ms : -1);
      int rc = shell_collect_process(cli_cmd, display_name, full_prompt, timeout_ms, out);
      free(full_prompt);
      return rc;
   }

   const agent_shell_driver_t *driver = agent_shell_driver_get(driver_name);
   if (!driver)
   {
      free(full_prompt);
      snprintf(out->error, sizeof(out->error), "%s shell driver is not registered", display_name);
      return -1;
   }

   void *handle = driver->open(driver, NULL);
   if (!handle)
   {
      free(full_prompt);
      snprintf(out->error, sizeof(out->error), "failed to start %s", display_name);
      return -1;
   }

   if (driver->send(handle, full_prompt) != 0)
   {
      free(full_prompt);
      driver->close(handle);
      snprintf(out->error, sizeof(out->error), "failed to send prompt to %s", display_name);
      return -1;
   }
   free(full_prompt);

   shell_collect_t collected = {0};
   volatile int interrupted = 0;
   int rc = driver->recv(handle, shell_collect_cb, &collected, &interrupted);
   driver->close(handle);
   if (rc != 0)
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "%s failed while reading output", display_name);
      return -1;
   }
   if (collected.error[0])
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "%s", collected.error);
      return -1;
   }
   if (!collected.text || !collected.text[0])
   {
      free(collected.text);
      snprintf(out->error, sizeof(out->error), "empty response from %s", display_name);
      return -1;
   }

   out->response = collected.text;
   out->success = 1;
   out->turns = 1;
   out->tool_calls = collected.tool_calls;
   return 0;
}
