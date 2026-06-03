/* platform_path.c: POSIX path and filesystem utilities */
#include "platform_path.h"
#include "aimee_home.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *platform_home_dir(void)
{
   return getenv("HOME");
}

const char *platform_config_dir(void)
{
   /* Delegate to aimee_home() so AIMEE_HOME / AIMEE_PROFILE overrides
    * apply to every caller automatically — no per-call-site changes
    * needed for callers already routing through this helper. */
   return aimee_home();
}

char platform_path_sep(void)
{
   return '/';
}

int platform_mkdir_p(const char *path, int mode)
{
   char tmp[4096];
   size_t len;
   if (!path)
      return -1;
   snprintf(tmp, sizeof(tmp), "%s", path);
   len = strlen(tmp);
   if (len == 0)
      return -1;
   if (tmp[len - 1] == '/')
      tmp[len - 1] = '\0';

   for (char *p = tmp + 1; *p; p++)
   {
      if (*p == '/')
      {
         *p = '\0';
         if (mkdir(tmp, (mode_t)mode) != 0 && errno != EEXIST)
            return -1;
         *p = '/';
      }
   }
   if (mkdir(tmp, (mode_t)mode) != 0 && errno != EEXIST)
      return -1;
   return 0;
}

int platform_unlink(const char *path)
{
   return unlink(path);
}

int platform_set_permissions(const char *path, int mode)
{
   return chmod(path, (mode_t)mode);
}

int platform_getppid(void)
{
   return (int)getppid();
}
