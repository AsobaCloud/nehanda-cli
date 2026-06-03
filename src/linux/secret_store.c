/* secret_store.c: Linux secret backend (libsecret if available, file fallback) */
#include "secrets_internal.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

#if defined(WITH_LIBSECRET)
#include <libsecret/secret.h>

/* Designated initializers leave libsecret's private "reserved" fields zeroed
 * (avoids -Werror=missing-field-initializers). */
static const SecretSchema aimee_schema = {
    .name = "com.aimee.secret",
    .flags = SECRET_SCHEMA_NONE,
    .attributes = {{"service", SECRET_SCHEMA_ATTRIBUTE_STRING},
                   {"key", SECRET_SCHEMA_ATTRIBUTE_STRING}},
};

static int libsecret_store(const char *service, const char *key, const char *value)
{
   GError *err = NULL;
   char label[256];
   snprintf(label, sizeof(label), "aimee %s/%s", service, key);
   gboolean ok = secret_password_store_sync(&aimee_schema, SECRET_COLLECTION_DEFAULT, label, value,
                                            NULL, &err, "service", service, "key", key, NULL);
   if (err)
      g_error_free(err);
   return ok ? 0 : -1;
}

static int libsecret_load(const char *service, const char *key, char *buf, size_t len)
{
   GError *err = NULL;
   gchar *pw =
       secret_password_lookup_sync(&aimee_schema, NULL, &err, "service", service, "key", key, NULL);
   if (err)
      g_error_free(err);
   if (!pw)
      return -1;
   snprintf(buf, len, "%s", pw);
   secret_password_free(pw);
   return 0;
}

static int libsecret_remove(const char *service, const char *key)
{
   GError *err = NULL;
   gboolean ok =
       secret_password_clear_sync(&aimee_schema, NULL, &err, "service", service, "key", key, NULL);
   if (err)
      g_error_free(err);
   return ok ? 0 : -1;
}

static const db1_secret_backend_t libsecret_backend = {
    .store = libsecret_store,
    .load = libsecret_load,
    .remove = libsecret_remove,
    .name = "libsecret",
};

const db1_secret_backend_t *db1_secret_backend_detect(const db1_secret_backend_t *file_fallback)
{
   GError *err = NULL;
   gchar *pw = secret_password_lookup_sync(&aimee_schema, NULL, &err, "service", "aimee", "key",
                                           "_test", NULL);
   if (!err)
   {
      if (pw)
         secret_password_free(pw);

      const char *probe_key = "__aimee_probe";
      const char *probe_val = "1";
      gboolean ok = secret_password_store_sync(&aimee_schema, SECRET_COLLECTION_DEFAULT,
                                               "aimee probe", probe_val, NULL, &err, "service",
                                               "aimee", "key", probe_key, NULL);
      if (ok && !err)
      {
         gchar *probe_read = secret_password_lookup_sync(&aimee_schema, NULL, &err, "service",
                                                         "aimee", "key", probe_key, NULL);
         if (!err && probe_read && strcmp(probe_read, probe_val) == 0)
         {
            secret_password_free(probe_read);
            g_clear_error(&err);
            LOG_DEBUG("secret", "secret backend: libsecret");
            return &libsecret_backend;
         }

         if (probe_read)
            secret_password_free(probe_read);
      }

      secret_password_clear_sync(&aimee_schema, NULL, NULL, "service", "aimee", "key", probe_key,
                                 NULL);
      g_clear_error(&err);
   }

   g_error_free(err);
   LOG_DEBUG("secret", "secret backend: file");
   return file_fallback;
}

#else /* !WITH_LIBSECRET */

const db1_secret_backend_t *db1_secret_backend_detect(const db1_secret_backend_t *file_fallback)
{
   LOG_DEBUG("secret", "secret backend: file");
   return file_fallback;
}

#endif /* WITH_LIBSECRET */
