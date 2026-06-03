/* history.c: Windows readline stub (simple fgets, no raw terminal) */
#include "history.h"
#include <io.h>
#include <stdio.h>
#include <string.h>

#define isatty       _isatty
#define STDIN_FILENO 0
#define LINE_MAX_LEN 8192

char *history_readline(chat_history_t *h, const char *prompt)
{
   static char buf[LINE_MAX_LEN];
   (void)h;

   fprintf(stdout, "%s", prompt);
   fflush(stdout);
   if (!fgets(buf, sizeof(buf), stdin))
      return NULL;
   size_t n = strlen(buf);
   while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
      buf[--n] = '\0';
   return n > 0 ? buf : NULL;
}
