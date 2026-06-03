/* platform_path.c: Windows path and filesystem utilities */
#include "platform_path.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tlhelp32.h>
#include <windows.h>

const char *platform_home_dir(void)
{
   static char path[MAX_PATH];
   DWORD rc = GetEnvironmentVariableA("USERPROFILE", path, sizeof(path));
   return (rc > 0 && rc < sizeof(path)) ? path : NULL;
}

const char *platform_config_dir(void)
{
   static char path[MAX_PATH];
   DWORD rc = GetEnvironmentVariableA("APPDATA", path, sizeof(path));
   if (rc == 0 || rc >= sizeof(path))
      return NULL;
   snprintf(path + strlen(path), sizeof(path) - strlen(path), "\\aimee");
   return path;
}

char platform_path_sep(void)
{
   return '\\';
}

int platform_mkdir_p(const char *path, int mode)
{
   (void)mode;
   char tmp[MAX_PATH];
   size_t len;
   if (!path)
      return -1;
   snprintf(tmp, sizeof(tmp), "%s", path);
   len = strlen(tmp);
   if (len == 0)
      return -1;
   if (tmp[len - 1] == '\\' || tmp[len - 1] == '/')
      tmp[len - 1] = '\0';

   for (char *p = tmp + 1; *p; p++)
   {
      if (*p == '\\' || *p == '/')
      {
         char saved = *p;
         *p = '\0';
         if (strlen(tmp) > 0 && !CreateDirectoryA(tmp, NULL) &&
             GetLastError() != ERROR_ALREADY_EXISTS)
            return -1;
         *p = saved;
      }
   }
   if (!CreateDirectoryA(tmp, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
      return -1;
   return 0;
}

int platform_unlink(const char *path)
{
   return DeleteFileA(path) ? 0 : -1;
}

int platform_set_permissions(const char *path, int mode)
{
   (void)path;
   (void)mode;
   return 0;
}

int platform_getppid(void)
{
   DWORD self = GetCurrentProcessId();
   HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
   if (snap == INVALID_HANDLE_VALUE)
      return -1;

   PROCESSENTRY32 pe;
   pe.dwSize = sizeof(pe);
   if (!Process32First(snap, &pe))
   {
      CloseHandle(snap);
      return -1;
   }

   do
   {
      if (pe.th32ProcessID == self)
      {
         CloseHandle(snap);
         return (int)pe.th32ParentProcessID;
      }
   } while (Process32Next(snap, &pe));

   CloseHandle(snap);
   return -1;
}
