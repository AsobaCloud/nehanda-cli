/* posix/agent_shell_claude.c: agent_shell_driver_t implementation for the Claude CLI. */
#include "agent_shell.h"
#include "cJSON.h"
#include "util.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CLAUDE_LINE_MAX (256 * 1024)

static const char *resolve_claude_bin(char *buf, size_t buf_len)
{
   struct stat st;
   char exe[1024];
   ssize_t exe_len = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
   if (exe_len > 0)
   {
      exe[exe_len] = '\0';
      char *slash = strrchr(exe, '/');
      if (slash)
      {
         snprintf(buf, buf_len, "%.*s/claude", (int)(slash - exe), exe);
         if (stat(buf, &st) == 0 && (st.st_mode & S_IXUSR))
            return buf;
      }
   }
   const char *home = getenv("HOME");
   if (home && home[0])
   {
      snprintf(buf, buf_len, "%s/.local/bin/claude", home);
      if (stat(buf, &st) == 0 && (st.st_mode & S_IXUSR))
         return buf;
   }
   return "claude";
}

typedef struct
{
   int in_fd;
   int out_fd;
   pid_t pid;
   FILE *fp;
   char *line_buf;
} claude_handle_t;

static void *claude_open(const agent_shell_driver_t *d, const char *resume_id)
{
   (void)d;

   char cmd[1024];
   if (resume_id && resume_id[0])
   {
      char safe_sid[256];
      snprintf(safe_sid, sizeof(safe_sid), "%s", resume_id);
      sanitize_shell_token(safe_sid);
      snprintf(cmd, sizeof(cmd),
               "claude -p --output-format stream-json --verbose "
               "--include-partial-messages --resume '%s'",
               safe_sid);
   }
   else
   {
      snprintf(cmd, sizeof(cmd),
               "claude -p --output-format stream-json --verbose "
               "--include-partial-messages");
   }

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
      char claude_bin_buf[1024];
      const char *claude_bin = resolve_claude_bin(claude_bin_buf, sizeof(claude_bin_buf));
      char cmd_abs[1280];
      /* cmd always starts with "claude"; replace with absolute path */
      snprintf(cmd_abs, sizeof(cmd_abs), "%s%s", claude_bin, cmd + strlen("claude"));
      execl("/bin/sh", "sh", "-c", cmd_abs, (char *)NULL);
      _exit(127);
   }

   close(in_pipe[0]);
   close(out_pipe[1]);

   claude_handle_t *h = calloc(1, sizeof(*h));
   if (!h)
   {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      close(in_pipe[1]);
      close(out_pipe[0]);
      return NULL;
   }

   h->line_buf = malloc(CLAUDE_LINE_MAX);
   if (!h->line_buf)
   {
      free(h);
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

static int claude_send(void *handle, const char *message)
{
   claude_handle_t *h = (claude_handle_t *)handle;
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

static int claude_recv(void *handle, agent_shell_cb_t cb, void *user, volatile int *interrupted)
{
   claude_handle_t *h = (claude_handle_t *)handle;
   if (!h)
      return -1;

   if (!h->fp)
   {
      h->fp = fdopen(h->out_fd, "r");
      if (!h->fp)
         return -1;
      h->out_fd = -1; /* fp now owns it */
   }

   while (fgets(h->line_buf, CLAUDE_LINE_MAX, h->fp))
   {
      if (interrupted && *interrupted)
      {
         kill(h->pid, SIGTERM);
         break;
      }

      cJSON *obj = cJSON_Parse(h->line_buf);
      if (!obj)
         continue;

      cJSON *type_j = cJSON_GetObjectItem(obj, "type");
      const char *type = (type_j && cJSON_IsString(type_j)) ? type_j->valuestring : "";

      if (strcmp(type, "stream_event") == 0)
      {
         cJSON *event = cJSON_GetObjectItem(obj, "event");
         if (event)
         {
            cJSON *etype = cJSON_GetObjectItem(event, "type");
            const char *et = (etype && cJSON_IsString(etype)) ? etype->valuestring : "";

            if (strcmp(et, "content_block_delta") == 0)
            {
               cJSON *delta = cJSON_GetObjectItem(event, "delta");
               cJSON *dt = delta ? cJSON_GetObjectItem(delta, "type") : NULL;
               const char *dts = (dt && cJSON_IsString(dt)) ? dt->valuestring : "";

               if (strcmp(dts, "text_delta") == 0)
               {
                  cJSON *text = cJSON_GetObjectItem(delta, "text");
                  if (text && cJSON_IsString(text) && text->valuestring[0])
                     cb(SHELL_EVENT_TEXT_DELTA, text->valuestring, user);
               }
            }
            else if (strcmp(et, "content_block_start") == 0)
            {
               cJSON *cb_block = cJSON_GetObjectItem(event, "content_block");
               cJSON *cbtype = cb_block ? cJSON_GetObjectItem(cb_block, "type") : NULL;
               if (cbtype && cJSON_IsString(cbtype) && strcmp(cbtype->valuestring, "tool_use") == 0)
               {
                  cJSON *name = cJSON_GetObjectItem(cb_block, "name");
                  cb(SHELL_EVENT_TOOL_START,
                     (name && cJSON_IsString(name)) ? name->valuestring : "?", user);
               }
            }
         }
      }
      else if (strcmp(type, "result") == 0)
      {
         cJSON *sid = cJSON_GetObjectItem(obj, "session_id");
         if (sid && cJSON_IsString(sid))
         {
            char safe_sid[256];
            snprintf(safe_sid, sizeof(safe_sid), "%s", sid->valuestring);
            sanitize_shell_token(safe_sid);
            cb(SHELL_EVENT_SESSION_ID, safe_sid, user);
         }
         cJSON_Delete(obj);
         cb(SHELL_EVENT_TURN_COMPLETE, NULL, user);
         return 0;
      }

      cJSON_Delete(obj);
   }

   cb(SHELL_EVENT_TURN_COMPLETE, NULL, user);
   return 0;
}

static void claude_close(void *handle)
{
   claude_handle_t *h = (claude_handle_t *)handle;
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
   free(h->line_buf);
   free(h);
}

agent_shell_driver_t claude_shell_driver = {
    .name = "claude",
    .open = claude_open,
    .send = claude_send,
    .recv = claude_recv,
    .close = claude_close,
};
