/* cmd_infra.c: POSIX background index scan on commit (forwards to aimee-kb),
 * and gateway pair management via ~/.config/aimee/gateway-pairs.json. */
#include "aimee.h"
#include "config.h"
#include "commands.h"
#include "kb_client.h"
#include "aimee_home.h"
#include <cJSON.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void platform_infra_background_scan(const char *cwd)
{
   pid_t pid = fork();
   if (pid == 0)
   {
      const char *proj_name = strrchr(cwd, '/');
      proj_name = proj_name ? proj_name + 1 : cwd;
      kb_client_index_scan_result_t res;
      (void)kb_client_index_scan(proj_name, cwd, 0, &res);
      _exit(0);
   }
   if (pid > 0)
      waitpid(pid, NULL, WNOHANG);
}

/* ---- aimee gateway pair ---- */

/* Gateway pairs are stored in ~/.config/aimee/gateway-pairs.json so both
 * the aimee CLI and the aimee-gateway binary can read and write them
 * without needing an IPC channel in Phase 1. */

static void gateway_pairs_path(char *buf, size_t bufsz)
{
   snprintf(buf, bufsz, "%s/gateway-pairs.json", aimee_home());
}

static cJSON *gateway_pairs_load(const char *path)
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
   (void)fread(buf, 1, (size_t)sz, f);
   buf[sz] = '\0';
   fclose(f);
   cJSON *root = cJSON_Parse(buf);
   free(buf);
   return root ? root : cJSON_CreateArray();
}

static int gateway_pairs_save(const char *path, cJSON *arr)
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

static void cmd_gateway_pair_list(const char *path)
{
   cJSON *arr = gateway_pairs_load(path);
   int n = cJSON_GetArraySize(arr);
   if (n == 0)
   {
      printf("No pairings.\n");
      cJSON_Delete(arr);
      return;
   }
   printf("%-12s %-24s %-8s %-8s %s\n", "PLATFORM", "USER_ID", "CODE", "STATUS", "EXPIRES");
   for (int i = 0; i < n; i++)
   {
      cJSON *e = cJSON_GetArrayItem(arr, i);
      const char *platform = cJSON_GetStringValue(cJSON_GetObjectItem(e, "platform"));
      const char *user_id = cJSON_GetStringValue(cJSON_GetObjectItem(e, "user_id"));
      const char *code = cJSON_GetStringValue(cJSON_GetObjectItem(e, "code"));
      int approved = cJSON_IsTrue(cJSON_GetObjectItem(e, "approved"));
      int revoked = cJSON_IsFalse(cJSON_GetObjectItem(e, "approved")) &&
                    cJSON_GetObjectItem(e, "approved") != NULL;
      long expires = (long)cJSON_GetNumberValue(cJSON_GetObjectItem(e, "expires_at"));
      char exp_buf[32];
      time_t t = (time_t)expires;
      struct tm *tm = localtime(&t);
      if (tm)
         strftime(exp_buf, sizeof(exp_buf), "%Y-%m-%d %H:%M", tm);
      else
         snprintf(exp_buf, sizeof(exp_buf), "%ld", expires);
      const char *status = approved ? "approved" : (revoked ? "revoked" : "pending");
      printf("%-12s %-24s %-8s %-8s %s\n", platform ? platform : "?", user_id ? user_id : "?",
             code ? code : "?", status, exp_buf);
   }
   cJSON_Delete(arr);
}

static void cmd_gateway_pair_issue(const char *path, const char *platform, const char *user_id,
                                   int ttl_s)
{
   cJSON *arr = gateway_pairs_load(path);
   /* Remove existing pending entry for this user */
   for (int i = cJSON_GetArraySize(arr) - 1; i >= 0; i--)
   {
      cJSON *e = cJSON_GetArrayItem(arr, i);
      const char *ep = cJSON_GetStringValue(cJSON_GetObjectItem(e, "platform"));
      const char *eu = cJSON_GetStringValue(cJSON_GetObjectItem(e, "user_id"));
      if (ep && eu && strcmp(ep, platform) == 0 && strcmp(eu, user_id) == 0)
         cJSON_DeleteItemFromArray(arr, i);
   }
   srand((unsigned)time(NULL) ^ (unsigned)getpid());
   int code_int = rand() % 1000000;
   char code[8];
   snprintf(code, sizeof(code), "%06d", code_int);
   time_t expires = time(NULL) + ttl_s;
   cJSON *entry = cJSON_CreateObject();
   cJSON_AddStringToObject(entry, "platform", platform);
   cJSON_AddStringToObject(entry, "user_id", user_id);
   cJSON_AddStringToObject(entry, "code", code);
   cJSON_AddNumberToObject(entry, "expires_at", (double)expires);
   /* Do not set "approved" yet; pending state */
   cJSON_AddItemToArray(arr, entry);
   if (gateway_pairs_save(path, arr) == 0)
      printf("Code: %s  (expires in %ds; use 'aimee gateway pair approve %s' to authorize)\n", code,
             ttl_s, code);
   else
      fprintf(stderr, "aimee gateway: failed to save pairs to %s\n", path);
   cJSON_Delete(arr);
}

