/* events.c: Windows-specific notification delivery */
#include "aimee.h"
#include <stdio.h>

void platform_events_deliver_detached(const char *cmd)
{
   /* On Windows, fire and forget via cmd.exe */
   char buf[4096];
   snprintf(buf, sizeof(buf), "cmd.exe /c start \"\" /B %s", cmd);
   system(buf);
}
