/* platform_ipc.c: peer credential lookup via getpeereid (macOS) */
#include "platform_ipc.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int platform_ipc_peer_cred(int fd, platform_peer_cred_t *out)
{
   /* macOS: getpeereid() gives uid+gid, no pid */
   uid_t euid;
   gid_t egid;
   if (getpeereid(fd, &euid, &egid) < 0)
      return -1;
   out->uid = euid;
   out->gid = egid;
   out->pid = 0; /* not available via getpeereid */
   return 0;
}
