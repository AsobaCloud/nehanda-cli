/* platform_ipc.c: Unix domain socket IPC (POSIX) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "platform_ipc.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

int platform_ipc_listen(const char *path, int backlog)
{
   /* Ensure parent directory exists */
   char dir[4096];
   snprintf(dir, sizeof(dir), "%s", path);
   char *slash = strrchr(dir, '/');
   if (slash)
   {
      *slash = '\0';
      mkdir(dir, 0700);
   }

   int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0)
      return -1;
   fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

   mode_t old_umask = umask(0077);
   int rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr));
   umask(old_umask);

   if (rc < 0)
   {
      close(fd);
      return -1;
   }

   if (listen(fd, backlog) < 0)
   {
      close(fd);
      unlink(path);
      return -1;
   }

   chmod(path, 0600);
   return fd;
}

int platform_ipc_accept(int listen_fd)
{
   int fd = accept4(listen_fd, NULL, NULL, SOCK_CLOEXEC | SOCK_NONBLOCK);
   if (fd < 0)
      return -1;
   return fd;
}

int platform_ipc_connect(const char *path, int timeout_ms)
{
   int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0)
      return -1;

   /* Set non-blocking for timeout support */
   int flags = fcntl(fd, F_GETFL, 0);
   if (flags >= 0)
      fcntl(fd, F_SETFL, flags | O_NONBLOCK);

   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

   int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
   if (rc == 0)
      goto connected;

   if (errno != EINPROGRESS)
   {
      close(fd);
      return -1;
   }

   /* Wait for connection */
   struct pollfd pfd = {.fd = fd, .events = POLLOUT};
   rc = poll(&pfd, 1, timeout_ms);
   if (rc <= 0)
   {
      close(fd);
      return -1;
   }

   int err = 0;
   socklen_t errlen = sizeof(err);
   getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
   if (err != 0)
   {
      close(fd);
      return -1;
   }

connected:
   /* Restore blocking mode */
   if (flags >= 0)
      fcntl(fd, F_SETFL, flags);

   return fd;
}

int platform_ipc_probe(const char *path)
{
   int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
   if (fd < 0)
      return -1;
   fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

   struct sockaddr_un addr;
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);

   int rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
   close(fd);
   return rc;
}

void platform_ipc_close(int fd)
{
   if (fd >= 0)
      close(fd);
}

void platform_ipc_cleanup_stale(const char *path)
{
   struct stat st;
   if (lstat(path, &st) != 0)
      return;

   /* Reject symlinks */
   if (S_ISLNK(st.st_mode))
      return;

   if (!S_ISSOCK(st.st_mode))
      return;

   /* Try connecting to see if a server is running */
   if (platform_ipc_probe(path) == 0)
      return; /* Server is alive, do not remove */

   /* Stale socket */
   unlink(path);
}
