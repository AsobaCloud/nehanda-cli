/* platform_ipc.c: named-pipe IPC (Windows) */
#include "platform_ipc.h"
#include <errno.h>
#include <sddl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WCHAR *path_to_pipe_name(const char *path)
{
   static WCHAR pipe_name[256];
   char username[128] = "user";
   DWORD user_len = GetEnvironmentVariableA("USERNAME", username, sizeof(username));
   if (user_len == 0 || user_len >= sizeof(username))
      snprintf(username, sizeof(username), "user");

   const char *base = path ? strrchr(path, '/') : NULL;
   if (!base)
      base = path ? strrchr(path, '\\') : NULL;
   base = base ? base + 1 : (path ? path : "aimee");

   char sanitized[128];
   size_t si = 0;
   for (size_t i = 0; base[i] && si + 1 < sizeof(sanitized); i++)
   {
      char c = base[i];
      if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
          c == '_')
         sanitized[si++] = c;
      else
         sanitized[si++] = '-';
   }
   if (si == 0)
      sanitized[si++] = 'a';
   sanitized[si] = '\0';

   swprintf(pipe_name, sizeof(pipe_name) / sizeof(pipe_name[0]), L"\\\\.\\pipe\\%S-%S", sanitized,
            username);
   return pipe_name;
}

static void pipe_name_a(const char *path, char *buf, size_t buf_sz)
{
   WCHAR *wname = path_to_pipe_name(path);
   snprintf(buf, buf_sz, "\\\\.\\pipe\\%S", wname + 9);
}

int platform_ipc_listen(const char *path, int backlog)
{
   char pipe_name[256];
   pipe_name_a(path, pipe_name, sizeof(pipe_name));
   DWORD instances = (backlog > 0) ? (DWORD)backlog : PIPE_UNLIMITED_INSTANCES;
   HANDLE h = CreateNamedPipeA(pipe_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                               PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, instances, 4096,
                               4096, 0, NULL);
   if (h == INVALID_HANDLE_VALUE)
      return -1;
   return (int)(intptr_t)h;
}

int platform_ipc_accept(int listen_fd)
{
   HANDLE h = (HANDLE)(intptr_t)listen_fd;
   OVERLAPPED ov;
   ZeroMemory(&ov, sizeof(ov));
   ov.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
   if (!ov.hEvent)
      return -1;

   BOOL ok = ConnectNamedPipe(h, &ov);
   DWORD err = ok ? ERROR_SUCCESS : GetLastError();
   if (!ok && err == ERROR_PIPE_CONNECTED)
   {
      CloseHandle(ov.hEvent);
      return listen_fd;
   }
   if (!ok && err == ERROR_IO_PENDING)
   {
      WaitForSingleObject(ov.hEvent, INFINITE);
      DWORD transferred = 0;
      ok = GetOverlappedResult(h, &ov, &transferred, FALSE);
   }
   CloseHandle(ov.hEvent);
   return ok ? listen_fd : -1;
}

int platform_ipc_connect(const char *path, int timeout_ms)
{
   char pipe_name[256];
   pipe_name_a(path, pipe_name, sizeof(pipe_name));
   DWORD wait_ms = (timeout_ms <= 0) ? NMPWAIT_WAIT_FOREVER : (DWORD)timeout_ms;

   for (;;)
   {
      HANDLE h = CreateFileA(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
      if (h != INVALID_HANDLE_VALUE)
         return (int)(intptr_t)h;

      DWORD err = GetLastError();
      if (err != ERROR_PIPE_BUSY)
         return -1;
      if (!WaitNamedPipeA(pipe_name, wait_ms))
         return -1;
      if (timeout_ms > 0)
         wait_ms = timeout_ms > 50 ? (DWORD)(timeout_ms - 50) : 1;
   }
}

int platform_ipc_probe(const char *path)
{
   char pipe_name[256];
   pipe_name_a(path, pipe_name, sizeof(pipe_name));
   HANDLE h = CreateFileA(pipe_name, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                          FILE_ATTRIBUTE_NORMAL, NULL);
   if (h == INVALID_HANDLE_VALUE)
      return -1;
   CloseHandle(h);
   return 0;
}

void platform_ipc_close(int fd)
{
   HANDLE h = (HANDLE)(intptr_t)fd;
   if (h && h != INVALID_HANDLE_VALUE)
      CloseHandle(h);
}

int platform_ipc_peer_cred(int fd, platform_peer_cred_t *out)
{
   HANDLE h = (HANDLE)(intptr_t)fd;
   ULONG client_pid = 0;
   if (!GetNamedPipeClientProcessId(h, &client_pid))
      return -1;

   out->pid = (unsigned long)client_pid;
   out->uid = 0;
   out->gid = 0;

   HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, client_pid);
   if (!proc)
      return 0;

   HANDLE token = NULL;
   if (OpenProcessToken(proc, TOKEN_QUERY, &token))
   {
      DWORD needed = 0;
      GetTokenInformation(token, TokenUser, NULL, 0, &needed);
      if (needed > 0)
      {
         TOKEN_USER *user = (TOKEN_USER *)malloc(needed);
         if (user)
         {
            if (GetTokenInformation(token, TokenUser, user, needed, &needed))
               out->uid = 0;
            free(user);
         }
      }
      CloseHandle(token);
   }
   CloseHandle(proc);
   return 0;
}

void platform_ipc_cleanup_stale(const char *path)
{
   (void)path;
}
