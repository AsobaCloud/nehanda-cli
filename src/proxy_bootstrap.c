/* proxy_bootstrap.c: Enterprise proxy and CA bundle detection from environment.
 *
 * Reads HTTPS_PROXY, HTTP_PROXY, NO_PROXY, SSL_CERT_FILE (and aliases) at
 * process startup and exposes a singleton for the HTTP client layer.
 *
 * NO_PROXY matching rules (matching curl/wget behaviour):
 *   - Exact hostname match: "github.com" bypasses proxy for "github.com".
 *   - Suffix match: "github.com" in NO_PROXY bypasses "api.github.com".
 *   - Leading-dot suffix: ".github.com" bypasses "api.github.com" but not
 *     "github.com" itself.
 *   - IP addresses are matched exactly.
 *   - CIDR ranges are not parsed (too complex for this layer). */

#include "proxy_bootstrap.h"
#include "log.h"
#include "platform_process.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Built-in hosts that should never go through a proxy */
static const char *const NO_PROXY_DEFAULTS[] = {
    "localhost",  "127.0.0.1",      "::1", "0.0.0.0", "anthropic.com", "api.anthropic.com",
    "github.com", "api.github.com", NULL,
};

/* ------------------------------------------------------------------ helpers */

/* Lowercase a string in-place, up to n bytes. */
static void str_lower(char *s, size_t n)
{
   for (size_t i = 0; i < n && s[i]; i++)
      s[i] = (char)tolower((unsigned char)s[i]);
}

/* Strip leading whitespace and return a pointer into the string. */
static const char *ltrim(const char *s)
{
   while (*s && isspace((unsigned char)*s))
      s++;
   return s;
}

/* Return the first non-empty value among a NULL-terminated list of
 * environment variable names. Returns NULL if all are unset or empty. */
static const char *getenv_first(const char *const *names)
{
   for (int i = 0; names[i]; i++)
   {
      const char *v = getenv(names[i]);
      if (v && v[0])
         return v;
   }
   return NULL;
}

/* ------------------------------------------------------------------ URL parser */

int proxy_parse_url(const char *url, proxy_config_t *out)
{
   if (!url || !*url || !out)
      return -1;

   memset(out, 0, sizeof(*out));
   out->port = 8080; /* sensible default */

   const char *p = url;

   /* Strip scheme: "http://" or "https://" */
   if (strncmp(p, "http://", 7) == 0)
      p += 7;
   else if (strncmp(p, "https://", 8) == 0)
      p += 8;

   /* Look for auth "user:pass@" */
   const char *at = strchr(p, '@');
   const char *colon_in_host = strchr(p, ':');
   if (at && (!colon_in_host || at < colon_in_host || colon_in_host < at))
   {
      /* auth present */
      size_t auth_len = (size_t)(at - p);
      if (auth_len >= PROXY_AUTH_LEN)
         auth_len = PROXY_AUTH_LEN - 1;
      memcpy(out->auth, p, auth_len);
      out->auth[auth_len] = '\0';
      p = at + 1;
   }

   /* Find port separator */
   const char *colon = strchr(p, ':');
   if (colon)
   {
      size_t host_len = (size_t)(colon - p);
      if (host_len >= PROXY_HOST_LEN)
         host_len = PROXY_HOST_LEN - 1;
      memcpy(out->host, p, host_len);
      out->host[host_len] = '\0';
      out->port = atoi(colon + 1);
      if (out->port <= 0 || out->port > 65535)
         out->port = 8080;
   }
   else
   {
      /* No port: just a hostname */
      size_t host_len = strlen(p);
      /* Strip trailing slash or path */
      const char *slash = strchr(p, '/');
      if (slash)
         host_len = (size_t)(slash - p);
      if (host_len >= PROXY_HOST_LEN)
         host_len = PROXY_HOST_LEN - 1;
      memcpy(out->host, p, host_len);
      out->host[host_len] = '\0';
   }

   /* Require a non-empty host */
   if (!out->host[0])
      return -1;

   str_lower(out->host, sizeof(out->host));
   return 0;
}

