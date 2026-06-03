/* secret_store.c: Windows Credential Manager backend */
#include "secrets_internal.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <wincred.h>

static int windows_target_name(const char *service, const char *key, char *buf, size_t len)
{
   int n = snprintf(buf, len, "aimee/%s/%s", service, key);
   if (n < 0 || (size_t)n >= len)
      return -1;
   return 0;
}

static int wincred_store(const char *service, const char *key, const char *value)
{
   char target[512];
   CREDENTIALA cred;

   if (windows_target_name(service, key, target, sizeof(target)) != 0)
      return -1;

   memset(&cred, 0, sizeof(cred));
   cred.Type = CRED_TYPE_GENERIC;
   cred.TargetName = target;
   cred.CredentialBlobSize = (DWORD)strlen(value);
   cred.CredentialBlob = (LPBYTE)value;
   cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
   cred.UserName = (LPSTR)key;

   return CredWriteA(&cred, 0) ? 0 : -1;
}

static int wincred_load(const char *service, const char *key, char *buf, size_t len)
{
   char target[512];
   PCREDENTIALA cred = NULL;
   size_t copy;

   if (windows_target_name(service, key, target, sizeof(target)) != 0)
      return -1;

   if (!CredReadA(target, CRED_TYPE_GENERIC, 0, &cred) || !cred)
      return -1;

   copy = cred->CredentialBlobSize < len - 1 ? cred->CredentialBlobSize : len - 1;
   memcpy(buf, cred->CredentialBlob, copy);
   buf[copy] = '\0';
   CredFree(cred);
   return 0;
}

static int wincred_remove(const char *service, const char *key)
{
   char target[512];

   if (windows_target_name(service, key, target, sizeof(target)) != 0)
      return -1;

   return CredDeleteA(target, CRED_TYPE_GENERIC, 0) ? 0 : -1;
}

static const db1_secret_backend_t wincred_backend = {
    .store = wincred_store,
    .load = wincred_load,
    .remove = wincred_remove,
    .name = "wincred",
};

const db1_secret_backend_t *db1_secret_backend_detect(const db1_secret_backend_t *file_fallback)
{
   (void)file_fallback;
   LOG_DEBUG("secret", "secret backend: wincred");
   return &wincred_backend;
}
