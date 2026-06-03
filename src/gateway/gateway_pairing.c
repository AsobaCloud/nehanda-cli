/* gateway_pairing.c: file-backed DM pairing for the gateway runtime.
 *
 * Shares ~/.aimee/gateway-pairs.json with the `aimee gateway pair` CLI.
 * Entry schema (one object per array element):
 *   { "platform": "telegram", "user_id": "123", "code": "048213",
 *     "expires_at": <unix>, "approved": true|false (absent = pending) }
 */
#include "gateway_pairing.h"
#include "aimee_home.h"
#include "log.h"
#include <cJSON.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define PAIRING_TTL_SECONDS 3600

static pthread_mutex_t g_pairing_mutex = PTHREAD_MUTEX_INITIALIZER;

static void pairs_path(char *buf, size_t bufsz)
{
   snprintf(buf, bufsz, "%s/gateway-pairs.json", aimee_home());
}

static cJSON *pairs_load(const char *path)
{
   FILE *f = fopen(path, "rb");
   if (!f)
      return cJSON_CreateArray();
   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0)
   {
      fclose(f);
      return cJSON_CreateArray();
   }
   char *buf = malloc((size_t)sz + 1);
   if (!buf)
   {
      fclose(f);
      return cJSON_CreateArray();
   }
   size_t n = fread(buf, 1, (size_t)sz, f);
   buf[n] = '\0';
   fclose(f);
   cJSON *root = cJSON_Parse(buf);
   free(buf);
   if (!root || !cJSON_IsArray(root))
   {
      cJSON_Delete(root);
      return cJSON_CreateArray();
   }
   return root;
}

static int pairs_save(const char *path, cJSON *arr)
{
   char *s = cJSON_PrintUnformatted(arr);
   if (!s)
      return -1;
   FILE *f = fopen(path, "w");
   if (!f)
   {
      free(s);
      return -1;
   }
   fputs(s, f);
   fclose(f);
   free(s);
   chmod(path, 0600);
   return 0;
}

static cJSON *find_entry(cJSON *arr, const char *platform, const char *user_id)
{
   int n = cJSON_GetArraySize(arr);
   for (int i = 0; i < n; i++)
   {
      cJSON *e = cJSON_GetArrayItem(arr, i);
      const char *ep = cJSON_GetStringValue(cJSON_GetObjectItem(e, "platform"));
      const char *eu = cJSON_GetStringValue(cJSON_GetObjectItem(e, "user_id"));
      if (ep && eu && strcmp(ep, platform) == 0 && strcmp(eu, user_id) == 0)
         return e;
   }
   return NULL;
}

int gateway_pairing_is_approved(const char *platform, const char *user_id)
{
   if (!platform || !user_id || !platform[0] || !user_id[0])
      return 0;
   char path[512];
   pairs_path(path, sizeof(path));

   pthread_mutex_lock(&g_pairing_mutex);
   cJSON *arr = pairs_load(path);
   cJSON *e = find_entry(arr, platform, user_id);
   int approved = 0;
   if (e)
   {
      cJSON *japproved = cJSON_GetObjectItem(e, "approved");
      double exp = cJSON_GetNumberValue(cJSON_GetObjectItem(e, "expires_at"));
      /* Approved pairings do not expire; the expiry only bounds the pending
       * code window. Treat approved==true as authorized regardless of expiry. */
      if (cJSON_IsTrue(japproved))
         approved = 1;
      else
         (void)exp;
   }
   cJSON_Delete(arr);
   pthread_mutex_unlock(&g_pairing_mutex);
   return approved;
}

int gateway_pairing_issue_if_absent(const char *platform, const char *user_id, char *code_out,
                                    size_t code_size)
{
   if (!platform || !user_id || !platform[0] || !user_id[0] || !code_out || code_size < 7)
      return -1;
   char path[512];
   pairs_path(path, sizeof(path));

   pthread_mutex_lock(&g_pairing_mutex);
   cJSON *arr = pairs_load(path);
   cJSON *e = find_entry(arr, platform, user_id);
   if (e)
   {
      const char *code = cJSON_GetStringValue(cJSON_GetObjectItem(e, "code"));
      double exp = cJSON_GetNumberValue(cJSON_GetObjectItem(e, "expires_at"));
      int pending_expired =
          !cJSON_IsTrue(cJSON_GetObjectItem(e, "approved")) && (time_t)exp < time(NULL);
      if (code && code[0] && !pending_expired)
      {
         snprintf(code_out, code_size, "%s", code);
         cJSON_Delete(arr);
         pthread_mutex_unlock(&g_pairing_mutex);
         return 0;
      }
      /* Expired pending entry: detach and delete it, then issue a fresh code. */
      int count = cJSON_GetArraySize(arr);
      for (int i = 0; i < count; i++)
      {
         if (cJSON_GetArrayItem(arr, i) == e)
         {
            cJSON_DeleteItemFromArray(arr, i);
            break;
         }
      }
   }

   /* Generate a 6-digit code. arc4random is not available everywhere; use
    * time+counter seeding which is adequate for a short-lived OOB code. */
   static unsigned long counter = 0;
   counter++;
   unsigned long seed = (unsigned long)time(NULL) ^ (counter * 2654435761UL);
   int code_int = (int)(seed % 1000000UL);
   char code[8];
   snprintf(code, sizeof(code), "%06d", code_int);

   cJSON *entry = cJSON_CreateObject();
   cJSON_AddStringToObject(entry, "platform", platform);
   cJSON_AddStringToObject(entry, "user_id", user_id);
   cJSON_AddStringToObject(entry, "code", code);
   cJSON_AddNumberToObject(entry, "expires_at", (double)(time(NULL) + PAIRING_TTL_SECONDS));
   cJSON_AddItemToArray(arr, entry);

   int rc = pairs_save(path, arr);
   cJSON_Delete(arr);
   pthread_mutex_unlock(&g_pairing_mutex);
   if (rc != 0)
   {
      LOG_WARN("gateway", "failed to write pairing code to %s", path);
      return -1;
   }
   snprintf(code_out, code_size, "%s", code);
   return 0;
}
