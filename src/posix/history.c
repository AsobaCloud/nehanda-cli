/* history.c: POSIX terminal line editor for history_readline() */
#include "history_impl.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#define LINE_MAX_LEN 8192

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Redraw the current line: CR, print prompt+buf, clear EOL, reposition. */
static void redraw(const char *prompt, const char *buf, int len, int pos)
{
   /* CR to column 0, print prompt + buffer, clear to EOL */
   fprintf(stdout, "\r%s%.*s\x1b[K", prompt, len, buf);
   /* Reposition cursor: go back to after prompt+pos */
   if (len > pos)
      fprintf(stdout, "\x1b[%dD", len - pos);
   fflush(stdout);
}

/* Check whether bytes are available on stdin within timeout_us microseconds.
 * Returns 1 if bytes are ready, 0 on timeout, -1 on error. */
static int stdin_ready(int timeout_us)
{
   fd_set rds;
   struct timeval tv;
   FD_ZERO(&rds);
   FD_SET(STDIN_FILENO, &rds);
   tv.tv_sec = 0;
   tv.tv_usec = timeout_us;
   return select(STDIN_FILENO + 1, &rds, NULL, NULL, &tv);
}

/* ------------------------------------------------------------------ */
/* Tab completion                                                       */
/* ------------------------------------------------------------------ */

/* Given the current buffer and cursor position, attempt tab completion.
 * Only fires when the line starts with '/'.  Cycles through candidates
 * on successive Tab presses.  Updates buf/len/pos in place.
 * Returns 1 if the display should be redrawn, 0 otherwise. */
static int do_tab_complete(chat_history_t *h, char *buf, int *len, int *pos)
{
   if (!h || h->completion_count == 0)
      return 0;

   /* Only complete when line starts with '/' */
   if (buf[0] != '/')
      return 0;

   /* The prefix being typed is buf[1..pos-1] (after the leading '/'). */
   int prefix_len = *pos - 1;
   if (prefix_len < 0)
      prefix_len = 0;
   char prefix[256];
   if (prefix_len >= (int)sizeof(prefix))
      prefix_len = (int)sizeof(prefix) - 1;
   memcpy(prefix, buf + 1, (size_t)prefix_len);
   prefix[prefix_len] = '\0';

   /* If the prefix changed since the last Tab press, reset cycling. */
   if (strcmp(prefix, h->completion_prefix) != 0)
   {
      snprintf(h->completion_prefix, sizeof(h->completion_prefix), "%s", prefix);
      h->completion_idx = -1;
   }

   /* Count candidates. */
   int match_count = 0;
   for (int i = 0; i < h->completion_count; i++)
   {
      if (strncmp(h->completions[i], prefix, (size_t)prefix_len) == 0)
         match_count++;
   }
   if (match_count == 0)
      return 0;

   /* Advance cycling index to the next match. */
   int start_idx = (h->completion_idx < 0) ? 0 : (h->completion_idx + 1);
   int chosen = -1;
   for (int i = 0; i < h->completion_count; i++)
   {
      int idx = (start_idx + i) % h->completion_count;
      if (strncmp(h->completions[idx], prefix, (size_t)prefix_len) == 0)
      {
         chosen = idx;
         break;
      }
   }
   if (chosen < 0)
      return 0;

   h->completion_idx = chosen;

   /* Replace buf with "/" + chosen word. */
   const char *word = h->completions[chosen];
   int word_len = (int)strlen(word);
   if (1 + word_len >= LINE_MAX_LEN)
      word_len = LINE_MAX_LEN - 2;
   buf[0] = '/';
   memcpy(buf + 1, word, (size_t)word_len);
   *len = 1 + word_len;
   *pos = *len;
   buf[*len] = '\0';
   return 1;
}

/* ------------------------------------------------------------------ */
/* Word motion helpers (for vim mode)                                  */
/* ------------------------------------------------------------------ */

/* Returns position of the start of the next word (moving right). */
static int word_fwd(const char *buf, int len, int pos)
{
   while (pos < len && !isspace((unsigned char)buf[pos]))
      pos++;
   while (pos < len && isspace((unsigned char)buf[pos]))
      pos++;
   return pos;
}

/* Returns position of the start of the previous word (moving left). */
static int word_bwd(const char *buf, int pos)
{
   if (pos <= 0)
      return 0;
   pos--;
   while (pos > 0 && isspace((unsigned char)buf[pos]))
      pos--;
   while (pos > 0 && !isspace((unsigned char)buf[pos - 1]))
      pos--;
   return pos;
}

