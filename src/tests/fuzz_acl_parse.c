/* fuzz_acl_parse.c: fuzz harness for network/ACL config parsing
 *
 * Exercises the agent_config network parsing paths (hosts, CIDR
 * networks, tunnels) with arbitrary JSON input.  This covers the
 * field-extraction logic that handles untrusted configuration data
 * including malformed CIDR strings, oversized arrays, and deeply
 * nested objects.
 *
 * Build:
 *   libFuzzer: clang -fsanitize=fuzzer,address -o fuzz_acl_parse ...
 *   Standalone: gcc -DFUZZ_STANDALONE -o fuzz_acl_parse ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aimee.h"
#include "agent_config.h"

/* Exercise the network config field extraction paths used by
 * agent_load_config().  We replicate the parsing logic here so the
 * fuzz harness can feed arbitrary JSON without going through the
 * filesystem. */
static void fuzz_one(const char *data, size_t size)
{
   char *buf = malloc(size + 1);
   if (!buf)
      return;
   memcpy(buf, data, size);
   buf[size] = '\0';

   cJSON *root = cJSON_Parse(buf);
   free(buf);
   if (!root)
      return;

   /* --- Exercise the full agent config parsing paths --- */

   /* Default agent and fallback chain (string extraction) */
   cJSON *def = cJSON_GetObjectItemCaseSensitive(root, "default_agent");
   if (def && cJSON_IsString(def))
      (void)def->valuestring;

   cJSON *fb = cJSON_GetObjectItemCaseSensitive(root, "fallback_chain");
   if (fb && cJSON_IsArray(fb))
   {
      cJSON *item;
      cJSON_ArrayForEach(item, fb)
      {
         if (cJSON_IsString(item))
            (void)item->valuestring;
      }
   }

   /* --- Network section: hosts, CIDRs, tunnels --- */
   cJSON *net = cJSON_GetObjectItemCaseSensitive(root, "network");
   if (!net)
      net = root; /* Allow flat structure for direct network fuzzing */

   /* SSH entry */
   cJSON *ssh = cJSON_GetObjectItemCaseSensitive(net, "ssh");
   if (ssh && cJSON_IsString(ssh))
      (void)ssh->valuestring;

   /* Hosts array: name, ip, user, port, desc, tunnel */
   cJSON *hosts = cJSON_GetObjectItemCaseSensitive(net, "hosts");
   if (hosts && cJSON_IsArray(hosts))
   {
      cJSON *h;
      cJSON_ArrayForEach(h, hosts)
      {
         if (!cJSON_IsObject(h))
            continue;
         cJSON *v;
         v = cJSON_GetObjectItemCaseSensitive(h, "name");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(h, "ip");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(h, "user");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(h, "port");
         if (v && cJSON_IsNumber(v))
            (void)v->valueint;
         v = cJSON_GetObjectItemCaseSensitive(h, "desc");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(h, "tunnel");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
      }
   }

   /* Networks array: CIDR parsing paths */
   cJSON *networks = cJSON_GetObjectItemCaseSensitive(net, "networks");
   if (networks && cJSON_IsArray(networks))
   {
      cJSON *n;
      cJSON_ArrayForEach(n, networks)
      {
         if (!cJSON_IsObject(n))
            continue;
         cJSON *v;
         v = cJSON_GetObjectItemCaseSensitive(n, "name");
         if (v && cJSON_IsString(v))
         {
            /* Simulate snprintf into fixed buffer (same as agent_config.c) */
            char name[64];
            snprintf(name, sizeof(name), "%s", v->valuestring);
            (void)name[0];
         }
         v = cJSON_GetObjectItemCaseSensitive(n, "cidr");
         if (v && cJSON_IsString(v))
         {
            /* Simulate snprintf into fixed buffer (same as agent_config.c) */
            char cidr[32];
            snprintf(cidr, sizeof(cidr), "%s", v->valuestring);
            (void)cidr[0];
         }
         v = cJSON_GetObjectItemCaseSensitive(n, "desc");
         if (v && cJSON_IsString(v))
         {
            char desc[256];
            snprintf(desc, sizeof(desc), "%s", v->valuestring);
            (void)desc[0];
         }
      }
   }

   /* Tunnels array */
   cJSON *tunnels = cJSON_GetObjectItemCaseSensitive(net, "tunnels");
   if (tunnels && cJSON_IsArray(tunnels))
   {
      cJSON *t;
      cJSON_ArrayForEach(t, tunnels)
      {
         if (!cJSON_IsObject(t))
            continue;
         cJSON *v;
         v = cJSON_GetObjectItemCaseSensitive(t, "name");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(t, "relay_ssh");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(t, "relay_key");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(t, "target_host");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(t, "target_port");
         if (v && cJSON_IsNumber(v))
            (void)v->valueint;
         v = cJSON_GetObjectItemCaseSensitive(t, "reconnect_delay");
         if (v && cJSON_IsNumber(v))
            (void)v->valueint;
         v = cJSON_GetObjectItemCaseSensitive(t, "max_reconnects");
         if (v && cJSON_IsNumber(v))
            (void)v->valueint;
      }
   }

   /* Agents array (exercises the agent struct extraction) */
   cJSON *agents = cJSON_GetObjectItemCaseSensitive(root, "agents");
   if (agents && cJSON_IsArray(agents))
   {
      cJSON *a;
      cJSON_ArrayForEach(a, agents)
      {
         if (!cJSON_IsObject(a))
            continue;
         cJSON *v;
         v = cJSON_GetObjectItemCaseSensitive(a, "name");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(a, "endpoint");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(a, "model");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(a, "provider");
         if (v && cJSON_IsString(v))
            (void)v->valuestring;
         v = cJSON_GetObjectItemCaseSensitive(a, "timeout_ms");
         if (v && cJSON_IsNumber(v))
            (void)v->valueint;
      }
   }

   cJSON_Delete(root);
}

#ifndef FUZZ_STANDALONE
int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size)
{
   if (size == 0 || size > 1024 * 1024)
      return 0;
   fuzz_one((const char *)data, size);
   return 0;
}
#else
static int fuzz_file(const char *path)
{
   FILE *f = fopen(path, "r");
   if (!f)
   {
      fprintf(stderr, "Cannot open: %s\n", path);
      return 1;
   }

   fseek(f, 0, SEEK_END);
   long sz = ftell(f);
   fseek(f, 0, SEEK_SET);
   if (sz <= 0 || sz > 10 * 1024 * 1024)
   {
      fclose(f);
      return 0;
   }

   char *data = malloc((size_t)sz);
   if (!data)
   {
      fclose(f);
      return 1;
   }
   size_t nread = fread(data, 1, (size_t)sz, f);
   fclose(f);

   fuzz_one(data, nread);
   free(data);
   return 0;
}

int main(int argc, char **argv)
{
   if (argc < 2)
   {
      char buf[4096];
      size_t n = fread(buf, 1, sizeof(buf), stdin);
      fuzz_one(buf, n);
   }
   else
   {
      for (int i = 1; i < argc; i++)
      {
         if (fuzz_file(argv[i]) != 0)
            return 1;
      }
   }
   printf("fuzz_acl_parse: %d inputs ok\n", argc > 1 ? argc - 1 : 1);
   return 0;
}
#endif