/* ------------------------------------------------------------------ NO_PROXY builder */

/* Append entry to buf (comma-separated, no duplicates). */
static void noproxy_append(char *buf, size_t cap, const char *entry)
{
   size_t cur_len = strlen(buf);

   /* Normalise entry: strip leading dot for comparison */
   const char *cmp = entry;
   if (*cmp == '.')
      cmp++;

   /* Scan existing entries for duplicate */
   const char *scan = buf;
   while (*scan)
   {
      const char *end = strchr(scan, ',');
      size_t elen = end ? (size_t)(end - scan) : strlen(scan);
      const char *ecmp = scan;
      if (*ecmp == '.')
         ecmp++;
      if (strlen(cmp) == (elen - (size_t)(ecmp - scan)) && strncasecmp(ecmp, cmp, strlen(cmp)) == 0)
         return; /* already present */
      scan += elen;
      if (*scan == ',')
         scan++;
   }

   size_t entry_len = strlen(entry);
   size_t needed = (cur_len ? 1 : 0) + entry_len + 1;
   if (cur_len + needed > cap)
      return; /* no room */

   if (cur_len)
      buf[cur_len++] = ',';
   memcpy(buf + cur_len, entry, entry_len + 1);
}

/* Build the merged NO_PROXY string: user value + defaults. */
static void build_no_proxy(char *buf, size_t cap, const char *user_noproxy)
{
   buf[0] = '\0';

   /* Start with user-supplied entries */
   if (user_noproxy && user_noproxy[0])
   {
      /* Split by comma and add each token */
      char tmp[PROXY_NO_PROXY_LEN];
      snprintf(tmp, sizeof(tmp), "%s", user_noproxy);
      char *tok = strtok(tmp, ",");
      while (tok)
      {
         tok = (char *)ltrim(tok);
         /* strip trailing whitespace */
         char *end = tok + strlen(tok);
         while (end > tok && isspace((unsigned char)end[-1]))
            *--end = '\0';
         if (*tok)
            noproxy_append(buf, cap, tok);
         tok = strtok(NULL, ",");
      }
   }

   /* Add built-in defaults */
   for (int i = 0; NO_PROXY_DEFAULTS[i]; i++)
      noproxy_append(buf, cap, NO_PROXY_DEFAULTS[i]);
}

/* ------------------------------------------------------------------ NO_PROXY matcher */

int proxy_should_use(const proxy_bootstrap_t *pb, const char *host)
{
   if (!pb || !pb->has_proxy || !host || !host[0])
      return 0;

   /* Lower-case copy of the target host */
   char lhost[PROXY_HOST_LEN];
   snprintf(lhost, sizeof(lhost), "%s", host);
   str_lower(lhost, sizeof(lhost));
   size_t hlen = strlen(lhost);

   /* Iterate comma-separated tokens in no_proxy */
   const char *p = pb->no_proxy;
   while (*p)
   {
      /* Skip leading commas/whitespace */
      while (*p == ',' || isspace((unsigned char)*p))
         p++;
      if (!*p)
         break;

      /* Find end of token */
      const char *tok_start = p;
      while (*p && *p != ',')
         p++;
      size_t tok_len = (size_t)(p - tok_start);

      /* Strip trailing whitespace */
      while (tok_len && isspace((unsigned char)tok_start[tok_len - 1]))
         tok_len--;

      if (!tok_len)
         continue;

      /* Leading dot: ".example.com" matches subdomains only.
       * No leading dot: "example.com" matches exact and subdomains. */
      int leading_dot = (tok_start[0] == '.');
      const char *match_str = tok_start + (leading_dot ? 1 : 0);
      size_t match_len = tok_len - (size_t)leading_dot;

      if (!leading_dot)
      {
         /* Exact match */
         if (hlen == match_len && strncasecmp(lhost, match_str, match_len) == 0)
            return 0;
         /* Suffix match: "example.com" bypasses "api.example.com" */
         if (hlen > match_len + 1 && lhost[hlen - match_len - 1] == '.' &&
             strncasecmp(lhost + hlen - match_len, match_str, match_len) == 0)
            return 0;
      }
      else
      {
         /* Leading dot: ".example.com" bypasses "api.example.com" but not "example.com" */
         if (hlen > match_len + 1 && lhost[hlen - match_len - 1] == '.' &&
             strncasecmp(lhost + hlen - match_len, match_str, match_len) == 0)
            return 0;
      }
   }

   return 1; /* use proxy */
}

