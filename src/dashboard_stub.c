/* dashboard_stub.c: dashboard_serve stub for non-POSIX/non-Windows builds */
#include "dashboard.h"
#include <stdio.h>

void dashboard_serve(int port)
{
   (void)port;
   fprintf(stderr, "aimee: dashboard UI not available on this platform\n");
}
