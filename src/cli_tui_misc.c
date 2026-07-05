/* cli_tui_misc.c: split from cli_tui.c into a real translation unit
 * (was cli_tui_misc.inc, textually included only to stay under the
 * line-check ceiling). Cross-TU declarations live in the module header. */
#include "cli_tui.h" /* join_message_args declaration */
#include <stdlib.h>
#include <string.h>

char *join_message_args(int argc, char **argv)
{
   size_t len = 0;
   for (int i = 0; i < argc; i++)
      len += strlen(argv[i]) + 1;
   char *out = malloc(len + 1);
   if (!out)
      return NULL;
   out[0] = '\0';
   for (int i = 0; i < argc; i++)
   {
      if (i)
         strcat(out, " ");
      strcat(out, argv[i]);
   }
   return out;
}