/* ------------------------------------------------------------------ init */

void proxy_bootstrap_init(proxy_bootstrap_t *pb)
{
   if (!pb)
      return;
   memset(pb, 0, sizeof(*pb));

   /* Resolve HTTPS proxy */
   static const char *const PROXY_VARS[] = {
       "HTTPS_PROXY", "https_proxy", "HTTP_PROXY", "http_proxy", NULL,
   };
   const char *proxy_url = getenv_first(PROXY_VARS);
   if (proxy_url && proxy_parse_url(proxy_url, &pb->https_proxy) == 0)
   {
      pb->has_proxy = 1;
      aimee_log(LOG_INFO, "proxy", "proxy detected: %s:%d", pb->https_proxy.host,
                pb->https_proxy.port);
   }

   /* Build merged NO_PROXY */
   static const char *const NOPROXY_VARS[] = {"NO_PROXY", "no_proxy", NULL};
   const char *user_noproxy = getenv_first(NOPROXY_VARS);
   build_no_proxy(pb->no_proxy, sizeof(pb->no_proxy), user_noproxy);

   /* Resolve CA bundle */
   static const char *const CA_VARS[] = {
       "SSL_CERT_FILE", "REQUESTS_CA_BUNDLE", "NODE_EXTRA_CA_CERTS", "CURL_CA_BUNDLE", NULL,
   };
   const char *ca = getenv_first(CA_VARS);
   if (ca && ca[0])
   {
      snprintf(pb->ca_bundle, sizeof(pb->ca_bundle), "%s", ca);
      aimee_log(LOG_INFO, "proxy", "CA bundle: %s", pb->ca_bundle);
   }
}

/* ------------------------------------------------------------------ global singleton */

static proxy_bootstrap_t g_proxy;
static pthread_once_t g_proxy_once = PTHREAD_ONCE_INIT;

static void proxy_init_once(void)
{
   proxy_bootstrap_init(&g_proxy);

   /* Propagate merged NO_PROXY to the environment so subprocesses inherit it */
   if (g_proxy.no_proxy[0] && !getenv("NO_PROXY"))
      platform_setenv("NO_PROXY", g_proxy.no_proxy);

   /* Propagate normalised HTTPS_PROXY for subprocess inheritance */
   if (g_proxy.has_proxy)
   {
      char proxy_str[PROXY_HOST_LEN + 16];
      if (g_proxy.https_proxy.auth[0])
         snprintf(proxy_str, sizeof(proxy_str), "http://%s@%s:%d", g_proxy.https_proxy.auth,
                  g_proxy.https_proxy.host, g_proxy.https_proxy.port);
      else
         snprintf(proxy_str, sizeof(proxy_str), "http://%s:%d", g_proxy.https_proxy.host,
                  g_proxy.https_proxy.port);
      if (!getenv("HTTPS_PROXY"))
         platform_setenv("HTTPS_PROXY", proxy_str);
   }
}

void proxy_bootstrap_init_global(void)
{
   pthread_once(&g_proxy_once, proxy_init_once);
}

const proxy_bootstrap_t *proxy_get(void)
{
   return &g_proxy;
}
