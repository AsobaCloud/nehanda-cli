/* platform_process.c: executable path lookup via _NSGetExecutablePath (macOS) */
#include "platform_process.h"
#include <mach-o/dyld.h>
#include <stdint.h>

int platform_get_exe_path(char *buf, size_t size)
{
   if (!buf || size == 0)
      return -1;
   uint32_t sz = (uint32_t)size;
   if (_NSGetExecutablePath(buf, &sz) != 0)
      return -1;
   return 0;
}
