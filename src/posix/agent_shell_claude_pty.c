/* posix/agent_shell_claude_pty.c: PTY-based agent_shell_driver_t for the Claude CLI.
 * Used when aimee chat runs in an interactive terminal so that tool-approval
 * prompts reach the user rather than executing silently (as they do in -p mode).
 */
#include "agent_shell.h"
#include "util.h"
#include <ctype.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define CLAUDE_LINE_MAX (256 * 1024)

static const char *resolve_claude_bin_pty(char *buf, size_t buf_len)
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
   int master_fd;
   pid_t pid;
   char *line_buf;
   FILE *fp;
} claude_pty_handle_t;

/* Strip ANSI escape sequences (ESC [ ... m and similar CSI sequences) in-place.
 * Also strips OSC sequences (ESC ] ... BEL/ST).
 * Returns pointer to dst (same as dst). */
static char *strip_ansi(char *dst, const char *src, size_t dst_size)
{
   size_t wi = 0;
   const char *p = src;
   while (*p && wi + 1 < dst_size)
   {
      if (*p == '\033')
      {
         p++;
         if (*p == '[')
         {
            /* CSI sequence: skip until a byte in 0x40–0x7E */
            p++;
            while (*p && !(*p >= 0x40 && *p <= 0x7e))
               p++;
            if (*p)
               p++; /* skip the final byte */
         }
         else if (*p == ']')
         {
            /* OSC sequence: skip until BEL or ST (ESC \) */
            p++;
            while (*p && *p != '\007' && !(*p == '\033' && *(p + 1) == '\\'))
               p++;
            if (*p == '\007')
               p++;
            else if (*p == '\033')
               p += 2;
         }
         else if (*p)
         {
            /* Two-character escape sequence (e.g. ESC M); skip both */
            p++;
         }
         continue;
      }
      dst[wi++] = *p++;
   }
   dst[wi] = '\0';
   return dst;
}

/* Detect Claude's interactive input prompt.
 * After a response, claude writes a line that is just whitespace and/or the
 * prompt indicator ">" followed by optional whitespace.  We treat such a line
 * as the end-of-turn sentinel. */
static int is_prompt_line(const char *line)
{
   const char *p = line;
   /* skip leading whitespace */
   while (*p && isspace((unsigned char)*p))
      p++;
   if (*p == '>')
   {
      p++;
      while (*p && isspace((unsigned char)*p))
         p++;
      /* Accept ">" alone, "> " alone, or ">" followed by end of line */
      return (*p == '\0' || *p == '\r' || *p == '\n');
   }
   return 0;
}

static void *claude_pty_open(const agent_shell_driver_t *d, const char *resume_id)
{
   (void)d;

   int master = -1, slave = -1;
   if (openpty(&master, &slave, NULL, NULL, NULL) < 0)
      return NULL;

   char cmd[1024];
   if (resume_id && resume_id[0])
   {
      char safe_sid[256];
      snprintf(safe_sid, sizeof(safe_sid), "%s", resume_id);
      sanitize_shell_token(safe_sid);
      snprintf(cmd, sizeof(cmd), "claude --resume '%s'", safe_sid);
   }
   else
   {
      snprintf(cmd, sizeof(cmd), "claude");
   }

   pid_t pid = fork();
   if (pid < 0)
   {
      close(master);
      close(slave);
      return NULL;
   }

   if (pid == 0)
   {
      /* Child: become session leader and attach slave as controlling terminal */
      close(master);
      setsid();
      if (ioctl(slave, TIOCSCTTY, 0) < 0)
         _exit(127);
      if (dup2(slave, STDIN_FILENO) < 0)
         _exit(127);
      if (dup2(slave, STDOUT_FILENO) < 0)
         _exit(127);
      if (dup2(slave, STDERR_FILENO) < 0)
         _exit(127);
      if (slave > STDERR_FILENO)
         close(slave);
      char claude_bin_buf[1024];
      const char *claude_bin = resolve_claude_bin_pty(claude_bin_buf, sizeof(claude_bin_buf));
      char cmd_abs[1280];
      /* cmd always starts with "claude"; replace with absolute path */
      snprintf(cmd_abs, sizeof(cmd_abs), "%s%s", claude_bin, cmd + strlen("claude"));
      execl("/bin/sh", "sh", "-c", cmd_abs, (char *)NULL);
      _exit(127);
   }

   /* Parent */
   close(slave);

   claude_pty_handle_t *h = calloc(1, sizeof(*h));
   if (!h)
   {
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      close(master);
      return NULL;
   }

   h->line_buf = malloc(CLAUDE_LINE_MAX);
   if (!h->line_buf)
   {
      free(h);
      kill(pid, SIGKILL);
      waitpid(pid, NULL, 0);
      close(master);
      return NULL;
   }

   h->master_fd = master;
   h->pid = pid;
   h->fp = NULL;
   return h;
}

static int claude_pty_send(void *handle, const char *message)
{
   claude_pty_handle_t *h = (claude_pty_handle_t *)handle;
   if (!h || !message)
      return -1;

   size_t msg_len = strlen(message);
   size_t written = 0;
   while (written < msg_len)
   {
      ssize_t w = write(h->master_fd, message + written, msg_len - written);
      if (w <= 0)
         return -1;
      written += (size_t)w;
   }
   /* Send newline to submit the message */
   if (write(h->master_fd, "\n", 1) < 0)
      return -1;
   return 0;
}

static int claude_pty_recv(void *handle, agent_shell_cb_t cb, void *user, volatile int *interrupted)
{
   claude_pty_handle_t *h = (claude_pty_handle_t *)handle;
   if (!h)
      return -1;

   if (!h->fp)
   {
      h->fp = fdopen(h->master_fd, "r");
      if (!h->fp)
         return -1;
      h->master_fd = -1; /* fp now owns it */
   }

   char *clean = malloc(CLAUDE_LINE_MAX);
   if (!clean)
      return -1;

   while (fgets(h->line_buf, CLAUDE_LINE_MAX, h->fp))
   {
      if (interrupted && *interrupted)
      {
         kill(h->pid, SIGTERM);
         break;
      }

      strip_ansi(clean, h->line_buf, CLAUDE_LINE_MAX);

      if (is_prompt_line(clean))
      {
         free(clean);
         cb(SHELL_EVENT_TURN_COMPLETE, NULL, user);
         return 0;
      }

      /* Emit non-empty lines as text deltas */
      if (clean[0] && clean[0] != '\r' && clean[0] != '\n')
         cb(SHELL_EVENT_TEXT_DELTA, clean, user);
   }

   free(clean);
   cb(SHELL_EVENT_TURN_COMPLETE, NULL, user);
   return 0;
}

static void claude_pty_close(void *handle)
{
   claude_pty_handle_t *h = (claude_pty_handle_t *)handle;
   if (!h)
      return;
   if (h->fp)
      fclose(h->fp);
   else if (h->master_fd >= 0)
      close(h->master_fd);
   if (h->pid > 0)
      waitpid(h->pid, NULL, 0);
   free(h->line_buf);
   free(h);
}

agent_shell_driver_t claude_pty_shell_driver = {
    .name = "claude-pty",
    .open = claude_pty_open,
    .send = claude_pty_send,
    .recv = claude_pty_recv,
    .close = claude_pty_close,
};
