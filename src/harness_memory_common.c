/* harness_memory_common.c — see harness_memory_common.h. */

#include "harness_memory_common.h"

#include "cJSON.h"

#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Self-contained SHA-256 (FIPS 180-4) — avoids an OpenSSL link dependency on
 * aimee-core and is portable to the Windows (Schannel) build. We only compare
 * our own hashes, so this is purely an internal content fingerprint. */
static uint32_t sha_ror(uint32_t x, int n)
{
   return (x >> n) | (x << (32 - n));
}

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

static void sha256_block(uint32_t H[8], const uint8_t *p)
{
   uint32_t w[64];
   for (int i = 0; i < 16; i++)
      w[i] = ((uint32_t)p[i * 4] << 24) | ((uint32_t)p[i * 4 + 1] << 16) |
             ((uint32_t)p[i * 4 + 2] << 8) | (uint32_t)p[i * 4 + 3];
   for (int i = 16; i < 64; i++)
   {
      uint32_t s0 = sha_ror(w[i - 15], 7) ^ sha_ror(w[i - 15], 18) ^ (w[i - 15] >> 3);
      uint32_t s1 = sha_ror(w[i - 2], 17) ^ sha_ror(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
   }
   uint32_t a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
   for (int i = 0; i < 64; i++)
   {
      uint32_t S1 = sha_ror(e, 6) ^ sha_ror(e, 11) ^ sha_ror(e, 25);
      uint32_t ch = (e & f) ^ (~e & g);
      uint32_t t1 = h + S1 + ch + SHA256_K[i] + w[i];
      uint32_t S0 = sha_ror(a, 2) ^ sha_ror(a, 13) ^ sha_ror(a, 22);
      uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
      uint32_t t2 = S0 + maj;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
   }
   H[0] += a;
   H[1] += b;
   H[2] += c;
   H[3] += d;
   H[4] += e;
   H[5] += f;
   H[6] += g;
   H[7] += h;
}

static void hmem_sha256(const uint8_t *msg, size_t len, uint8_t out[32])
{
   uint32_t H[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
   uint64_t bits = (uint64_t)len * 8;
   size_t i = 0;
   while (len - i >= 64)
   {
      sha256_block(H, msg + i);
      i += 64;
   }
   uint8_t buf[64];
   size_t rem = len - i;
   memcpy(buf, msg + i, rem);
   buf[rem++] = 0x80;
   if (rem > 56)
   {
      memset(buf + rem, 0, 64 - rem);
      sha256_block(H, buf);
      memset(buf, 0, 56);
   }
   else
   {
      memset(buf + rem, 0, 56 - rem);
   }
   for (int k = 0; k < 8; k++)
      buf[56 + k] = (uint8_t)(bits >> (56 - 8 * k));
   sha256_block(H, buf);
   for (int k = 0; k < 8; k++)
   {
      out[k * 4] = (uint8_t)(H[k] >> 24);
      out[k * 4 + 1] = (uint8_t)(H[k] >> 16);
      out[k * 4 + 2] = (uint8_t)(H[k] >> 8);
      out[k * 4 + 3] = (uint8_t)H[k];
   }
}

static int cmp_keys(const void *a, const void *b)
{
   return strcmp(*(const char *const *)a, *(const char *const *)b);
}

char *hmem_canon_meta(const char *meta_json)
{
   cJSON *root = (meta_json && meta_json[0]) ? cJSON_Parse(meta_json) : NULL;
   if (!root || !cJSON_IsObject(root))
   {
      if (root)
         cJSON_Delete(root);
      return strdup("{}");
   }

   int n = cJSON_GetArraySize(root);
   const char **keys = calloc((size_t)(n > 0 ? n : 1), sizeof(*keys));
   if (!keys)
   {
      cJSON_Delete(root);
      return strdup("{}");
   }

   int k = 0;
   for (cJSON *it = root->child; it; it = it->next)
   {
      if (!it->string)
         continue;
      if (cJSON_IsNull(it))
         continue;
      if (cJSON_IsString(it) && (!it->valuestring || !it->valuestring[0]))
         continue;
      if ((cJSON_IsArray(it) || cJSON_IsObject(it)) && cJSON_GetArraySize(it) == 0)
         continue;
      keys[k++] = it->string;
   }
   qsort(keys, (size_t)k, sizeof(*keys), cmp_keys);

   cJSON *out = cJSON_CreateObject();
   for (int i = 0; out && i < k; i++)
   {
      cJSON *v = cJSON_GetObjectItem(root, keys[i]);
      if (v)
         cJSON_AddItemToObject(out, keys[i], cJSON_Duplicate(v, 1));
   }
   char *s = out ? cJSON_PrintUnformatted(out) : NULL;

   free(keys);
   cJSON_Delete(root);
   if (out)
      cJSON_Delete(out);
   return s ? s : strdup("{}");
}

int hmem_content_hash(const char *type, const char *name, const char *description, const char *body,
                      const char *meta_json, char out[HMEM_HASH_HEX_LEN])
{
   if (!out)
      return -1;
   char *cmeta = hmem_canon_meta(meta_json);
   if (!cmeta)
      return -1;

   const char *fields[5] = {type ? type : "", name ? name : "", description ? description : "",
                            body ? body : "", cmeta};

   size_t total = 1;
   for (int i = 0; i < 5; i++)
      total += 24 + strlen(fields[i]); /* "<len>:" + field + 0x1f */
   char *buf = malloc(total);
   if (!buf)
   {
      free(cmeta);
      return -1;
   }
   size_t pos = 0;
   for (int i = 0; i < 5; i++)
      pos += (size_t)snprintf(buf + pos, total - pos, "%zu:%s\x1f", strlen(fields[i]), fields[i]);

   uint8_t h[32];
   hmem_sha256((const uint8_t *)buf, pos, h);
   for (int i = 0; i < 32; i++)
      snprintf(out + i * 2, 3, "%02x", h[i]);
   out[64] = '\0';

   free(buf);
   free(cmeta);
   return 0;
}

/* git worktree toplevel of `cwd` via fork/exec (no shell, no injection).
 * Returns 0 + path in out on success; -1 otherwise. */
static int git_toplevel(const char *cwd, char *out, size_t cap)
{
   int fds[2];
   if (pipe(fds) != 0)
      return -1;
   pid_t pid = fork();
   if (pid < 0)
   {
      close(fds[0]);
      close(fds[1]);
      return -1;
   }
   if (pid == 0)
   {
      dup2(fds[1], STDOUT_FILENO);
      int devnull = open("/dev/null", O_WRONLY);
      if (devnull >= 0)
         dup2(devnull, STDERR_FILENO);
      close(fds[0]);
      close(fds[1]);
      setenv("GIT_TERMINAL_PROMPT", "0", 1);
      if (cwd && cwd[0] && chdir(cwd) != 0)
         _exit(127);
      execlp("git", "git", "rev-parse", "--show-toplevel", (char *)NULL);
      _exit(127);
   }
   close(fds[1]);
   char buf[PATH_MAX + 2];
   size_t off = 0;
   ssize_t r;
   while (off < sizeof(buf) - 1 && (r = read(fds[0], buf + off, sizeof(buf) - 1 - off)) > 0)
      off += (size_t)r;
   close(fds[0]);
   int status = 0;
   waitpid(pid, &status, 0);
   if (off == 0 || !(WIFEXITED(status) && WEXITSTATUS(status) == 0))
      return -1;
   buf[off] = '\0';
   while (off > 0 && (buf[off - 1] == '\n' || buf[off - 1] == '\r' || buf[off - 1] == ' '))
      buf[--off] = '\0';
   if (off == 0)
      return -1;
   snprintf(out, cap, "%s", buf);
   return 0;
}

int hmem_resolve_project(const char *cwd, char *id_out, size_t id_cap, char *root_out,
                         size_t root_cap)
{
   char root[PATH_MAX];
   root[0] = '\0';
   if (git_toplevel(cwd, root, sizeof(root)) != 0)
   {
      const char *c = (cwd && cwd[0]) ? cwd : ".";
      if (!realpath(c, root))
         return -1;
   }
   if (!root[0])
      return -1;
   if (root_out)
      snprintf(root_out, root_cap, "%s", root);

   const char *env = getenv("AIMEE_PROJECT_ID");
   if (env && env[0])
   {
      if (env[0] == '/')
      {
         /* path-shaped id must be an ancestor of realpath(cwd) */
         char rc[PATH_MAX];
         const char *c = (cwd && cwd[0]) ? cwd : ".";
         if (!realpath(c, rc))
            return -1;
         size_t el = strlen(env);
         if (strncmp(rc, env, el) != 0 || (rc[el] != '\0' && rc[el] != '/'))
            return -1;
      }
      if (id_out)
         snprintf(id_out, id_cap, "%s", env);
   }
   else if (id_out)
   {
      snprintf(id_out, id_cap, "%s", root);
   }
   return 0;
}
