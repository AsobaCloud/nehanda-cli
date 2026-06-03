/* platform_random.c: /dev/urandom random bytes (POSIX) */
#include "platform_random.h"
#include <stdio.h>

int platform_random_bytes(void *buf, size_t len)
{
   FILE *f = fopen("/dev/urandom", "r");
   if (!f)
      return -1;
   size_t n = fread(buf, 1, len, f);
   fclose(f);
   return (n == len) ? 0 : -1;
}
