/* posix/agent_shell_gemini.c: one-shot text adapter for the Gemini CLI.
 * recv() forwards stdout lines until EOF and emits SHELL_EVENT_TURN_COMPLETE. */
#include "agent_shell.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define GEMINI_LINE_MAX 4096

typedef struct
{
   int in_fd;
   int out_fd;
   pid_t pid;
   FILE *fp;
} gemini_handle_t;

static void *gemini_open(const agent_shell_driver_t *d, const char *resume_id)
{
   (void)d;
   (void)resume_id;

   int in_pipe[2] = {-1, -1};
   int out_pipe[2] = {-1, -1};

   if (pipe(in_pipe) < 0)
      return NULL;
   if (pipe(out_pipe) < 0)
   {
      close(in_pipe[0]);
      close(in_pipe[1]);
      return NULL;
   }

   pid_t pid = fork();
   if (pid < 0)
   {
      close(in_pipe[0]);
      close(in_pipe[1]);
      close(out_pipe[0]);
      close(out_pipe[1]);
      return NULL;
   }

   if (pid == 0)
   {
      close(in_pipe[1]);
      close(out_pipe[0]);
      dup2(in_pipe[0], STDIN_FILENO);
      dup2(out_pipe[1], STDOUT_FILENO);
      close(in_pipe[0]);
      close(out_pipe[1]);
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0)
      {
         dup2(devnull, STDERR_FILENO);
         close(devnull);
      }
      execl("/bin/sh", "sh", "-c", "gemini", (char *)NULL);
      _exit(127);
   }

   close(in_pipe[0]);
   close(out_pipe[1]);

   gemini_handle_t *h = calloc(1, sizeof(*h));
   if (!h)
   {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      close(in_pipe[1]);
      close(out_pipe[0]);
      return NULL;
   }

   h->in_fd = in_pipe[1];
   h->out_fd = out_pipe[0];
   h->pid = pid;
   h->fp = NULL;
   return h;
}

static int gemini_send(void *handle, const char *message)
{
   gemini_handle_t *h = (gemini_handle_t *)handle;
   if (!h || !message)
      return -1;

   size_t msg_len = strlen(message);
   size_t written = 0;
   while (written < msg_len)
   {
      ssize_t w = write(h->in_fd, message + written, msg_len - written);
      if (w <= 0)
         return -1;
      written += (size_t)w;
   }
   close(h->in_fd);
   h->in_fd = -1;
   return 0;
}

static int gemini_recv(void *handle, agent_shell_cb_t cb, void *user, volatile int *interrupted)
{
   gemini_handle_t *h = (gemini_handle_t *)handle;
   if (!h)
      return -1;

   if (!h->fp)
   {
      h->fp = fdopen(h->out_fd, "r");
      if (!h->fp)
         return -1;
      h->out_fd = -1;
   }

   char line[GEMINI_LINE_MAX];
   while (fgets(line, sizeof(line), h->fp))
   {
      if (interrupted && *interrupted)
      {
         kill(h->pid, SIGTERM);
         break;
      }
      if (line[0])
         cb(SHELL_EVENT_TEXT_DELTA, line, user);
   }

   cb(SHELL_EVENT_TURN_COMPLETE, NULL, user);
   return 0;
}

static void gemini_close(void *handle)
{
   gemini_handle_t *h = (gemini_handle_t *)handle;
   if (!h)
      return;
   if (h->in_fd >= 0)
      close(h->in_fd);
   if (h->fp)
      fclose(h->fp);
   else if (h->out_fd >= 0)
      close(h->out_fd);
   if (h->pid > 0)
      waitpid(h->pid, NULL, 0);
   free(h);
}

agent_shell_driver_t gemini_shell_driver = {
    .name = "gemini",
    .open = gemini_open,
    .send = gemini_send,
    .recv = gemini_recv,
    .close = gemini_close,
};
