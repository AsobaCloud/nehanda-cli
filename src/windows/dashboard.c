/* dashboard.c: dashboard stub (not supported on Windows) */
#include "dashboard.h"
#include <stdio.h>

void dashboard_serve(int port)
{
   (void)port;
   fprintf(stderr, "dashboard: not supported on this platform\n");
}
