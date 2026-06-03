/* aimee_tls.c: OpenSSL TLS client wrapper (WITH_TLS builds only). */
#include "aimee_tls.h"
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <stdlib.h>
#include <string.h>

struct aimee_tls
{
   SSL_CTX *ctx;
   SSL *ssl;
};

static int tls_insecure(void)
{
   const char *v = getenv("AIMEE_TLS_INSECURE");
   return v && *v && strcmp(v, "0") != 0;
}

aimee_tls_t *aimee_tls_connect(int fd, const char *host)
{
   SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
   if (!ctx)
      return NULL;
   SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);

   int insecure = tls_insecure();
   if (!insecure)
   {
      SSL_CTX_set_default_verify_paths(ctx);
      SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
   }

   SSL *ssl = SSL_new(ctx);
   if (!ssl)
   {
      SSL_CTX_free(ctx);
      return NULL;
   }
   SSL_set_fd(ssl, fd);
   if (host && *host)
   {
      SSL_set_tlsext_host_name(ssl, host); /* SNI */
      if (!insecure)
      {
         X509_VERIFY_PARAM *param = SSL_get0_param(ssl);
         X509_VERIFY_PARAM_set_hostflags(param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
         X509_VERIFY_PARAM_set1_host(param, host, 0);
      }
   }

   if (SSL_connect(ssl) != 1)
   {
      SSL_free(ssl);
      SSL_CTX_free(ctx);
      return NULL;
   }

   aimee_tls_t *t = calloc(1, sizeof(*t));
   if (!t)
   {
      SSL_free(ssl);
      SSL_CTX_free(ctx);
      return NULL;
   }
   t->ctx = ctx;
   t->ssl = ssl;
   return t;
}

int aimee_tls_write_all(aimee_tls_t *t, const void *buf, size_t len)
{
   const char *p = (const char *)buf;
   size_t off = 0;
   while (off < len)
   {
      int n = SSL_write(t->ssl, p + off, (int)(len - off));
      if (n <= 0)
         return -1;
      off += (size_t)n;
   }
   return 0;
}

long aimee_tls_read(aimee_tls_t *t, void *buf, size_t len)
{
   int n = SSL_read(t->ssl, buf, (int)len);
   return n < 0 ? -1 : (long)n;
}

void aimee_tls_free(aimee_tls_t *t)
{
   if (!t)
      return;
   if (t->ssl)
   {
      SSL_shutdown(t->ssl);
      SSL_free(t->ssl);
   }
   if (t->ctx)
      SSL_CTX_free(t->ctx);
   free(t);
}
