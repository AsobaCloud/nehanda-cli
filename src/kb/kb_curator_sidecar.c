/* kb_curator_sidecar.c: shared LLM-sidecar invocation. See header. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "kb_curator_sidecar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CS_DEFAULT_OUTBUF 8192

char *kb_curator_sidecar_run(const char *cmd, const char *json_input, int out_cap, char *errbuf,
                             size_t errlen)
{
   if (errbuf && errlen)
      errbuf[0] = '\0';
   if (!cmd || !cmd[0])
   {
      if (errbuf)
         snprintf(errbuf, errlen, "no command configured");
      return NULL;
   }
   size_t cap = out_cap > 0 ? (size_t)out_cap : CS_DEFAULT_OUTBUF;

   /* Write the request JSON to a temp file, then pipe it into the command. */
   char tmppath[256];
   snprintf(tmppath, sizeof(tmppath), "/tmp/aimee_curator_sidecar_XXXXXX");
   int fd = mkstemp(tmppath);
   if (fd < 0)
   {
      if (errbuf)
         snprintf(errbuf, errlen, "mkstemp failed");
      return NULL;
   }

   size_t inlen = json_input ? strlen(json_input) : 0;
   if (inlen && write(fd, json_input, inlen) != (ssize_t)inlen)
   {
      close(fd);
      unlink(tmppath);
      if (errbuf)
         snprintf(errbuf, errlen, "write to tmpfile failed");
      return NULL;
   }
   close(fd);

   char full_cmd[1024];
   snprintf(full_cmd, sizeof(full_cmd), "%s < %s", cmd, tmppath);

   FILE *fp = popen(full_cmd, "r");
   if (!fp)
   {
      unlink(tmppath);
      if (errbuf)
         snprintf(errbuf, errlen, "popen failed for: %s", full_cmd);
      return NULL;
   }

   char *out = malloc(cap);
   if (!out)
   {
      pclose(fp);
      unlink(tmppath);
      if (errbuf)
         snprintf(errbuf, errlen, "out of memory");
      return NULL;
   }

   size_t total = 0;
   size_t n;
   while ((n = fread(out + total, 1, cap - total - 1, fp)) > 0)
   {
      total += n;
      if (total >= cap - 1)
         break;
   }
   out[total] = '\0';
   int rc = pclose(fp);
   unlink(tmppath);

   if (rc != 0)
   {
      if (errbuf)
         snprintf(errbuf, errlen, "sidecar exited %d", rc);
      free(out);
      return NULL;
   }
   return out;
}
