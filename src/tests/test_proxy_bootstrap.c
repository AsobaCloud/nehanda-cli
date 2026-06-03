/* test_proxy_bootstrap.c: unit tests for proxy_bootstrap */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../headers/proxy_bootstrap.h"

#define PASS(name) printf("  PASS: %s\n", (name))

/* ------------------------------------------------------------------ proxy_parse_url */

static void test_parse_url_host_port(void)
{
   proxy_config_t cfg;
   int rc = proxy_parse_url("http://proxy.corp.example.com:3128", &cfg);
   assert(rc == 0);
   assert(strcmp(cfg.host, "proxy.corp.example.com") == 0);
   assert(cfg.port == 3128);
   assert(cfg.auth[0] == '\0');
   PASS("parse_url_host_port");
}

static void test_parse_url_with_auth(void)
{
   proxy_config_t cfg;
   int rc = proxy_parse_url("http://alice:secret@proxy.example.com:8080", &cfg);
   assert(rc == 0);
   assert(strcmp(cfg.host, "proxy.example.com") == 0);
   assert(cfg.port == 8080);
   assert(strcmp(cfg.auth, "alice:secret") == 0);
   PASS("parse_url_with_auth");
}

static void test_parse_url_no_scheme(void)
{
   proxy_config_t cfg;
   int rc = proxy_parse_url("proxy.example.com:1234", &cfg);
   assert(rc == 0);
   assert(strcmp(cfg.host, "proxy.example.com") == 0);
   assert(cfg.port == 1234);
   PASS("parse_url_no_scheme");
}

static void test_parse_url_default_port(void)
{
   proxy_config_t cfg;
   int rc = proxy_parse_url("http://proxy.example.com", &cfg);
   assert(rc == 0);
   assert(strcmp(cfg.host, "proxy.example.com") == 0);
   assert(cfg.port == 8080); /* default */
   PASS("parse_url_default_port");
}

static void test_parse_url_null_returns_error(void)
{
   proxy_config_t cfg;
   assert(proxy_parse_url(NULL, &cfg) == -1);
   assert(proxy_parse_url("", &cfg) == -1);
   PASS("parse_url_null_returns_error");
}

static void test_parse_url_host_lowercased(void)
{
   proxy_config_t cfg;
   int rc = proxy_parse_url("http://PROXY.CORP.COM:3128", &cfg);
   assert(rc == 0);
   assert(strcmp(cfg.host, "proxy.corp.com") == 0);
   PASS("parse_url_host_lowercased");
}

/* ------------------------------------------------------------------ proxy_should_use */

/* Build a minimal proxy_bootstrap_t with a proxy set and the given no_proxy list. */
static proxy_bootstrap_t make_pb(const char *no_proxy_str)
{
   proxy_bootstrap_t pb;
   memset(&pb, 0, sizeof(pb));
   pb.has_proxy = 1;
   snprintf(pb.https_proxy.host, sizeof(pb.https_proxy.host), "proxy.example.com");
   pb.https_proxy.port = 3128;
   if (no_proxy_str)
      snprintf(pb.no_proxy, sizeof(pb.no_proxy), "%s", no_proxy_str);
   return pb;
}

static void test_should_use_empty_noproxy(void)
{
   proxy_bootstrap_t pb = make_pb("");
   assert(proxy_should_use(&pb, "api.example.com") == 1);
   PASS("should_use_empty_noproxy");
}

static void test_should_use_exact_match_bypassed(void)
{
   proxy_bootstrap_t pb = make_pb("api.example.com");
   assert(proxy_should_use(&pb, "api.example.com") == 0);
   PASS("should_use_exact_match_bypassed");
}

static void test_should_use_suffix_match_bypassed(void)
{
   proxy_bootstrap_t pb = make_pb("example.com");
   assert(proxy_should_use(&pb, "api.example.com") == 0);
   assert(proxy_should_use(&pb, "www.example.com") == 0);
   assert(proxy_should_use(&pb, "example.com") == 0); /* exact also matches */
   PASS("should_use_suffix_match_bypassed");
}

static void test_should_use_leading_dot_bypasses_subdomain_only(void)
{
   proxy_bootstrap_t pb = make_pb(".example.com");
   assert(proxy_should_use(&pb, "api.example.com") == 0); /* subdomain: bypassed */
   assert(proxy_should_use(&pb, "example.com") == 1);     /* no dot prefix: proxied */
   PASS("should_use_leading_dot_bypasses_subdomain_only");
}

static void test_should_use_no_partial_match(void)
{
   proxy_bootstrap_t pb = make_pb("ample.com");
   /* "ample.com" should NOT match "example.com" */
   assert(proxy_should_use(&pb, "example.com") == 1);
   PASS("should_use_no_partial_match");
}

static void test_should_use_localhost_always_bypassed(void)
{
   /* proxy_bootstrap_init merges default NO_PROXY hosts including localhost */
   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   if (pb.has_proxy)
   {
      assert(proxy_should_use(&pb, "localhost") == 0);
      assert(proxy_should_use(&pb, "127.0.0.1") == 0);
   }
   PASS("should_use_localhost_always_bypassed");
}

