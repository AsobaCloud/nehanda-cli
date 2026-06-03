/* platform_ipc.c: peer credential lookup via SO_PEERCRED (Linux) */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "platform_ipc.h"
#include <sys/socket.h>
#include <sys/un.h>

int platform_ipc_peer_cred(int fd, platform_peer_cred_t *out)
{
   struct ucred cred;
   socklen_t len = sizeof(cred);
   if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) < 0)
      return -1;
   out->uid = cred.uid;
   out->gid = cred.gid;
   out->pid = cred.pid;
   return 0;
}
