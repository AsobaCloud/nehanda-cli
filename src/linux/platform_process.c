/* platform_process.c: executable path lookup via /proc/self/exe (Linux) */
#include "platform_process.h"
#include <unistd.h>

int platform_get_exe_path(char *buf, size_t size)
{
   if (!buf || size == 0)
      return -1;
   ssize_t len = readlink("/proc/self/exe", buf, size - 1);
   if (len <= 0)
      return -1;
   buf[len] = '\0';
   return 0;
}