static void cmd_gateway_pair_approve(const char *path, const char *code)
{
   cJSON *arr = gateway_pairs_load(path);
   int found = 0;
   for (int i = 0; i < cJSON_GetArraySize(arr); i++)
   {
      cJSON *e = cJSON_GetArrayItem(arr, i);
      const char *ec = cJSON_GetStringValue(cJSON_GetObjectItem(e, "code"));
      if (ec && strcmp(ec, code) == 0)
      {
         double exp = cJSON_GetNumberValue(cJSON_GetObjectItem(e, "expires_at"));
         if ((time_t)exp < time(NULL))
         {
            fprintf(stderr, "aimee gateway: code expired\n");
            cJSON_Delete(arr);
            return;
         }
         cJSON_DeleteItemFromObject(e, "approved");
         cJSON_AddBoolToObject(e, "approved", 1);
         found = 1;
         break;
      }
   }
   if (found)
   {
      if (gateway_pairs_save(path, arr) == 0)
         printf("approved\n");
      else
         fprintf(stderr, "aimee gateway: save failed\n");
   }
   else
      fprintf(stderr, "aimee gateway: code not found\n");
   cJSON_Delete(arr);
}

static void cmd_gateway_pair_revoke(const char *path, const char *platform, const char *user_id)
{
   cJSON *arr = gateway_pairs_load(path);
   int found = 0;
   for (int i = 0; i < cJSON_GetArraySize(arr); i++)
   {
      cJSON *e = cJSON_GetArrayItem(arr, i);
      const char *ep = cJSON_GetStringValue(cJSON_GetObjectItem(e, "platform"));
      const char *eu = cJSON_GetStringValue(cJSON_GetObjectItem(e, "user_id"));
      if (ep && eu && strcmp(ep, platform) == 0 && strcmp(eu, user_id) == 0)
      {
         cJSON_DeleteItemFromObject(e, "approved");
         cJSON_AddBoolToObject(e, "approved", 0);
         found = 1;
      }
   }
   if (found)
   {
      if (gateway_pairs_save(path, arr) == 0)
         printf("revoked\n");
      else
         fprintf(stderr, "aimee gateway: save failed\n");
   }
   else
      fprintf(stderr, "aimee gateway: pairing not found for %s/%s\n", platform, user_id);
   cJSON_Delete(arr);
}

void cmd_gateway(app_ctx_t *ctx, int argc, char **argv)
{
   (void)ctx;
   char path[512];
   gateway_pairs_path(path, sizeof(path));

   if (argc < 1)
   {
      printf("Usage: aimee gateway <subcommand>\n");
      printf("  pair list                         — list all pairings\n");
      printf("  pair issue <platform> <user_id>   — generate a pairing code\n");
      printf("  pair approve <code>               — approve a pairing code\n");
      printf("  pair revoke <platform> <user_id>  — revoke a pairing\n");
      return;
   }

   if (strcmp(argv[0], "pair") == 0)
   {
      if (argc < 2)
      {
         fprintf(stderr, "aimee gateway pair: subcommand required (list|issue|approve|revoke)\n");
         return;
      }
      if (strcmp(argv[1], "list") == 0)
         cmd_gateway_pair_list(path);
      else if (strcmp(argv[1], "issue") == 0)
      {
         if (argc < 4)
         {
            fprintf(stderr, "aimee gateway pair issue: <platform> <user_id> required\n");
            return;
         }
         cmd_gateway_pair_issue(path, argv[2], argv[3], 300);
      }
      else if (strcmp(argv[1], "approve") == 0)
      {
         if (argc < 3)
         {
            fprintf(stderr, "aimee gateway pair approve: <code> required\n");
            return;
         }
         cmd_gateway_pair_approve(path, argv[2]);
      }
      else if (strcmp(argv[1], "revoke") == 0)
      {
         if (argc < 4)
         {
            fprintf(stderr, "aimee gateway pair revoke: <platform> <user_id> required\n");
            return;
         }
         cmd_gateway_pair_revoke(path, argv[2], argv[3]);
      }
      else
         fprintf(stderr, "aimee gateway pair: unknown subcommand '%s'\n", argv[1]);
      return;
   }
   fprintf(stderr, "aimee gateway: unknown subcommand '%s'\n", argv[0]);
}