static void test_should_use_multiple_entries(void)
{
   proxy_bootstrap_t pb = make_pb("foo.com, bar.com, baz.com");
   assert(proxy_should_use(&pb, "foo.com") == 0);
   assert(proxy_should_use(&pb, "bar.com") == 0);
   assert(proxy_should_use(&pb, "baz.com") == 0);
   assert(proxy_should_use(&pb, "qux.com") == 1);
   PASS("should_use_multiple_entries");
}

static void test_should_use_no_proxy_false(void)
{
   proxy_bootstrap_t pb;
   memset(&pb, 0, sizeof(pb));
   pb.has_proxy = 0; /* no proxy configured */
   assert(proxy_should_use(&pb, "anything.com") == 0);
   PASS("should_use_no_proxy_false");
}

static void test_should_use_case_insensitive(void)
{
   proxy_bootstrap_t pb = make_pb("EXAMPLE.COM");
   assert(proxy_should_use(&pb, "EXAMPLE.COM") == 0);
   assert(proxy_should_use(&pb, "example.com") == 0);
   assert(proxy_should_use(&pb, "api.EXAMPLE.COM") == 0);
   PASS("should_use_case_insensitive");
}

/* ------------------------------------------------------------------ proxy_bootstrap_init */

static void test_init_no_proxy_env(void)
{
   /* Without proxy env vars, has_proxy should be 0 */
   unsetenv("HTTPS_PROXY");
   unsetenv("https_proxy");
   unsetenv("HTTP_PROXY");
   unsetenv("http_proxy");

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(pb.has_proxy == 0);
   /* Default NO_PROXY hosts should still be populated */
   assert(strstr(pb.no_proxy, "localhost") != NULL);
   assert(strstr(pb.no_proxy, "127.0.0.1") != NULL);
   PASS("init_no_proxy_env");
}

static void test_init_with_proxy_env(void)
{
   setenv("HTTPS_PROXY", "http://proxy.example.com:3128", 1);

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(pb.has_proxy == 1);
   assert(strcmp(pb.https_proxy.host, "proxy.example.com") == 0);
   assert(pb.https_proxy.port == 3128);

   unsetenv("HTTPS_PROXY");
   PASS("init_with_proxy_env");
}

static void test_init_merges_user_and_default_noproxy(void)
{
   setenv("HTTPS_PROXY", "http://proxy.example.com:3128", 1);
   setenv("NO_PROXY", "myinternalhost.corp", 1);

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(pb.has_proxy == 1);
   /* User entry */
   assert(strstr(pb.no_proxy, "myinternalhost.corp") != NULL);
   /* Built-in defaults */
   assert(strstr(pb.no_proxy, "localhost") != NULL);
   assert(strstr(pb.no_proxy, "anthropic.com") != NULL);

   unsetenv("HTTPS_PROXY");
   unsetenv("NO_PROXY");
   PASS("init_merges_user_and_default_noproxy");
}

static void test_init_ca_bundle_from_env(void)
{
   setenv("SSL_CERT_FILE", "/etc/ssl/certs/ca-certificates.crt", 1);

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(strcmp(pb.ca_bundle, "/etc/ssl/certs/ca-certificates.crt") == 0);

   unsetenv("SSL_CERT_FILE");
   PASS("init_ca_bundle_from_env");
}

static void test_init_ca_bundle_fallback_vars(void)
{
   unsetenv("SSL_CERT_FILE");
   setenv("REQUESTS_CA_BUNDLE", "/custom/ca.pem", 1);

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(strcmp(pb.ca_bundle, "/custom/ca.pem") == 0);

   unsetenv("REQUESTS_CA_BUNDLE");
   PASS("init_ca_bundle_fallback_vars");
}

static void test_init_anthropic_and_github_bypassed(void)
{
   setenv("HTTPS_PROXY", "http://proxy.example.com:3128", 1);
   unsetenv("NO_PROXY");

   proxy_bootstrap_t pb;
   proxy_bootstrap_init(&pb);
   assert(pb.has_proxy == 1);
   /* Anthropic and GitHub APIs should be in NO_PROXY by default */
   assert(proxy_should_use(&pb, "api.anthropic.com") == 0);
   assert(proxy_should_use(&pb, "anthropic.com") == 0);
   assert(proxy_should_use(&pb, "github.com") == 0);
   assert(proxy_should_use(&pb, "api.github.com") == 0);

   unsetenv("HTTPS_PROXY");
   PASS("init_anthropic_and_github_bypassed");
}

/* ------------------------------------------------------------------ main */

int main(void)
{
   printf("proxy_bootstrap:\n");

   test_parse_url_host_port();
   test_parse_url_with_auth();
   test_parse_url_no_scheme();
   test_parse_url_default_port();
   test_parse_url_null_returns_error();
   test_parse_url_host_lowercased();

   test_should_use_empty_noproxy();
   test_should_use_exact_match_bypassed();
   test_should_use_suffix_match_bypassed();
   test_should_use_leading_dot_bypasses_subdomain_only();
   test_should_use_no_partial_match();
   test_should_use_localhost_always_bypassed();
   test_should_use_multiple_entries();
   test_should_use_no_proxy_false();
   test_should_use_case_insensitive();

   test_init_no_proxy_env();
   test_init_with_proxy_env();
   test_init_merges_user_and_default_noproxy();
   test_init_ca_bundle_from_env();
   test_init_ca_bundle_fallback_vars();
   test_init_anthropic_and_github_bypassed();

   printf("all proxy_bootstrap tests passed\n");
   return 0;
}