/* ------------------------------------------------------------------ */
/* history_readline                                                     */
/* ------------------------------------------------------------------ */

char *history_readline(chat_history_t *h, const char *prompt)
{
   static char buf[LINE_MAX_LEN];

   if (!isatty(STDIN_FILENO))
   {
      /* Non-interactive: use simple fgets */
      fprintf(stdout, "%s", prompt);
      fflush(stdout);
      if (!fgets(buf, sizeof(buf), stdin))
         return NULL;
      size_t n = strlen(buf);
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
         buf[--n] = '\0';
      return n > 0 ? buf : NULL;
   }

   struct termios orig, raw;
   if (tcgetattr(STDIN_FILENO, &orig) < 0)
   {
      /* Can't get terminal attrs, fall back */
      fprintf(stdout, "%s", prompt);
      fflush(stdout);
      if (!fgets(buf, sizeof(buf), stdin))
         return NULL;
      size_t n = strlen(buf);
      while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
         buf[--n] = '\0';
      return n > 0 ? buf : NULL;
   }

   raw = orig;
   raw.c_iflag &= ~(unsigned)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
   raw.c_oflag &= ~(unsigned)(OPOST);
   raw.c_cflag |= (unsigned)(CS8);
   raw.c_lflag &= ~(unsigned)(ECHO | ICANON | IEXTEN | ISIG);
   raw.c_cc[VMIN] = 1;
   raw.c_cc[VTIME] = 0;
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

   char *saved_txt = NULL; /* user's in-progress line before navigating */

   int len = 0; /* current line length */
   int pos = 0; /* cursor position     */
   buf[0] = '\0';

   /* Reset tab-completion cycling for this readline call. */
   if (h)
   {
      h->completion_idx = -1;
      h->completion_prefix[0] = '\0';
   }

   /* Vim mode: 0 = insert (default), 1 = normal */
   int vim_normal = 0;
   int vim_enabled = h ? h->vim_mode_enabled : 0;

   /* Print initial prompt */
   fprintf(stdout, "%s", prompt);
   fflush(stdout);

   for (;;)
   {
      unsigned char c;
      if (read(STDIN_FILENO, &c, 1) != 1)
      {
         /* EOF or error */
         tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
         free(saved_txt);
         return NULL;
      }

      if (c == '\r' || c == '\n')
      {
         /* Submit */
         buf[len] = '\0';
         fprintf(stdout, "\r\n");
         fflush(stdout);
         tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
         free(saved_txt);
         if (h)
            history_reset_nav(h);
         return len > 0 ? buf : NULL;
      }

      if (c == 3)
      {
         /* Ctrl-C: cancel line, restore terminal */
         fprintf(stdout, "\r\n");
         fflush(stdout);
         tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
         free(saved_txt);
         if (h)
            history_reset_nav(h);
         buf[0] = '\0';
         return NULL;
      }

      if (c == 4)
      {
         /* Ctrl-D: EOF if line is empty, else delete char at cursor (insert mode) */
         if (len == 0)
         {
            fprintf(stdout, "\r\n");
            fflush(stdout);
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
            free(saved_txt);
            return NULL;
         }
         if (!vim_normal && pos < len)
         {
            memmove(buf + pos, buf + pos + 1, (size_t)(len - pos - 1));
            len--;
            buf[len] = '\0';
            redraw(prompt, buf, len, pos);
         }
         continue;
      }

      /* Tab: cycle slash-command completions (insert mode only) */
      if (c == '\t')
      {
         if (!vim_normal && do_tab_complete(h, buf, &len, &pos))
            redraw(prompt, buf, len, pos);
         continue;
      }

      if ((c == 127 || c == 8) && !vim_normal)
      {
         /* Backspace / DEL (insert mode) */
         if (h)
         {
            h->completion_idx = -1;
            h->completion_prefix[0] = '\0';
         }
         if (pos > 0)
         {
            memmove(buf + pos - 1, buf + pos, (size_t)(len - pos));
            pos--;
            len--;
            buf[len] = '\0';
            redraw(prompt, buf, len, pos);
         }
         continue;
      }

      if (c == 1 && !vim_normal)
      {
         /* Ctrl-A: go to start */
         pos = 0;
         redraw(prompt, buf, len, pos);
         continue;
      }

      if (c == 5 && !vim_normal)
      {
         /* Ctrl-E: go to end */
         pos = len;
         redraw(prompt, buf, len, pos);
         continue;
      }

      if (c == 11 && !vim_normal)
      {
         /* Ctrl-K: kill to end of line */
         len = pos;
         buf[len] = '\0';
         redraw(prompt, buf, len, pos);
         continue;
      }

      if (c == 21 && !vim_normal)
      {
         /* Ctrl-U: kill whole line */
         len = pos = 0;
         buf[0] = '\0';
         if (h)
         {
            h->completion_idx = -1;
            h->completion_prefix[0] = '\0';
         }
         redraw(prompt, buf, len, pos);
         continue;
      }

      if (c == 27)
      {
         /* Use select() with a short timeout to distinguish plain Esc
          * (vim normal-mode switch) from Esc+[ ANSI escape sequences. */
         int ready = stdin_ready(100000); /* 100 ms */

         if (ready <= 0)
         {
            /* Plain Esc — no follow-up bytes within the timeout. */
            if (vim_enabled && !vim_normal)
            {
               vim_normal = 1;
               /* Vim convention: cursor moves back one on entering normal mode. */
               if (pos > 0 && pos == len)
                  pos--;
               redraw(prompt, buf, len, pos);
            }
            continue;
         }

         /* Follow-up bytes ready — read the escape sequence. */
         unsigned char seq[3];
         if (read(STDIN_FILENO, &seq[0], 1) != 1)
            continue;

         if (seq[0] != '[')
         {
            /* Not a CSI sequence; treat as plain Esc (Alt+key discarded). */
            if (vim_enabled && !vim_normal)
            {
               vim_normal = 1;
               if (pos > 0 && pos == len)
                  pos--;
               redraw(prompt, buf, len, pos);
            }
            continue;
         }

         if (read(STDIN_FILENO, &seq[1], 1) != 1)
            continue;

         if (seq[1] == 'A')
         {
            /* Up arrow: history prev (both modes) */
            if (!h)
               continue;
            if (history_at_tip(h) && saved_txt == NULL)
            {
               buf[len] = '\0';
               saved_txt = strdup(buf);
            }
            const char *prev = history_prev(h);
            if (prev)
            {
               len = pos = (int)strlen(prev);
               if (len >= LINE_MAX_LEN)
                  len = pos = LINE_MAX_LEN - 1;
               memcpy(buf, prev, (size_t)len);
               buf[len] = '\0';
               redraw(prompt, buf, len, pos);
            }
         }
         else if (seq[1] == 'B')
         {
            /* Down arrow: history next (both modes) */
            if (!h)
               continue;
            const char *next = history_next(h);
            if (next)
            {
               len = pos = (int)strlen(next);
               if (len >= LINE_MAX_LEN)
                  len = pos = LINE_MAX_LEN - 1;
               memcpy(buf, next, (size_t)len);
               buf[len] = '\0';
            }
            else
            {
               const char *restore = saved_txt ? saved_txt : "";
               len = pos = (int)strlen(restore);
               if (len >= LINE_MAX_LEN)
                  len = pos = LINE_MAX_LEN - 1;
               memcpy(buf, restore, (size_t)len);
               buf[len] = '\0';
               history_reset_nav(h);
               free(saved_txt);
               saved_txt = NULL;
            }
            redraw(prompt, buf, len, pos);
         }
         else if (seq[1] == 'C')
         {
            /* Right arrow (both modes) */
            if (pos < len)
            {
               pos++;
               redraw(prompt, buf, len, pos);
            }
         }
         else if (seq[1] == 'D')
         {
            /* Left arrow (both modes) */
            if (pos > 0)
            {
               pos--;
               redraw(prompt, buf, len, pos);
            }
         }
         else if (seq[1] == '3')
         {
            /* Delete key: ESC [ 3 ~ (insert mode) */
            unsigned char tilde;
            if (read(STDIN_FILENO, &tilde, 1) == 1 && tilde == '~')
            {
               if (!vim_normal && pos < len)
               {
                  memmove(buf + pos, buf + pos + 1, (size_t)(len - pos - 1));
                  len--;
                  buf[len] = '\0';
                  redraw(prompt, buf, len, pos);
               }
            }
         }
         continue;
      }

      /* ----------------------------------------------------------------
       * Vim normal mode commands
       * ---------------------------------------------------------------- */
      if (vim_normal)
      {
         switch (c)
         {
         case 'h': /* move left */
            if (pos > 0)
            {
               pos--;
               redraw(prompt, buf, len, pos);
            }
            break;

         case 'l': /* move right (stop before end in normal mode) */
            if (pos < len - 1 || (len <= 1 && pos < len))
            {
               pos++;
               redraw(prompt, buf, len, pos);
            }
            break;

         case 'k': /* up — history prev */
            if (h)
            {
               if (history_at_tip(h) && saved_txt == NULL)
               {
                  buf[len] = '\0';
                  saved_txt = strdup(buf);
               }
               const char *prev = history_prev(h);
               if (prev)
               {
                  len = (int)strlen(prev);
                  if (len >= LINE_MAX_LEN)
                     len = LINE_MAX_LEN - 1;
                  pos = len > 0 ? len - 1 : 0;
                  memcpy(buf, prev, (size_t)len);
                  buf[len] = '\0';
                  redraw(prompt, buf, len, pos);
               }
            }
            break;

         case 'j': /* down — history next */
            if (h)
            {
               const char *next = history_next(h);
               if (next)
               {
                  len = (int)strlen(next);
                  if (len >= LINE_MAX_LEN)
                     len = LINE_MAX_LEN - 1;
                  pos = len > 0 ? len - 1 : 0;
                  memcpy(buf, next, (size_t)len);
                  buf[len] = '\0';
               }
               else
               {
                  const char *restore = saved_txt ? saved_txt : "";
                  len = (int)strlen(restore);
                  if (len >= LINE_MAX_LEN)
                     len = LINE_MAX_LEN - 1;
                  pos = len > 0 ? len - 1 : 0;
                  memcpy(buf, restore, (size_t)len);
                  buf[len] = '\0';
                  history_reset_nav(h);
                  free(saved_txt);
                  saved_txt = NULL;
               }
               redraw(prompt, buf, len, pos);
            }
            break;

         case '0': /* beginning of line */
            pos = 0;
            redraw(prompt, buf, len, pos);
            break;

         case '$': /* end of line (on last char, not past it) */
            pos = len > 0 ? len - 1 : 0;
            redraw(prompt, buf, len, pos);
            break;

         case 'w': /* word forward */
         {
            int np = word_fwd(buf, len, pos);
            if (np >= len && len > 0)
               np = len - 1;
            if (np != pos)
            {
               pos = np;
               redraw(prompt, buf, len, pos);
            }
            break;
         }

         case 'b': /* word backward */
         {
            int np = word_bwd(buf, pos);
            if (np != pos)
            {
               pos = np;
               redraw(prompt, buf, len, pos);
            }
            break;
         }

         case 'x': /* delete char at cursor */
            if (len > 0 && pos < len)
            {
               memmove(buf + pos, buf + pos + 1, (size_t)(len - pos - 1));
               len--;
               buf[len] = '\0';
               if (pos >= len && pos > 0)
                  pos--;
               redraw(prompt, buf, len, pos);
            }
            break;

         case 'd': /* 'dd': delete whole line — wait briefly for second 'd' */
         {
            int ready2 = stdin_ready(500000); /* 500 ms */
            if (ready2 > 0)
            {
               unsigned char c2;
               if (read(STDIN_FILENO, &c2, 1) == 1 && c2 == 'd')
               {
                  len = pos = 0;
                  buf[0] = '\0';
                  redraw(prompt, buf, len, pos);
               }
            }
            break;
         }

         case 'i': /* enter insert mode at cursor */
            vim_normal = 0;
            break;

         case 'a': /* enter insert mode after cursor */
            vim_normal = 0;
            if (pos < len)
            {
               pos++;
               redraw(prompt, buf, len, pos);
            }
            break;

         case 'A': /* enter insert mode at end */
            vim_normal = 0;
            pos = len;
            redraw(prompt, buf, len, pos);
            break;

         default:
            break;
         }
         continue;
      }

      /* Printable character: insert at cursor (insert mode) */
      if (isprint((unsigned char)c) && len < LINE_MAX_LEN - 1)
      {
         /* Non-Tab edit resets tab-completion cycling. */
         if (h)
         {
            h->completion_idx = -1;
            h->completion_prefix[0] = '\0';
         }
         if (pos < len)
            memmove(buf + pos + 1, buf + pos, (size_t)(len - pos));
         buf[pos++] = (char)c;
         len++;
         buf[len] = '\0';
         redraw(prompt, buf, len, pos);
      }
   }

   /* unreachable */
   tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
   free(saved_txt);
   return NULL;
}
