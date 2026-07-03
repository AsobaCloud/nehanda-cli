/* cli_tui_misc.c: split from cli_tui.c into a real translation unit
 * (was cli_tui_misc.inc, textually included only to stay under the
 * line-check ceiling). Cross-TU declarations live in the module header. */
#include "aimee_home.h"
#include "cli_client.h"
#include "cli_agent_keys.h"
#include "cli_tui.h"
#include "aimee_client.h"
#include "history.h"
#include "markdown.h"
#include "platform.h"
#include "platform_path.h"
#include "platform_process.h"
#include "session_compact.h"
#include "cJSON.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>

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
